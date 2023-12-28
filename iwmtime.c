//------------------------------------------------------------------------------
#define   IWM_COPYRIGHT       "(C)2021-2023 iwm-iwama"
#define   IWM_VERSION         "iwmtime_20231225"
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

	MS *mp1 = 0;

	WS *pARG = $ARG;
	BOOL bOutput = TRUE;

	// -q | -quiet
	if(iCLI_getOptMatch(0, L"-q", L"-quiet"))
	{
		P2(
			IESC_OPT2	"[Quiet Mode]"
			IESC_RESET
		);

		// 末尾の空白忘れずに
		if(iwb_cmpf(pARG, L"-q "))
		{
			pARG += 3;
		}
		else if(iwb_cmpf(pARG, L"-quiet "))
		{
			pARG += 7;
		}

		bOutput = FALSE;
	}

	MEMORYSTATUSEX memex = { sizeof(MEMORYSTATUSEX) };

	// 処理前のメモリ値
	GlobalMemoryStatusEx(&memex);
	CONST UINT64 uBaseMem = memex.ullAvailPhys;

	// 実行
	imv_system(pARG, bOutput);

	// 計測終了
	DOUBLE dPassedSec = iExecSec_next();

	P1(IESC_STR2);
	LN(80);

	// Program
	mp1 = W2M(pARG);
		P(
			"  Program  %s\n"
			, mp1
		);
	ifree(mp1);

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
