//------------------------------------------------------------------------------
#define   IWM_VERSION         "iwmtime_20220311"
#define   IWM_COPYRIGHT       "Copyright (C)2021-2023 iwm-iwama"
//------------------------------------------------------------------------------
#include "lib_iwmutil2.h"

INT  main();
VOID print_version();
VOID print_help();

#define   CLR_RESET           "\033[0m"
#define   CLR_TITLE1          "\033[38;2;250;250;250m\033[104m" // 白／青
#define   CLR_OPT1            "\033[38;2;250;150;150m"          // 赤
#define   CLR_OPT2            "\033[38;2;150;150;250m"          // 青
#define   CLR_OPT21           "\033[38;2;80;250;250m"           // 水
#define   CLR_OPT22           "\033[38;2;250;100;250m"          // 紅紫
#define   CLR_LBL1            "\033[38;2;250;250;100m"          // 黄
#define   CLR_LBL2            "\033[38;2;100;100;250m"          // 青
#define   CLR_STR1            "\033[38;2;225;225;225m"          // 白
#define   CLR_STR2            "\033[38;2;175;175;175m"          // 銀

INT
main()
{
	// lib_iwmutil 初期化
	iExecSec_init();       //=> $ExecSecBgn
	iCLI_getCommandLine(); //=> $CMD, $ARGC, $ARGV
	iConsole_EscOn();

	// -h | -help
	if(! $ARGC || iCLI_getOptMatch(0, L"-h", L"--help"))
	{
		print_help();
		imain_end();
	}

	// -v | -version
	if(iCLI_getOptMatch(0, L"-v", L"--version"))
	{
		print_version();
		imain_end();
	}

	WCS *wp1 = 0, *wp2 = 0;
	MBS *mp1 = 0;

	UINT uExecArgc = 0;

	// Quiet Mode
	BOOL bQuiet = FALSE;

	// Chcp
	UINT uChcp = $CP;

	// -q | -quiet
	if(iCLI_getOptMatch(0, L"-q", L"-quiet"))
	{
		++uExecArgc;
		bQuiet = TRUE;
	}
	// -cp | -codepage
	else if((wp1 = iCLI_getOptValue(0, L"-cp=", L"-codepage=")))
	{
		++uExecArgc;
		uChcp = (UINT)_wtoi(wp1);
	}

	wp1 = iwa_njoin($ARGV, L" ", uExecArgc, $ARGC);
		MBS *opCmd = W2U(wp1);
	ifree(wp1);

	MEMORYSTATUSEX memex = { sizeof(MEMORYSTATUSEX) };

	GlobalMemoryStatusEx(&memex);
	CONST DWORDLONG iBgnEmpMem = memex.ullAvailPhys;

	if(bQuiet)
	{
		mp1 = ims_cats(2, opCmd, " > NUL");
			system(mp1);
		ifree(mp1);

		P("%s[Quiet Mode]%s\n",CLR_OPT2, CLR_RESET);
	}
	else
	{
		SetConsoleOutputCP(uChcp);
		system(opCmd);
		SetConsoleOutputCP($CP);
	}

	// 計測終了
	DOUBLE dPassedSec = iExecSec_next();

	P(CLR_STR2);
	LN();
	// Program
	P("  Program  %s\n", opCmd);

	// Exec
	wp1 = iws_sprintf(L"%.4f", dPassedSec);
	wp2 = iws_addTokenNStr(wp1);
	mp1 = W2U(wp2);
		P("  Exec     %s sec\n", mp1);
	ifree(mp1);
	ifree(wp2);
	ifree(wp1);

	// Memory
	P2("  Memory   ");
	CONST INT iMs = 500;
	CONST INT iLoop = 5;
	INT i1 = 0;
	while(i1 < iLoop)
	{
		GlobalMemoryStatusEx(&memex);

		wp1 = iws_sprintf(L"%d", ((memex.ullAvailPhys - iBgnEmpMem) / 1024));
		wp2 = iws_addTokenNStr(wp1);
		mp1 = W2U(wp2);
			P("   %4d ms %7s KB\n", (iMs * i1), mp1);
		ifree(mp1);
		ifree(wp2);
		ifree(wp1);

		Sleep(iMs);
		++i1;
	}

	LN();
	P(CLR_RESET);

	// Debug
	/// icalloc_mapPrint(); ifree_all(); icalloc_mapPrint();

	// 最終処理
	imain_end();
}

VOID
print_version()
{
	P(CLR_STR2);
	LN();
	P(" %s\n", IWM_COPYRIGHT);
	P("    Ver.%s+%s\n", IWM_VERSION, LIB_IWMUTIL_VERSION);
	LN();
	P(CLR_RESET);
}

VOID
print_help()
{
	MBS *_cmd = W2U($CMD);

	print_version();
	P("%s コマンドの実行時間を計測 %s\n", CLR_TITLE1, CLR_RESET);
	P("%s    %s %s[Option] %s[Command]\n", CLR_STR1, _cmd, CLR_OPT2, CLR_OPT1);
	P("\n");
	P("%s (例１)\n", CLR_LBL1);
	P("%s    %s %snotepad\n", CLR_STR1, _cmd, CLR_OPT1);
	P("\n");
	P("%s (例２)\n", CLR_LBL1);
	P("%s    %s %s-quiet %sdir /b \"..\"\n", CLR_STR1, _cmd, CLR_OPT2, CLR_OPT1);
	P("\n");
	P("%s [Option]\n", CLR_OPT2);
	P("%s    -quiet | -q\n", CLR_OPT21);
	P("%s        コマンド出力を表示しない\n", CLR_STR1);
	P("\n");
	P("%s    -codepage=Num | -cp=Num\n", CLR_OPT21);
	P("%s        コマンド出力のコードページをNumに変更（初期値：%u）\n", CLR_STR1, $CP);
	P("%s            932 = Shift_JIS     65001 = UTF-8\n", CLR_STR1);
	P("\n");
	P(CLR_STR2);
	LN();
	P(CLR_RESET);

	ifree(_cmd);
}
