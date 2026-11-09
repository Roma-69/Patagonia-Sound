# Prueba para GitHub Actions: arma una clave protegida como las de las salidas de sonido
# (dueño SYSTEM, Administradores solo lectura) y comprueba que el instalador pueda escribirla.
. (Join-Path $PSScriptRoot 'comun.ps1')
$ruta = 'SOFTWARE\PatagoniaPrueba\Salida'
foreach ($sub in '', '\FxProperties', '\Properties') { New-Item -Force -Path "HKLM:\$ruta$sub" | Out-Null }

$system = New-Object Security.Principal.SecurityIdentifier 'S-1-5-18'
$admins = New-Object Security.Principal.SecurityIdentifier 'S-1-5-32-544'
foreach ($sub in '\FxProperties', '\Properties', '') {
    $acl = New-Object Security.AccessControl.RegistrySecurity
    $acl.SetAccessRuleProtection($true, $false)
    $acl.AddAccessRule((New-Object Security.AccessControl.RegistryAccessRule($system, 'FullControl', 'ContainerInherit', 'None', 'Allow')))
    $acl.AddAccessRule((New-Object Security.AccessControl.RegistryAccessRule($admins, 'ReadKey', 'ContainerInherit', 'None', 'Allow')))
    $acl.SetOwner($system)
    [PrivilegiosPatagonia]::Activar('SeRestorePrivilege')
    Set-Acl -Path "HKLM:\$ruta$sub" -AclObject $acl
}

$bloqueada = $false
try { New-ItemProperty -Path "HKLM:\$ruta\FxProperties" -Name prueba -Value 1 -PropertyType DWord -Force -ErrorAction Stop | Out-Null }
catch { $bloqueada = $true }
if (-not $bloqueada) { throw 'La clave de prueba no quedó protegida: la prueba no sirve.' }

Enable-PermisoClave $ruta
Enable-PermisoClave "$ruta\FxProperties"
Enable-PermisoClave "$ruta\Properties"
New-ItemProperty -Path "HKLM:\$ruta\FxProperties" -Name $K_SFX_COMPUESTO -Value ([string[]]@('{DA2C9ECE-7418-4906-B4FA-0A00B3EB88AA}', $CLSID)) -PropertyType MultiString -Force -ErrorAction Stop | Out-Null
New-ItemProperty -Path "HKLM:\$ruta\Properties" -Name $K_SIN_EFECTOS -Value 0 -PropertyType DWord -Force -ErrorAction Stop | Out-Null
$leido = (Get-ItemProperty "HKLM:\$ruta\FxProperties").$K_SFX_COMPUESTO
if ($leido[-1] -ne $CLSID) { throw "No quedó escrito: $leido" }
Write-Host 'OK: el instalador puede tomar y escribir claves protegidas.'
