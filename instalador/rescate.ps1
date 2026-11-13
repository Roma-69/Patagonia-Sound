# Saca Patagonia Sound y deja el sonido de Windows exactamente como estaba antes de instalarlo.
. (Join-Path $PSScriptRoot 'comun.ps1')

if (-not (Test-Admin)) { throw 'Hay que ejecutarlo como administrador.' }

Write-Host ''
Write-Host '  Patagonia Sound - rescate' -ForegroundColor Yellow
Write-Host ''

# 1. Restaurar cada salida desde su respaldo.
if (Test-Path $DirRespaldo) {
    foreach ($r in Get-ChildItem $DirRespaldo -Filter '*.reg') {
        $ruta = "$RaizRender\$($r.BaseName)"
        try {
            Enable-PermisoClave $ruta
            foreach ($sub in 'FxProperties', 'Properties') {
                if (Test-Path "HKLM:\$ruta\$sub") { Enable-PermisoClave "$ruta\$sub" }
            }
            # Se borra lo que haya y se vuelve a cargar el respaldo tal cual.
            if (Test-Path "HKLM:\$ruta\FxProperties") { Remove-Item "HKLM:\$ruta\FxProperties" -Recurse -Force }
            & reg.exe import $r.FullName /reg:64 2>&1 | Out-Null
            Write-Host "        restaurado: $($r.BaseName)"
        } catch {
            Write-Host "        no se pudo restaurar $($r.BaseName): $_" -ForegroundColor Red
        }
    }
}

# 2. Por las dudas: sacar el motor de cualquier salida que todavía lo nombre.
foreach ($d in Get-ChildItem "HKLM:\$RaizRender") {
    $fxRuta = "$($d.PSPath)\FxProperties"
    $fx = Get-ItemProperty $fxRuta -ErrorAction SilentlyContinue
    if (-not $fx) { continue }
    $lista = @($fx.$K_SFX_COMPUESTO)
    if ($lista -contains $CLSID) {
        Enable-PermisoClave "$RaizRender\$($d.PSChildName)\FxProperties"
        $resto = @($lista | Where-Object { $_ -and ($_ -ne $CLSID) })
        if ($resto.Count) { Set-ItemProperty $fxRuta -Name $K_SFX_COMPUESTO -Value ([string[]]$resto) }
        else { Remove-ItemProperty $fxRuta -Name $K_SFX_COMPUESTO }
    }
}
Write-Host '  [1/3] Salidas de sonido como estaban' -ForegroundColor Green

# Audio protegido: como estaba antes de instalar.
$marca = Join-Path $DirRespaldo 'audio-protegido.txt'
if (Test-Path $marca) {
    $antes = (Get-Content $marca -Raw).Trim()
    if ($antes -eq 'no-existia') { Remove-ItemProperty $ClaveAudio -Name DisableProtectedAudioDG -ErrorAction SilentlyContinue }
    else { New-ItemProperty -Path $ClaveAudio -Name DisableProtectedAudioDG -Value ([int]$antes) -PropertyType DWord -Force | Out-Null }
    Remove-Item $marca -Force
}

# 3. Desregistrar y reiniciar el audio (así Windows suelta la DLL).
Remove-Item "HKLM:\SOFTWARE\Classes\AudioEngine\AudioProcessingObjects\$CLSID" -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item "HKLM:\SOFTWARE\Classes\CLSID\$CLSID" -Recurse -Force -ErrorAction SilentlyContinue
Restart-Audio
Write-Host '  [2/3] Motor desregistrado y audio reiniciado' -ForegroundColor Green

Remove-Item $DirPrograma -Recurse -Force -ErrorAction SilentlyContinue
Write-Host '  [3/3] Archivos del motor borrados' -ForegroundColor Green
Write-Host ''
Write-Host '  Listo: el sonido quedó como antes de instalar Patagonia Sound.' -ForegroundColor Cyan
Write-Host '  (Los respaldos quedan en C:\ProgramData\Patagonia Sound por si hicieran falta.)'
Write-Host ''
