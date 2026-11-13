#define _USE_MATH_DEFINES // M_PI en Visual Studio
// Pruebas del procesamiento de sonido (corren en cualquier máquina, sin Windows).
// g++ -O2 -std=c++17 -I motor pruebas/prueba_procesador.cpp -o prueba && ./prueba
#include "Procesador.h"
#include <cstdio>
#include <vector>
#include <cmath>
#include <ctime>
using namespace patagonia;

static int fallas = 0;
#define CHEQUEAR(cond, ...) do { if (!(cond)) { std::printf("FALLA: "); std::printf(__VA_ARGS__); std::printf("\n"); ++fallas; } } while (0)

// Ganancia en dB de una senoidal de frecuencia f (estéreo), medida en régimen.
static double medir(Procesador& p, double fs, double f, double amp = 0.1) {
    p.reiniciar();
    const unsigned N = static_cast<unsigned>(fs); // 1 segundo
    std::vector<float> buf(N * 2);
    for (unsigned i = 0; i < N; ++i) buf[2 * i] = buf[2 * i + 1] = float(amp * std::sin(2 * M_PI * f * i / fs));
    p.procesar(buf.data(), buf.data(), N);
    double e = 0;
    for (unsigned i = N / 2; i < N; ++i) e += double(buf[2 * i]) * buf[2 * i];
    const double rms = std::sqrt(e / (N - N / 2));
    return 20 * std::log10(rms / (amp / std::sqrt(2.0)));
}

int main() {
    const double fs = 48000;
    Procesador p;
    Config c;

    // 1. Todo en 0: no procesa (el sonido pasa tal cual).
    c.graves = 0;
    CHEQUEAR(!p.preparar(fs, 2, c), "todo en 0 tendria que dejar pasar");

    // 2. Apagado y frecuencias altas: no procesa.
    c.graves = 8; c.activo = false;
    CHEQUEAR(!p.preparar(fs, 2, c), "apagado tendria que dejar pasar");
    c.activo = true;
    const char* motivo = nullptr;
    CHEQUEAR(!p.preparar(192000, 4, c, &motivo), "192 kHz tendria que dejar pasar");
    std::printf("192 kHz -> %s\n", motivo);

    // 3. Graves +8: sube abajo, no toca arriba (con pre-ganancia de -4 dB).
    c = Config(); c.graves = 8; c.limitador = false;
    CHEQUEAR(p.preparar(fs, 2, c), "graves +8 tendria que procesar");
    double g40 = medir(p, fs, 40), g5k = medir(p, fs, 5000);
    std::printf("graves +8: 40 Hz %+.1f dB, 5 kHz %+.1f dB\n", g40, g5k);
    CHEQUEAR(std::fabs(g40 - 4) < 0.7 && std::fabs(g5k + 4) < 0.3, "graves fuera de lo esperado");

    // 4. Cada banda del ecualizador a +6: sube en su frecuencia (+3 netos por la pre-ganancia).
    for (int b = 0; b < BANDAS; ++b) {
        c = Config(); c.graves = 0; c.limitador = false; c.bandas[b] = 6;
        p.preparar(fs, 2, c);
        double g = medir(p, fs, FRECUENCIAS[b]);
        std::printf("banda %5.0f Hz +6 -> %+.2f dB\n", FRECUENCIAS[b], g);
        CHEQUEAR(std::fabs(g - 3) < 0.3, "banda %.0f mal", FRECUENCIAS[b]);
    }
    // Una banda negativa baja y no cambia la pre-ganancia.
    c = Config(); c.graves = 0; c.limitador = false; c.bandas[5] = -6;
    p.preparar(fs, 2, c);
    CHEQUEAR(std::fabs(medir(p, fs, 1000) + 6) < 0.3, "banda negativa mal");
    CHEQUEAR(std::fabs(medir(p, fs, 60)) < 0.3, "banda negativa toca otras frecuencias");

    // 5. Ganancia +12 con limitador: nunca pasa de -1 dBFS, aunque entre a tope.
    c = Config(); c.graves = 8; c.ganancia = 12; c.limitador = true;
    p.preparar(fs, 2, c);
    {
        const unsigned N = 48000; std::vector<float> buf(N * 2);
        for (unsigned i = 0; i < N; ++i) buf[2*i] = buf[2*i+1] = float(0.99 * std::sin(2 * M_PI * 60 * i / fs));
        p.procesar(buf.data(), buf.data(), N);
        double pico = 0; for (float v : buf) pico = std::fmax(pico, std::fabs(v));
        std::printf("ganancia +12 a tope: pico %.4f (techo %.4f)\n", pico, std::pow(10.0, -0.05));
        CHEQUEAR(pico <= std::pow(10.0, -0.05) + 1e-6, "el limitador dejo pasar un pico");
    }
    // Ganancia +6 con señal baja: sube 6 dB (limitador sin trabajar).
    c = Config(); c.graves = 0; c.ganancia = 6;
    p.preparar(fs, 2, c);
    double g6 = medir(p, fs, 1000, 0.05);
    std::printf("ganancia +6 senal baja: %+.2f dB\n", g6);
    CHEQUEAR(std::fabs(g6 - 6) < 0.2, "ganancia mal");

    // 6. Estabilidad: ruido + silencio largo, nada de NaN/infinito; velocidad.
    c = Config(); c.graves = 12; for (int b = 0; b < BANDAS; ++b) c.bandas[b] = (b % 2) ? 12 : -12; c.ganancia = 12;
    p.preparar(fs, 8, c);
    {
        const unsigned N = 48000 * 10; std::vector<float> buf(N * 8, 0.0f);
        unsigned s = 1;
        for (unsigned i = 0; i < N / 10 * 8; ++i) { s = s * 1103515245u + 12345u; buf[i] = (float((s >> 8) & 0xFFFF) / 32768.0f - 1.0f); }
        clock_t t = clock();
        p.procesar(buf.data(), buf.data(), N);
        double seg = double(clock() - t) / CLOCKS_PER_SEC;
        bool ok = true; for (float v : buf) if (!std::isfinite(v) || std::fabs(v) > 1.0f) { ok = false; break; }
        CHEQUEAR(ok, "salio NaN, infinito o pico arriba de 1");
        std::printf("10 s de audio, 8 canales, 11 filtros: %.3f s de CPU (%.0fx mas rapido que tiempo real)\n", seg, 10 / seg);
    }

    std::printf(fallas ? "\n%d FALLAS\n" : "\nTODO OK\n", fallas);
    return fallas ? 1 : 0;
}
