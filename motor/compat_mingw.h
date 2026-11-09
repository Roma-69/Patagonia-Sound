#pragma once
// Declaraciones mínimas del SDK de Windows (audioenginebaseapo.h / audiomediatype.h)
// para revisar que el código compile fuera de Windows con MinGW.
// La DLL de verdad se compila en GitHub con Visual Studio y los encabezados oficiales.
#include <windows.h>
#include <unknwn.h>

typedef LONGLONG HNSTIME;

typedef enum APO_BUFFER_FLAGS { BUFFER_INVALID = 0, BUFFER_VALID = 1, BUFFER_SILENT = 2 } APO_BUFFER_FLAGS;
typedef enum APO_CONNECTION_BUFFER_TYPE { APO_CONNECTION_BUFFER_TYPE_ALLOCATED = 0 } APO_CONNECTION_BUFFER_TYPE;
typedef enum APO_FLAG {
    APO_FLAG_NONE = 0, APO_FLAG_INPLACE = 1, APO_FLAG_SAMPLESPERFRAME_MUST_MATCH = 2,
    APO_FLAG_FRAMESPERSECOND_MUST_MATCH = 4, APO_FLAG_BITSPERSAMPLE_MUST_MATCH = 8
} APO_FLAG;

typedef struct UNCOMPRESSEDAUDIOFORMAT {
    GUID guidFormatType; DWORD dwSamplesPerFrame; DWORD dwBytesPerSampleContainer;
    DWORD dwValidBitsPerSample; FLOAT fFramesPerSecond; DWORD dwChannelMask;
} UNCOMPRESSEDAUDIOFORMAT;

struct IAudioMediaType : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE IsCompressedFormat(BOOL*) = 0;
    virtual HRESULT STDMETHODCALLTYPE IsEqual(IAudioMediaType*, DWORD*) = 0;
    virtual const WAVEFORMATEX* STDMETHODCALLTYPE GetAudioFormat() = 0;
    virtual HRESULT STDMETHODCALLTYPE GetUncompressedAudioFormat(UNCOMPRESSEDAUDIOFORMAT*) = 0;
};
__CRT_UUID_DECL(IAudioMediaType, 0x4E997F73, 0xB71F, 0x4798, 0x87, 0x3B, 0xED, 0x7D, 0xFC, 0xF1, 0x5B, 0x4D)

typedef struct APO_CONNECTION_DESCRIPTOR {
    APO_CONNECTION_BUFFER_TYPE Type; UINT_PTR pBuffer; UINT32 u32MaxFrameCount;
    IAudioMediaType* pFormat; UINT32 u32Signature;
} APO_CONNECTION_DESCRIPTOR;

typedef struct APO_CONNECTION_PROPERTY {
    UINT_PTR pBuffer; UINT32 u32ValidFrameCount; APO_BUFFER_FLAGS u32BufferFlags; UINT32 u32Signature;
} APO_CONNECTION_PROPERTY;

typedef struct APO_REG_PROPERTIES {
    CLSID clsid; APO_FLAG Flags; WCHAR szFriendlyName[256]; WCHAR szCopyrightInfo[256];
    UINT32 u32MajorVersion; UINT32 u32MinorVersion; UINT32 u32MinInputConnections;
    UINT32 u32MaxInputConnections; UINT32 u32MinOutputConnections; UINT32 u32MaxOutputConnections;
    UINT32 u32MaxInstances; UINT32 u32NumAPOInterfaces; IID iidAPOInterfaceList[1];
} APO_REG_PROPERTIES;

struct IAudioProcessingObject : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE Reset() = 0;
    virtual HRESULT STDMETHODCALLTYPE GetLatency(HNSTIME*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetRegistrationProperties(APO_REG_PROPERTIES**) = 0;
    virtual HRESULT STDMETHODCALLTYPE Initialize(UINT32, BYTE*) = 0;
    virtual HRESULT STDMETHODCALLTYPE IsInputFormatSupported(IAudioMediaType*, IAudioMediaType*, IAudioMediaType**) = 0;
    virtual HRESULT STDMETHODCALLTYPE IsOutputFormatSupported(IAudioMediaType*, IAudioMediaType*, IAudioMediaType**) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetInputChannelCount(UINT32*) = 0;
};
__CRT_UUID_DECL(IAudioProcessingObject, 0xFD7F2B29, 0x24D0, 0x4B5C, 0xB1, 0x77, 0x59, 0x2C, 0x39, 0xF9, 0xCA, 0x10)

struct IAudioProcessingObjectRT : public IUnknown {
    virtual void STDMETHODCALLTYPE APOProcess(UINT32, APO_CONNECTION_PROPERTY**, UINT32, APO_CONNECTION_PROPERTY**) = 0;
    virtual UINT32 STDMETHODCALLTYPE CalcInputFrames(UINT32) = 0;
    virtual UINT32 STDMETHODCALLTYPE CalcOutputFrames(UINT32) = 0;
};
__CRT_UUID_DECL(IAudioProcessingObjectRT, 0x9E1D6A6D, 0xDDBC, 0x4E95, 0xA4, 0xC7, 0xAD, 0x64, 0xBA, 0x37, 0x84, 0x6C)

struct IAudioProcessingObjectConfiguration : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE LockForProcess(UINT32, APO_CONNECTION_DESCRIPTOR**, UINT32, APO_CONNECTION_DESCRIPTOR**) = 0;
    virtual HRESULT STDMETHODCALLTYPE UnlockForProcess() = 0;
};
__CRT_UUID_DECL(IAudioProcessingObjectConfiguration, 0x0E5ED805, 0xABA6, 0x49C3, 0x8F, 0x9A, 0x2B, 0x8C, 0x88, 0x9C, 0x4F, 0xA8)

struct IAudioSystemEffects : public IUnknown {};
__CRT_UUID_DECL(IAudioSystemEffects, 0x5FA00F27, 0xADD6, 0x499A, 0x8A, 0x9D, 0x6B, 0x98, 0x52, 0x1F, 0xA7, 0x5B)

