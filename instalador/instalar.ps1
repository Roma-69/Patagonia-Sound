# Instala el motor de Patagonia Sound (paso 1: prueba de graves).
# Antes de tocar nada guarda un respaldo de cada salida de sonido, para que
# "Rescate" deje todo exactamente como estaba.
. (Join-Path $PSScriptRoot 'comun.ps1')

if (-not (Test-Admin)) { throw 'Hay que ejecutarlo como administrador.' }
New-Item -ItemType Directory -Force -Path $DirDatos | Out-Null
Start-Transcript -Path (Join-Path $DirDatos 'instalacion.log') -Force | Out-Null

Write-Host ''
Write-Host '  Patagonia Sound FDX - instalando el motor' -ForegroundColor Cyan
Write-Host ''

# 1. Copiar el motor.
$dll = Join-Path (Split-Path $PSScriptRoot) 'PatagoniaSound.dll'
Unblock-File $dll -ErrorAction SilentlyContinue
if (-not (Test-Path $dll)) { throw "No encuentro PatagoniaSound.dll al lado del instalador." }
New-Item -ItemType Directory -Force -Path $DirPrograma, $DirDatos, $DirRespaldo | Out-Null
Copy-Item $dll (Join-Path $DirPrograma 'PatagoniaSound.dll') -Force
Write-Host '  [1/5] Motor copiado' -ForegroundColor Green

# 2. Ajustes y permisos: el motor corre como "Servicio local" (audiodg.exe) y
#    tiene que poder leer los ajustes y anotar si arrancó.
$ini = Join-Path $DirDatos 'ajustes.ini'
if (-not (Test-Path $ini)) { "[motor]`r`nactivo=1`r`ngraves=8`r`n" | Set-Content -Path $ini -Encoding ASCII }
& icacls $DirDatos /grant '*S-1-5-19:(OI)(CI)M' '*S-1-15-2-1:(OI)(CI)RX' /T /Q | Out-Null
& icacls $DirPrograma /grant '*S-1-5-19:(OI)(CI)RX' '*S-1-15-2-1:(OI)(CI)RX' /T /Q | Out-Null
Write-Host '  [2/5] Ajustes listos (graves +8 dB)' -ForegroundColor Green

# Windows corre el audio "protegido" y así no carga motores sin firma digital
# (lo mismo que necesita Equalizer APO). Se guarda cómo estaba para que Rescate lo deje igual.
$marca = Join-Path $DirRespaldo 'audio-protegido.txt'
if (-not (Test-Path $marca)) {
    $antes = (Get-ItemProperty $ClaveAudio -Name DisableProtectedAudioDG -ErrorAction SilentlyContinue).DisableProtectedAudioDG
    $(if ($null -eq $antes) { 'no-existia' } else { "$antes" }) | Set-Content $marca -Encoding ASCII
}
if (-not (Test-Path $ClaveAudio)) { New-Item -Path $ClaveAudio | Out-Null }
New-ItemProperty -Path $ClaveAudio -Name DisableProtectedAudioDG -Value 1 -PropertyType DWord -Force | Out-Null

# 3. Registrar el motor en Windows (COM + catálogo de efectos de audio).
$com = "HKLM:\SOFTWARE\Classes\CLSID\$CLSID"
New-Item -Force -Path "$com\InprocServer32" | Out-Null
Set-ItemProperty -Path $com -Name '(default)' -Value 'Patagonia Sound FDX'
Set-ItemProperty -Path "$com\InprocServer32" -Name '(default)' -Value (Join-Path $DirPrograma 'PatagoniaSound.dll')
Set-ItemProperty -Path "$com\InprocServer32" -Name 'ThreadingModel' -Value 'Both'

$apo = "HKLM:\SOFTWARE\Classes\AudioEngine\AudioProcessingObjects\$CLSID"
New-Item -Force -Path $apo | Out-Null
$valores = @{
    FriendlyName = 'Patagonia Sound FDX'; Copyright = 'Roma.69'
    MajorVersion = 0; MinorVersion = 1; Flags = 0xF
    MinInputConnections = 1; MaxInputConnections = 1
    MinOutputConnections = 1; MaxOutputConnections = 1
    MaxInstances = -1; NumAPOInterfaces = 1; APOInterface0 = $IID_SFX   # -1 = 0xFFFFFFFF (sin límite)
}
foreach ($n in $valores.Keys) {
    $tipo = if ($valores[$n] -is [string]) { 'String' } else { 'DWord' }
    $v = $valores[$n]; if ($tipo -eq 'DWord') { $v = [int]$v }
    New-ItemProperty -Path $apo -Name $n -Value $v -PropertyType $tipo -Force | Out-Null
}
Write-Host '  [3/5] Motor registrado en Windows' -ForegroundColor Green

# 4. Engancharlo en cada salida de sonido activa.
$salidas = @(Get-SalidasActivas)
if ($salidas.Count -eq 0) { throw 'No encontré ninguna salida de sonido activa.' }
$enganchadas = 0
foreach ($s in $salidas) {
    try {
        $ruta = "$RaizRender\$($s.Id)"
        Enable-PermisoClave $ruta
        if (Test-Path "HKLM:\$ruta\FxProperties") { Enable-PermisoClave "$ruta\FxProperties" }
        Enable-PermisoClave "$ruta\Properties"
        $respaldo = Join-Path $DirRespaldo "$($s.Id).reg"
        if (-not (Test-Path $respaldo)) {
            # Respaldo completo de la salida, una sola vez (la primera instalación),
            # ya con permiso de lectura. Sin respaldo no se toca la salida.
            & reg.exe export "HKLM\$ruta" $respaldo /y /reg:64 2>&1 | Out-Null
            if (-not (Test-Path $respaldo)) { throw 'no pude guardar el respaldo, no la toco' }
        }
        if (-not (Test-Path "HKLM:\$ruta\FxProperties")) { New-Item -Path "HKLM:\$ruta\FxProperties" | Out-Null }
        Enable-PermisoClave "$ruta\FxProperties"

        $fx = Get-ItemProperty "HKLM:\$ruta\FxProperties"
        $lista = @()
        if ($fx.$K_SFX_COMPUESTO) { $lista = @($fx.$K_SFX_COMPUESTO) }
        elseif ($fx.$K_SFX_SIMPLE) { $lista = @($fx.$K_SFX_SIMPLE) }   # se conserva el efecto del driver
        $lista = @($lista | Where-Object { $_ -and ($_ -ne $CLSID) }) + $CLSID
        New-ItemProperty -Path "HKLM:\$ruta\FxProperties" -Name $K_SFX_COMPUESTO -Value ([string[]]$lista) -PropertyType MultiString -Force | Out-Null

        # Si las "Mejoras de audio" están apagadas, Windows saltea todos los efectos.
        New-ItemProperty -Path "HKLM:\$ruta\Properties" -Name $K_SIN_EFECTOS -Value 0 -PropertyType DWord -Force | Out-Null
        Write-Host "        + $($s.Nombre)"
        $enganchadas++
    } catch {
        Write-Host "        x $($s.Nombre): $($_.Exception.Message)" -ForegroundColor Red
    }
}
if ($enganchadas -eq 0) { throw 'No pude enganchar el motor en ninguna salida de sonido (ver arriba).' }
Write-Host '  [4/5] Enganchado en las salidas de sonido' -ForegroundColor Green

# 5. Reiniciar el audio de Windows para que cargue el motor.
Remove-Item (Join-Path $DirDatos 'estado.txt') -ErrorAction SilentlyContinue
Restart-Audio
Write-Host '  [5/5] Audio de Windows reiniciado' -ForegroundColor Green
Write-Host ''
Write-Host '  Listo. Poné música (un tema con bajo) y después abrí "Diagnostico.cmd".' -ForegroundColor Cyan
Write-Host '  Si algo sale mal o te quedás sin sonido: "Rescate.cmd".' -ForegroundColor Yellow
Write-Host ''
Stop-Transcript | Out-Null
