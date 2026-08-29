$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$workspace = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path.TrimEnd('\')
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'

if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) {
    throw "vswhere.exe is not available at $vswhere"
}

$installations = @(& $vswhere -all -products * -format json | ConvertFrom-Json)
$vsRoot = $null
$vcToolsVersion = $null
$vcRuntimeRelativePath = $null
$sdkRoot = $null
$sdkVersion = $null

foreach ($installation in ($installations | Sort-Object { [version]$_.installationVersion } -Descending)) {
    $candidateVsRoot = ([string]$installation.installationPath).Trim()
    $candidateVsDevCmd = Join-Path $candidateVsRoot 'Common7\Tools\VsDevCmd.bat'
    if (-not (Test-Path -LiteralPath $candidateVsDevCmd -PathType Leaf)) {
        continue
    }

    $environmentCommand = "`"$candidateVsDevCmd`" -no_logo -arch=amd64 -host_arch=amd64 >nul && set"
    $environmentOutput = & $env:ComSpec /d /s /c $environmentCommand
    if ($LASTEXITCODE -ne 0) {
        continue
    }

    $developerEnvironment = @{}
    foreach ($line in $environmentOutput) {
        $separator = $line.IndexOf('=')
        if ($separator -gt 0) {
            $developerEnvironment[$line.Substring(0, $separator)] = $line.Substring($separator + 1)
        }
    }

    $candidateVcToolsVersion = ([string]$developerEnvironment['VCToolsVersion']).TrimEnd('\')
    $candidateVcRedistDir = ([string]$developerEnvironment['VCToolsRedistDir']).TrimEnd('\')
    $candidateSdkRoot = ([string]$developerEnvironment['WindowsSdkDir']).TrimEnd('\')
    $candidateSdkVersion = ([string]$developerEnvironment['WindowsSDKVersion']).TrimEnd('\')
    if (-not $candidateVcToolsVersion -or -not $candidateVcRedistDir -or -not $candidateSdkRoot -or -not $candidateSdkVersion) {
        continue
    }

    $candidateVcRuntimeDir = Join-Path $candidateVcRedistDir 'x64\Microsoft.VC143.CRT'
    if (-not $candidateVcRuntimeDir.StartsWith($candidateVsRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        continue
    }

    $requiredCandidateTools = @(
        (Join-Path $candidateVsRoot "VC\Tools\MSVC\$candidateVcToolsVersion\bin\Hostx64\x64\cl.exe")
        (Join-Path $candidateVsRoot "VC\Tools\MSVC\$candidateVcToolsVersion\bin\Hostx64\x64\link.exe")
        (Join-Path $candidateSdkRoot "bin\$candidateSdkVersion\x64\rc.exe")
        (Join-Path $candidateSdkRoot "bin\$candidateSdkVersion\x64\mt.exe")
        (Join-Path $candidateVcRuntimeDir 'msvcp140.dll')
        (Join-Path $candidateVcRuntimeDir 'vcruntime140.dll')
    )
    $missingTool = $requiredCandidateTools | Where-Object {
        -not (Test-Path -LiteralPath $_ -PathType Leaf)
    } | Select-Object -First 1
    if ($missingTool) {
        continue
    }

    $vsRoot = $candidateVsRoot
    $vcToolsVersion = $candidateVcToolsVersion
    $vcRuntimeRelativePath = $candidateVcRuntimeDir.Substring($candidateVsRoot.Length).TrimStart('\')
    $sdkRoot = $candidateSdkRoot
    $sdkVersion = $candidateSdkVersion
    break
}

if (-not $vsRoot) {
    throw 'No installed Visual Studio instance provides MSVC x64 and compatible Windows SDK tools.'
}

Write-Host "Visual Studio installation: $vsRoot"
Write-Host "Visual C++ tools version: $vcToolsVersion"
Write-Host "Visual C++ runtime: $vcRuntimeRelativePath"
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
    "D6R_VC_RUNTIME_DIR=C:\HostTools\VS\$vcRuntimeRelativePath"
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
