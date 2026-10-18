# Rebuild the existing TIFF dependency without the optional GPL JBIG codec.
param([string]$MsysRoot = 'C:\msys64')
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$work = Join-Path $root 'build\tiff-runtime'
$version = '4.7.2'
$sourceHash = '672bd7d10aee4606171afb864f3570b83340f6a33e2c186dc0512f7145ffdf6a'
$archive = Join-Path $work "tiff-$version.tar.gz"
$source = Join-Path $work "tiff-$version"
$build = Join-Path $work 'build'
$provenance = Join-Path $work 'provenance'
$bin = Join-Path $MsysRoot 'ucrt64\bin'
$env:PATH = "$bin;$env:PATH"
New-Item -ItemType Directory -Force -Path $work, $provenance | Out-Null
if (-not (Test-Path -LiteralPath $archive)) {
    Invoke-WebRequest -UseBasicParsing -Uri "https://download.osgeo.org/libtiff/tiff-$version.tar.gz" -OutFile $archive
}
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $sourceHash) { throw 'TIFF source archive checksum mismatch' }
Push-Location $work
try {
    & (Join-Path $bin 'cmake.exe') -E tar xzf $archive
    if ($LASTEXITCODE -ne 0) { throw 'TIFF source extraction failed' }
} finally { Pop-Location }
& (Join-Path $bin 'cmake.exe') -S $source -B $build -G Ninja '-DCMAKE_BUILD_TYPE=Release' '-DBUILD_SHARED_LIBS=ON' '-Djbig=OFF' '-Dtiff-tools=OFF' '-Dtiff-tests=ON' '-Dtiff-contrib=OFF' '-Dtiff-docs=OFF'
if ($LASTEXITCODE -ne 0) { throw 'TIFF configure failed' }
& (Join-Path $bin 'cmake.exe') --build $build --parallel 2
if ($LASTEXITCODE -ne 0) { throw 'TIFF build failed' }
& (Join-Path $bin 'ctest.exe') --test-dir $build --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'TIFF tests failed' }
$dll = Join-Path $build 'libtiff\libtiff-6.dll'
$imports = @(& (Join-Path $bin 'objdump.exe') -p $dll)
if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect TIFF imports' }
if ($imports -match 'DLL Name:.*jbig') { throw 'Rebuilt TIFF still imports JBIG' }
Copy-Item -LiteralPath $dll -Destination (Join-Path $provenance 'libtiff-6.dll') -Force
Copy-Item -LiteralPath $archive -Destination $provenance -Force
Copy-Item -LiteralPath (Join-Path $source 'LICENSE.md') -Destination $provenance -Force
Copy-Item -LiteralPath $PSCommandPath -Destination $provenance -Force
$manifest = [ordered]@{
    schemaVersion = 1
    version = $version
    jbig = $false
    sourceUrl = "https://download.osgeo.org/libtiff/tiff-$version.tar.gz"
    sourceSha256 = $sourceHash
    dllSha256 = (Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash.ToLowerInvariant()
    recipeSha256 = (Get-FileHash -LiteralPath $PSCommandPath -Algorithm SHA256).Hash.ToLowerInvariant()
    licenseSha256 = (Get-FileHash -LiteralPath (Join-Path $source 'LICENSE.md') -Algorithm SHA256).Hash.ToLowerInvariant()
    checks = @('libtiff ctest', 'PE imports contain no JBIG')
    limitation = 'TIFF with JBIG compression is unsupported; other configured TIFF codecs remain enabled.'
}
[IO.File]::WriteAllText((Join-Path $provenance 'manifest.json'), ($manifest | ConvertTo-Json -Depth 5), [Text.UTF8Encoding]::new($false))
Write-Output "Verified TIFF runtime: $provenance"
