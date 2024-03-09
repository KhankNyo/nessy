@echo off

set "INCARG=-I.\src\Include\ -I.\src\Include\Win32TCC"
set "CC=gcc -Wno-unused-const-variable -Wno-type-limits -Wall -Wextra -Wpedantic %INCARG%"
set "MSVC_CL=cl /std:c11 /Zi /DDEBUG /DEBUG /MTd /I..\src\Include\"

if "clean"=="%1" (
    if exist bin\ rmdir /q /s bin
) else if "cl"=="%1" (
    if not exist bin\ mkdir bin

    rem uhhh hopefully people have msvc installed in their C drive 
    if "%VisualStudioVersion%"=="" call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

    pushd bin
        %MSVC_CL% /DSTANDALONE ^
            /FeDisassembler.exe ..\src\Disassembler.c ..\src\utils.c
        %MSVC_CL% /DSTANDALONE ^
            /Fe6502.exe ..\src\6502.c ..\src\utils.c
        %MSVC_CL% ^
            /FeNessy.exe ..\src\Win32.c ..\src\utils.c ^
             ^
            comctl32.lib gdi32.lib comdlg32.lib user32.lib kernel32.lib ole32.lib
    popd
) else (
    if not exist bin\ mkdir bin

    %CC% -DSTANDALONE ^
        -o bin\Disassembler.exe src\Disassembler.c src\Utils.c
    %CC% -DSTANDALONE ^
        -o bin\6502.exe src\6502.c src\Utils.c
    %CC% -o bin\Nessy.exe ^
        src\Win32.c src\Utils.c^
        -lcomctl32 -lgdi32 -lcomdlg32 -lole32
)
