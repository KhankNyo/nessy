@echo off

set "INCARG=-I.\src\Include\"
set "CC=tcc %INCARG%"

if "clean"=="%1" (
    if exist bin\ rmdir /q /s bin
) else if "cl"=="%1" (
    if not exist bin\ mkdir bin

    pushd bin
        cl /Zi /DSTANDALONE /DDISASSEMBLER_IMPLEMENTATION ^
            /FeDisassembler.exe ..\src\Placeholder.c
    popd
) else (
    if not exist bin\ mkdir bin

    %CC% -DSTANDALONE -DDISASSEMBLER_IMPLEMENTATION ^
        -o bin\Disassembler.exe src\Placeholder.c
)
