// Patagonia Sound: motor de audio propio para Windows.
// Es un "APO" (Audio Processing Object): una DLL que el motor de audio de Windows
// (audiodg.exe) carga y mete en la cadena de cada salida de sonido.
//
// Paso 1 (prueba de motor): un refuerzo de graves. Si se escucha, Windows cargó
// el motor de Patagonia y todo lo demás se construye encima.
//
// Reglas del procesamiento en tiempo real (APOProcess): nada de memoria nueva,
// nada de archivos, nada de esperas. Solo cuentas.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include <mmreg.h>
#include <new>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <xmmintrin.h>
#include <pmmintrin.h>

#ifdef _MSC_VER
#include <audioenginebaseapo.h>
#include <audiomediatype.h>
#else
#include "compat_mingw.h" // solo para revisar la sintaxis fuera de Windows
#endif

#include "Ajustes.h"

// {C90115C4-8422-4C4D-B4B4-F1EBF7E03B4D}
static const CLSID CLSID_PatagoniaSound =
    { 0xc90115c4, 0x8422, 0x4c4d, { 0xb4, 0xb4, 0xf1, 0xeb, 0xf7, 0xe0, 0x3b, 0x4d } };

// Audio en punto flotante (KSDATAFORMAT_SUBTYPE_IEEE_FLOAT), el formato interno de Windows.
static const GUID FORMATO_FLOAT =
    { 0x00000003, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };

static LONG g_objetos = 0;
static LONG g_bloqueos = 0;

#ifndef APOERR_FORMAT_NOT_SUPPORTED
#define APOERR_FORMAT_NOT_SUPPORTED ((HRESULT)0x887D0006L)
#endif
#ifndef APOERR_ALREADY_UNLOCKED
#define APOERR_ALREADY_UNLOCKED ((HRESULT)0x887D0005L)
#endif
#ifndef APOERR_ALREADY_INITIALIZED
#define APOERR_ALREADY_INITIALIZED ((HRESULT)0x887D0001L)
#endif

static const UINT32 MAX_CANALES = 16;

// Filtro "low shelf" (refuerzo de graves) de la receta clásica de R. Bristow-Johnson.
struct Biquad {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;

    void lowShelf(double frecuenciaMuestreo, double frecuencia, double gananciaDb) {
        const double A = std::pow(10.0, gananciaDb / 40.0);
        const double w0 = 2.0 * 3.14159265358979323846 * frecuencia / frecuenciaMuestreo;
        const double cosw = std::cos(w0), sinw = std::sin(w0);
        const double alfa = sinw / 2.0 * std::sqrt(2.0); // pendiente S = 1
        const double raiz = 2.0 * std::sqrt(A) * alfa;
        const double a0 = (A + 1) + (A - 1) * cosw + raiz;
        b0 = A * ((A + 1) - (A - 1) * cosw + raiz) / a0;
        b1 = 2 * A * ((A - 1) - (A + 1) * cosw) / a0;
        b2 = A * ((A + 1) - (A - 1) * cosw - raiz) / a0;
        a1 = -2 * ((A - 1) + (A + 1) * cosw) / a0;
        a2 = ((A + 1) + (A - 1) * cosw - raiz) / a0;
    }
};

struct EstadoCanal { double z1 = 0, z2 = 0; };

class MotorPatagonia final : public IAudioProcessingObject,
                             public IAudioProcessingObjectRT,
                             public IAudioProcessingObjectConfiguration,
                             public IAudioSystemEffects {
public:
    MotorPatagonia() { InterlockedIncrement(&g_objetos); }
    ~MotorPatagonia() { InterlockedDecrement(&g_objetos); }

    // ---- IUnknown ----
    STDMETHODIMP QueryInterface(REFIID iid, void** salida) override {
        if (!salida) return E_POINTER;
        *salida = nullptr;
        if (iid == __uuidof(IUnknown) || iid == __uuidof(IAudioProcessingObject))
            *salida = static_cast<IAudioProcessingObject*>(this);
        else if (iid == __uuidof(IAudioProcessingObjectRT))
            *salida = static_cast<IAudioProcessingObjectRT*>(this);
        else if (iid == __uuidof(IAudioProcessingObjectConfiguration))
            *salida = static_cast<IAudioProcessingObjectConfiguration*>(this);
        else if (iid == __uuidof(IAudioSystemEffects))
            *salida = static_cast<IAudioSystemEffects*>(this);
        else
            return E_NOINTERFACE;
        AddRef();
        return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&referencias_); }
    STDMETHODIMP_(ULONG) Release() override {
        const ULONG r = InterlockedDecrement(&referencias_);
        if (r == 0) delete this;
        return r;
    }

    // ---- IAudioProcessingObject ----
    STDMETHODIMP Reset() override {
        for (auto& c : canales_) c = EstadoCanal{};
        return S_OK;
    }
    STDMETHODIMP GetLatency(HNSTIME* latencia) override {
        if (!latencia) return E_POINTER;
        *latencia = 0;
        return S_OK;
    }
    STDMETHODIMP GetRegistrationProperties(APO_REG_PROPERTIES** propiedades) override {
        if (!propiedades) return E_POINTER;
        auto* p = static_cast<APO_REG_PROPERTIES*>(CoTaskMemAlloc(sizeof(APO_REG_PROPERTIES)));
        if (!p) return E_OUTOFMEMORY;
        ZeroMemory(p, sizeof(*p));
        p->clsid = CLSID_PatagoniaSound;
        p->Flags = static_cast<APO_FLAG>(APO_FLAG_INPLACE | APO_FLAG_SAMPLESPERFRAME_MUST_MATCH |
                                         APO_FLAG_FRAMESPERSECOND_MUST_MATCH | APO_FLAG_BITSPERSAMPLE_MUST_MATCH);
        wcscpy_s(p->szFriendlyName, L"Patagonia Sound");
        wcscpy_s(p->szCopyrightInfo, L"Roma.69");
        p->u32MajorVersion = 0;
        p->u32MinorVersion = 1;
        p->u32MinInputConnections = 1;
        p->u32MaxInputConnections = 1;
        p->u32MinOutputConnections = 1;
        p->u32MaxOutputConnections = 1;
        p->u32MaxInstances = 0xffffffff;
        p->u32NumAPOInterfaces = 1;
        p->iidAPOInterfaceList[0] = __uuidof(IAudioSystemEffects);
        *propiedades = p;
        return S_OK;
    }
    STDMETHODIMP Initialize(UINT32, BYTE*) override {
        if (inicializado_) return APOERR_ALREADY_INITIALIZED;
        inicializado_ = true;
        return S_OK;
    }
    STDMETHODIMP IsInputFormatSupported(IAudioMediaType* opuesto, IAudioMediaType* pedido,
                                        IAudioMediaType** soportado) override {
        return formatoSoportado(opuesto, pedido, soportado);
    }
    STDMETHODIMP IsOutputFormatSupported(IAudioMediaType* opuesto, IAudioMediaType* pedido,
                                         IAudioMediaType** soportado) override {
        return formatoSoportado(opuesto, pedido, soportado);
    }
    STDMETHODIMP GetInputChannelCount(UINT32* canales) override {
        if (!canales) return E_POINTER;
        *canales = numCanales_;
        return S_OK;
    }

    // ---- IAudioProcessingObjectConfiguration ----
    STDMETHODIMP LockForProcess(UINT32 numEntradas, APO_CONNECTION_DESCRIPTOR** entradas,
                                UINT32 numSalidas, APO_CONNECTION_DESCRIPTOR** salidas) override {
        if (numEntradas != 1 || numSalidas != 1 || !entradas || !salidas || !entradas[0] || !entradas[0]->pFormat)
            return E_INVALIDARG;
        UNCOMPRESSEDAUDIOFORMAT formato{};
        HRESULT hr = entradas[0]->pFormat->GetUncompressedAudioFormat(&formato);
        if (FAILED(hr)) return hr;
        if (formato.guidFormatType != FORMATO_FLOAT || formato.dwBytesPerSampleContainer != 4)
            return APOERR_FORMAT_NOT_SUPPORTED;

        numCanales_ = formato.dwSamplesPerFrame;
        frecuencia_ = formato.fFramesPerSecond;

        // Los ajustes se leen acá (fuera del tiempo real).
        ajustes_ = Ajustes::leer();
        filtro_.lowShelf(frecuencia_, 100.0, ajustes_.graves);
        // Se baja la entrada la mitad de lo que suben los graves para no saturar.
        preGanancia_ = static_cast<float>(std::pow(10.0, -ajustes_.graves / 40.0));
        Reset();
        Ajustes::anotarEstado(frecuencia_, numCanales_, ajustes_);
        bloqueado_ = true;
        return S_OK;
    }
    STDMETHODIMP UnlockForProcess() override {
        if (!bloqueado_) return APOERR_ALREADY_UNLOCKED;
        bloqueado_ = false;
        return S_OK;
    }

    // ---- IAudioProcessingObjectRT (tiempo real) ----
    STDMETHODIMP_(void) APOProcess(UINT32 numEntradas, APO_CONNECTION_PROPERTY** entradas,
                                   UINT32 numSalidas, APO_CONNECTION_PROPERTY** salidas) override {
        if (numEntradas < 1 || numSalidas < 1) return;
        APO_CONNECTION_PROPERTY* entrada = entradas[0];
        APO_CONNECTION_PROPERTY* salida = salidas[0];
        const UINT32 cuadros = entrada->u32ValidFrameCount;
        auto* in = reinterpret_cast<const float*>(entrada->pBuffer);
        auto* out = reinterpret_cast<float*>(salida->pBuffer);

        if (entrada->u32BufferFlags == BUFFER_SILENT || !ajustes_.activo || ajustes_.graves == 0.0 ||
            numCanales_ == 0) {
            // Sin nada que hacer: el sonido pasa tal cual (o silencio limpio).
            if (out != in) {
                if (entrada->u32BufferFlags == BUFFER_SILENT) std::memset(out, 0, sizeof(float) * cuadros * numCanales_);
                else std::memcpy(out, in, sizeof(float) * cuadros * numCanales_);
            }
            salida->u32ValidFrameCount = cuadros;
            salida->u32BufferFlags = entrada->u32BufferFlags;
            return;
        }

        // Números minúsculos ("denormales") que deja el filtro al apagarse un sonido
        // pueden hacer que el procesador se arrastre y el audio salga robótico:
        // se tratan como cero mientras dura este bloque y después se deja todo como estaba.
        const unsigned int mxcsrAnterior = _mm_getcsr();
        _mm_setcsr(mxcsrAnterior | 0x8040); // FTZ + DAZ

        const UINT32 canalesFiltrados = numCanales_ < MAX_CANALES ? numCanales_ : MAX_CANALES;
        const Biquad f = filtro_;
        const float g = preGanancia_;
        for (UINT32 i = 0; i < cuadros; ++i) {
            for (UINT32 c = 0; c < numCanales_; ++c) {
                const size_t k = static_cast<size_t>(i) * numCanales_ + c;
                double x = static_cast<double>(in[k]) * g;
                if (c < canalesFiltrados) {
                    EstadoCanal& e = canales_[c];
                    // Forma directa transpuesta II.
                    const double y = f.b0 * x + e.z1;
                    e.z1 = f.b1 * x - f.a1 * y + e.z2;
                    e.z2 = f.b2 * x - f.a2 * y;
                    x = y;
                }
                // Sin tope acá: los efectos de la placa que van antes pueden mandar
                // sonido por encima de 1.0 a propósito, y recortarlo lo dejaba como
                // una lata. El tope final lo pone Windows; el limitador propio viene después.
                out[k] = static_cast<float>(x);
            }
        }
        _mm_setcsr(mxcsrAnterior);
        salida->u32ValidFrameCount = cuadros;
        salida->u32BufferFlags = BUFFER_VALID;
    }
    STDMETHODIMP_(UINT32) CalcInputFrames(UINT32 cuadrosSalida) override { return cuadrosSalida; }
    STDMETHODIMP_(UINT32) CalcOutputFrames(UINT32 cuadrosEntrada) override { return cuadrosEntrada; }

private:
    // Solo audio en punto flotante de 32 bits (el que usa Windows por dentro),
    // y entrada igual a salida.
    HRESULT formatoSoportado(IAudioMediaType* opuesto, IAudioMediaType* pedido, IAudioMediaType** soportado) {
        if (!pedido || !soportado) return E_POINTER;
        *soportado = nullptr;
        UNCOMPRESSEDAUDIOFORMAT formato{};
        if (FAILED(pedido->GetUncompressedAudioFormat(&formato)) ||
            formato.guidFormatType != FORMATO_FLOAT || formato.dwBytesPerSampleContainer != 4)
            return APOERR_FORMAT_NOT_SUPPORTED;
        if (opuesto) {
            UNCOMPRESSEDAUDIOFORMAT otro{};
            if (FAILED(opuesto->GetUncompressedAudioFormat(&otro)) ||
                otro.guidFormatType != formato.guidFormatType ||
                otro.dwSamplesPerFrame != formato.dwSamplesPerFrame ||
                otro.dwBytesPerSampleContainer != formato.dwBytesPerSampleContainer ||
                otro.fFramesPerSecond != formato.fFramesPerSecond)
                return APOERR_FORMAT_NOT_SUPPORTED;
        }
        pedido->AddRef();
        *soportado = pedido;
        return S_OK;
    }

    LONG referencias_ = 1;
    bool inicializado_ = false;
    bool bloqueado_ = false;
    UINT32 numCanales_ = 2;
    float frecuencia_ = 48000.0f;
    float preGanancia_ = 1.0f;
    Ajustes ajustes_{};
    Biquad filtro_{};
    EstadoCanal canales_[MAX_CANALES]{};
};

// ---- Fábrica COM ----
class Fabrica final : public IClassFactory {
public:
    STDMETHODIMP QueryInterface(REFIID iid, void** salida) override {
        if (!salida) return E_POINTER;
        if (iid == __uuidof(IUnknown) || iid == __uuidof(IClassFactory)) {
            *salida = static_cast<IClassFactory*>(this);
            AddRef();
            return S_OK;
        }
        *salida = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return 2; }
    STDMETHODIMP_(ULONG) Release() override { return 1; }
    STDMETHODIMP CreateInstance(IUnknown* externo, REFIID iid, void** salida) override {
        if (!salida) return E_POINTER;
        *salida = nullptr;
        if (externo) return CLASS_E_NOAGGREGATION;
        auto* motor = new (std::nothrow) MotorPatagonia();
        if (!motor) return E_OUTOFMEMORY;
        const HRESULT hr = motor->QueryInterface(iid, salida);
        motor->Release();
        return hr;
    }
    STDMETHODIMP LockServer(BOOL bloquear) override {
        if (bloquear) InterlockedIncrement(&g_bloqueos); else InterlockedDecrement(&g_bloqueos);
        return S_OK;
    }
};

static Fabrica g_fabrica;

extern "C" HRESULT __stdcall DllGetClassObject(REFCLSID clsid, REFIID iid, void** salida) {
    if (!salida) return E_POINTER;
    *salida = nullptr;
    if (clsid != CLSID_PatagoniaSound) return CLASS_E_CLASSNOTAVAILABLE;
    return g_fabrica.QueryInterface(iid, salida);
}

extern "C" HRESULT __stdcall DllCanUnloadNow() {
    return (g_objetos == 0 && g_bloqueos == 0) ? S_OK : S_FALSE;
}

BOOL APIENTRY DllMain(HMODULE modulo, DWORD motivo, LPVOID) {
    if (motivo == DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(modulo);
    return TRUE;
}
