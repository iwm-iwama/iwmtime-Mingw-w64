//------------------------------------------------------------------------------
#define   IWM_VERSION         "iwmtime_20230830"
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

	// 処理前のメモリ値
	GlobalMemoryStatusEx(&memex);
	CONST UINT64 uBaseMem = memex.ullAvailPhys;

	if(bQuiet)
	{
		mp1 = ims_cats(2, opCmd, " > NUL");
			system(mp1);
		ifree(mp1);

		P2(
			IESC_OPT2	"[Quiet Mode]"
			IESC_RESET
		);
	}
	else
	{
		system(opCmd);
	}

	// 計測終了
	DOUBLE dPassedSec = iExecSec_next();

	P1(IESC_STR2);
	LN(80);

	// Program
	P(
		"  Program  %s\n"
		, opCmd
	);

	// Exec
	mp1 = ims_DblToMs(dPassedSec, 3);
		P(
			"  Exec     %s sec\n"
			, mp1
		);
	ifree(mp1);

	// Memory
	P2("  Memory");

	CONST INT Ms = 500;
	CONST INT Loop = 5;
	INT i1 = 0;
	while(i1 < Loop)
	{
		GlobalMemoryStatusEx(&memex);
		P(
			"   %4d ms %8lld KB\n"
			, (Ms * i1), ((INT64)(memex.ullAvailPhys - uBaseMem) / 1024)
		);
		Sleep(Ms);
		++i1;
	}

	LN(80);
	P1(IESC_RESET);

	// Debug
	/// icalloc_mapPrint(); ifree_all(); icalloc_mapPrint();

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
		"    Ver.%s+%s\n"
		, IWM_COPYRIGHT, IWM_VERSION, LIB_IWMUTIL_VERSION
	);
	LN(80);
	P1(IESC_RESET);
}

VOID
print_help()
{
	MS *_cmd = W2M($CMD);

	print_version();
	P(
		IESC_TITLE1	" コマンドの実行時間を計測 "
		IESC_RESET	"\n"
		IESC_STR1	"    %s"
		IESC_OPT2	" [Option]"
		IESC_OPT1	" [Command]\n\n"
		IESC_LBL1	" (例１)\n"
		IESC_STR1	"    %s"
		IESC_OPT1	" notepad.exe\n\n"
		IESC_LBL1	" (例２)\n"
		IESC_STR1	"    %s"
		IESC_OPT2	" -quiet"
		IESC_OPT1	" dir /b \"..\"\n\n"
		, _cmd, _cmd, _cmd
	);
	P1(
		IESC_OPT2	" [Option]\n"
		IESC_OPT21	"    -quiet | -q\n"
		IESC_STR1	"        コマンド出力を表示しない\n\n"
	);
	P1(IESC_STR2);
	LN(80);
	P1(IESC_RESET);

	ifree(_cmd);
}
