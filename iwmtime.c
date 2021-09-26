//------------------------------------------------------------------------------
#define  IWM_VERSION         "iwmtime_20210924"
#define  IWM_COPYRIGHT       "Copyright (C)2021 iwm-iwama"
//------------------------------------------------------------------------------
#include "lib_iwmutil.h"

INT  main();
VOID print_version();
VOID print_help();

// [文字色] + ([背景色] * 16)
//  0 = Black    1 = Navy     2 = Green    3 = Teal
//  4 = Maroon   5 = Purple   6 = Olive    7 = Silver
//  8 = Gray     9 = Blue    10 = Lime    11 = Aqua
// 12 = Red     13 = Fuchsia 14 = Yellow  15 = White

// タイトル
#define  COLOR01             (15 + ( 9 * 16))
// 入力例／注
#define  COLOR11             (15 + (12 * 16))
#define  COLOR12             (13 + ( 0 * 16))
#define  COLOR13             (12 + ( 0 * 16))
// オプション
#define  COLOR21             (14 + ( 0 * 16))
#define  COLOR22             (11 + ( 0 * 16))
// 説明
#define  COLOR91             (15 + ( 0 * 16))
#define  COLOR92             ( 7 + ( 0 * 16))

INT
main()
{
	// lib_iwmutil 初期化
	iCLI_getARGV();      //=> $IWM_CMD, $IWM_ARGV, $IWM_ARGC
	iConsole_getColor(); //=> $IWM_ColorDefault, $IWM_StdoutHandle

	// -h | -help
	if(! $IWM_ARGC || imb_cmpp($IWM_ARGV[0], "-h") || imb_cmpp($IWM_ARGV[0], "-help"))
	{
		print_help();
		imain_end();
	}

	// -v | -version
	if(imb_cmpp($IWM_ARGV[0], "-v") || imb_cmpp($IWM_ARGV[0], "-version"))
	{
		print_version();
		imain_end();
	}

	// -q | -quiet
	UINT iOptionQuiet = (
		imb_cmpp($IWM_ARGV[0], "-q") || imb_cmpp($IWM_ARGV[0], "-quiet") ?
		imi_len($IWM_ARGV[0]) + 1 :
		0
	);

	MBS *cmd = iary_join($IWM_ARGV, " ");

	MEMORYSTATUSEX memex = { sizeof(MEMORYSTATUSEX) };
	DWORDLONG iBgnEmpMem = 0;
	DWORDLONG iEndEmpMem = 0;

	GlobalMemoryStatusEx(&memex);
	iBgnEmpMem = memex.ullAvailPhys;

	// 計測開始
	iExecSec_init(); //=> $IWM_ExecSecBgn

	if(iOptionQuiet)
	{
		system(ims_cats((cmd + iOptionQuiet), " > ", NULL_DEVICE, NULL));
		PZ(COLOR21, NULL);
		P2("[Quiet Mode]");
	}
	else
	{
		system(cmd);
	}

	// 計測終了
	DOUBLE dPassedSec = iExecSec_next();

	GlobalMemoryStatusEx(&memex);
	iEndEmpMem = memex.ullAvailPhys;

	PZ(COLOR91, NULL);
	LN();
	// Program
	P ("  Program  %s\n", (cmd + iOptionQuiet));
	// Memory
	P("  Memory   %s KB (%s KB => %s KB)\n",
		ims_addTokenNStr(ims_sprintf("%d", ((iEndEmpMem - iBgnEmpMem) / 1024))),
		ims_addTokenNStr(ims_sprintf("%d", (iBgnEmpMem / 1024))),
		ims_addTokenNStr(ims_sprintf("%d", (iEndEmpMem / 1024)))
	);
	// Exec
	P("  Exec     %s sec\n", ims_addTokenNStr(ims_sprintf("%.4f", dPassedSec)));
	LN();
	PZ(-1, NULL);

	// Debug
	/// icalloc_mapPrint(); ifree_all(); icalloc_mapPrint();

	// 最終処理
	imain_end();
}

VOID
print_version()
{
	PZ(COLOR92, NULL);
	LN();
	P(" %s\n", IWM_COPYRIGHT);
	P("   Ver.%s+%s\n", IWM_VERSION, LIB_IWMUTIL_VERSION);
	LN();
	PZ(-1, NULL);
}

VOID
print_help()
{
	print_version();
	PZ(COLOR01, " コマンドの実行時間を計測 \n\n");
	PZ(COLOR11, " %s [コマンド] [オプション] [引数] ... \n\n", $IWM_CMD);
	PZ(COLOR12, " (例１)\n");
	PZ(COLOR91, "   > %s notepad\n\n", $IWM_CMD);
	PZ(COLOR12, " (例２)\n");
	PZ(COLOR91, "   > %s dir \"..\" /b\n\n", $IWM_CMD);
	PZ(COLOR21, " [オプション]\n");
	PZ(COLOR22, "   -quiet | -q\n");
	PZ(COLOR91, "       結果を表\示しない\n\n");
	PZ(COLOR92, NULL);
	LN();
	PZ(-1, NULL);
}
