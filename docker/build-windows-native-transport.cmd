@echo off
setlocal

call C:\BuildTools\Common7\Tools\VsDevCmd.bat -arch=amd64 -host_arch=amd64
if errorlevel 1 exit /b %errorlevel%

set "D6R_WINDOWS_SDK_BIN="
for /f "delims=" %%D in ('dir /b /ad /o-n "C:\Program Files (x86)\Windows Kits\10\bin\10.*" 2^>nul') do if not defined D6R_WINDOWS_SDK_BIN set "D6R_WINDOWS_SDK_BIN=C:\Program Files (x86)\Windows Kits\10\bin\%%D\x64"
if not defined D6R_WINDOWS_SDK_BIN (
    echo Unable to locate the Windows SDK tools directory. 1>&2
    exit /b 1
)
set "PATH=%D6R_WINDOWS_SDK_BIN%;%PATH%"
where rc.exe >nul 2>&1 || (echo Unable to locate rc.exe in the Windows SDK. 1>&2 & exit /b 1)
where mt.exe >nul 2>&1 || (echo Unable to locate mt.exe in the Windows SDK. 1>&2 & exit /b 1)
echo Windows SDK tools: %D6R_WINDOWS_SDK_BIN%

cmake -S C:\workspace -B C:\workspace\build-windows-native-transport -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DD6R_TRANSPORT_ONLY=ON
if errorlevel 1 exit /b %errorlevel%

cmake --build C:\workspace\build-windows-native-transport --config Release --target duel6r-session-transport-tests duel6r-server
if errorlevel 1 exit /b %errorlevel%

ctest --test-dir C:\workspace\build-windows-native-transport -C Release --output-on-failure -R "^duel6r-session-transport-(tests|process-tests)$"
exit /b %errorlevel%
