//------------------------------------------------------------------------------
#define   IWM_COPYRIGHT       "(C)2021-2024 iwm-iwama"
#define   IWM_VERSION         "iwmtime_20240118"
//------------------------------------------------------------------------------
#include "lib_iwmutil2.h"
#include <psapi.h>

#define   MB                  (DOUBLE)(1024*1024)
#define   NANO100             (DOUBLE)10000000

INT       main();
VOID      print_version();
VOID      print_help();

INT
main()
{
	// lib_iwmutil2 初期化
	imain_begin();

	///iCLI_VarList();

	// -h | --help
	if(! $ARGC || iCLI_getOptMatch(0, L"-h", L"--help"))
	{
		print_help();
		imain_end();
	}

	// -v | --version
	if(iCLI_getOptMatch(0, L"-v", L"--version"))
	{
		print_version();
		imain_end();
	}

	WS *pARG = $ARG;
	WS *wpCmd = 0;

	// -c | -cmd
	if(iCLI_getOptMatch(0, L"-c", L"-cmd"))
	{
		// 末尾の空白忘れずに
		if(iwb_cmpf(pARG, L"-c "))
		{
			pARG += 3;
		}
		else if(iwb_cmpf(pARG, L"-cmd "))
		{
			pARG += 5;
		}
		wpCmd = iws_cats(2, L"cmd.exe /c ", pARG);
	}
	else
	{
		wpCmd = iws_clone(pARG);
	}

	STARTUPINFOW si;
		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(si);
	PROCESS_INFORMATION pi;
		ZeroMemory(&pi, sizeof(pi));

	if(CreateProcessW(NULL, wpCmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
	{
		WaitForSingleObject(pi.hProcess, INFINITE);

		HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pi.dwProcessId);
			FILETIME creationTime, exitTime, kernelTime, userTime;
			GetProcessTimes(hProcess, &creationTime, &exitTime, &kernelTime, &userTime);
			PROCESS_MEMORY_COUNTERS pmc;
			GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc));
			MS *mpCmd = W2M(wpCmd);
				SetConsoleOutputCP(65001);
				P1("\033[92m");
				LN(80);
				P(
					"\033[92m"	"  Program"	"\033[96m"	"  %s\n"
					"\033[92m"	"  Execute"	"\033[96m"	"  %.3f sec\n"
					"\033[92m"	"  Memory "	"\033[96m"	"  %.3f MB\n"
					,
					mpCmd,
					((iFinfo_ftimeToINT64(exitTime) - iFinfo_ftimeToINT64(creationTime)) / NANO100),
					(pmc.PeakPagefileUsage / MB)
				);
				P1("\033[92m");
				LN(80);
				P2(IESC_RESET);
			ifree(mpCmd);
		CloseHandle(hProcess);
	}
	else
	{
		P2(IESC_FALSE1 "[Err] コマンドを確認してください!" IESC_RESET);
	}
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	ifree(wpCmd);

	///icalloc_mapPrint(); ifree_all(); icalloc_mapPrint();

	// 最終処理
	imain_end();
}

VOID
print_version()
{
	P1(IESC_STR2);
	LN(80);
	P(
		" %s\n"
		"    %s+%s\n"
		,
		IWM_COPYRIGHT,
		IWM_VERSION,
		LIB_IWMUTIL_VERSION
	);
	LN(80);
	P1(IESC_RESET);
}

VOID
print_help()
{
	MS *_cmd = "iwmtime.exe";

	print_version();
	P(
		IESC_TITLE1	" コマンドの実行時間を計測 "	IESC_RESET	"\n\n"
		IESC_STR1	"    %s"
		IESC_OPT2	" [Option]"
		IESC_OPT1	" [Command]\n\n\n"
		IESC_LBL1	" (例１)\n"
		IESC_STR1	"    %s"
		IESC_OPT1	" notepad.exe\n\n"
		IESC_LBL1	" (例２)\n"
		IESC_STR1	"    %s"
		IESC_OPT2	" -cmd"
		IESC_OPT1	" dir /b\n\n\n"
		,
		_cmd,
		_cmd,
		_cmd
	);
	P1(
		IESC_OPT2	" [Option]\n"
		IESC_OPT21	"    -cmd | -c\n"
		IESC_OPT1	"        cmd.exe /c [Command]"
		IESC_STR1	" を実行\n\n"
	);
	P1(IESC_STR2);
	LN(80);
	P1(IESC_RESET);
}
