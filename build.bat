@echo off

set "INCARG=-I.\src\Include\"
set "CC=gcc -DDEBUG -Wall -Wextra -Wpedantic %INCARG%"

if "clean"=="%1" (
    if exist bin\ rmdir /q /s bin
) else if "cl"=="%1" (
    if not exist bin\ mkdir bin

    pushd bin
        cl /Zi /DSTANDALONE ^
            /FeDisassembler.exe ..\src\Disassembler.c ..\src\utils.c
        cl /Zi /DSTANDALONE ^
            /Fe6502.exe ..\src\6502.c ..\src\utils.c
        cl /Zi ^
            /FeNessy.exe ..\src\Win32.c ..\src\utils.c
    popd
) else (
    if not exist bin\ mkdir bin

    %CC% -DSTANDALONE ^
        -o bin\Disassembler.exe src\Disassembler.c src\Utils.c
    %CC% -DSTANDALONE ^
        -o bin\6502.exe src\6502.c src\Utils.c
    %CC% -o bin\Nessy.exe ^
        src\Win32.c src\Utils.c^
        -lcomctl32 -lgdi32
)
