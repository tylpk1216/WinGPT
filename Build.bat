@echo off

set Path=D:\TDM-gcc-64\bin

cls

gcc -std=c99 WinGPT.c -o WinGPT.exe

if "%ERRORLEVEL%" == "0" (
    WinGPT.exe
)


REM pause