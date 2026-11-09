# Cosas compartidas por el instalador, el rescate y el diagnóstico de Patagonia Sound.
$ErrorActionPreference = 'Stop'

$Script:CLSID       = '{C90115C4-8422-4C4D-B4B4-F1EBF7E03B4D}'
$Script:IID_SFX     = '{5FA00F27-ADD6-499A-8A9D-6B98521FA75B}'   # IAudioSystemEffects
$Script:DirPrograma = Join-Path $env:ProgramFiles 'Patagonia Sound'
$Script:DirDatos    = Join-Path $env:ProgramData 'Patagonia Sound'
$Script:DirRespaldo = Join-Path $Script:DirDatos 'respaldo'
$Script:RaizRender  = 'SOFTWARE\Microsoft\Windows\CurrentVersion\MMDevices\Audio\Render'

# Claves de efectos de Windows (documentadas por Microsoft para drivers de audio).
$Script:K_SFX_COMPUESTO = '{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},13'  # lista de efectos de flujo (Win10 1803+)
$Script:K_SFX_SIMPLE    = '{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},5'   # efecto de flujo único (viejo)
$Script:K_SIN_EFECTOS   = '{1da5d803-d492-4edd-8c23-e0c0ffee7f0e},5'   # 1 = "Mejoras de audio" apagadas

function Test-Admin {
    $id = [Security.Principal.WindowsIdentity]::GetCurrent()
    (New-Object Security.Principal.WindowsPrincipal $id).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

# Las claves de los dispositivos de audio son de Windows: para tocarlas hay que tomar
# posesión y darle permiso a los Administradores (lo mismo que hace Equalizer APO).
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class PrivilegiosPatagonia {
    [DllImport("advapi32.dll", SetLastError=true)] static extern bool OpenProcessToken(IntPtr h, int acc, out IntPtr tok);
    [DllImport("advapi32.dll", SetLastError=true)] static extern bool LookupPrivilegeValue(string sys, string name, out long luid);
    [DllImport("advapi32.dll", SetLastError=true)] static extern bool AdjustTokenPrivileges(IntPtr tok, bool all, ref TP nuevo, int len, IntPtr prev, IntPtr ret);
    [DllImport("kernel32.dll")] static extern IntPtr GetCurrentProcess();
    [StructLayout(LayoutKind.Sequential, Pack=1)] struct TP { public int Count; public long Luid; public int Attr; }
    public static void Activar(string nombre) {
        IntPtr tok; long luid;
        if (!OpenProcessToken(GetCurrentProcess(), 0x28, out tok)) throw new Exception("OpenProcessToken");
        if (!LookupPrivilegeValue(null, nombre, out luid)) throw new Exception("LookupPrivilegeValue");
        TP tp = new TP { Count = 1, Luid = luid, Attr = 2 };
        if (!AdjustTokenPrivileges(tok, false, ref tp, 0, IntPtr.Zero, IntPtr.Zero)) throw new Exception("AdjustTokenPrivileges");
    }
}
'@

function Enable-PermisoClave([string]$ruta) {
    [PrivilegiosPatagonia]::Activar('SeTakeOwnershipPrivilege')
    [PrivilegiosPatagonia]::Activar('SeRestorePrivilege')
    $admins = New-Object Security.Principal.SecurityIdentifier 'S-1-5-32-544'
    $base = [Microsoft.Win32.RegistryKey]::OpenBaseKey('LocalMachine', 'Registry64')
    $k = $base.OpenSubKey($ruta, [Microsoft.Win32.RegistryKeyPermissionCheck]::ReadWriteSubTree,
        [Security.AccessControl.RegistryRights]::TakeOwnership)
    if (-not $k) { return }
    $acl = $k.GetAccessControl([Security.AccessControl.AccessControlSections]::None)
    $acl.SetOwner($admins)
    $k.SetAccessControl($acl); $k.Close()
    # Para cambiar los permisos hay que poder leerlos también (ReadPermissions).
    $k = $base.OpenSubKey($ruta, [Microsoft.Win32.RegistryKeyPermissionCheck]::ReadWriteSubTree,
        [Security.AccessControl.RegistryRights]'ReadPermissions, ChangePermissions')
    $acl = $k.GetAccessControl()
    $regla = New-Object Security.AccessControl.RegistryAccessRule($admins, 'FullControl',
        'ContainerInherit', 'None', 'Allow')
    $acl.AddAccessRule($regla)
    $k.SetAccessControl($acl); $k.Close()
}

# Salidas de sonido activas (parlantes, auriculares, HDMI...).
function Get-SalidasActivas {
    $raiz = "HKLM:\$Script:RaizRender"
    foreach ($d in Get-ChildItem $raiz) {
        $estado = (Get-ItemProperty $d.PSPath -Name DeviceState -ErrorAction SilentlyContinue).DeviceState
        if ($estado -ne 1) { continue }
        $nombre = (Get-ItemProperty "$($d.PSPath)\Properties" -ErrorAction SilentlyContinue).'{a45c254e-df1c-4efd-8020-67d146a850e0},2'
        $placa  = (Get-ItemProperty "$($d.PSPath)\Properties" -ErrorAction SilentlyContinue).'{b3f8fa53-0004-438e-9003-51a46e139bfc},6'
        [pscustomobject]@{ Id = $d.PSChildName; Nombre = "$nombre ($placa)" }
    }
}

function Restart-Audio {
    Restart-Service -Name Audiosrv -Force
    Start-Sleep -Seconds 2
}
