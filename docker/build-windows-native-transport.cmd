@echo off
setlocal

call C:\BuildTools\Common7\Tools\VsDevCmd.bat -arch=amd64 -host_arch=amd64
if errorlevel 1 exit /b %errorlevel%

cmake -S C:\workspace -B C:\workspace\build-windows-native-transport -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DD6R_TRANSPORT_ONLY=ON
if errorlevel 1 exit /b %errorlevel%

cmake --build C:\workspace\build-windows-native-transport --config Release --target duel6r-session-transport-tests duel6r-server
if errorlevel 1 exit /b %errorlevel%

ctest --test-dir C:\workspace\build-windows-native-transport -C Release --output-on-failure -R "^duel6r-session-transport-(tests|process-tests)$"
exit /b %errorlevel%
