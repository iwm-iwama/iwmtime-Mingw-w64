:: Ini ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	@echo off
	cls

	:: ファイル名はソースと同じ
	set fn=%~n0
	set src=%fn%.c
	set fn_exe=%fn%.exe
	set cc=gcc.exe
	set op_link=-Os -Wall -lgdi32 -luser32 -lshlwapi
	set lib=lib_iwmutil2.a

:: Make ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

	echo --- Compile -S ------------------------------------
	for %%s in (%src%) do (
		%cc% %%s -S %op_link%
		echo %%~ns.s
	)
	echo.

	echo --- Make ------------------------------------------
	for %%s in (%src%) do (
		echo %%s
		%cc% %%s -c -Wall %op_link%
	)
	%cc% *.o %lib% -o %fn_exe% %op_link%
	echo.

	:: 後処理
	strip %fn_exe%
	rm *.o

	:: 失敗
	if not exist "%fn_exe%" goto end

	:: 成功
	echo.
	pause

:: Test ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	chcp 65001
	cls
	set t=%time%
	echo [%t%]

	%fn_exe%
	%fn_exe% sleep 5

	echo [%t%]
	echo [%time%]

:: Quit ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
:end
	echo.
	pause
	exit
