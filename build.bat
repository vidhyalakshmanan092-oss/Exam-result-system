@echo off
echo Compiling...
gcc server.c -o server.exe -lws2_32
if errorlevel 1 (
  echo.
  echo Compile failed. Make sure MinGW gcc is installed and in PATH.
  pause
  exit /b 1
)
echo Starting server...
server.exe
pause
