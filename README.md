# Patagonia Sound

Ecualizador para Windows con **motor propio**, de la familia Patagonia (Roma.69).

## Cómo funciona

El motor es un *Audio Processing Object* (APO): una DLL que el audio de Windows (`audiodg.exe`) carga y mete en la cadena de cada salida de sonido. No es un driver: corre en modo usuario y no necesita firma de drivers.

- `motor/`: el motor de audio (C++).
- `instalador/`: instalar, rescate (deja todo como estaba) y diagnóstico.
- Ajustes y estado del motor: `C:\ProgramData\Patagonia Sound\`.

## Estado

**Paso 1, prueba de motor:** refuerzo de graves en toda la PC. Sirve para comprobar que Windows carga el motor.

Próximos pasos: ecualizador de 10 bandas, efectos (claridad, amplitud estéreo, refuerzo de volumen), limitador, y la app con el ícono en la barra de Windows.

## Probarlo

Bajá [Patagonia-Sound-prueba.zip](https://github.com/Roma-69/Patagonia-Sound/releases/download/motor-prueba/Patagonia-Sound-prueba.zip) y seguí `LEEME.txt`.

## Compilar

GitHub Actions lo compila solo en Windows con Visual Studio (`.github/workflows/compilar.yml`).
