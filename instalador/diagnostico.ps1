# Muestra si Windows cargó el motor de Patagonia Sound y guarda un informe
# (diagnostico.txt, en el Escritorio) para pasárselo a Claude si algo no anda.
. (Join-Path $PSScriptRoot 'comun.ps1')

$informe = New-Object System.Collections.Generic.List[string]
function Anotar([string]$texto, [string]$color = 'Gray') { Write-Host "  $texto" -ForegroundColor $color; $informe.Add($texto) }

Write-Host ''
Anotar 'Patagonia Sound - diagnóstico' 'Cyan'
Anotar ("Windows: " + (Get-CimInstance Win32_OperatingSystem).Caption + ' ' + [Environment]::OSVersion.Version)
Anotar ''

$estado = Join-Path $DirDatos 'estado.txt'
if (Test-Path $estado) {
    Anotar 'MOTOR CARGADO: Windows está pasando el sonido por Patagonia Sound.' 'Green'
    Get-Content $estado | ForEach-Object { Anotar "   $_" }
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
    Anotar ("      Patagonia enganchado: " + ($lista -contains $CLSID) + " | efectos: " + ($lista -join ', '))
    Anotar ("      Efecto simple del driver: " + $fx.$K_SFX_SIMPLE + " | Mejoras apagadas: " + $pr.$K_SIN_EFECTOS)
    foreach ($n in '{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},1', '{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},2') {
        if ($fx.$n) { Anotar ("      Efecto viejo ($n): " + $fx.$n) }
    }
}
$destino = Join-Path ([Environment]::GetFolderPath('Desktop')) 'diagnostico.txt'
$informe | Set-Content -Path $destino -Encoding UTF8
Write-Host ''
Write-Host "  Informe guardado en: $destino" -ForegroundColor Cyan
Write-Host ''
