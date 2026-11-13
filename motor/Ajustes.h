#pragma once
// Ajustes y diagnóstico de Patagonia Sound, en la carpeta C:\ProgramData\Patagonia Sound:
//   ajustes.ini -> lo que el usuario elige (por ahora: activo y graves)
//   estado.txt  -> lo escribe el motor cada vez que arranca un sonido: así se
//                  puede comprobar que Windows lo cargó.
// Solo se usa fuera del tiempo real (LockForProcess).

#include <windows.h>
#include <cwchar>

struct Ajustes {
    bool activo = true;
    double graves = 8.0; // dB, entre -12 y +12

    static void carpeta(wchar_t* destino, size_t largo) {
        wchar_t base[MAX_PATH] = L"";
        if (!GetEnvironmentVariableW(L"ProgramData", base, MAX_PATH)) wcscpy_s(base, L"C:\\ProgramData");
        swprintf(destino, largo, L"%ls\\Patagonia Sound", base);
    }

    static Ajustes leer() {
        wchar_t dir[MAX_PATH], ini[MAX_PATH];
        carpeta(dir, MAX_PATH);
        swprintf(ini, MAX_PATH, L"%ls\\ajustes.ini", dir);
        Ajustes a;
        a.activo = GetPrivateProfileIntW(L"motor", L"activo", 1, ini) != 0;
        int g = static_cast<int>(GetPrivateProfileIntW(L"motor", L"graves", 8, ini));
        if (g > 12) g = 12;
        if (g < -12) g = -12;
        a.graves = g;
        return a;
    }

    static void anotarEstado(float frecuencia, unsigned canales, const Ajustes& a) {
        wchar_t dir[MAX_PATH], archivo[MAX_PATH];
        carpeta(dir, MAX_PATH);
        swprintf(archivo, MAX_PATH, L"%ls\\estado.txt", dir);
        HANDLE h = CreateFileW(archivo, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE) return;
        SYSTEMTIME t;
        GetLocalTime(&t);
        char texto[256];
        const int n = sprintf_s(texto,
            "Motor Patagonia Sound FDX CARGADO\r\nFecha: %02u/%02u/%04u %02u:%02u:%02u\r\n"
            "Audio: %.0f Hz, %u canales\r\nActivo: %s\r\nGraves: %+.0f dB\r\n",
            t.wDay, t.wMonth, t.wYear, t.wHour, t.wMinute, t.wSecond,
            static_cast<double>(frecuencia), canales, a.activo ? "si" : "no", a.graves);
        DWORD escritos = 0;
        if (n > 0) WriteFile(h, texto, static_cast<DWORD>(n), &escritos, nullptr);
        CloseHandle(h);
    }
};
