//------------------------------------------------------------------------------
#define  IWM_VERSION         "iwmtime_20220912"
#define  IWM_COPYRIGHT       "Copyright (C)2021-2022 iwm-iwama"
//------------------------------------------------------------------------------
#include "lib_iwmutil2.h"

INT  main();
VOID print_version();
VOID print_help();

// リセット
#define  PRGB00()            P0("\033[0m")
// ラベル
#define  PRGB01()            P0("\033[38;2;255;255;0m")    // 黄
#define  PRGB02()            P0("\033[38;2;255;255;255m")  // 白
// 入力例／注
#define  PRGB11()            P0("\033[38;2;255;255;100m")  // 黄
#define  PRGB12()            P0("\033[38;2;255;220;150m")  // 橙
#define  PRGB13()            P0("\033[38;2;100;100;255m")  // 青
// オプション
#define  PRGB21()            P0("\033[38;2;80;255;255m")   // 水
#define  PRGB22()            P0("\033[38;2;255;100;255m")  // 紅紫
// 本文
#define  PRGB91()            P0("\033[38;2;255;255;255m")  // 白
#define  PRGB92()            P0("\033[38;2;200;200;200m")  // 銀

INT
main()
{
	// lib_iwmutil 初期化
	iExecSec_init();       //=> $ExecSecBgn
	iCLI_getCommandLine(); //=> $CMD, $ARG, $ARGC, $ARGV
	iConsole_EscOn();

	// -h | -help
	if(! $ARGC || iCLI_getOptMatch(0, L"-h", L"-help"))
	{
		print_help();
		imain_end();
	}

	// -v | -version
	if(iCLI_getOptMatch(0, L"-v", L"-version"))
	{
		print_version();
		imain_end();
	}

	WCS *wp1 = 0, *wp2 = 0;
	U8N *up1 = 0;

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
		uChcp = (UINT)inum_wtoi(wp1);
	}

	wp1 = iwa_njoin($ARGV, L" ", uExecArgc, $ARGC);
		U8N *opCmd = W2U(wp1);
	ifree(wp1);

	MEMORYSTATUSEX memex = { sizeof(MEMORYSTATUSEX) };

	GlobalMemoryStatusEx(&memex);
	CONST DWORDLONG iBgnEmpMem = memex.ullAvailPhys;

	if(bQuiet)
	{
		up1 = ims_cats(2, opCmd, " > NUL");
			system(up1);
		ifree(up1);

		PRGB21();
		P2("[Quiet Mode]");
		PRGB00();
	}
	else
	{
		SetConsoleOutputCP(uChcp);
		system(opCmd);
		SetConsoleOutputCP($CP);
	}

	// 計測終了
	DOUBLE dPassedSec = iExecSec_next();

	PRGB91();
	LN();
	// Program
	P("  Program  %s\n", opCmd);

	// Exec
	wp1 = iws_sprintf(L"%.4f", dPassedSec);
	wp2 = iws_addTokenNStr(wp1);
	up1 = W2U(wp2);
		P("  Exec     %s sec\n", up1);
	ifree(up1);
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
		up1 = W2U(wp2);
			P("   %4d ms %7s KB\n", (iMs * i1), up1);
		ifree(up1);
		ifree(wp2);
		ifree(wp1);

		Sleep(iMs);
		++i1;
	}

	LN();
	PRGB00();

	// Debug
	/// icalloc_mapPrint(); ifree_all(); icalloc_mapPrint();

	// 最終処理
	imain_end();
}

VOID
print_version()
{
	PRGB92();
	LN();
	P (" %s\n", IWM_COPYRIGHT);
	P ("   Ver.%s+%s\n", IWM_VERSION, LIB_IWMUTIL_VERSION);
	LN();
	PRGB00();
}

VOID
print_help()
{
	U8N *_cmd = W2U($CMD);

	print_version();
	PRGB01();
	P2("\033[48;2;50;50;200m コマンドの実行時間を計測 \033[0m");
	NL();
	PRGB02();
	P ("\033[48;2;200;50;50m %s [Option] [Command] \033[0m\n\n", _cmd);
	PRGB11();
	P0(" (例１) ");
	PRGB91();
	P ("%s \033[38;2;150;150;255mnotepad\n\n", _cmd);
	PRGB11();
	P0(" (例２) ");
	PRGB91();
	P ("%s \033[38;2;255;150;150m-quiet \033[38;2;150;150;255mdir \"..\" /b\n\n", _cmd);
	PRGB02();
	P2("\033[48;2;200;50;50m [Option] \033[0m");
	PRGB21();
	P2("   -quiet | -q");
	PRGB91();
	P2("       コマンド出力を表示しない");
	NL();
	PRGB21();
	P2("   -codepage=Num | -cp=Num");
	PRGB91();
	P ("       コマンド出力のコードページをNumに変更（初期値：%u）\n", $CP);
	P2("           65001 = UTF-8   932 = Shift_JIS");
	NL();
	PRGB92();
	LN();
	PRGB00();

	ifree(_cmd);
}
