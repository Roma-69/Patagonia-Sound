# Patagonia Sound FDX

Ecualizador para Windows con **motor propio**, de la familia Patagonia (Roma.69).

## Cómo funciona

El motor es un *Audio Processing Object* (APO): una DLL que el audio de Windows (`audiodg.exe`) carga y mete en la cadena de cada salida de sonido. No es un driver: corre en modo usuario y no necesita firma de drivers.

- `motor/`: el motor de audio (C++).
- `instalador/`: instalar, rescate (deja todo como estaba) y diagnóstico.
- Ajustes y estado del motor: `C:\ProgramData\Patagonia Sound\`.

## Estado

**Motor:** refuerzo de graves, ecualizador de 10 bandas (31 Hz a 16 kHz), ganancia y limitador (techo -1 dBFS). Se configura en `C:\ProgramData\Patagonia Sound\ajustes.ini` (ver `LEEME.txt`). A más de 96 kHz el motor deja pasar el sonido sin tocarlo.

El procesamiento (`motor/Procesador.h`) es C++ puro y tiene pruebas que corren en cualquier máquina (`pruebas/`).

Próximos pasos: la app con el ícono en la barra de Windows (deslizadores, perfiles y visor de ondas) y efectos (claridad, amplitud estéreo).

## Probarlo

Bajá [Patagonia-Sound-FDX.zip](https://github.com/Roma-69/Patagonia-Sound/releases/download/motor-prueba/Patagonia-Sound-FDX.zip) y seguí `LEEME.txt`.

## Compilar

GitHub Actions lo compila solo en Windows con Visual Studio (`.github/workflows/compilar.yml`).
