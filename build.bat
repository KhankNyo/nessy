@echo off

if "clean"=="%1" (
    if exist bin\ rmdir /q /s bin
) else if "cl"=="%1" (
    if not exist bin\ mkdir bin

    pushd bin
        cl /Zi /DSTANDALONE /DMC6502_IMPLEMENTATION ^
            /Fe6502.exe ..\src\Include\6502.h
    popd
) else (
    if not exist bin\ mkdir bin

    set "CC=tcc"
    %CC% -DSTANDALONE -DMC6502_IMPLEMENTATION ^
        -o bin\6502.exe src\Include\6502.h
)
