# Regenera las capturas del README (docs/screenshots/*.png) jugando el ejecutable solo.
#
# Uso, desde la carpeta del proyecto y con el juego ya compilado:
#     powershell -ExecutionPolicy Bypass -File docs\take_screenshots.ps1
#     powershell -ExecutionPolicy Bypass -File docs\take_screenshots.ps1 -Config Release
#
# Abre el juego, pulsa las teclas de cada ejemplo (1/2/3, F1, F2) y captura el area
# cliente de la ventana. Mientras corre (unos 20 s) NO toques el teclado ni el raton:
# las teclas se mandan a la ventana que tenga el foco.
#
# Para una captura suelta no hace falta este script: dentro del juego, F9 guarda la
# pantalla en screenshots/ (ver engine/Screenshot.h).
#
# El script nunca guarda el nivel (no pulsa Ctrl+S y cierra el juego a la fuerza), y al
# terminar comprueba que el .level.json siga identico.

param(
    [ValidateSet('Debug', 'Release')] [string]$Config = 'Debug',
    [string]$OutDir = 'docs\screenshots',
    # Fila de la lista "Objetos" del editor que se selecciona para que el Inspector
    # salga lleno (0 = la primera). Con el nivel actual, la 5 es "#7 Fruit" (cerezas).
    [int]$EditorRow = 5,
    # Las capturas mas anchas que esto se reducen (el editor maximizado pesa mucho).
    [int]$MaxWidth = 1600
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$exe = Join-Path $root "x64\$Config\sdl_game_engine.exe"
if (-not (Test-Path $exe)) { throw "No existe $exe. Compila primero ($Config|x64)." }
$level = Join-Path $root 'assets\maps\platformer_level1.level.json'
$levelHash = (Get-FileHash $level).Hash
New-Item -ItemType Directory -Force $OutDir | Out-Null

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public class W {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint f);
  [DllImport("user32.dll")] public static extern uint GetDpiForWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr v);
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, int dx, int dy, uint d, UIntPtr e);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
}
"@
# Sin esto, con la pantalla escalada (p.ej. al 150 %) Windows le da a PowerShell
# coordenadas "virtuales" y la captura sale recortada. -4 = PER_MONITOR_AWARE_V2.
[void][W]::SetProcessDpiAwarenessContext([IntPtr](-4))

# Codigos de tecla virtuales de Windows
$VK_SPACE = 0x20; $VK_RIGHT = 0x27; $VK_DOWN = 0x28
$VK_1 = 0x31; $VK_2 = 0x32; $VK_3 = 0x33; $VK_F1 = 0x70; $VK_F2 = 0x71

function Find-GameWindow([int]$procId) {
    $script:found = [IntPtr]::Zero
    [W]::EnumWindows({ param($h, $p)
        $owner = 0; [void][W]::GetWindowThreadProcessId($h, [ref]$owner)
        if ($owner -eq $procId -and [W]::IsWindowVisible($h)) {
            $sb = New-Object System.Text.StringBuilder 256
            [void][W]::GetWindowText($h, $sb, 256)
            # La ventana del juego (no la consola) tiene el titulo "Ejemplo N: ..."
            if ($sb.ToString().StartsWith('Ejemplo')) { $script:found = $h; return $false }
        }
        return $true }, [IntPtr]::Zero) | Out-Null
    return $script:found
}

# Guarda el area cliente (sin marco ni barra de titulo). PrintWindow con el flag 2
# (PW_RENDERFULLCONTENT) captura aunque otra ventana tape al juego.
function Save-Shot([IntPtr]$h, [string]$name) {
    $wr = New-Object W+RECT; [void][W]::GetWindowRect($h, [ref]$wr)
    $full = New-Object System.Drawing.Bitmap ($wr.R - $wr.L), ($wr.B - $wr.T)
    $g = [System.Drawing.Graphics]::FromImage($full)
    $hdc = $g.GetHdc(); [void][W]::PrintWindow($h, $hdc, 2); $g.ReleaseHdc($hdc); $g.Dispose()

    $cr = New-Object W+RECT; [void][W]::GetClientRect($h, [ref]$cr)
    $pt = New-Object W+POINT; [void][W]::ClientToScreen($h, [ref]$pt)
    $area = New-Object System.Drawing.Rectangle ($pt.X - $wr.L), ($pt.Y - $wr.T), $cr.R, $cr.B
    $shot = $full.Clone($area, $full.PixelFormat)
    $full.Dispose()

    if ($shot.Width -gt $MaxWidth) {
        $w = $MaxWidth; $hh = [int]($shot.Height * $w / $shot.Width)
        $small = New-Object System.Drawing.Bitmap $w, $hh
        $g = [System.Drawing.Graphics]::FromImage($small)
        $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $g.DrawImage($shot, 0, 0, $w, $hh); $g.Dispose()
        $shot.Dispose(); $shot = $small
    }
    $path = Join-Path (Resolve-Path $OutDir) $name
    $shot.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    Write-Output ("  {0} ({1}x{2})" -f $name, $shot.Width, $shot.Height)
    $shot.Dispose()
}

function Key-Down([IntPtr]$h, [byte]$vk) {
    [void][W]::SetForegroundWindow($h); Start-Sleep -Milliseconds 100
    [W]::keybd_event($vk, 0, 0, [UIntPtr]::Zero)
}
function Key-Up([byte]$vk) { [W]::keybd_event($vk, 0, 2, [UIntPtr]::Zero) }
function Press([IntPtr]$h, [byte]$vk, [int]$holdMs = 120) {
    Key-Down $h $vk; Start-Sleep -Milliseconds $holdMs; Key-Up $vk; Start-Sleep -Milliseconds 150
}

# Clic izquierdo en coordenadas de la ventana (pixeles fisicos del area cliente).
function Click([IntPtr]$h, [int]$x, [int]$y) {
    $pt = New-Object W+POINT; $pt.X = $x; $pt.Y = $y; [void][W]::ClientToScreen($h, [ref]$pt)
    [void][W]::SetCursorPos($pt.X, $pt.Y); Start-Sleep -Milliseconds 200
    [W]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 40
    [W]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 100
}

$game = Start-Process -FilePath $exe -WorkingDirectory $root -PassThru
try {
    $h = [IntPtr]::Zero
    for ($i = 0; $i -lt 50 -and $h -eq [IntPtr]::Zero; $i++) {
        Start-Sleep -Milliseconds 200; $h = Find-GameWindow $game.Id
    }
    if ($h -eq [IntPtr]::Zero) { throw 'No aparecio la ventana del juego.' }
    Start-Sleep -Seconds 2
    Write-Output "Capturas en $OutDir :"

    # 1) Platformer: corre a la derecha y salta; la foto es en pleno salto
    Key-Down $h $VK_RIGHT
    Start-Sleep -Milliseconds 700
    Press $h $VK_SPACE
    Start-Sleep -Milliseconds 180
    Save-Shot $h 'platformer.png'
    Key-Up $VK_RIGHT
    Start-Sleep -Seconds 1

    # 1b) El mismo nivel con F1 (colliders)
    Press $h $VK_F1; Start-Sleep -Milliseconds 400
    Save-Shot $h 'platformer_debug.png'
    Press $h $VK_F1

    # 2) Top-down: camina un poco hacia abajo
    Press $h $VK_2; Start-Sleep -Seconds 1
    Press $h $VK_DOWN 600
    Start-Sleep -Milliseconds 400
    Save-Shot $h 'topdown.png'

    # 3) Shooter: espera a que entren enemigos y dispara dos veces
    Press $h $VK_3; Start-Sleep -Seconds 4
    Press $h $VK_SPACE; Start-Sleep -Milliseconds 150
    Press $h $VK_SPACE; Start-Sleep -Milliseconds 100
    Save-Shot $h 'shooter.png'

    # 4) Editor: vuelve al platformer, F2, y elige una fila de la lista "Objetos" para
    #    que el Inspector muestre datos. La lista esta arriba a la izquierda y ImGui se
    #    escala con la pantalla, asi que la posicion se calcula con esa escala.
    Press $h $VK_1; Start-Sleep -Seconds 1
    Press $h $VK_F2; Start-Sleep -Seconds 2
    $scale = [W]::GetDpiForWindow($h) / 96.0
    Click $h ([int](60 * $scale)) ([int]((107.5 + 17 * $EditorRow) * $scale))
    # El raton a la barra de estado de abajo: fuera de los paneles, para que ningun boton
    # salga resaltado por tener el cursor encima.
    $cr = New-Object W+RECT; [void][W]::GetClientRect($h, [ref]$cr)
    $pt = New-Object W+POINT; $pt.X = [int]($cr.R / 2); $pt.Y = $cr.B - 4
    [void][W]::ClientToScreen($h, [ref]$pt); [void][W]::SetCursorPos($pt.X, $pt.Y)
    Start-Sleep -Milliseconds 500
    Save-Shot $h 'editor.png'
} finally {
    Key-Up $VK_RIGHT
    if (-not $game.HasExited) { Stop-Process -Id $game.Id -Force }
}

if ((Get-FileHash $level).Hash -ne $levelHash) {
    Write-Warning "El archivo de nivel cambio durante las capturas: revisalo con git diff."
}
