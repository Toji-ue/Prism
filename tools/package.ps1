# Packages deploy\ for testers:
#   dist\Prism-Setup.exe    - NSIS installer (per-user, no admin, wizard,
#                             Add/Remove Programs entry, uninstaller)
#   dist\Prism-win64-<date>.zip - portable zip (extract and run, no install)
#
# The payload is self-contained: the app needs nothing installed on the target
# machine beyond Windows 10 64-bit (verified by running --selftest with a PATH
# of only C:\Windows).
#
# Usage: powershell -File tools\package.ps1

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$deploy = Join-Path $root 'deploy'
$dist = Join-Path $root 'dist'
$installer = Join-Path $root 'tools\installer'
$makensis = 'C:\msys64\mingw64\bin\makensis.exe'

if (-not (Test-Path (Join-Path $deploy 'prism.exe'))) {
    throw "deploy\prism.exe missing - run tools\deploy.ps1 first"
}

# --- stage the payload: app files + generated tester notes -------------------
$stage = Join-Path $dist 'stage'
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force -Path $stage | Out-Null
Copy-Item "$deploy\*" $stage -Recurse -Force
foreach ($f in @('Prism-Setup.ps1', 'Install-Prism.cmd', 'Prism-ClickHere.cmd')) {
    Remove-Item (Join-Path $stage $f) -Force -ErrorAction SilentlyContinue
}

# The tester guide ships inside the package but is not a tracked source file:
# keeping it here means the repo carries no extra document and the notes can
# never go stale relative to what actually gets built.
$notes = @'
PRISM - HUONG DAN CHO TESTER
============================

YEU CAU MAY TINH
  Windows 10 (64-bit) tro len. KHONG can cai them gi ca: khong Qt, khong
  .NET, khong Visual C++ Redistributable. O dia trong ~160 MB.

CAI DAT
  Cach 1 (khuyen nghi): chay Prism-Setup.exe -> Install. Shortcut "Prism"
      se co tren Desktop va Start Menu. Go bo: Settings > Apps > Prism >
      Uninstall.
  Cach 2 (portable): giai nen file zip roi chay thang prism.exe.
  Neu Windows hoi "Windows protected your PC": More info > Run anyway
  (installer khong tai gi tu mang).

BAO LOI
  Gui kem file log: %APPDATA%\Prism\Prism\prism.log
  (dan duong dan tren vao Explorer la toi). Log ghi loi QML va ket qua
  thao tac, co no la tim nguyen nhan nhanh hon nhieu.

NEN TEST NHUNG GI
  1. Tao document moi / mo anh PNG, JPG / luu .prism / Export PNG, JPG.
  2. Ve bang brush: doi Size, Opacity, Hardness bang ca slider va o so.
  3. Eraser: ve roi tay - kiem tra undo (Ctrl+Z) / redo (Ctrl+Shift+Z).
  4. Layers: them (+), doi ten (double-click vao ten), an/hien, lock;
     KEO-THA doi thu tu; nut Raise/Lower; keo opacity slider (so % tren
     dong layer phai doi ngay); thu blend mode (Multiply, Color Burn,
     Luminosity...); Layer > Import Image as Layer (anh lon hon canvas se
     bi cat va co thong bao o thanh trang thai).
  5. Selection: Select All, Deselect, wand, lasso; ve trong vung chon.
  6. Transform, crop, fill, text, cac shape (line/rect/ellipse).
  7. Filters: Invert, Desaturate, Blur, Brightness/Contrast.
  8. Doi hotkey: Settings > Configure Tool Shortcuts (go phim trung se co
     canh bao).
  9. Zoom slider goc phai duoi + phim tat +/-.
 10. Luu project roi mo lai - kiem tra layers/blend/opacity con dung.

DA SUA GAN DAY
  * Ve muot hon nhieu: chi nap phan canvas thay doi len GPU thay vi ca anh
    moi khung hinh. Do duoc: canvas 2000x3000 nhanh ~10 lan, 4000x4000 ~4 lan.
  * Nha chuot khong con khung: truoc day moi net ve xong phai ve lai net va
    ghep lai toan bo canvas (4000x4000 mat ~1.2 giay), gio chi xu ly dung
    vung net ve (~0.15 giay).
  * Click mot cai la co cham muc ngay (truoc day phai keo moi hien).

LOI DA BIET, CHUA SUA
  * Chua co layer group / clipping mask / alpha lock.
  * Image > Resize Canvas... chua hoat dong (dang disable).
  * Crop khong undo duoc.
  * Transform xoay bang keo ngang, khong phai khung 8 handle nhu Krita.
  Neu VAN con lag, gui kem: kich thuoc canvas, so layer, ve bang chuot hay
  but cam ung, va file log.

CAM ON BAN DA TEST!
'@
[System.IO.File]::WriteAllText((Join-Path $stage 'README-TESTER.txt'), $notes,
    [System.Text.UTF8Encoding]::new($false))

New-Item -ItemType Directory -Force -Path $dist | Out-Null

# --- installer ---------------------------------------------------------------
if (-not (Test-Path $makensis)) {
    throw "makensis not found at $makensis (pacman -S mingw-w64-x86_64-nsis)"
}
$setup = Join-Path $dist 'Prism-Setup.exe'
Remove-Item $setup -Force -ErrorAction SilentlyContinue
& $makensis '-V2' "-DSTAGE=$stage" (Join-Path $installer 'prism.nsi') | Write-Output
if ($LASTEXITCODE -ne 0) { throw "makensis failed (exit $LASTEXITCODE)" }
Write-Output ("installer: {0} ({1:N1} MB)" -f $setup, ((Get-Item $setup).Length / 1MB))

# --- portable zip ------------------------------------------------------------
$zip = Join-Path $dist ("Prism-win64-{0}.zip" -f (Get-Date -Format 'yyyyMMdd'))
Remove-Item $zip -Force -ErrorAction SilentlyContinue
Compress-Archive -Path "$stage\*" -DestinationPath $zip -CompressionLevel Optimal
Write-Output ("portable : {0} ({1:N1} MB)" -f $zip, ((Get-Item $zip).Length / 1MB))
