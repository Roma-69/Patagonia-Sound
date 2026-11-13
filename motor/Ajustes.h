#pragma once
// Ajustes y diagnóstico de Patagonia Sound FDX, en la carpeta C:\ProgramData\Patagonia Sound:
//   ajustes.ini -> lo que el usuario elige:
//                  [motor]        activo, graves, ganancia, limitador
//                  [ecualizador]  b31 b62 b125 b250 b500 b1k b2k b4k b8k b16k (dB)
//   estado.txt  -> lo escribe el motor cada vez que arranca un sonido (si Windows lo deja).
// Solo se usa fuera del tiempo real (LockForProcess).

#include <windows.h>
#include <cwchar>
#include <cstdio>
#include "Procesador.h"

struct Ajustes {
    static void carpeta(wchar_t* destino, size_t largo) {
        wchar_t base[MAX_PATH] = L"";
        if (!GetEnvironmentVariableW(L"ProgramData", base, MAX_PATH)) wcscpy_s(base, L"C:\\ProgramData");
        swprintf(destino, largo, L"%ls\\Patagonia Sound", base);
    }

    // GetPrivateProfileInt no acepta negativos (devuelve 0): se lee como texto.
    static int entero(const wchar_t* ini, const wchar_t* seccion, const wchar_t* clave, int porDefecto) {
        wchar_t texto[32] = L"";
        GetPrivateProfileStringW(seccion, clave, L"", texto, 32, ini);
        if (!texto[0]) return porDefecto;
        wchar_t* fin = nullptr;
        const long v = wcstol(texto, &fin, 10);
        return fin == texto ? porDefecto : static_cast<int>(v);
    }

    static patagonia::Config leer() {
        static const wchar_t* CLAVES[patagonia::BANDAS] = {
            L"b31", L"b62", L"b125", L"b250", L"b500", L"b1k", L"b2k", L"b4k", L"b8k", L"b16k"};
        wchar_t dir[MAX_PATH], ini[MAX_PATH];
        carpeta(dir, MAX_PATH);
        swprintf(ini, MAX_PATH, L"%ls\\ajustes.ini", dir);
        patagonia::Config c;
        c.activo = entero(ini, L"motor", L"activo", 1) != 0;
        c.graves = entero(ini, L"motor", L"graves", 8);
        c.ganancia = entero(ini, L"motor", L"ganancia", 0);
        c.limitador = entero(ini, L"motor", L"limitador", 1) != 0;
        for (int b = 0; b < patagonia::BANDAS; ++b) c.bandas[b] = entero(ini, L"ecualizador", CLAVES[b], 0);
        return c;
    }

    static void anotarEstado(float frecuencia, unsigned canales, const patagonia::Config& c, const char* motivo) {
        wchar_t dir[MAX_PATH], archivo[MAX_PATH];
        carpeta(dir, MAX_PATH);
        swprintf(archivo, MAX_PATH, L"%ls\\estado.txt", dir);
        HANDLE h = CreateFileW(archivo, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE) return;
        SYSTEMTIME t;
        GetLocalTime(&t);
        char texto[512];
        const int n = sprintf_s(texto,
            "Motor Patagonia Sound FDX CARGADO\r\nFecha: %02u/%02u/%04u %02u:%02u:%02u\r\n"
            "Audio: %.0f Hz, %u canales\r\nProcesando: %s\r\n"
            "Graves: %+d dB | Ganancia: %+d dB | Limitador: %s\r\n"
            "Ecualizador: %d %d %d %d %d %d %d %d %d %d\r\n",
            t.wDay, t.wMonth, t.wYear, t.wHour, t.wMinute, t.wSecond,
            static_cast<double>(frecuencia), canales, motivo ? motivo : "si",
            c.graves, c.ganancia, c.limitador ? "si" : "no",
            c.bandas[0], c.bandas[1], c.bandas[2], c.bandas[3], c.bandas[4],
            c.bandas[5], c.bandas[6], c.bandas[7], c.bandas[8], c.bandas[9]);
        DWORD escritos = 0;
        if (n > 0) WriteFile(h, texto, static_cast<DWORD>(n), &escritos, nullptr);
        CloseHandle(h);
    }
};
