@echo off
call C:\Qt\Qt5.12.12\5.12.12\mingw73_64\bin\qtenv2.bat

cd /d C:\Users\One_Step_Beyond\Documents\test\test\MedChat

if not exist build mkdir build
cd build

echo === Running qmake ===
qmake ..\MedChat.pro -spec win32-g++
if errorlevel 1 (
    echo [ERROR] qmake failed
    exit /b 1
)

echo === Running mingw32-make ===
mingw32-make -j4
if errorlevel 1 (
    echo [ERROR] Build failed
    exit /b 1
)

echo === Build successful ===
