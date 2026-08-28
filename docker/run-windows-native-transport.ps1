$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$workspace = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path.TrimEnd('\')
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'

if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) {
    throw "vswhere.exe is not available at $vswhere"
}

$vsRoot = [string](& $vswhere -latest -version '[17.0,18.0)' -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1)
$vsRoot = $vsRoot.Trim()
if (-not $vsRoot) {
    throw 'Visual Studio 2022 with the x64 C++ toolchain is not available.'
}

$vsDevCmd = Join-Path $vsRoot 'Common7\Tools\VsDevCmd.bat'
if (-not (Test-Path -LiteralPath $vsDevCmd -PathType Leaf)) {
    throw "VsDevCmd.bat is not available at $vsDevCmd"
}

$environmentCommand = "`"$vsDevCmd`" -no_logo -arch=amd64 -host_arch=amd64 >nul && set"
$environmentOutput = & $env:ComSpec /d /s /c $environmentCommand
if ($LASTEXITCODE -ne 0) {
    throw "VsDevCmd.bat failed with exit code $LASTEXITCODE"
}

$developerEnvironment = @{}
foreach ($line in $environmentOutput) {
    $separator = $line.IndexOf('=')
    if ($separator -gt 0) {
        $developerEnvironment[$line.Substring(0, $separator)] = $line.Substring($separator + 1)
    }
}

$vcToolsVersion = [string]$developerEnvironment['VCToolsVersion']
$sdkRoot = [string]$developerEnvironment['WindowsSdkDir']
$sdkVersion = [string]$developerEnvironment['WindowsSDKVersion']
$vcToolsVersion = $vcToolsVersion.TrimEnd('\')
$sdkRoot = $sdkRoot.TrimEnd('\')
$sdkVersion = $sdkVersion.TrimEnd('\')

if (-not $vcToolsVersion -or -not $sdkRoot -or -not $sdkVersion) {
    throw 'VsDevCmd.bat did not report the Visual C++ tools and Windows SDK paths.'
}

$requiredHostTools = @(
    (Join-Path $vsRoot "VC\Tools\MSVC\$vcToolsVersion\bin\Hostx64\x64\cl.exe")
    (Join-Path $vsRoot "VC\Tools\MSVC\$vcToolsVersion\bin\Hostx64\x64\link.exe")
    (Join-Path $sdkRoot "bin\$sdkVersion\x64\rc.exe")
    (Join-Path $sdkRoot "bin\$sdkVersion\x64\mt.exe")
)

foreach ($tool in $requiredHostTools) {
    if (-not (Test-Path -LiteralPath $tool -PathType Leaf)) {
        throw "Required native Windows tool is not available at $tool"
    }
}

Write-Host "Visual Studio installation: $vsRoot"
Write-Host "Visual C++ tools version: $vcToolsVersion"
Write-Host "Windows SDK installation: $sdkRoot"
Write-Host "Windows SDK version: $sdkVersion"

$dockerArguments = @(
    'run'
    '--rm'
    '--mount'
    "type=bind,source=$workspace,target=C:\workspace"
    '--mount'
    "type=bind,source=$vsRoot,target=C:\HostTools\VS,readonly"
    '--mount'
    "type=bind,source=$sdkRoot,target=C:\HostTools\WindowsKits10,readonly"
    '--env'
    'D6R_VS_ROOT=C:\HostTools\VS'
    '--env'
    "D6R_VCTOOLS_VERSION=$vcToolsVersion"
    '--env'
    'D6R_WINDOWS_SDK_ROOT=C:\HostTools\WindowsKits10'
    '--env'
    "D6R_WINDOWS_SDK_VERSION=$sdkVersion"
    'duel6r-windows-transport:ci'
)

& docker @dockerArguments
if ($LASTEXITCODE -ne 0) {
    throw "Native Windows container failed with exit code $LASTEXITCODE"
}
