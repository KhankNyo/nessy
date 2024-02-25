@echo off

set "INCARG=-I.\src\Include\"
set "CC=gcc -DDEBUG -Wall -Wextra -Wpedantic %INCARG%"

if "clean"=="%1" (
    if exist bin\ rmdir /q /s bin
) else if "cl"=="%1" (
    if not exist bin\ mkdir bin

    pushd bin
        cl /Zi /DSTANDALONE /DDISASSEMBLER_IMPLEMENTATION ^
            /FeDisassembler.exe ..\src\Placeholder.c
        cl /Zi /DSTANDALONE /DMC6502_IMPLEMENTATION ^
            /FeDisassembler.exe ..\src\Placeholder.c
    popd
) else (
    if not exist bin\ mkdir bin

    %CC% -DSTANDALONE -DDISASSEMBLER_IMPLEMENTATION ^
        -o bin\Disassembler.exe src\Placeholder.c
    %CC% -DSTANDALONE -DMC6502_IMPLEMENTATION ^
        -o bin\MC6502.exe src\Placeholder.c
    %CC% -o bin\Nessy.exe ^
        src\Win32.c src\Utils.c^
        -lcomctl32 -lgdi32
)
