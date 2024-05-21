@echo off
cls

:: "ベースファイル名" + "_make.bat"
set batch=%~nx0
set src=%batch:_make.bat=%
set fn=
for /f "delims=. tokens=1,2" %%i in ("%src%") do (
	set fn=%%i%
)
set fn_exe=%fn%.exe
set cc=gcc.exe -std=c2x
set lib=lib_iwmutil2.a
set op_link=-Os -Wall -Wextra -lgdi32 -luser32 -lshlwapi

:: Assembler
::	echo --- Compile -S ------------------------------------
::	cp -f %fn%.s %fn%.s.old
::	for %%s in (%src%) do (
::		%cc% %%s -S %op_link%
::		echo %%~ns.s
::	)
::	echo.

:: Make
	echo --- Make ------------------------------------------
	%cc% %src% %lib% %op_link% -o %fn_exe%
	strip %fn_exe%
	echo.

:: Dump
::	cp -f %fn%.objdump %fn%.objdump.old
::	objdump -d -s %fn_exe% > %fn%.objdump
::	echo.

:: Test
	pause
	chcp 65001
	cls

	%fn_exe%
	%fn_exe% sleep 2

:: Quit
	echo.
	pause
	exit
