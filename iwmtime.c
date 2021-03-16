//------------------------------------------------------------------------------
#define   IWM_VERSION         "iwmtime_20210316"
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
#define   ColorTitle          (15 + ( 9 * 16))
#define   ColorHeaderFooter   ( 7 + ( 0 * 16))
#define   ColorBgText1        (15 + (12 * 16))
#define   ColorExp1           (13 + ( 0 * 16))
#define   ColorExp2           (14 + ( 0 * 16))
#define   ColorExp3           (11 + ( 0 * 16))
#define   ColorText1          (15 + ( 0 * 16))

MBS  *$program     = "";
MBS  **$args       = {0};
UINT $argsSize     = 0;
UINT $colorDefault = 0;
UINT $execMS       = 0;

MEMORYSTATUSEX $msex = { sizeof(MEMORYSTATUSEX) };

INT
main()
{
	$program      = iCmdline_getCmd();
	$args         = iCmdline_getArgs();
	$argsSize     = iary_size($args);
	$colorDefault = iConsole_getBgcolor();

	MBS *cmd = iary_join($args, " ");

	if(! imi_len(cmd))
	{
		print_help();
		imain_end();
	}

	UINT iBgnMem = 0;
	UINT iEndMem = 0;

	GlobalMemoryStatusEx(&$msex);
	iBgnMem = $msex.ullAvailPhys;
	$execMS = iExecSec_init();

	system(cmd);

	DOUBLE dPassedSec = iExecSec_next($execMS);
	GlobalMemoryStatusEx(&$msex);
	iEndMem = $msex.ullAvailPhys;

	MBS s1[32] = "";

	P ("\e[0;97m");
	LN();
	P ("  Program  %s", cmd);
	NL();
	sprintf(s1, "%d", ((iEndMem - iBgnMem) / 1024));
	P ("  Memory   %s KB (Including System Usage)", ims_addTokenNStr(s1));
	NL();
	sprintf(s1, "%.4f", dPassedSec);
	P ("  Exec     %s SEC", ims_addTokenNStr(s1));
	NL();
	LN();
	P ("\e[0;98m");

	/// icalloc_mapPrint();
	ifree_all();
	/// icalloc_mapPrint();

	// 最終処理
	imain_end();
}

VOID
print_version()
{
	LN();
	P (" %s", IWM_COPYRIGHT);
	NL();
	P ("   Ver.%s+%s", IWM_VERSION, LIB_IWMUTIL_VERSION);
	NL();
}

VOID
print_help()
{
	iConsole_setTextColor(ColorHeaderFooter);
		print_version();
		LN();
	iConsole_setTextColor(ColorTitle);
		P (" コマンドの実行時間を計測 ");
	iConsole_setTextColor($colorDefault);
		NL();
		NL();
	iConsole_setTextColor(ColorBgText1);
		P (" %s [コマンド] [引数] ... ", $program);
	iConsole_setTextColor($colorDefault);
		NL();
		NL();
	iConsole_setTextColor(ColorExp1);
		P2(" (例１)");
	iConsole_setTextColor(ColorText1);
		P ("   > %s notepad", $program);
	iConsole_setTextColor(ColorExp1);
		NL();
		NL();
		P2(" (例２)");
	iConsole_setTextColor(ColorText1);
		P ("   > %s dir \"..\" /b", $program);
	iConsole_setTextColor(ColorHeaderFooter);
		NL();
		NL();
		LN();
	iConsole_setTextColor($colorDefault);
}
