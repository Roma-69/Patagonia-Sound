#pragma once
// Procesamiento de sonido de Patagonia Sound FDX: C++ puro, sin nada de Windows,
// así se puede probar en cualquier máquina.
//
// Cadena: pre-ganancia (deja lugar) -> graves -> ecualizador de 10 bandas
//         -> ganancia -> limitador (nunca pasa de -1 dBFS).
//
// procesar() corre en tiempo real: nada de memoria nueva, archivos ni esperas.

#include <cmath>
#include <cstddef>
#if defined(_M_X64) || defined(__x86_64__) || defined(__SSE2__)
#include <xmmintrin.h>
#define PATAGONIA_SSE 1
#endif

namespace patagonia {

static const int BANDAS = 10;
static const double FRECUENCIAS[BANDAS] = {31, 62, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};
static const double FRECUENCIA_MAXIMA = 96000.0; // arriba de esto el motor no toca el sonido

struct Config {
    bool activo = true;
    int graves = 8;            // dB, -12..12
    int bandas[BANDAS] = {};   // dB, -12..12
    int ganancia = 0;          // dB, 0..12
    bool limitador = true;
};

struct Biquad {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;

    // Recetas clásicas de R. Bristow-Johnson ("Audio EQ Cookbook").
    void lowShelf(double fs, double f, double db) {
        const double A = std::pow(10.0, db / 40.0);
        const double w0 = 2.0 * 3.14159265358979323846 * f / fs;
        const double c = std::cos(w0), s = std::sin(w0);
        const double alfa = s / 2.0 * std::sqrt(2.0); // pendiente S = 1
        const double r = 2.0 * std::sqrt(A) * alfa;
        const double a0 = (A + 1) + (A - 1) * c + r;
        b0 = A * ((A + 1) - (A - 1) * c + r) / a0;
        b1 = 2 * A * ((A - 1) - (A + 1) * c) / a0;
        b2 = A * ((A + 1) - (A - 1) * c - r) / a0;
        a1 = -2 * ((A - 1) + (A + 1) * c) / a0;
        a2 = ((A + 1) + (A - 1) * c - r) / a0;
    }
    void peaking(double fs, double f, double q, double db) {
        const double A = std::pow(10.0, db / 40.0);
        const double w0 = 2.0 * 3.14159265358979323846 * f / fs;
        const double c = std::cos(w0), alfa = std::sin(w0) / (2.0 * q);
        const double a0 = 1 + alfa / A;
        b0 = (1 + alfa * A) / a0;
        b1 = -2 * c / a0;
        b2 = (1 - alfa * A) / a0;
        a1 = -2 * c / a0;
        a2 = (1 - alfa / A) / a0;
    }
};

static inline int recortar(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

class Procesador {
public:
    static const unsigned MAX_CANALES = 16;
    static const int MAX_FILTROS = 1 + BANDAS;

    // Devuelve true si hay que procesar; false = dejar pasar el sonido tal cual.
    // motivo (opcional) explica por qué no procesa.
    bool preparar(double fs, unsigned canales, const Config& cfg, const char** motivo = nullptr) {
        const char* m = nullptr;
        numFiltros_ = 0;
        canales_ = canales;
        if (!cfg.activo) m = "apagado en ajustes.ini";
        else if (fs > FRECUENCIA_MAXIMA) m = "la salida esta en mas de 96000 Hz: bajala a 48000 Hz";
        else if (canales == 0 || canales > MAX_CANALES) m = "cantidad de canales no soportada";
        int maxSube = 0;
        if (!m) {
            const int g = recortar(cfg.graves, -12, 12);
            if (g != 0) { filtros_[numFiltros_++].lowShelf(fs, 100.0, g); if (g > maxSube) maxSube = g; }
            for (int b = 0; b < BANDAS; ++b) {
                const int db = recortar(cfg.bandas[b], -12, 12);
                if (db == 0 || FRECUENCIAS[b] >= 0.45 * fs) continue; // banda sin efecto o fuera de rango
                filtros_[numFiltros_++].peaking(fs, FRECUENCIAS[b], 1.41, db);
                if (db > maxSube) maxSube = db;
            }
            if (numFiltros_ == 0 && recortar(cfg.ganancia, 0, 12) == 0) m = "nada para hacer (todo en 0)";
        }
        if (motivo) *motivo = m;
        procesar_ = (m == nullptr);
        if (!procesar_) return false;

        // Se baja la entrada la mitad de lo que más sube el ecualizador, para dejar lugar.
        pre_ = std::pow(10.0, -maxSube / 40.0);
        post_ = std::pow(10.0, recortar(cfg.ganancia, 0, 12) / 20.0);
        limitador_ = cfg.limitador;
        techo_ = std::pow(10.0, -1.0 / 20.0);                  // -1 dBFS
        soltar_ = std::exp(-1.0 / (0.080 * fs));               // el limitador suelta en ~80 ms
        reiniciar();
        return true;
    }

    void reiniciar() {
        for (auto& c : estado_) for (auto& e : c) e[0] = e[1] = 0.0;
        envolvente_ = 1.0;
    }

    bool procesa() const { return procesar_; }
    unsigned canales() const { return canales_; }
    int filtros() const { return numFiltros_; }

    // Audio intercalado (L R L R...). in y out pueden ser el mismo buffer.
    void procesar(const float* in, float* out, unsigned cuadros) {
        // Números minúsculos ("denormales") que deja un filtro al apagarse el sonido
        // pueden hacer que el procesador se arrastre y el audio salga robótico:
        // mientras dura este bloque se tratan como cero, y después se deja todo como estaba.
#ifdef PATAGONIA_SSE
        const unsigned int mxcsr = _mm_getcsr();
        _mm_setcsr(mxcsr | 0x8040); // FTZ + DAZ
#endif
        const unsigned n = canales_;
        double cuadro[MAX_CANALES];
        for (unsigned i = 0; i < cuadros; ++i) {
            const size_t base = static_cast<size_t>(i) * n;
            double pico = 0.0;
            for (unsigned c = 0; c < n; ++c) {
                double x = static_cast<double>(in[base + c]) * pre_;
                for (int f = 0; f < numFiltros_; ++f) {
                    const Biquad& q = filtros_[f];
                    double* z = estado_[c][f];
                    const double y = q.b0 * x + z[0]; // forma directa transpuesta II
                    z[0] = q.b1 * x - q.a1 * y + z[1];
                    z[1] = q.b2 * x - q.a2 * y;
                    x = y;
                }
                x *= post_;
                cuadro[c] = x;
                const double a = std::fabs(x);
                if (a > pico) pico = a;
            }
            double g = 1.0;
            if (limitador_) {
                // Ataque instantáneo (nunca deja pasar un pico), suelta de a poco.
                const double necesaria = pico > techo_ ? techo_ / pico : 1.0;
                if (necesaria < envolvente_) envolvente_ = necesaria;
                else envolvente_ = necesaria - (necesaria - envolvente_) * soltar_;
                g = envolvente_;
            }
            for (unsigned c = 0; c < n; ++c) out[base + c] = static_cast<float>(cuadro[c] * g);
        }
#ifdef PATAGONIA_SSE
        _mm_setcsr(mxcsr);
#endif
    }

private:
    Biquad filtros_[MAX_FILTROS];
    double estado_[MAX_CANALES][MAX_FILTROS][2] = {};
    int numFiltros_ = 0;
    unsigned canales_ = 0;
    bool procesar_ = false;
    bool limitador_ = true;
    double pre_ = 1.0, post_ = 1.0, techo_ = 1.0, soltar_ = 0.0, envolvente_ = 1.0;
};

} // namespace patagonia
