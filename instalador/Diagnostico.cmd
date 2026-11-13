@echo off
chcp 65001 >nul
if not exist "%~dp0instalador\diagnostico.ps1" (
  echo.
  echo   Primero hay que DESCOMPRIMIR el zip.
  echo   Clic derecho en Patagonia-Sound-FDX.zip ^> "Extraer todo..." ^> Extraer,
  echo   y despues abri este archivo desde la carpeta que se crea.
  echo.
  pause
  exit /b
)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0instalador\diagnostico.ps1"
pause
