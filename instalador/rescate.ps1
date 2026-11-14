# Saca Patagonia Sound y deja el sonido de Windows exactamente como estaba antes de instalarlo.
. (Join-Path $PSScriptRoot 'comun.ps1')

if (-not (Test-Admin)) { throw 'Hay que ejecutarlo como administrador.' }

Write-Host ''
Write-Host '  Patagonia Sound FDX - rescate' -ForegroundColor Yellow
Write-Host ''

# Windows anota en cada salida cuál está elegida (valores "Role:0/1/2"). Los respaldos son
# de antes de instalar: se guardan los de ahora y se vuelven a poner después de restaurar,
# así el sonido sigue saliendo por donde lo tenías.
$roles = @{}
foreach ($d in Get-ChildItem "HKLM:\$RaizRender") {
    $v = Get-ItemProperty $d.PSPath -ErrorAction SilentlyContinue
    foreach ($n in 'Role:0', 'Role:1', 'Role:2') {
        if ($null -ne $v.$n) { $roles["$($d.PSChildName)|$n"] = [byte[]]$v.$n }
    }
}

# 1. Restaurar cada salida desde su respaldo.
if (Test-Path $DirRespaldo) {
    foreach ($r in Get-ChildItem $DirRespaldo -Filter '*.reg') {
        $ruta = "$RaizRender\$($r.BaseName)"
        try {
            Enable-PermisoClave $ruta
            foreach ($sub in 'FxProperties', 'Properties') {
                if (Test-Path "HKLM:\$ruta\$sub") { Enable-PermisoClave "$ruta\$sub" }
            }
            # Se borra lo que haya y se vuelve a cargar el respaldo tal cual. Con reg.exe por cmd:
            # reg.exe escribe "La operación se completó correctamente" como error, y
            # Remove-Item de PowerShell falla en estas claves de audio aunque existan.
            if (Test-Path "HKLM:\$ruta\FxProperties") { & cmd.exe /c "reg.exe delete `"HKLM\$ruta\FxProperties`" /f /reg:64 >nul 2>&1" }
            & cmd.exe /c "reg.exe import `"$($r.FullName)`" /reg:64 >nul 2>&1"
            if ($LASTEXITCODE -ne 0) { throw "reg import devolvió $LASTEXITCODE" }
            Write-Host "        restaurado: $($r.BaseName)"
        } catch {
            Write-Host "        no se pudo restaurar $($r.BaseName): $_" -ForegroundColor Red
        }
    }
}

foreach ($k in $roles.Keys) {
    $id, $n = $k -split '\|', 2
    try { Set-ItemProperty "HKLM:\$RaizRender\$id" -Name $n -Value $roles[$k] -Type Binary } catch { }
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
Write-Host '  Listo: el sonido quedó como antes de instalar Patagonia Sound FDX.' -ForegroundColor Cyan
Write-Host '  (Los respaldos quedan en C:\ProgramData\Patagonia Sound por si hicieran falta.)'
Write-Host ''
