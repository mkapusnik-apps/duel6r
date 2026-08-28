@echo off
setlocal

if not defined D6R_VS_ROOT (echo D6R_VS_ROOT is required. 1>&2 & exit /b 1)
if not defined D6R_VCTOOLS_VERSION (echo D6R_VCTOOLS_VERSION is required. 1>&2 & exit /b 1)
if not defined D6R_WINDOWS_SDK_ROOT (echo D6R_WINDOWS_SDK_ROOT is required. 1>&2 & exit /b 1)
if not defined D6R_WINDOWS_SDK_VERSION (echo D6R_WINDOWS_SDK_VERSION is required. 1>&2 & exit /b 1)

set "VCToolsInstallDir=%D6R_VS_ROOT%\VC\Tools\MSVC\%D6R_VCTOOLS_VERSION%\"
set "VCINSTALLDIR=%D6R_VS_ROOT%\VC\"
set "VSINSTALLDIR=%D6R_VS_ROOT%\"
set "WindowsSdkDir=%D6R_WINDOWS_SDK_ROOT%\"
set "WindowsSDKVersion=%D6R_WINDOWS_SDK_VERSION%\"
set "UniversalCRTSdkDir=%D6R_WINDOWS_SDK_ROOT%\"
set "UCRTVersion=%D6R_WINDOWS_SDK_VERSION%"
set "VSCMD_ARG_HOST_ARCH=x64"
set "VSCMD_ARG_TGT_ARCH=x64"
set "Platform=x64"
set "INCLUDE=%VCToolsInstallDir%include;%WindowsSdkDir%Include\%D6R_WINDOWS_SDK_VERSION%\ucrt;%WindowsSdkDir%Include\%D6R_WINDOWS_SDK_VERSION%\shared;%WindowsSdkDir%Include\%D6R_WINDOWS_SDK_VERSION%\um;%WindowsSdkDir%Include\%D6R_WINDOWS_SDK_VERSION%\winrt;%WindowsSdkDir%Include\%D6R_WINDOWS_SDK_VERSION%\cppwinrt"
set "LIB=%VCToolsInstallDir%lib\x64;%WindowsSdkDir%Lib\%D6R_WINDOWS_SDK_VERSION%\ucrt\x64;%WindowsSdkDir%Lib\%D6R_WINDOWS_SDK_VERSION%\um\x64"
set "LIBPATH=%VCToolsInstallDir%lib\x64;%WindowsSdkDir%UnionMetadata\%D6R_WINDOWS_SDK_VERSION%;%WindowsSdkDir%References\%D6R_WINDOWS_SDK_VERSION%"
set "PATH=%VCToolsInstallDir%bin\Hostx64\x64;%WindowsSdkDir%bin\%D6R_WINDOWS_SDK_VERSION%\x64;C:\Tools\cmake\bin;C:\Tools\ninja;C:\Python313;%PATH%"

where cl.exe >nul 2>&1 || (echo Unable to locate cl.exe in the mounted Visual Studio toolchain. 1>&2 & exit /b 1)
where link.exe >nul 2>&1 || (echo Unable to locate link.exe in the mounted Visual Studio toolchain. 1>&2 & exit /b 1)
where rc.exe >nul 2>&1 || (echo Unable to locate rc.exe in the mounted Windows SDK. 1>&2 & exit /b 1)
where mt.exe >nul 2>&1 || (echo Unable to locate mt.exe in the mounted Windows SDK. 1>&2 & exit /b 1)
echo Visual Studio C++ tools: %D6R_VCTOOLS_VERSION%
echo Windows SDK: %D6R_WINDOWS_SDK_VERSION%

cmake -S C:\workspace -B C:\workspace\build-windows-native-transport -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DD6R_TRANSPORT_ONLY=ON
if errorlevel 1 exit /b %errorlevel%

cmake --build C:\workspace\build-windows-native-transport --config Release --target duel6r-session-transport-tests duel6r-network-trust-policy-tests duel6r-server
if errorlevel 1 exit /b %errorlevel%

ctest --test-dir C:\workspace\build-windows-native-transport -C Release --output-on-failure -R "^(duel6r-network-trust-policy-tests|duel6r-session-transport-(tests|process-tests))$"
exit /b %errorlevel%
