//------------------------------------------------------------------------------
#define   IWM_VERSION         "iwmtime_20210319"
#define   IWM_COPYRIGHT       "Copyright (C)2021 iwm-iwama"
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
#define   COLOR01             (15 + ( 9 * 16))
// 入力例／注
#define   COLOR11             (15 + (12 * 16))
#define   COLOR12             (13 + ( 0 * 16))
#define   COLOR13             (12 + ( 0 * 16))
// 引数
#define   COLOR21             (14 + ( 0 * 16))
#define   COLOR22             (11 + ( 0 * 16))
// 説明
#define   COLOR91             (15 + ( 0 * 16))
#define   COLOR92             ( 7 + ( 0 * 16))

MEMORYSTATUSEX $msex = { sizeof(MEMORYSTATUSEX) };

INT
main()
{
	// lib_iwmutil 初期化
	iCLI_getCmd();       //=> $IWM_Cmd
	iCLI_getCmdOpt();    //=> $IWM_CmdOption, $IWM_CmdOptionSize
	iConsole_getColor(); //=> $IWM_ColorDefault, $IWM_StdoutHandle

	MBS *cmd = iary_join($IWM_CmdOption, " ");

	if(! imi_len(cmd))
	{
		print_help();
		imain_end();
	}

	UINT iBgnMem = 0;
	UINT iEndMem = 0;

	GlobalMemoryStatusEx(&$msex);
	iBgnMem = $msex.ullAvailPhys;
	iExecSec_init(); //=> $IWM_ExecSecBgn

	system(cmd);

	DOUBLE dPassedSec = iExecSec_next();
	GlobalMemoryStatusEx(&$msex);
	iEndMem = $msex.ullAvailPhys;

	MBS s1[32] = "";

	PZ(COLOR91, NULL);
	LN();
	P ("  Program  %s\n", cmd);
	sprintf(s1, "%d", ((iEndMem - iBgnMem) / 1024));
	P ("  Memory   %s KB (Including System Usage)\n", ims_addTokenNStr(s1));
	sprintf(s1, "%.4f", dPassedSec);
	P ("  Exec     %s SEC\n", ims_addTokenNStr(s1));
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
	LN();
	P (" %s\n", IWM_COPYRIGHT);
	P ("   Ver.%s+%s\n", IWM_VERSION, LIB_IWMUTIL_VERSION);
	LN();
}

VOID
print_help()
{
	PZ(COLOR92, NULL);
		print_version();
	PZ(COLOR01, " コマンドの実行時間を計測 \n\n");
	PZ(COLOR11, " %s [コマンド] [引数] ... \n\n", $IWM_Cmd);
	PZ(COLOR12, " (例１)\n");
	PZ(COLOR91, "   > %s notepad\n\n", $IWM_Cmd);
	PZ(COLOR12, " (例２)\n");
	PZ(COLOR91, "   > %s dir \"..\" /b\n\n", $IWM_Cmd);
	PZ(COLOR92, NULL);
		LN();
	PZ(-1, NULL);
}
