# Muestra si Windows cargó el motor de Patagonia Sound y guarda un informe
# (diagnostico.txt, en el Escritorio) para pasárselo a Claude si algo no anda.
. (Join-Path $PSScriptRoot 'comun.ps1')

$informe = New-Object System.Collections.Generic.List[string]
function Anotar([string]$texto, [string]$color = 'Gray') { Write-Host "  $texto" -ForegroundColor $color; $informe.Add($texto) }

Write-Host ''
Anotar 'Patagonia Sound FDX - diagnóstico' 'Cyan'
Anotar ("Windows: " + (Get-CimInstance Win32_OperatingSystem).Caption + ' ' + [Environment]::OSVersion.Version)
Anotar ''

$estado = Join-Path $DirDatos 'estado.txt'
$cargado = [bool](Get-Process audiodg -ErrorAction SilentlyContinue | Where-Object {
    try { $_.Modules.ModuleName -contains 'PatagoniaSound.dll' } catch { $false } })
if (-not $cargado) { $cargado = [bool]((& tasklist /m PatagoniaSound.dll 2>$null) -match 'audiodg') }
if ($cargado -or (Test-Path $estado)) {
    Anotar 'MOTOR CARGADO: Windows está pasando el sonido por Patagonia Sound FDX.' 'Green'
    if (Test-Path $estado) { Get-Content $estado | ForEach-Object { Anotar "   $_" } }
} else {
    Anotar 'El motor TODAVÍA NO arrancó.' 'Yellow'
    Anotar '   Poné algo a sonar (YouTube, Spotify) unos segundos y volvé a abrir este diagnóstico.'
    Anotar '   Si sigue igual, mandale a Claude el archivo diagnostico.txt del Escritorio.'
}
Anotar ''
Anotar ("Motor registrado: " + (Test-Path "HKLM:\SOFTWARE\Classes\AudioEngine\AudioProcessingObjects\$CLSID"))
Anotar ("DLL instalada: " + (Test-Path (Join-Path $DirPrograma 'PatagoniaSound.dll')))
Anotar ("Ajustes: " + ((Get-Content (Join-Path $DirDatos 'ajustes.ini') -ErrorAction SilentlyContinue) -join ' '))
$prot = (Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Audio' -ErrorAction SilentlyContinue).DisableProtectedAudioDG
Anotar ("Audio protegido desactivado: " + ($(if ($prot) { 'sí' } else { 'no' })))
Anotar ''
Anotar 'Salidas de sonido:'
foreach ($s in Get-SalidasActivas) {
    $fx = Get-ItemProperty "HKLM:\$RaizRender\$($s.Id)\FxProperties" -ErrorAction SilentlyContinue
    $pr = Get-ItemProperty "HKLM:\$RaizRender\$($s.Id)\Properties" -ErrorAction SilentlyContinue
    $lista = @($fx.$K_SFX_COMPUESTO)
    Anotar ("  - " + $s.Nombre)
    $fmt = (Get-ItemProperty "HKLM:\$RaizRender\$($s.Id)\Properties" -ErrorAction SilentlyContinue).'{f19f064d-082c-4e27-bc73-6882a1bb8e4c},0'
    if ($fmt) {
        $hz = [BitConverter]::ToUInt32($fmt, 12); $can = [BitConverter]::ToUInt16($fmt, 10)
        if ($hz -gt 96000) { Anotar ("      Formato: $can canales, $hz Hz  <-- ATENCIÓN: a más de 96000 Hz el motor no actúa. Bajala a 48000 Hz.") 'Yellow' }
        else { Anotar ("      Formato: $can canales, $hz Hz") }
    }
    Anotar ("      Patagonia enganchado: " + ($lista -contains $CLSID) + " | efectos: " + ($lista -join ', '))
    Anotar ("      Efecto simple del driver: " + $fx.$K_SFX_SIMPLE + " | Mejoras apagadas: " + $pr.$K_SIN_EFECTOS)
    foreach ($n in '{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},1', '{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},2') {
        if ($fx.$n) { Anotar ("      Efecto viejo ($n): " + $fx.$n) }
    }
}
$log = Join-Path $DirDatos 'instalacion.log'
if (Test-Path $log) {
    Anotar ''
    Anotar 'Última instalación:'
    Get-Content $log | Where-Object { $_ -match '^\s+(\[|\+|x )|error|No pude' } | ForEach-Object { Anotar "  $_" }
}
$destino = Join-Path ([Environment]::GetFolderPath('Desktop')) 'diagnostico.txt'
$informe | Set-Content -Path $destino -Encoding UTF8
Write-Host ''
Write-Host "  Informe guardado en: $destino" -ForegroundColor Cyan
Write-Host ''
