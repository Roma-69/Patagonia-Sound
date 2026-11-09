@echo off
chcp 65001 >nul
if not exist "%~dp0instalador\rescate.ps1" (
  echo.
  echo   Primero hay que DESCOMPRIMIR el zip.
  echo   Clic derecho en Patagonia-Sound-prueba.zip ^> "Extraer todo..." ^> Extraer,
  echo   y despues abri este archivo desde la carpeta que se crea.
  echo.
  pause
  exit /b
)
net session >nul 2>&1 || (powershell -NoProfile -Command "Start-Process -Verb RunAs -FilePath '%~f0'" & exit /b)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0instalador\rescate.ps1"
pause
