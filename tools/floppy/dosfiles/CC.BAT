@echo off
rem Compile and link C sources with Turbo C 2.01 (small memory model).
if "%1"=="" goto usage
set PATH=A:\TC;%PATH%
A:\TC\TCC -ms -IA:\TC\INCLUDE -LA:\TC\LIB %1 %2 %3 %4 %5 %6 %7 %8 %9
goto end
:usage
echo usage: CC file.c [more files]
echo        CC hello.c   --^> hello.exe
:end
