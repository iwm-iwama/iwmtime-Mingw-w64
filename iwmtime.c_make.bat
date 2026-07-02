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
set cc=gcc.exe -std=c23
set lib=lib_iwmutil2.a
set op_link=-Os -Wall -Wextra -lgdi32 -luser32 -lshlwapi

::-----------------------------------------------------------------------------
:: 本処理
::-----------------------------------------------------------------------------
goto R20
:R10
:: Assembler
	if exist "%fn%_old.s" (
		rm "%fn%_old.s"
	)
	if exist "%fn%.s" (
		mv "%fn%.s" "%fn%_old.s"
	)
	for %%s in (%src%) do (
		%cc% %%s -S %op_link%
	)
	echo --- -S ------------------------------------------------------
	ls -la %fn%.s
	echo.

:R20
:: Make
	echo --- Make ------------------------------------------
	%cc% %src% %lib% %op_link% -o %fn_exe%
	strip %fn_exe%
	echo.

goto R90
:R30
:: Dump
	if exist "%fn%.objdump.old" (
		rm "%fn%.objdump.old"
	)
	if exist "%fn%.objdump" (
		mv "%fn%.objdump" "%fn%.objdump.old"
	)
	echo --- Dump ----------------------------------------------------
	objdump -d -s %fn_a% > %fn%.objdump
	ls -la %fn%.objdump
	echo.

:R90
:: Test
	pause
	chcp 65001
	cls

	%fn_exe%
	%fn_exe% sleep 2

:R99
:: Quit
	echo.
	pause
	exit
