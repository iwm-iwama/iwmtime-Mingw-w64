//------------------------------------------------------------------------------
#define   IWM_VERSION         "iwmtime_20230809"
#define   IWM_COPYRIGHT       "Copyright (C)2021-2023 iwm-iwama"
//------------------------------------------------------------------------------
#include "lib_iwmutil2.h"

INT       main();
VOID      print_version();
VOID      print_help();

INT
main()
{
	// lib_iwmutil2 初期化
	imain_begin();

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

	WS *wp1 = 0;
	MS *mp1 = 0;

	UINT uExecArgc = 0;

	// Quiet Mode
	BOOL bQuiet = FALSE;

	// -q | -quiet
	if(iCLI_getOptMatch(0, L"-q", L"-quiet"))
	{
		++uExecArgc;
		bQuiet = TRUE;
	}

	wp1 = iwas_njoin($ARGV, L" ", uExecArgc, $ARGC);
		MS *opCmd = W2M(wp1);
	ifree(wp1);

	MEMORYSTATUSEX memex = { sizeof(MEMORYSTATUSEX) };

	GlobalMemoryStatusEx(&memex);
	CONST DWORDLONG iBgnEmpMem = memex.ullAvailPhys;

	if(bQuiet)
	{
		mp1 = ims_cats(2, opCmd, " > NUL");
			system(mp1);
		ifree(mp1);

		P("%s[Quiet Mode]%s\n",ICLR_OPT2, ICLR_RESET);
	}
	else
	{
		system(opCmd);
	}

	// 計測終了
	DOUBLE dPassedSec = iExecSec_next();

	P(ICLR_STR2);
	LN(80);
	// Program
	P("  Program  %s\n", opCmd);

	// Exec
	mp1 = ims_DblToMs(dPassedSec, 3);
		P("  Exec     %s sec\n", mp1);
	ifree(mp1);

	// Memory
	P2("  Memory   ");
	CONST INT iMs = 500;
	CONST INT iLoop = 5;
	INT i1 = 0;
	while(i1 < iLoop)
	{
		GlobalMemoryStatusEx(&memex);

		mp1 = ims_sprintf("%d", ((memex.ullAvailPhys - iBgnEmpMem) / 1024));
			P("   %4d ms %7s KB\n", (iMs * i1), mp1);
		ifree(mp1);

		Sleep(iMs);
		++i1;
	}

	LN(80);
	P(ICLR_RESET);

	// Debug
	/// icalloc_mapPrint(); ifree_all(); icalloc_mapPrint();

	// 最終処理
	imain_end();
}

VOID
print_version()
{
	P(ICLR_STR2);
	LN(80);
	P(" %s\n", IWM_COPYRIGHT);
	P("    Ver.%s+%s\n", IWM_VERSION, LIB_IWMUTIL_VERSION);
	LN(80);
	P(ICLR_RESET);
}

VOID
print_help()
{
	MS *_cmd = W2M($CMD);

	print_version();
	P("%s コマンドの実行時間を計測 %s\n", ICLR_TITLE1, ICLR_RESET);
	P("%s    %s %s[Option] %s[Command]\n", ICLR_STR1, _cmd, ICLR_OPT2, ICLR_OPT1);
	P("\n");
	P("%s (例１)\n", ICLR_LBL1);
	P("%s    %s %snotepad\n", ICLR_STR1, _cmd, ICLR_OPT1);
	P("\n");
	P("%s (例２)\n", ICLR_LBL1);
	P("%s    %s %s-quiet %sdir /b \"..\"\n", ICLR_STR1, _cmd, ICLR_OPT2, ICLR_OPT1);
	P("\n");
	P("%s [Option]\n", ICLR_OPT2);
	P("%s    -quiet | -q\n", ICLR_OPT21);
	P("%s        コマンド出力を表示しない\n", ICLR_STR1);
	P("\n");
	P(ICLR_STR2);
	LN(80);
	P(ICLR_RESET);

	ifree(_cmd);
}
