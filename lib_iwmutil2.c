/*
[2011-04-16]
	入口でのチェックは、利用者に『安全』を提供する。
	知り得ている危険に手を抜いてはいけない。たとえ、コード量が増加しようとも。
	『安全』と『速度』は反比例しない。

[2013-01-31] + [2022-09-03]
	マクロで複雑な関数を書くな。デバッグが難しくなる。
	「コードの短さ」より「コードの生産性」を優先する。

[2014-01-03] + [2023-07-10]
	動的メモリの確保／初期化／解放をこまめに行えば、
	十分な『安全』と『速度』を今時のハードウェアは提供する。

[2016-08-19] + [2023-12-30]
	-------------------------------
	| lib_iwmutil2 で規定する表記
	-------------------------------
	・大域変数 => １文字目は "$"
		$ARGV, $icallocMap など
	・#define定数（関数は含まない） => すべて大文字
		IMAX_PATH, WS など

	--------------------------
	| ソース別に期待する表記
	--------------------------
	・大域変数, #define定数（関数は含まない） => １文字目は大文字
		Buf, BufSIze など
	・オプション変数 => １文字目は "_"／２文字目は大文字
		_NL, _Format など
	・局所変数 => １文字目は "_"／２文字目は小文字
		_i1, _u1, _mp1, _wp1 など

[2016-01-27] + [2022-09-17]
	基本関数名ルール
		i = iwm-iwama
		m = MS(1byte) | u = MS(UTF-8) | w = WS(UTF-16) | v = VOID
		a = string[]  | b = bool      | n = length     | p = pointer | s = string

[2021-11-18] +[2022-09-23]
	ポインタ *(p + n) と配列 p[n] どちらが速い？
		Mingw-w64においては最適化するとどちらも同じになる。
		今後は可読性を考慮した配列型でコーディングする。

*/
#include "lib_iwmutil2.h"
//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	大域変数
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
/* (例)
	// lib_iwmutil2 初期化
	imain_begin();
	// Debug
	iCLI_VarList();
	// 処理時間
	P("-- %.3fsec\n\n", iExecSec_next());
	// Debug
	icalloc_mapPrint(); ifree_all(); icalloc_mapPrint();
	// 最終処理
	imain_end();
*/
WS     *$CMD         = L"";   // コマンド名を格納
WS     *$ARG         = 0;     // 引数からコマンド名を消去したもの
UINT   $ARGC         = 0;     // 引数配列数
WS     **$ARGV       = 0;     // 引数配列／ダブルクォーテーションを消去したもの
HANDLE $StdoutHandle = 0;     // 画面制御用ハンドル
UINT64 $ExecSecBgn   = 0;     // 実行開始時間
//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	Command Line
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
//-------------------------
// コマンド名／引数を取得
//-------------------------
// v2023-08-25
VOID
iCLI_signal()
{
	imain_err();
}
// v2023-12-22
VOID
iCLI_begin()
{
	// [Ctrl]+[C]
	signal(SIGINT, iCLI_signal);

	// CodePage
	SetConsoleCP(65001);
	SetConsoleOutputCP(65001);

	WS *str = GetCommandLineW();
	$ARG = icalloc_WS(1);
	UINT uArgSize = 0;
	$ARGV = icalloc_WS_ary(1);
	$ARGC = 0;

	WS *pBgn = 0;
	WS *pEnd = 0;
	WS *p1 = 0;
	BOOL bArgc = FALSE;

	for(pEnd = str; pEnd < (str + wcslen(str)); pEnd++)
	{
		for(; *pEnd == ' '; pEnd++);

		if(*pEnd == '\"')
		{
			pBgn = pEnd;
			++pEnd;
			for(; *pEnd && *pEnd != '\"'; pEnd++);
			++pEnd;
			p1 = iws_pclone(pBgn, pEnd);
		}
		else
		{
			pBgn = pEnd;
			for(; *pEnd && *pEnd != ' '; pEnd++)
			{
				if(*pEnd == '\"')
				{
					++pEnd;
					for(; *pEnd && *pEnd != '\"'; pEnd++);
				}
			}
			p1 = iws_pclone(pBgn, pEnd);
		}

		if(bArgc)
		{
			uArgSize += wcslen(p1) + 2;
			$ARG = irealloc_WS($ARG, uArgSize);
			UINT u1 = wcslen($ARG);
			if(u1)
			{
				wcscpy(($ARG + u1), L" ");
				++u1;
			}
			wcscpy(($ARG + u1), p1);

			$ARGV = irealloc_WS_ary($ARGV, ($ARGC + 1));
			$ARGV[$ARGC] = iws_replace(p1, L"\"", L"", FALSE);
			++$ARGC;

			ifree(p1);
		}
		else
		{
			$CMD = p1;
			bArgc = TRUE;
		}
	}

	if(! $ARGV[0] || ! *$ARGV[0])
	{
		$ARGC = 0;
	}
}
// v2023-12-19
VOID
iCLI_end(
	INT exitStatus
)
{
	// CodePage
	SetConsoleOutputCP(GetACP());

	exit(exitStatus);
}
//-----------------------------------------
// 引数 [Key]+[Value] から [Value] を取得
//-----------------------------------------
/* (例)
	iCLI_begin();
	// $ARGV[0] = "-w=size <= 1000" のとき
	P2W(iCLI_getOptValue(0, L"-w=", NULL)); //=> "size <= 1000"
*/
// v2023-07-24
WS
*iCLI_getOptValue(
	UINT argc, // $ARGV[argc]
	WS *opt1,  // (例) "-w=", NULL
	WS *opt2   // (例) "-where=", NULL
)
{
	if(argc >= $ARGC)
	{
		return 0;
	}
	WS *p1 = $ARGV[argc];
	if(opt1 && iwb_cmpf(p1, opt1))
	{
		return (p1 + wcslen(opt1));
	}
	if(opt2 && iwb_cmpf(p1, opt2))
	{
		return (p1 + wcslen(opt2));
	}
	return 0;
}
//--------------------------
// 引数 [Key] と一致するか
//--------------------------
/* (例)
	iCLI_begin();
	// $ARGV[0] => "-repeat"
	P3(iCLI_getOptMatch(0, L"-repeat", NULL)); //=> TRUE
	// $ARGV[0] => "-w=size <= 1000"
	P3(iCLI_getOptMatch(0, L"-w=", NULL));     //=> FALSE
*/
// v2023-07-21
BOOL
iCLI_getOptMatch(
	UINT argc, // $ARGV[argc]
	WS *opt1,  // (例) "-r", NULL
	WS *opt2   // (例) "-repeat", NULL
)
{
	if(argc >= $ARGC)
	{
		return FALSE;
	}
	WS *p1 = $ARGV[argc];
	if(opt1 && iwb_cmpp(p1, opt1))
	{
		return TRUE;
	}
	if(opt2 && iwb_cmpp(p1, opt2))
	{
		return TRUE;
	}
	return FALSE;
}
// v2023-12-16
VOID
iCLI_VarList()
{
	P1("\033[97m");
	MS *p1 = 0;
	p1 = W2M($CMD);
		P(
			"\033[44m $CMD \033[49m\n"
			"    %s\n"
			,
			p1
		);
	ifree(p1);
	p1 = W2M($ARG);
		P(
			"\033[44m $ARG \033[49m\n"
			"    %s\n"
			,
			p1
		);
	ifree(p1);
		P(
			"\033[44m $ARGC \033[49m\n"
			"    %d\n"
			,
			$ARGC
		);
		P1("\033[44m $ARGV \033[49m\n");
		iwav_print($ARGV);
	P2("\033[0m");
}
//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	実行開始時間
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
/* (例)
	iExecSec_init(); //=> $ExecSecBgn
	Sleep(2000);
	P("-- %.6fsec\n\n", iExecSec_next());
	Sleep(1000);
	P("-- %.6fsec\n\n", iExecSec_next());
*/
// v2023-07-30
UINT64
iExecSec(
	CONST UINT64 microSec // 0 のとき Init
)
{
	UINT64 u1 = GetTickCount64();
	if(! microSec)
	{
		$ExecSecBgn = u1;
	}
	return (u1 < microSec ? 0 : (u1 - microSec)); // Err=0
}
//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	メモリ確保
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
/*
	icalloc() 用に確保される配列
	IcallocDiv は必要に応じて変更
*/
// v2023-07-29
typedef struct
{
	VOID *ptr;     // ポインタ位置
	UINT uAry;     // 配列個数（配列以外=0）
	UINT64 uAlloc; // アロケート長
	UINT uSizeOf;  // sizeof
	UINT id;       // 順番
}
$struct_icallocMap;

$struct_icallocMap *$icallocMap = 0; // 可変長
UINT $icallocMapSize = 0;            // *$icallocMap のサイズ＋1
UINT $icallocMapEOD = 0;             // *$icallocMap の現在位置＋1
UINT $icallocMapFreeCnt = 0;         // *$icallocMap 中の空白領域
UINT $icallocMapId = 0;              // *$icallocMap の順番
CONST UINT $sizeof_icallocMap = sizeof($struct_icallocMap);
// *$icallocMap の基本区画サイズ
#define   IcallocDiv          64
// 8個余分
#define   MemX(n, size)       (UINT64)((n+8)*size)

//---------
// calloc
//---------
/* (例)
	WS *wp1 = 0;
	UINT wp1Pos = 0;
	wp1 = icalloc_WS(100);
		wp1Pos += iwn_cpy((wp1 + wp1Pos), L"12345");
		wp1Pos += iwn_cpy((wp1 + wp1Pos), L"ABCDE");
		PL2W(wp1); //=> "12345ABCDE"
	wp1 = irealloc_WS(wp1, 200);
		wp1Pos += iwn_cpy((wp1 + wp1Pos), L"あいうえお");
		PL2W(wp1); //=> "12345ABCDEあいうえお"
	ifree(wp1);
*/
// v2023-07-29
VOID
*icalloc(
	UINT64 n,    // 個数
	UINT64 size, // sizeof
	BOOL aryOn   // TRUE=配列
)
{
	UINT size2 = sizeof($struct_icallocMap);
	// 初回 $icallocMap を更新
	if(! $icallocMapSize)
	{
		$icallocMapSize = IcallocDiv;
		$icallocMap = ($struct_icallocMap*)calloc($icallocMapSize, size2);
		icalloc_err($icallocMap);
		$icallocMapId = 0;
	}
	else if($icallocMapSize <= $icallocMapEOD)
	{
		$icallocMapSize += IcallocDiv;
		$icallocMap = ($struct_icallocMap*)realloc($icallocMap, ($icallocMapSize * size2));
		icalloc_err($icallocMap);
	}
	// 引数にポインタ割当
	UINT64 uAlloc = MemX(n, size);
	VOID *rtn = malloc(uAlloc);
	icalloc_err(rtn);
	memset(rtn, 0, uAlloc);
	($icallocMap + $icallocMapEOD)->ptr = rtn;
	($icallocMap + $icallocMapEOD)->uAry = (aryOn ? n : 0);
	($icallocMap + $icallocMapEOD)->uAlloc = uAlloc;
	($icallocMap + $icallocMapEOD)->uSizeOf = size;
	// 順番
	++$icallocMapId;
	($icallocMap + $icallocMapEOD)->id = $icallocMapId;
	++$icallocMapEOD;
	return rtn;
}
//----------
// realloc
//----------
// v2023-07-29
VOID
*irealloc(
	VOID *ptr,  // icalloc()ポインタ
	UINT64 n,   // 個数
	UINT64 size // sizeof
)
{
	// 該当なしのときは ptr を返す
	VOID *rtn = ptr;
	UINT64 uAlloc = MemX(n, size);
	// $icallocMap を更新
	UINT u1 = 0;
	while(u1 < $icallocMapEOD)
	{
		if(ptr == ($icallocMap + u1)->ptr)
		{
			if(($icallocMap + u1)->uAry)
			{
				($icallocMap + u1)->uAry = n;
			}
			if(($icallocMap + u1)->uAlloc < uAlloc)
			{
				// reallocの初期化版
				rtn = (VOID*)malloc(uAlloc);
				icalloc_err(rtn);
				memset(rtn, 0, uAlloc);
				memcpy(rtn, ptr, ($icallocMap + u1)->uAlloc);
				memset(ptr, 0, ($icallocMap + u1)->uAlloc);
				free(ptr);
				ptr = 0;
				($icallocMap + u1)->ptr = rtn;
				($icallocMap + u1)->uAlloc = uAlloc;
				break;
			}
		}
		++u1;
	}
	return rtn;
}
//--------------------------------
// icalloc, ireallocのエラー処理
//--------------------------------
/* (例)
	// 強制的にエラーを発生させる
	MS *p1 = NULL;
	icalloc_err(p1);
*/
// v2023-12-22
VOID
icalloc_err(
	VOID *ptr
)
{
	if(! ptr)
	{
		P2(
			IESC_FALSE1 "[Err] Can't allocate memories!"
			IESC_RESET
		);
		imain_err();
	}
}
//---------------------------
// ($icallocMap + n) をfree
//---------------------------
// v2023-07-10
VOID
icalloc_free(
	VOID *ptr
)
{
	$struct_icallocMap *map = 0;
	UINT u1 = 0, u2 = 0;
	while(u1 < $icallocMapEOD)
	{
		map = ($icallocMap + u1);
		if(ptr == (map->ptr))
		{
			// 配列から先に free
			if(map->uAry)
			{
				// １次元削除
				u2 = 0;
				while(u2 < (map->uAry))
				{
					if(! (*((VOID**)(map->ptr) + u2)))
					{
						break;
					}
					icalloc_free(*((VOID**)(map->ptr) + u2));
					++u2;
				}
				++$icallocMapFreeCnt;
				// memset() + NULL代入 で free() の代替
				// ２次元削除
				// ポインタ配列を消去
				memset(map->ptr, 0, map->uAlloc);
				free(map->ptr);
				map->ptr = 0;
				memset(map, 0, $sizeof_icallocMap);
				return;
			}
			else
			{
				free(map->ptr);
				map->ptr = 0;
				memset(map, 0, $sizeof_icallocMap);
				++$icallocMapFreeCnt;
				return;
			}
		}
		++u1;
	}
}
//--------------------
// $icallocMapをfree
//--------------------
// v2016-01-10
VOID
icalloc_freeAll()
{
	// [0]はポインタなので残す
	// [1..]をfree
	while($icallocMapEOD)
	{
		icalloc_free($icallocMap->ptr);
		--$icallocMapEOD;
	}
	$icallocMap = ($struct_icallocMap*)realloc($icallocMap, 0); // free()不可
	$icallocMapSize = 0;
	$icallocMapFreeCnt = 0;
}
//--------------------
// $icallocMapを掃除
//--------------------
// v2022-09-03
VOID
icalloc_mapSweep()
{
	// 毎回呼び出しても影響ない
	UINT uSweep = 0;
	$struct_icallocMap *map1 = 0, *map2 = 0;
	UINT u1 = 0, u2 = 0;
	// 隙間を詰める
	while(u1 < $icallocMapEOD)
	{
		map1 = ($icallocMap + u1);
		if(! (VOID**)(map1->ptr))
		{
			++uSweep;
			u2 = u1 + 1;
			while(u2 < $icallocMapEOD)
			{
				map2 = ($icallocMap + u2);
				if((VOID**)(map2->ptr))
				{
					*map1 = *map2; // 構造体コピー
					memset(map2, 0, $sizeof_icallocMap);
					--uSweep;
					break;
				}
				++u2;
			}
		}
		++u1;
	}
	// 初期化
	$icallocMapFreeCnt -= uSweep;
	$icallocMapEOD -= uSweep;
}
//--------------------------
// $icallocMapをリスト出力
//--------------------------
// v2023-12-11
VOID
icalloc_mapPrint1()
{
	iConsole_EscOn();
	P2(
		"\033[94m"
		"- count - id ---- pointer -------- array ----- size - sizeof -------------------"
	);
	$struct_icallocMap *map = 0;
	UINT uAllocUsed = 0;
	UINT u1 = 0;
	while(u1 < $icallocMapEOD)
	{
		map = ($icallocMap + u1);
		if((map->ptr))
		{
			uAllocUsed += (map->uAlloc);
			if((map->uAry))
			{
				P1("\033[44m");
			}
			P(
				"\033[97m"
				"  %-7u %07u %p %5u %10u %8u "
				,
				(u1 + 1),
				(map->id),
				(map->ptr),
				(map->uAry),
				(map->uAlloc),
				(map->uSizeOf)
			);
			if(! (map->uAry))
			{
				P1("\033[37m");
				switch(map->uSizeOf)
				{
					case sizeof(WS):
						P1W(map->ptr);
						break;
					case sizeof(MS):
						P1(map->ptr);
						break;
					default:
						break;
				}
			}
			P2("\033[97;49m");
		}
		++u1;
	}
	P(
		"\033[94m"
		"---------------------------------- %16lld byte -----------------------"
		"\033[0m\n\n"
		,
		uAllocUsed
	);
}
//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	Print関係
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
//-----------
// printf()
//-----------
/* (例)
	P("abc\n");       //=> "abc\n"
	P("%s\n", "abc"); //=> "abc\n"

	// printf()系は遅い。可能であれば P1(), P2() を使用する。
	P1("abc\n");      //=> "abc\n"
	P2("abc");        //=> "abc\n"
	QP1("abc\n");     //=> "abc\n"
	QP2("abc");       //=> "abc\n"
*/
// v2015-01-24
VOID
P(
	MS *format,
	...
)
{
	va_list va;
	va_start(va, format);
		vfprintf(stdout, format, va);
	va_end(va);
}
//--------------
// Quick Print
//--------------
/* (例)
	UINT BufSize = 10 * 100;
	MS *pBgn = icalloc_MS(BufSize);
	MS *pEnd = pBgn;
	UINT uCnt = 0;

	for(UINT _u1 = 1; _u1 <= 100; _u1++)
	{
		pEnd += sprintf(pEnd, "%d ", _u1);
		++uCnt;
		if(uCnt >= 15)
		{
			uCnt = 0;
			QP(pBgn, (pEnd - pBgn));
			NL();
			*pBgn = 0;
			pEnd = pBgn;
		}
	}

	QP(pBgn, (pEnd - pBgn));
	NL();
*/
// v2023-07-28
VOID
QP(
	MS *str,
	UINT size
)
{
	fflush(stdout);
	WriteFile($StdoutHandle, str, size, NULL, NULL);
	FlushFileBuffers($StdoutHandle);
}
// v2023-07-11
VOID
P1W(
	WS *str
)
{
	MS *p1 = W2M(str);
		fputs(p1, stdout);
	ifree(p1);
}
// v2024-01-18
VOID
PR1(
	MS *str,
	UINT uRepeat
)
{
	if(! str || ! *str || ! uRepeat)
	{
		return;
	}
	while((uRepeat--) > 0)
	{
		P1(str);
	}
}
//-------------------------
// Escape Sequence へ変換
//-------------------------
/* (例)
	WS *wp1 = L"A\\nB\\rC";
	WS *wp2 = iws_cnv_escape(wp1);
		PL2W(wp1); //=> "A\\nB\\rC"
		PL2W(wp2); //=> "A\nC"
	ifree(wp2);
	ifree(wp1);
*/
// v2023-12-20
WS
*iws_cnv_escape(
	WS *str
)
{
	if(! str || ! *str)
	{
		return icalloc_WS(0);
	}
	WS *rtn = icalloc_WS(wcslen(str));
	WS *pEnd = rtn;
	while(*str)
	{
		if(*str == '\\')
		{
			++str;
			switch(*str)
			{
				case('a'):
					*pEnd = '\a';
					break;
				case('b'):
					*pEnd = '\b';
					break;
				case('e'):
					*pEnd = '\e';
					break;
				case('t'):
					*pEnd = '\t';
					break;
				case('n'):
					*pEnd = '\n';
					break;
				case('v'):
					*pEnd = '\v';
					break;
				case('f'):
					*pEnd = '\f';
					break;
				case('r'):
					*pEnd = '\r';
					break;
				default:
					*pEnd = '\\';
					++pEnd;
					*pEnd = *str;
					break;
			}
		}
		else
		{
			*pEnd = *str;
		}
		++pEnd;
		++str;
	}
	// \033[
	WS *wp1 = rtn;
		rtn = iws_replace(wp1, L"\\033[", L"\033[", TRUE);
	ifree(wp1);
	return rtn;
}
//---------------------
// コマンド実行／出力
//---------------------
/* (例)
	imv_system(L"dir", TRUE);
	imv_system(L"dir", FALSE);
)
*/
// v2023-12-18
VOID
imv_system(
	WS *cmd,
	BOOL bOutput // FALSE=実行のみ／出力しない
)
{
	if(bOutput)
	{
		_wsystem(cmd);
	}
	else
	{
		WS *wp1 = iws_cats(2, cmd, L" > NUL");
			_wsystem(wp1);
		ifree(wp1);
	}
}
//---------------
// コマンド実行
//---------------
/* (例)
	WS *wp1 = 0;

	wp1 = iws_popen($ARG);
		P1W(wp1);
	ifree(wp1);

	wp1 = iws_popen(L"dir /b");
		P1W(wp1); //=> コマンドの実行結果
	ifree(wp1);
*/
// v2023-12-23
WS
*iws_popen(
	WS *cmd
)
{
	WS *rtn = 0;
	MS *mCmd = W2M(cmd);
		FILE *fp = popen(mCmd, "rt");
			UINT uSize = 16;
			MS *mp1 = icalloc_MS(uSize);
				UINT uPos = 0;
				INT c = 0;
				while((c = fgetc(fp)) != EOF)
				{
					mp1[uPos] = (MS)c;
					++uPos;
					if(uPos >= uSize)
					{
						uSize *= 2;
						mp1 = irealloc_MS(mp1, uSize);
					}
				}
				mp1[uPos] = 0;
				///PL3(imn_Codepage(mp1));
				rtn = icnv_M2W(mp1, imn_Codepage(mp1));
			ifree(mp1);
		pclose(fp);
	ifree(mCmd);
	// CodePage
	SetConsoleOutputCP(65001);
	return rtn;
}
//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	UTF-16／UTF-8変換
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
// v2023-12-22
MS
*icnv_W2M(
	WS *str,
	UINT uCP
)
{
	if(! str || ! *str)
	{
		return icalloc_MS(0);
	}
	UINT64 u1 = WideCharToMultiByte(uCP, 0, str, -1, NULL, 0, NULL, NULL);
	MS *rtn = icalloc_MS(u1);
	WideCharToMultiByte(uCP, 0, str, -1, rtn, u1, NULL, NULL);
	return rtn;
}
// v2023-12-22
WS
*icnv_M2W(
	MS *str,
	UINT uCP
)
{
	if(! str || ! *str)
	{
		return icalloc_WS(0);
	}
	UINT64 u1 = MultiByteToWideChar(uCP, 0, str, -1, NULL, 0);
	WS *rtn = icalloc_WS(u1);
	MultiByteToWideChar(uCP, 0, str, -1, rtn, u1);
	return rtn;
}
//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	文字列処理
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
//---------------
// 文字長を返す
//---------------
/* (例)
	MS *p1 = NULL;
	// strlen(NULL) => NG
	PL3(imn_len(p1)); //=> 0
*/
// v2023-07-29
UINT64
imn_len(
	MS *str
)
{
	if(! str || ! *str)
	{
		return 0;
	}
	return strlen(str);
}
// v2023-07-29
UINT64
iwn_len(
	WS *str
)
{
	if(! str || ! *str)
	{
		return 0;
	}
	return wcslen(str);
}
// v2023-12-14
UINT64
iun_len(
	MS *str
)
{
	if(! str || ! *str)
	{
		return 0;
	}
	// BOM
	if(strlen(str) >= 3 && str[0] == (MS)0xEF && str[1] == (MS)0xBB && str[2] == (MS)0xBF)
	{
		str += 3;
	}
	UINT64 rtn = 0;
	while(*str)
	{
		// 1byte
		if((*str & 0x80) == 0x00)
		{
			str += 1;
			++rtn;
		}
		// 2byte
		else if((*str & 0xE0) == 0xC0)
		{
			str += 2;
			++rtn;
		}
		// 3byte
		else if((*str & 0xF0) == 0xE0)
		{
			str += 3;
			++rtn;
		}
		// 4byte
		else if((*str & 0xF8) == 0xF0)
		{
			str += 4;
			++rtn;
		}
		else
		{
			return rtn;
		}
	}
	return rtn;
}
//-----------
// Codepage
//-----------
UINT
imn_Codepage(
	MS *str
)
{
	if(! str || ! *str)
	{
		return 0;
	}
	// UTF-8 BOM
	if(strlen(str) >= 3 && str[0] == (MS)0xEF && str[1] == (MS)0xBB && str[2] == (MS)0xBF)
	{
		return 65001;
	}
	// UTF-8 NoBOM
	while(*str)
	{
		// 1byte
		if((*str & 0x80) == 0x00)
		{
		}
		// 2..4byte
		else if((*str & 0xE0) == 0xC0 || (*str & 0xF0) == 0xE0 || (*str & 0xF8) == 0xF0)
		{
			return 65001;
		}
		// Shift_JIS
		else if ((*str & 0xE0) == 0x80 || (*str & 0xE0) == 0xE0)
		{
			return 932;
		}
		++str;
	}
	// ASCII
	return 20127;
}
//-------------------------
// コピーした文字長を返す
//-------------------------
/* (例)
	WS *from = L"abcde";
	WS *to = icalloc_WS(100);
		UINT u1 = iwn_cpy(to, from);
		PL3(u1);  //=> 5
		PL2W(to); //=> "abcde"
	ifree(to);
*/
// v2023-07-29
UINT64
imn_cpy(
	MS *to,
	MS *from
)
{
	if(! from || ! *from)
	{
		return 0;
	}
	strcpy(to, from);
	return strlen(from);
}
// v2023-07-29
UINT64
iwn_cpy(
	WS *to,
	WS *from
)
{
	if(! from || ! *from)
	{
		return 0;
	}
	wcscpy(to, from);
	return wcslen(from);
}
/* (例)
	WS *p1 = iws_clone(L"123456789");
	WS *p2 = iws_clone(L"AB\0CDE");
	PL3(iwn_pcpy(p1, p2, (p2 + 5))); //=> 2
	PL2W(p1);                        //=> "AB"
*/
// v2023-09-01
UINT64
imn_pcpy(
	MS *to,
	MS *from1,
	MS *from2
)
{
	if(! from1 || ! from2 || from1 >= from2)
	{
		return 0;
	}
	strncpy(to, from1, (from2 - from1));
	return strlen(to);
}
// v2023-09-01
UINT64
iwn_pcpy(
	WS *to,
	WS *from1,
	WS *from2
)
{
	if(! from1 || ! from2 || from1 >= from2)
	{
		return 0;
	}
	wcsncpy(to, from1, (from2 - from1));
	return wcslen(to);
}
//-----------------------
// 新規部分文字列を生成
//-----------------------
/* (例)
	PL2W(iws_clone(L"あいうえお")); //=> "あいうえお"
*/
// v2023-09-01
MS
*ims_clone(
	MS *from
)
{
	if(! from || ! *from)
	{
		return icalloc_MS(0);
	}
	MS *rtn = icalloc_MS(strlen(from));
	return strcpy(rtn, from);
}
// v2023-09-01
WS
*iws_clone(
	WS *from
)
{
	if(! from || ! *from)
	{
		return icalloc_WS(0);
	}
	WS *rtn = icalloc_WS(wcslen(from));
	return wcscpy(rtn, from);
}
/* (例)
	WS *from = L"あいうえお";
	WS *p1 = iws_pclone(from, (from + 3));
		PL2W(p1); //=> "あいう"
	ifree(p1);
*/
// v2023-09-01
WS
*iws_pclone(
	WS *from1,
	WS *from2
)
{
	if(! from1 || ! from2 || from1 >= from2)
	{
		return icalloc_WS(0);
	}
	UINT64 len = from2 - from1;
	WS *rtn = icalloc_WS(len);
	return wcsncpy(rtn, from1, len);
}
/* (例)
	MS *mp1 = ims_cats(5, "123", NULL, "abcde", "", "あいうえお");
		PL2(mp1); //=> "123abcdeあいうえお"
	ifree(mp1);
*/
// v2023-08-15
MS
*ims_cats(
	UINT size, // 要素数(n+1)
	...        // ary[0..n]
)
{
	va_list va;
	UINT rtnSize = 0;
	MS *rtn = icalloc_MS(rtnSize);
	UINT pEnd = 0;
	va_start(va, size);
		while(size--)
		{
			MS *_mp1 = va_arg(va, MS*);
			if(_mp1 && *_mp1)
			{
				rtnSize += strlen(_mp1);
				rtn = irealloc_MS(rtn, rtnSize);
				pEnd += imn_cpy((rtn + pEnd), _mp1);
			}
		}
	va_end(va);
	return rtn;
}
// v2023-08-15
WS
*iws_cats(
	UINT size, // 要素数(n+1)
	...        // ary[0..n]
)
{
	va_list va;
	UINT rtnSize = 0;
	WS *rtn = icalloc_WS(rtnSize);
	UINT pEnd = 0;
	va_start(va, size);
		while(size--)
		{
			WS *_wp1 = va_arg(va, WS*);
			if(_wp1 && *_wp1)
			{
				rtnSize += wcslen(_wp1);
				rtn = irealloc_WS(rtn, rtnSize);
				pEnd += iwn_cpy((rtn + pEnd), _wp1);
			}
		}
	va_end(va);
	return rtn;
}
//--------------------
// sprintf()の拡張版
//--------------------
/* (例)
	MS *p1 = ims_sprintf("%s-%s%05d", "ABC", "123", 456);
		PL2(p1); //=> "ABC-12300456"
	ifree(p1);
*/
// v2023-07-25
MS
*ims_sprintf(
	MS *format,
	...
)
{
	FILE *oFp = fopen("NUL", "wb");
		va_list va;
		va_start(va, format);
			MS *rtn = icalloc_MS(vfprintf(oFp, format, va));
			vsprintf(rtn, format, va);
		va_end(va);
	fclose(oFp);
	return rtn;
}
/* (例)
	WS *p1 = M2W("あいうえお");
	WS *p2 = M2W("ワイド文字：%S\n");
	WS *wp3 = iws_sprintf(p2, p1);
		P2W(wp3);
	ifree(wp3);
	ifree(p2);
	ifree(p1);
*/
// v2023-07-25
WS
*iws_sprintf(
	WS *format,
	...
)
{
	FILE *oFp = fopen("NUL", "wb");
		va_list va;
		va_start(va, format);
			INT len = vfwprintf(oFp, format, va);
			WS *rtn = icalloc_WS(len);
			vswprintf(rtn, len, format, va);
		va_end(va);
	fclose(oFp);
	return rtn;
}
//---------------------------------
// lstrcmp()／lstrcmpi() より安全
//---------------------------------
/* (例)
	PL3(iwb_cmp(L"", L"abc", FALSE, FALSE));   //=> FALSE
	PL3(iwb_cmp(L"abc", L"", FALSE, FALSE));   //=> TRUE
	PL3(iwb_cmp(L"", L"", FALSE, FALSE));      //=> TRUE
	PL3(iwb_cmp(NULL, L"abc", FALSE, FALSE));  //=> FALSE
	PL3(iwb_cmp(L"abc", NULL, FALSE, FALSE));  //=> FALSE
	PL3(iwb_cmp(NULL, NULL, FALSE, FALSE));    //=> FALSE
	PL3(iwb_cmp(NULL, L"", FALSE, FALSE));     //=> FALSE
	PL3(iwb_cmp(L"", NULL, FALSE, FALSE));     //=> FALSE
	NL();

	// iwb_cmpf(str, search)
	PL3(iwb_cmp(L"abc", L"AB", FALSE, FALSE)); //=> FALSE
	// iwb_cmpfi(str, search)
	PL3(iwb_cmp(L"abc", L"AB", FALSE, TRUE));  //=> TRUE
	// iwb_cmpp(str, search)
	PL3(iwb_cmp(L"abc", L"AB", TRUE, FALSE));  //=> FALSE
	// iwb_cmppi(str, search)
	PL3(iwb_cmp(L"abc", L"AB", TRUE, TRUE));   //=> FALSE
	NL();

	// searchに１文字でも合致すればTRUEを返す
	// iwb_cmp_leqfi(str,search)
	PL3(iwb_cmp_leqfi(L""   , L".."));         //=> TRUE
	PL3(iwb_cmp_leqfi(L"."  , L".."));         //=> TRUE
	PL3(iwb_cmp_leqfi(L".." , L".."));         //=> TRUE
	PL3(iwb_cmp_leqfi(L"...", L".."));         //=> FALSE
	PL3(iwb_cmp_leqfi(L"...", L""  ));         //=> FALSE
	NL();
*/
// v2023-07-27
BOOL
iwb_cmp(
	WS *str,      // 検索対象
	WS *search,   // 検索文字列
	BOOL perfect, // TRUE=長さ一致／FALSE=前方一致
	BOOL icase    // TRUE=大小文字区別しない
)
{
	// NULL は存在しないので FALSE
	if(! str || ! search)
	{
		return FALSE;
	}
	// 例外
	if(! *str && ! *search)
	{
		return TRUE;
	}
	// 検索対象 == "" のときは FALSE
	if(! *str)
	{
		return FALSE;
	}
	// 長さ一致
	if(perfect)
	{
		if(wcslen(str) == wcslen(search))
		{
			if(! icase)
			{
				return (wcscmp(str, search) ? FALSE : TRUE);
			}
		}
		else
		{
			return FALSE;
		}
	}
	// 大文字小文字区別しない
	if(icase)
	{
		return (wcsnicmp(str, search, wcslen(search)) ? FALSE : TRUE);
	}
	else
	{
		return (wcsncmp(str, search, wcslen(search)) ? FALSE : TRUE);
	}
}
// v2023-07-30
UINT64
iwn_searchCnt(
	WS *str,    // 文字列
	WS *search, // 検索文字列
	BOOL icase  // TRUE=大文字小文字区別しない
)
{
	if(! search || ! *search)
	{
		return 0;
	}
	UINT64 rtn = 0;
	CONST UINT64 uSearch = wcslen(search);
	while(*str)
	{
		if(iwb_cmp(str, search, FALSE, icase))
		{
			str += uSearch;
			++rtn;
		}
		else
		{
			++str;
		}
	}
	return rtn;
}
//---------------------------
// 文字列を分割し配列を作成
//---------------------------
/* (例)
	WS *str = L"-4712/ 1/ 1  00:00:00:::";
	WS *tokens = L"-/: ";
	WS **wa1 = {};
	wa1 = iwaa_split(str, tokens, TRUE);
		iwav_print(wa1); //=> {"4712", "1", "1", "00", "00", "00"}
	ifree(wa1);
	wa1 = iwaa_split(str, tokens, FALSE);
		iwav_print(wa1); //=> {"", "4712", "", "1", "", "1", "00", "00", "00", "", ""}
	ifree(wa1);
*/
// v2023-07-29
WS
**iwaa_split(
	WS *str,      // 文字列
	WS *tokens,   // 区切り文字群 (例)"-/: " => "-", "/", ":", " "
	BOOL bRmEmpty // TRUE=配列から空白を除く
)
{
	WS **rtn = {};
	if(! str || ! *str)
	{
		rtn = icalloc_WS_ary(1);
		rtn[0] = 0;
		return rtn;
	}
	if(! tokens || ! *tokens)
	{
		UINT64 u1 = 0;
		UINT64 u2 = wcslen(str);
		rtn = icalloc_WS_ary(u2);
		while(u1 < u2)
		{
			rtn[u1] = iws_pclone((str + u1), (str + u1 + 1));
			++u1;
		}
		return rtn;
	}
	UINT64 uAry = 1;
	rtn = icalloc_WS_ary(uAry);
	CONST WS *pEOD = str + wcslen(str);
	WS *pBgn = str;
	WS *pEnd = pBgn;
	while(pEnd < pEOD)
	{
		pEnd += wcscspn(pBgn, tokens);
		if(! bRmEmpty || (bRmEmpty && pBgn < pEnd))
		{
			rtn = irealloc_WS_ary(rtn, uAry);
			rtn[uAry - 1] = iws_pclone(pBgn, pEnd);
			++uAry;
		}
		++pEnd;
		pBgn = pEnd;
	}
	return rtn;
}
//-------------
// 文字列置換
//-------------
/* (例)
	PL2W(iws_replace(L"100YEN yen", L"YEN", L"円", TRUE));  //=> "100円 円"
	PL2W(iws_replace(L"100YEN yen", L"YEN", L"円", FALSE)); //=> "100円 yen"
*/
// v2023-07-30
WS
*iws_replace(
	WS *from,   // 文字列
	WS *before, // 変換前の文字列
	WS *after,  // 変換後の文字列
	BOOL icase  // TRUE=大文字小文字区別しない
)
{
	if(! from || ! *from || ! before || ! *before)
	{
		return icalloc_WS(0);
	}
	if(! after)
	{
		after = L"";
	}
	WS *fW = 0;
	WS *bW = 0;
	if(icase)
	{
		fW = iws_clone(from);
		CharLowerW(fW);
		bW = iws_clone(before);
		CharLowerW(bW);
	}
	else
	{
		fW = from;
		bW = before;
	}
	UINT64 uFrom = wcslen(from);
	UINT64 uBefore = wcslen(before);
	UINT64 uAfter = wcslen(after);
	// ゼロ長対策
	WS *rtn = icalloc_WS(uFrom * (1 + (uAfter / uBefore)));
	WS *fWB = fW;
	WS *rtnE = rtn;
	while(*fWB)
	{
		if(! wcsncmp(fWB, bW, uBefore))
		{
			rtnE += iwn_cpy(rtnE, after);
			fWB += uBefore;
		}
		else
		{
			*rtnE++ = *fWB++;
		}
	}
	if(icase)
	{
		ifree(bW);
		ifree(fW);
	}
	return rtn;
}
//-------------------------
// 数値を３桁区切りで表示
//-------------------------
/* (例)
	DOUBLE d1 = -1234567.890;
	MS *p1 = 0;
		p1 = ims_IntToMs(d1);    //=> -1,234,567／切り捨て
		PL2(p1);
	ifree(p1);
		p1 = ims_DblToMs(d1, 4); //=> -1,234,567.8900
		PL2(p1);
	ifree(p1);
		p1 = ims_DblToMs(d1, 2); //=> -1,234,567.89
		PL2(p1);
	ifree(p1);
		p1 = ims_DblToMs(d1, 1); //=> -1,234,567.9
		PL2(p1);
	ifree(p1);
		p1 = ims_DblToMs(d1, 0); //=> -1,234,568／四捨五入
		PL2(p1);
	ifree(p1);
*/
// v2023-09-26
MS
*ims_IntToMs(
	INT64 num // 正数
)
{
	INT iSign = 1;
	if(num < 0)
	{
		iSign = -1;
		num = -num;
	}
	MS *pNum = ims_sprintf("%lld", num);
	UINT64 uNum = strlen(pNum);
	MS *rtn = icalloc_MS(uNum * 2);
	MS *pEnd = rtn;
	if(iSign < 0)
	{
		*pEnd = '-';
		++pEnd;
	}
	UINT64 u1 = uNum;
	UINT64 u2 = u1 % 3;
	MS *p1 = pNum;
	if(u2 > 0)
	{
		pEnd += imn_pcpy(pEnd, p1, (p1 + u2));
	}
	p1 += u2;
	u1 -= u2;
	while(u1 > 0)
	{
		if(u2)
		{
			*pEnd = ',';
			++pEnd;
		}
		else
		{
			u2 = 1;
		}
		pEnd += imn_pcpy(pEnd, p1, (p1 + 3));
		p1 += 3;
		u1 -= 3;
	}
	ifree(pNum);
	return rtn;
}
// v2023-07-21
MS
*ims_DblToMs(
	DOUBLE num, // 小数
	INT iDigit  // 小数点桁数
)
{
	if(iDigit <= 0)
	{
		iDigit = 0;
		num = round(num);
	}
	DOUBLE dDecimal = fabs(fmod(num, 1.0));
	MS *format = ims_sprintf("%%.%df", iDigit);
	MS *p1 = ims_IntToMs(num);
	MS *p2 = ims_sprintf(format, dDecimal);
	MS *rtn = ims_cats(2, p1, (p2 + 1));
	ifree(p2);
	ifree(p1);
	ifree(format);
	return rtn;
}
//------------------------------------------------------------
// 文字列前後の空白 ' ' L'　' '\t' '\r' '\n' '\0' を消去する
//------------------------------------------------------------
// v2024-01-16
WS
*iws_strip(
	WS *str,
	BOOL bStripLeft,
	BOOL bStripRight
)
{
	UINT uRight = iwn_len(str);
	if(! uRight)
	{
		return str;
	}
	UINT uLeft = 0;
	if(bStripLeft)
	{
		while(str[uLeft] == ' ' || str[uLeft] == L'　' || str[uLeft] == '\t' || str[uLeft] == '\n' || str[uLeft] == '\r' || str[uLeft] == '\f' || str[uLeft] == '\v')
		{
			++uLeft;
		}
	}
	if(bStripRight)
	{
		while(str[uRight] == '\0' || str[uRight] == '\n' || str[uRight] == '\r' || str[uRight] == ' ' || str[uRight] == L'　' || str[uRight] == '\t' || str[uLeft] == '\f' || str[uLeft] == '\v')
		{
			if(! --uRight)
			{
				break;
			}
		}
	}
	return iws_pclone((str + uLeft), (str + uRight + 1));
}
//---------------------------
// Dir末尾の "\" を消去する
//---------------------------
// v2023-07-24
WS
*iws_cutYenR(
	WS *path
)
{
	WS *rtn = iws_clone(path);
	WS *pEnd = rtn + wcslen(rtn) - 1;
	while(*pEnd == L'\\')
	{
		*pEnd = 0;
		--pEnd;
	}
	return rtn;
}
//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	Array
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
//-------------------
// 配列サイズを取得
//-------------------
/* (例)
	iCLI_begin();
	PL3(iwan_size($ARGV));
*/
// v2023-07-29
UINT64
iwan_size(
	WS **ary
)
{
	UINT64 rtn = 0;
	while(*ary)
	{
		++rtn;
		++ary;
	}
	return rtn;
}
//---------------------
// 配列の合計長を取得
//---------------------
/* (例)
	iCLI_begin();
	PL3(iwan_strlen($ARGV));
*/
// v2023-07-29
UINT64
iwan_strlen(
	WS **ary
)
{
	UINT64 rtn = 0;
	while(*ary)
	{
		rtn += wcslen(*ary);
		++ary;
	}
	return rtn;
}
//--------------
// 配列をqsort
//--------------
/* (例)
	// 順ソート／大小文字区別する
	qsort($ARGV, $ARGC, sizeof(WS*), iwan_sort_Asc);
	iwav_print($ARGV);
*/
// v2023-07-19
INT
iwan_sort_Asc(
	CONST VOID *arg1,
	CONST VOID *arg2
)
{
	return wcscmp(*(WS**)arg1, *(WS**)arg2);
}
/* (例)
	// 順ソート／大小文字区別しない
	qsort($ARGV, $ARGC, sizeof(WS*), iwan_sort_iAsc);
	iwav_print($ARGV);
*/
// v2023-07-19
INT
iwan_sort_iAsc(
	CONST VOID *arg1,
	CONST VOID *arg2
)
{
	return wcsicmp(*(WS**)arg1, *(WS**)arg2);
}
/* (例)
	// 逆順ソート／大小文字区別する
	qsort($ARGV, $ARGC, sizeof(WS*), iwan_sort_Desc);
	iwav_print($ARGV);
*/
// v2023-07-19
INT
iwan_sort_Desc(
	CONST VOID *arg1,
	CONST VOID *arg2
)
{
	return -(wcscmp(*(WS**)arg1, *(WS**)arg2));
}
/* (例)
	// 逆順ソート／大小文字区別しない
	qsort($ARGV, $ARGC, sizeof(WS*), iwan_sort_iDesc);
	iwav_print($ARGV);
*/
// v2023-07-19
INT
iwan_sort_iDesc(
	CONST VOID *arg1,
	CONST VOID *arg2
)
{
	return -(wcsicmp(*(WS**)arg1, *(WS**)arg2));
}
//---------------------
// 配列を文字列に変換
//---------------------
// v2023-07-30
WS
*iwas_njoin(
	WS **ary,   // 配列
	WS *token,  // 区切文字
	UINT start, // 取得位置
	UINT count  // 個数
)
{
	UINT64 uAry = iwan_size(ary);
	if(! uAry || start >= uAry || ! count)
	{
		return icalloc_WS(0);
	}
	if(! token)
	{
		token = L"";
	}
	UINT64 uToken = wcslen(token);
	WS *rtn = icalloc_WS(iwan_strlen(ary) + (uAry * uToken));
	WS *pEnd = rtn;
	UINT64 u1 = 0;
	while(u1 < uAry && count > 0)
	{
		if(u1 >= start)
		{
			--count;
			pEnd += iwn_cpy(pEnd, ary[u1]);
			pEnd += iwn_cpy(pEnd, token);
		}
		++u1;
	}
	*(pEnd - uToken) = 0;
	return rtn;
}
//---------------------------
// 配列から空白／重複を除く
//---------------------------
/* (例)
	WS *args[] = {L"aaa", L"AAA", L"BBB", L"", L"bbb", NULL};
	WS **wa1 = { 0 };
	INT i1 = 0;
	//
	// TRUE=大小文字区別しない
	//
	wa1 = iwaa_uniq(args, TRUE);
	i1 = 0;
	while((wa1[i1]))
	{
		PL2W(wa1[i1]); //=> "aaa", "BBB"
		++i1;
	}
	ifree(wa1);
	//
	// FALSE=大小文字区別する
	//
	wa1 = iwaa_uniq(args, FALSE);
	i1 = 0;
	while((wa1[i1]))
	{
		PL2W(wa1[i1]); //=> "aaa", "AAA", "BBB", "bbb"
		++i1;
	}
	ifree(wa1);

*/
// v2024-01-15
WS
**iwaa_uniq(
	WS **ary,
	BOOL icase // TRUE=大文字小文字区別しない
)
{
	CONST UINT uArySize = iwan_size(ary);
	UINT u1 = 0, u2 = 0;
	// iAryFlg 生成
	INT *iAryFlg = icalloc_INT(uArySize);
	// 上位へ集約
	u1 = 0;
	while(u1 < uArySize)
	{
		if(*ary[u1] && iAryFlg[u1] > -1)
		{
			iAryFlg[u1] = 1; // 仮
			u2 = u1 + 1;
			while(u2 < uArySize)
			{
				if(icase)
				{
					if(iwb_cmppi(ary[u1], ary[u2]))
					{
						iAryFlg[u2] = -1; // NG
					}
				}
				else
				{
					if(iwb_cmpp(ary[u1], ary[u2]))
					{
						iAryFlg[u2] = -1; // NG
					}
				}
				++u2;
			}
		}
		++u1;
	}
	// rtn作成
	UINT uAryUsed = 0;
	u1 = 0;
	while(u1 < uArySize)
	{
		if(iAryFlg[u1] == 1)
		{
			++uAryUsed;
		}
		++u1;
	}
	WS **rtn = icalloc_WS_ary(uAryUsed);
	u1 = u2 = 0;
	while(u1 < uArySize)
	{
		if(iAryFlg[u1] == 1)
		{
			rtn[u2] = iws_clone(ary[u1]);
			++u2;
		}
		++u1;
	}
	ifree(iAryFlg);
	return rtn;
}
//----------------------------
// ARGS から Dir／Fileを抽出
//----------------------------
/* (例)
	WS *args[] = {L"", L"D:", L"C:\\Windows\\", L"a:", L"c:", L"c:\\foo", NULL};
	WS **wa1 = iwaa_getDirFile(args, 1);
		iwav_print(wa1); //=> 'c:\' 'C:\Windows\' 'D:\'
	ifree(wa1);
*/
// v2023-09-18
WS
**iwaa_getDirFile(
	WS **ary,
	INT iType // 0=Dir&File／2=Dir／3=File
)
{
	// 重複排除
	WS **wa1 = iwaa_uniq(ary, TRUE);
		UINT uArySize = iwan_size(wa1);
		WS **rtn = icalloc_WS_ary(uArySize);
		UINT u1 = 0, u2 = 0;
		// Dir名整形／存在するDirのみ抽出
		while(u1 < uArySize)
		{
			if(iType == 0 || iType == 1)
			{
				if(iFchk_DirName(wa1[u1]) && PathFileExistsW(wa1[u1]))
				{
					rtn[u2] = iFget_APath(wa1[u1]);
					++u2;
				}
			}
			if(iType == 0 || iType == 2)
			{
				if(! iFchk_DirName(wa1[u1]) && PathFileExistsW(wa1[u1]))
				{
					WS *_wp1 = icalloc_WS(IMAX_PATH);
					_wfullpath(_wp1, wa1[u1], IMAX_PATH);
					rtn[u2] = _wp1;
					++u2;
				}
			}
			++u1;
		}
	ifree(wa1);
	// 順ソート／大小文字区別しない
	iwav_sort_iAsc(rtn);
	return rtn;
}
//----------------
// 上位Dirを抽出
//----------------
/* (例)
	WS *args[] = {L"", L"D:", L"C:\\Windows\\", L"a:", L"c:", L"d:\\TMP", NULL};
	WS **wa1 = iwaa_higherDir(args);
		iwav_print(wa1); //=> 'c:\' 'D:\'
	ifree(wa1);
*/
// v2023-09-18
WS
**iwaa_higherDir(
	WS **ary
)
{
	WS **rtn = iwaa_getDirFile(ary, 1);
	UINT uArySize = iwan_size(rtn);
	UINT u1 = 0, u2 = 0;
	// 上位Dirを取得
	while(u1 < uArySize)
	{
		u2 = u1 + 1;
		while(u2 < uArySize)
		{
			// 前方一致／大小文字区別しない
			if(iwb_cmpfi(rtn[u2], rtn[u1]))
			{
				rtn[u2] = L"";
				++u2;
			}
			else
			{
				break;
			}
		}
		u1 = u2;
	}
	// 実データを前に詰める
	u1 = 0;
	while(u1 < uArySize)
	{
		if(! *rtn[u1])
		{
			u2 = u1 + 1;
			while(u2 < uArySize)
			{
				if(*rtn[u2])
				{
					rtn[u1] = rtn[u2];
					rtn[u2] = L"";
					break;
				}
				++u2;
			}
		}
		++u1;
	}
	// 末尾の空白をNULL詰め
	u1 = uArySize;
	while(u1 > 0)
	{
		--u1;
		if(! *rtn[u1])
		{
			rtn[u1] = 0;
		}
	}
	return rtn;
}
//-----------
// 配列一覧
//-----------
/* (例)
	iwav_print($ARGV);
*/
// v2024-01-17
VOID
iwav_print(
	WS **ary
)
{
	if(! ary)
	{
		return;
	}
	UINT u1 = 0;
	while(*ary)
	{
		MS *mp1 = W2M(*ary);
			P(" %4u) '%s'\n", ++u1, mp1);
		ifree(mp1);
		++ary;
	}
}
/* (例)
	iwav_print2($ARGV, L"'", L"'\n");
*/
// v2024-01-18
VOID
iwav_print2(
	WS **ary,
	WS *sLeft,
	WS *sRight
)
{
	if(! ary)
	{
		return;
	}
	MS *mp1L = W2M(sLeft);
	MS *mp1R = W2M(sRight);
		while(*ary)
		{
			MS *mp1 = W2M(*ary);
				P1(mp1L);
				P1(mp1);
				P1(mp1R);
			ifree(mp1);
			++ary;
		}
	ifree(mp1R);
	ifree(mp1L);
}
//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	Variable Buffer
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
/* (例)
	$struct_iVBMS *IVBMS = iVBMS_alloc2(10000);
		iVBMS_add(IVBMS, "123456789");
		iVBMS_add(IVBMS, "あいうえお");
			P2(iVBMS_getStr(IVBMS));
			PL3(iVBMS_getLength(IVBMS));
			PL3(iVBMS_getSize(IVBMS));
		iVBMS_clear(IVBMS);
			for(UINT64 _u1 = 0; _u1 < 1000000; _u1++)
			{
				iVBMS_add(IVBMS, "かきくけこ");
			}
			QP(iVBMS_getStr(IVBMS), iVBMS_getLength(IVBMS));
			NL();
			PL3(iVBMS_getLength(IVBMS));
			PL3(iVBMS_getSize(IVBMS));
	iVBMS_free(IVBMS);
*/
// v2024-01-15
$struct_iVBStr
*iVBStr_alloc(
	UINT64 startSize, // 開始時の文字列長（自動拡張）
	INT sizeOfChar    // sizeof(MS), sizeof(WS)
)
{
	$struct_iVBStr *IVBS = icalloc(1, sizeof($struct_iVBStr), FALSE);
		IVBS->size = startSize;
		IVBS->str = (VOID*)icalloc(IVBS->size, sizeOfChar, FALSE);
		IVBS->length = 0;
	return IVBS;
}
// v2024-01-15
VOID
*iVBStr_add(
	$struct_iVBStr *IVBS, // 格納場所
	VOID *str,            // 追記する文字列
	UINT64 strLen,        // 追記する文字列長／strlen(str), wcslen(str)
	INT sizeOfChar        // sizeof(MS), sizeof(WS)
)
{
	if((strLen + IVBS->length) >= IVBS->size)
	{
		IVBS->size += strLen;
		IVBS->size *= 2;
		IVBS->str = (VOID*)irealloc(IVBS->str, IVBS->size, sizeOfChar);
	}

	VOID *vp1 = (IVBS->str + (IVBS->length * sizeOfChar));
	UINT64 u1 = strLen * sizeOfChar;
	memcpy(vp1, str, u1);
	memset((vp1 + u1), 0, sizeOfChar);
	IVBS->length += strLen;
	return IVBS->str;
}
//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	File/Dir処理／WIN32_FIND_DATAW
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
//-------------------
// ファイル情報取得
//-------------------
/*
//
// (例１) 複数取得する場合
//
VOID
ifind1(
	$struct_iFinfo *FI,
	WIN32_FIND_DATAW F,
	WS *dir
)
{
	// FI->sPath 末尾に "*" を付与
	UINT dirLen = iwn_cpy(FI->sPath, dir);
		*(FI->sPath + dirLen) = '*';
		*(FI->sPath + dirLen + 1) = 0;
	HANDLE hfind = FindFirstFileW(FI->sPath, &F);
		// アクセス不可なフォルダ等はスルー
		if(hfind == INVALID_HANDLE_VALUE)
		{
			FindClose(hfind);
			return;
		}
		// Dir
		iFinfo_init(FI, &F, dir, NULL);
			MS *mp1 = W2M(FI->sPath);
				P("%llu\t%s\n", FI->uFsize, mp1);
			ifree(mp1);
		// File
		do
		{
			if(iFinfo_init(FI, &F, dir, F.cFileName))
			{
				// Dir
				if(FI->bType)
				{
					WS *wp1 = iws_clone(FI->sPath);
						ifind1(FI, F, wp1); // Dir(下位)
					ifree(wp1);
				}
				// File
				else
				{
					MS *mp1 = W2M(FI->sPath);
						P("%llu\t%s\n", FI->uFsize, mp1);
					ifree(mp1);
				}
			}
		}
		while(FindNextFileW(hfind, &F));
	FindClose(hfind);
}
	//
	// main()
	//
	WS *dir = iFget_RPath(L".");
		if(dir)
		{
			$struct_iFinfo *FI = iFinfo_alloc();
				WIN32_FIND_DATAW F;
				ifind1(FI, F, dir);
			iFinfo_free(FI);
		}
	ifree(dir);
//
// (例２) 単一パスから情報取得する場合
//
VOID
ifind2(
	WS *path
)
{
	INT iPos = iwn_len(path) - 1;
	for(; iPos >= 0; iPos--)
	{
		if(path[iPos] == '\\' || path[iPos] == '/')
		{
			break;
		}
	}
	++iPos;
	WS *dir = iws_clone(path);
		dir[iPos] = 0;
	WS *name = iws_clone(path + iPos);
		$struct_iFinfo *FI = iFinfo_alloc();
			iwn_cpy(FI->sPath, path);
			WIN32_FIND_DATAW F;
			HANDLE hfind = FindFirstFileW(FI->sPath, &F);
				// アクセス不可なフォルダなど
				if(hfind == INVALID_HANDLE_VALUE)
				{
					FindClose(hfind);
					return;
				}
				if(iFinfo_init(FI, &F, dir, name))
				{
					WS *wp1 = 0;
					P1("Path:    ");
						P2W(FI->sPath);
					P1("FnPos:   ");
						P3(FI->uFnPos);
					P1("Fn:      ");
						P2W(FI->sPath + FI->uFnPos);
					P1("Bytes:   ");
						P3(FI->uFsize);
					P1("Ctime:   ");
						wp1 = idate_format_cjdToWS(NULL, FI->cjdCtime);
							P2W(wp1);
						ifree(wp1);
					P1("Mtime:   ");
						wp1 = idate_format_cjdToWS(NULL, FI->cjdMtime);
							P2W(wp1);
						ifree(wp1);
					P1("Atime:   ");
						wp1 = idate_format_cjdToWS(NULL, FI->cjdAtime);
							P2W(wp1);
						ifree(wp1);
					NL();
				}
			FindClose(hfind);
		iFinfo_free(FI);
	ifree(name);
	ifree(dir);
}
	//
	// main()
	//
	ifind2(L"lib_iwmutil2.c");
*/
// v2016-08-09
$struct_iFinfo
*iFinfo_alloc()
{
	return icalloc(1, sizeof($struct_iFinfo), FALSE);
}
//---------------------------
// ファイル情報取得の前処理
//---------------------------
// v2024-01-04
BOOL
iFinfo_init(
	$struct_iFinfo *FI,
	WIN32_FIND_DATAW *F,
	WS *dir, // "\"を付与して呼ぶ
	WS *fname
)
{
	// 初期化
	*FI->sPath   = 0;
	FI->uFnPos   = 0;
	FI->uAttr    = 0;
	FI->bType    = FALSE;
	FI->cjdCtime = 0.0;
	FI->cjdMtime = 0.0;
	FI->cjdAtime = 0.0;
	FI->uFsize   = 0;
	// Dir "." ".." は除外
	if(fname && *fname && iwb_cmp_leqfi(fname, L".."))
	{
		return FALSE;
	}
	// FI->uAttr
	FI->uAttr = (UINT)F->dwFileAttributes;
	// < 32768
	if(FI->uAttr >> 15)
	{
		return FALSE;
	}
	// FI->sPath
	// FI->uFname
	UINT dirLen  = iwn_cpy(FI->sPath, dir);
	UINT nameLen = iwn_cpy((FI->sPath + dirLen), fname);
	// FI->bType
	// FI->uFsize
	if(FI->uAttr & FILE_ATTRIBUTE_DIRECTORY)
	{
		// Dir直下は nameLen = 0
		if(nameLen)
		{
			dirLen += nameLen + 1;
			*(FI->sPath + dirLen - 1) = L'\\'; // '\\' 付与
			*(FI->sPath + dirLen) = 0;         // '\0' 付与
		}
		FI->bType = TRUE;
		// FI->uFsize = 0 初期化値
	}
	else
	{
		// FI->bType = FALSE 初期化値
		FI->uFsize = (UINT64)F->nFileSizeLow + (F->nFileSizeHigh ? (UINT64)(F->nFileSizeHigh) * MAXDWORD + 1 : 0);
	}
	FI->uFnPos = dirLen;
	// JST変換
	// FI->cftime
	// FI->mftime
	// FI->aftime
	FILETIME ft;
	FileTimeToLocalFileTime(&F->ftCreationTime, &ft);
		FI->cjdCtime = iFinfo_ftimeToCjd(ft);
	FileTimeToLocalFileTime(&F->ftLastWriteTime, &ft);
		FI->cjdMtime = iFinfo_ftimeToCjd(ft);
	FileTimeToLocalFileTime(&F->ftLastAccessTime, &ft);
		FI->cjdAtime = iFinfo_ftimeToCjd(ft);
	return TRUE;
}
// v2016-08-09
VOID
iFinfo_free(
	$struct_iFinfo *FI
)
{
	ifree(FI);
}
//---------------------
// ファイル情報を変換
//---------------------
/*
	// 1: READONLY
	FILE_ATTRIBUTE_READONLY

	// 2: HIDDEN
	FILE_ATTRIBUTE_HIDDEN

	// 4: SYSTEM
	FILE_ATTRIBUTE_SYSTEM

	// 16: DIRECTORY
	FILE_ATTRIBUTE_DIRECTORY

	// 32: ARCHIVE
	FILE_ATTRIBUTE_ARCHIVE

	// 64: DEVICE
	FILE_ATTRIBUTE_DEVICE

	// 128: NORMAL
	FILE_ATTRIBUTE_NORMAL

	// 256: TEMPORARY
	FILE_ATTRIBUTE_TEMPORARY

	// 512: SPARSE FILE
	FILE_ATTRIBUTE_SPARSE_FILE

	// 1024: REPARSE_POINT
	FILE_ATTRIBUTE_REPARSE_POINT

	// 2048: COMPRESSED
	FILE_ATTRIBUTE_COMPRESSED

	// 8192: NOT CONTENT INDEXED
	FILE_ATTRIBUTE_NOT_CONTENT_INDEXED

	// 16384: ENCRYPTED
	FILE_ATTRIBUTE_ENCRYPTED
*/
// v2023-07-24
WS
*iFinfo_attrToWS(
	UINT uAttr
)
{
	WS *rtn = icalloc_WS(5);
	rtn[0] = (uAttr & FILE_ATTRIBUTE_DIRECTORY ? 'd' : '-'); // 16=Dir
	rtn[1] = (uAttr & FILE_ATTRIBUTE_READONLY  ? 'r' : '-'); //  1=ReadOnly
	rtn[2] = (uAttr & FILE_ATTRIBUTE_HIDDEN    ? 'h' : '-'); //  2=Hidden
	rtn[3] = (uAttr & FILE_ATTRIBUTE_SYSTEM    ? 's' : '-'); //  4=System
	rtn[4] = (uAttr & FILE_ATTRIBUTE_ARCHIVE   ? 'a' : '-'); // 32=Archive
	return rtn;
}
// v2023-08-31
UINT
iFinfo_attrToINT(
	WS *sAttr // L"rhsda" => 55
)
{
	if(! sAttr || ! *sAttr)
	{
		return 0;
	}
	UINT rtn = 0;
	WS *pEnd = sAttr;
	while(*pEnd)
	{
		// 頻出順
		switch(*pEnd)
		{
			// 32 = ARCHIVE
			case('a'):
			case('A'):
				rtn += FILE_ATTRIBUTE_ARCHIVE;
				break;
			// 16 = DIRECTORY
			case('d'):
			case('D'):
				rtn += FILE_ATTRIBUTE_DIRECTORY;
				break;
			// 4 = SYSTEM
			case('s'):
			case('S'):
				rtn += FILE_ATTRIBUTE_SYSTEM;
				break;
			// 2 = HIDDEN
			case('h'):
			case('H'):
				rtn += FILE_ATTRIBUTE_HIDDEN;
				break;
			// 1 = READONLY
			case('r'):
			case('R'):
				rtn += FILE_ATTRIBUTE_READONLY;
				break;
		}
		++pEnd;
	}
	return rtn;
}
//---------------
// FileTime関係
//---------------
/*
	基本、FILETIME(UTC)で処理。
	必要に応じて、JST(UTC+9h)に変換した値を渡すこと。
*/
// v2023-07-20
WS
*iFinfo_ftimeToWS(
	FILETIME ft
)
{
	WS *rtn = icalloc_WS(20);
	SYSTEMTIME st;
	FileTimeToSystemTime(&ft, &st);
	if(st.wYear <= 1980 || st.wYear >= 2100)
	{
		rtn = 0;
	}
	else
	{
		wsprintfW(
			rtn,
			DATETIME_FORMAT,
			st.wYear,
			st.wMonth,
			st.wDay,
			st.wHour,
			st.wMinute,
			st.wSecond
		);
	}
	return rtn;
}
// v2024-01-06
INT64
iFinfo_ftimeToINT64(
	FILETIME ftime
)
{
	return (((INT64)ftime.dwHighDateTime << 32) + ftime.dwLowDateTime);
}
// v2024-01-06
DOUBLE
iFinfo_ftimeToCjd(
	FILETIME ftime
)
{
	INT64 i1 = iFinfo_ftimeToINT64(ftime);
	i1 /= 10000000; // (重要) MicroSecond 削除
	return ((DOUBLE)i1 / 86400) + 2305814.0;
}
// v2021-11-15
FILETIME
iFinfo_ymdhnsToFtime(
	INT i_y,   // 年
	INT i_m,   // 月
	INT i_d,   // 日
	INT i_h,   // 時
	INT i_n,   // 分
	INT i_s,   // 秒
	BOOL reChk // TRUE=年月日を正規化／FALSE=入力値を信用
)
{
	SYSTEMTIME st;
	FILETIME ft;
	if(reChk)
	{
		INT *ai = idate_reYmdhns(i_y, i_m, i_d, i_h, i_n, i_s);
			i_y = ai[0];
			i_m = ai[1];
			i_d = ai[2];
			i_h = ai[3];
			i_n = ai[4];
			i_s = ai[5];
		ifree(ai);
	}
	st.wYear         = i_y;
	st.wMonth        = i_m;
	st.wDay          = i_d;
	st.wHour         = i_h;
	st.wMinute       = i_n;
	st.wSecond       = i_s;
	st.wMilliseconds = 0;
	SystemTimeToFileTime(&st, &ft);
	return ft;
}
//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	File/Dir処理
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
/*
[2014-01-13]
	フォルダ名／ファイル名を引数とする処理は WS(=WideChar)版 を使用する。
	理由は以下のとおり。
	・[MS]漢字等を含むとき fopen() がエラーを返す／[WS]_wfopen() はOK
	・[MS]文字列走査が複雑になる／[WS]１文字毎に走査可能
*/
//-------------------------------
// Binary File のときTRUEを返す
//-------------------------------
/* (例)
	// "aaa.exe" は存在／"aaa.txt" は不在、と仮定
	PL3(iFchk_BfileW(L"aaa.exe")); //=> TRUE
	PL3(iFchk_BfileW(L"aaa.txt")); //=> FALSE
	PL3(iFchk_BfileW(L"???"));     //=> FALSE (存在しないとき)
*/
// v2024-01-13
BOOL
iFchk_Binfile(
	WS *Fn
)
{
	BOOL rtn = FALSE;
	FILE *Fp = _wfopen(Fn, L"rb");
	if(Fp)
	{
		INT c = 0;
		while((c = getc(Fp)) != EOF)
		{
			if(! c)
			{
				rtn = TRUE;
				break;
			}
		}
	}
	fclose(Fp);
	return rtn;
}
//---------------------
// ファイル名等を抽出
//---------------------
/* (例)
	WS *p1 = L"c:\\windows\\win.ini";
	PL2W(iFget_extPathname(p1, 0)); //=> "c:\windows\win.ini"
	PL2W(iFget_extPathname(p1, 1)); //=> "win.ini"
	PL2W(iFget_extPathname(p1, 2)); //=> "win"
*/
// v2023-09-17
WS
*iFget_extPathname(
	WS *path,
	INT option // 1=拡張子付きファイル名／2=拡張子なしファイル名
)
{
	if(! path || ! *path)
	{
		return icalloc_WS(0);
	}
	WS *rtn = icalloc_WS(wcslen(path));
	// Dir or File ?
	if(iFchk_DirName(path))
	{
		if(option == 0)
		{
			wcscpy(rtn, path);
		}
	}
	else
	{
		switch(option)
		{
			// path
			case(0):
				wcscpy(rtn, path);
				break;
			// name + ext
			case(1):
				wcscpy(rtn, PathFindFileNameW(path));
				break;
			// name
			case(2):
				WS *pBgn = PathFindFileNameW(path);
				WS *pEnd = PathFindExtensionW(pBgn);
				iwn_pcpy(rtn, pBgn, pEnd);
				break;
		}
	}
	return rtn;
}
//--------------------------
// Path を 絶対Path に変換
//--------------------------
/* (例)
	// "." = "d:\foo" のとき
	PL2W(iFget_APath(L".")); //=> "d:\foo\"
*/
// v2023-09-17
WS
*iFget_APath(
	WS *path
)
{
	if(! path || ! *path)
	{
		return icalloc_WS(0);
	}
	WS *rtn = 0;
	WS *p1 = iws_cutYenR(path);
	// "c:" のような表記は特別処理
	if(p1[1] == ':' && wcslen(p1) == 2 && iFchk_DirName(p1))
	{
		rtn = iws_cats(2, p1 , L"\\");
	}
	else
	{
		rtn = icalloc_WS(IMAX_PATH);
		if(iFchk_DirName(p1) && _wfullpath(rtn, p1, IMAX_PATH))
		{
			wcscat(rtn, L"\\");
		}
	}
	ifree(p1);
	return rtn;
}
//------------------------
// 相対Path に '\\' 付与
//------------------------
/* (例)
	PL2W(iFget_RPath(L".")); //=> ".\"
*/
// v2023-09-17
WS
*iFget_RPath(
	WS *path
)
{
	if(! path || ! *path)
	{
		return icalloc_WS(0);
	}
	WS *rtn = iws_cutYenR(path);
	if(iFchk_DirName(path))
	{
		wcscat(rtn, L"\\");
	}
	return rtn;
}
//--------------------
// 複階層のDirを作成
//--------------------
/* (例)
	PL3(iF_mkdir(L"aaa\\bbb")); //=> 2
*/
// v2023-09-03
UINT
iF_mkdir(
	WS *path
)
{
	UINT rtn = 0;
	WS *pBgn = iws_cats(2, path, L"\\");
		WS *pEnd = pBgn;
		while(*pEnd)
		{
			if(*pEnd == '\\')
			{
				WS *_p1 = iws_pclone(pBgn, pEnd);
					if(CreateDirectoryW(_p1, NULL))
					{
						++rtn;
					}
				ifree(_p1);
			}
			++pEnd;
		}
	ifree(pBgn);
	return rtn;
}
//-------------------------
// Dir/Fileをゴミ箱へ移動
//-------------------------
/* (例)
	// '\t' '\r' '\n' 区切り文字列に対応
	PL3(iF_trash(L"新しいテキスト ドキュメント.txt\n新しいフォルダー\n"));
*/
// v2024-01-18
WS
**iF_trash(
	WS *path
)
{
	if(! iwn_len(path))
	{
		return 0;
	}
	WS **rtn = icalloc_WS_ary(0);
	// '\t' '\r' '\n' で分割
	WS **awp1 = iwaa_split(path, L"\t\r\n", TRUE);
		// Trim
		for(UINT _u1 = 0; _u1 < iwan_size(awp1); _u1++)
		{
			WS *wp1 = iws_trim(awp1[_u1]);
				wcscpy(awp1[_u1], wp1);
			ifree(wp1);
		}
		// Uniq
		WS **awp2 = iwaa_uniq(awp1, TRUE);
			CONST UINT uAwp2Size = iwan_size(awp2);
			if(uAwp2Size)
			{
				SHFILEOPSTRUCTW sfos;
					ZeroMemory(&sfos, sizeof(SHFILEOPSTRUCTW));
					sfos.hwnd = 0;
					sfos.wFunc = FO_DELETE;
					sfos.fFlags = FOF_ALLOWUNDO | FOF_NO_UI;
				rtn = irealloc_WS_ary(rtn, uAwp2Size);
				for(UINT _u1 = 0, _u2 = 0; _u1 < uAwp2Size; _u1++)
				{
					// Dir/File Exist?
					// ファルダごと移動されたファイルは、ここでは存在しない。
					if(iFchk_existPath(awp2[_u1]))
					{
						sfos.pFrom = awp2[_u1];
						if(! SHFileOperationW(&sfos))
						{
							rtn[_u2++] = iws_clone(awp2[_u1]);
						}
					}
				}
			}
		ifree(awp2);
	ifree(awp1);
	return rtn;
}
//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	Console
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
//------------------
// RGB色を使用する
//------------------
/* (例)
	// ESC有効化
	iConsole_EscOn();

	// リセット
	//   すべて   \033[0m
	//   文字のみ \033[39m
	//   背景のみ \033[49m

	// SGRによる指定例
	//  文字色 9n
	//  背景色 10n
	//    0=黒    1=赤    2=黄緑  3=黄
	//    4=青    5=紅紫  6=水    7=白
	P2("\033[91m 文字[赤] \033[0m");
	P2("\033[101m 背景[赤] \033[0m");
	P2("\033[91;107m 文字[赤]／背景[白] \033[0m");

	// RGBによる指定例
	//  文字色   \033[38;2;R;G;Bm
	//  背景色   \033[48;2;R;G;Bm
	P2("\033[38;2;255;255;255m\033[48;2;0;0;255m 文字[白]／背景[青] \033[0m");
*/
// v2023-08-30
VOID
iConsole_EscOn()
{
	$StdoutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
	ULONG consoleMode = 0;
	GetConsoleMode($StdoutHandle, &consoleMode);
	consoleMode = (consoleMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
	SetConsoleMode($StdoutHandle, consoleMode);
}
//---------------------
// STDIN から直接読込
//---------------------
/* (例)
	// 手入力のとき
	//   [Enter] [Ctrl]+[Z] [Enter] で入力終了
	WS *wp1 = iCLI_GetStdin();
		P("\033[93m" "[wcslen = %d]" "\033[0m\n", wcslen(wp1));
		P1("\033[92m");
		P1W(wp1);
		P1("\033[0m\n\n");
	ifree(wp1);
*/
// v2024-01-06
WS
*iCLI_GetStdin()
{
	INT64 mp1Size = 256;
	INT64 mp1End = 0;
	MS *mp1 = icalloc_MS(mp1Size);

	// 手入力のとき
	//   [Enter] [Ctrl]+[Z] [Enter] で入力終了
	INT c = 0;
	while((c = getchar()) != EOF)
	{
		mp1[mp1End] = (MS)c;
		++mp1End;
		if(mp1End >= mp1Size)
		{
			mp1Size *= 2;
			mp1 = irealloc_MS(mp1, mp1Size);
		}
	}

	// 末尾の特殊文字を消去してから'\n'付与
	//   -1=EOF／10='\n'／32=' '
	for(--mp1End; mp1End >= 0 && mp1[mp1End] <= 32; mp1End--)
	{
		mp1[mp1End] = 0;
	}
	mp1[mp1End + 1] = '\n';

	///PL3(imn_Codepage(mp1));
	WS *rtn = icnv_M2W(mp1, imn_Codepage(mp1));
	ifree(mp1);

	// リセット
	iConsole_EscOn();
	SetConsoleOutputCP(65001);

	return rtn;
}
//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	暦
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
//--------------------------
// 空白暦：1582/10/5-10/14
//--------------------------
// {-4712-1-1からの通算日, yyyymmdd}
INT NS_BEFORE[2] = {2299160, 15821004};
INT NS_AFTER[2]  = {2299161, 15821015};
//---------------------------
// 曜日表示設定 [7] = Err値
//---------------------------
WS *WDAYS[8] = {L"Su", L"Mo", L"Tu", L"We", L"Th", L"Fr", L"Sa", L"**"};
//-----------------------
// 月末日(-1y12m - 12m)
//-----------------------
INT MDAYS[13] = {31, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
//---------------------------------------------------
// yyyy/mm/dd hh:nn:ss フォーマットに類似しているか
//---------------------------------------------------
/* (例)
	PL3(idate_chk_ymdhnsW(NULL));                   //=> FALSE
	PL3(idate_chk_ymdhnsW(L""));                    //=> FALSE
	PL3(idate_chk_ymdhnsW(L"abc"));                 //=> FALSE
	PL3(idate_chk_ymdhnsW(L"-1234.56"));            //=> FALSE
	PL3(idate_chk_ymdhnsW(L"+2022-09-22"));         //=> TRUE
	PL3(idate_chk_ymdhnsW(L"2022/09/22 13:40:10")); //=> TRUE
	PL3(idate_chk_ymdhnsW(L"-100/1/1"));            //=> TRUE
*/
// v2022-09-22
BOOL
idate_chk_ymdhnsW(
	WS *str
)
{
	if(! str || ! *str)
	{
		return FALSE;
	}
	UINT u1 = 0;
	while(*str)
	{
		if((*str >= '0' && *str <= '9') || *str == ':' || *str == ' ' || *str == '+')
		{
		}
		else if(*str == '/' || *str == '-')
		{
			++u1;
		}
		else
		{
			return FALSE;
		}
		++str;
	}
	return (u1 > 1 ? TRUE : FALSE);
}
//---------------
// 閏年チェック
//---------------
/* (例)
	idate_chk_uruu(2012); //=> TRUE
	idate_chk_uruu(2013); //=> FALSE
*/
// v2022-04-29
BOOL
idate_chk_uruu(
	INT i_y // 年
)
{
	if(i_y > (INT)(NS_AFTER[1] / 10000))
	{
		if(! (i_y % 400))
		{
			return TRUE;
		}
		if(! (i_y % 100))
		{
			return FALSE;
		}
	}
	return (! (i_y % 4) ? TRUE : FALSE);
}
//-------------
// 月を正規化
//-------------
/* (例)
	INT *ai = idate_cnv_month(2011, 14, 1, 12);
	for(INT i1 = 0; i1 < 2; i1++)
	{
		PL3(ai[i1]); //=> 2012, 2
	}
	ifree(ai);
*/
// v2021-11-15
INT
*idate_cnv_month(
	INT i_y,    // 年
	INT i_m,    // 月
	INT from_m, // 開始月
	INT to_m    // 終了月
)
{
	INT *rtn = icalloc_INT(2);
	while(i_m < from_m)
	{
		i_m += 12;
		i_y -= 1;
	}
	while(i_m > to_m)
	{
		i_m -= 12;
		i_y += 1;
	}
	rtn[0] = i_y;
	rtn[1] = i_m;
	return rtn;
}
//---------------
// 月末日を返す
//---------------
/* (例)
	idate_month_end(2012, 2); //=> 29
	idate_month_end(2013, 2); //=> 28
*/
// v2021-11-15
INT
idate_month_end(
	INT i_y, // 年
	INT i_m  // 月
)
{
	INT *ai = idate_cnv_month1(i_y, i_m);
	INT i_d = MDAYS[ai[1]];
	// 閏２月末日
	if(ai[1] == 2 && idate_chk_uruu(ai[0]))
	{
		i_d = 29;
	}
	ifree(ai);
	return i_d;
}
//-----------------
// 月末日かどうか
//-----------------
/* (例)
	PL3(idate_chk_month_end(2012, 2, 28, FALSE)); //=> FALSE
	PL3(idate_chk_month_end(2012, 2, 29, FALSE)); //=> TRUE
	PL3(idate_chk_month_end(2012, 1, 60, TRUE )); //=> TRUE
	PL3(idate_chk_month_end(2012, 1, 60, FALSE)); //=> FALSE
*/
// v2021-11-15
BOOL
idate_chk_month_end(
	INT i_y,   // 年
	INT i_m,   // 月
	INT i_d,   // 日
	BOOL reChk // TRUE=年月日を正規化／FALSE=入力値を信用
)
{
	if(reChk)
	{
		INT *ai1 = idate_reYmdhns(i_y, i_m, i_d, 0, 0, 0);
			i_y = ai1[0];
			i_m = ai1[1];
			i_d = ai1[2];
		ifree(ai1);
	}
	return (i_d == idate_month_end(i_y, i_m) ? TRUE : FALSE);
}
//-------------------------
// strを年月日に分割(int)
//-------------------------
/* (例)
	INT *ai1 = idate_WsToiAryYmdhns(L"-2012-8-12 12:45:00");
	for(INT i1 = 0; i1 < 6; i1++)
	{
		PL3(ai1[i1]); //=> -2012, 8, 12, 12, 45, 0
	}
	ifree(ai1);
*/
// v2023-07-21
INT
*idate_WsToiAryYmdhns(
	WS *str // (例) "2012-03-12 13:40:00"
)
{
	INT *rtn = icalloc_INT(6);
	if(! str)
	{
		return rtn;
	}
	INT i1 = 1;
	if(*str == '-')
	{
		i1 = -1;
		++str;
	}
	WS **as1 = iwaa_split(str, L"/.-: ", TRUE);
	INT i2 = 0;
	while(as1[i2])
	{
		rtn[i2] = _wtoi(as1[i2]);
		++i2;
	}
	ifree(as1);
	if(i1 == -1)
	{
		rtn[0] *= -1;
	}
	return rtn;
}
//---------------------
// 年月日を数字に変換
//---------------------
/* (例)
	idate_ymd_num(2012, 6, 17); //=> 20120617
*/
// v2013-03-17
INT
idate_ymd_num(
	INT i_y, // 年
	INT i_m, // 月
	INT i_d  // 日
)
{
	return (i_y * 10000) + (i_m * 100) + (i_d);
}
//-----------------------------------------------
// 年月日をCJDに変換
//   (注)空白日のとき、一律 NS_BEFORE[0] を返す
//-----------------------------------------------
// v2021-11-15
DOUBLE
idate_ymdhnsToCjd(
	INT i_y, // 年
	INT i_m, // 月
	INT i_d, // 日
	INT i_h, // 時
	INT i_n, // 分
	INT i_s  // 秒
)
{
	DOUBLE cjd = 0.0;
	INT i1 = 0, i2 = 0;
	INT *ai = idate_cnv_month1(i_y, i_m);
		i_y = ai[0];
		i_m = ai[1];
	ifree(ai);
	// 1m=>13m, 2m=>14m
	if(i_m <= 2)
	{
		i_y -= 1;
		i_m += 12;
	}
	// 空白日
	i1 = idate_ymd_num(i_y, i_m, i_d);
	if(i1 > NS_BEFORE[1] && i1 < NS_AFTER[1])
	{
		return (DOUBLE)NS_BEFORE[0];
	}
	// ユリウス通日
	cjd = floor((DOUBLE)(365.25 * (i_y + 4716)) + floor(30.6001 * (i_m + 1)) + i_d - 1524.0);
	if((INT)cjd >= NS_AFTER[0])
	{
		i1 = (INT)floor(i_y / 100.0);
		i2 = 2 - i1 + (INT)floor(i1 / 4);
		cjd += (DOUBLE)i2;
	}
	return cjd + ((i_h * 3600 + i_n * 60 + i_s) / 86400.0);
}
//--------------------
// CJDを時分秒に変換
//--------------------
// v2023-07-13
INT
*idate_cjdToiAryHhs(
	DOUBLE cjd
)
{
	INT *rtn = icalloc_INT(3);
	INT i_h = 0, i_n = 0, i_s = 0;
	// 小数点部分を抽出
	//  [sec]   [補正前]  [cjd]
	//   0 = 0   -         0.00000000000000000
	//   1 = 1   -         0.00001157407407407
	//   2 = 2   -         0.00002314814814815
	//   3 > 2   NG        0.00003472222222222
	//   4 = 4   -         0.00004629629629630
	//   5 = 5   -         0.00005787037037037
	//   6 > 5   NG        0.00006944444444444
	//   7 = 7   -         0.00008101851851852
	//   8 = 8   -         0.00009259259259259
	//   9 = 9   -         0.00010416666666667
	DOUBLE d1 = (cjd - (INT)cjd);
		d1 += 0.00001; // 補正
		d1 *= 24.0;
	i_h = (INT)d1;
		d1 -= i_h;
		d1 *= 60.0;
	i_n = (INT)d1;
		d1 -= i_n;
		d1 *= 60.0;
	i_s = (INT)d1;
	rtn[0] = i_h;
	rtn[1] = i_n;
	rtn[2] = i_s;
	return rtn;
}
//--------------------
// CJDをYMDHNSに変換
//--------------------
// v2023-07-13
INT
*idate_cjdToiAryYmdhns(
	DOUBLE cjd
)
{
	INT *rtn = icalloc_INT(6);
	INT i_y = 0, i_m = 0, i_d = 0;
	INT iCJD = (INT)cjd;
	INT i1 = 0, i2 = 0, i3 = 0, i4 = 0;
	if((INT)cjd >= NS_AFTER[0])
	{
		i1 = (INT)floor((cjd - 1867216.25) / 36524.25);
		iCJD += (i1 - (INT)floor(i1 / 4.0) + 1);
	}
	i1 = iCJD + 1524;
	i2 = (INT)floor((i1 - 122.1) / 365.25);
	i3 = (INT)floor(365.25 * i2);
	i4 = (INT)((i1 - i3) / 30.6001);
	// d
	i_d = (i1 - i3 - (INT)floor(30.6001 * i4));
	// y, m
	if(i4 <= 13)
	{
		i_m = i4 - 1;
		i_y = i2 - 4716;
	}
	else
	{
		i_m = i4 - 13;
		i_y = i2 - 4715;
	}
	// h, n, s
	INT *ai2 = idate_cjdToiAryHhs(cjd);
		rtn[0] = i_y;
		rtn[1] = i_m;
		rtn[2] = i_d;
		rtn[3] = ai2[0];
		rtn[4] = ai2[1];
		rtn[5] = ai2[2];
	ifree(ai2);
	return rtn;
}
//---------
// 再計算
//---------
// v2023-07-13
INT
*idate_reYmdhns(
	INT i_y, // 年
	INT i_m, // 月
	INT i_d, // 日
	INT i_h, // 時
	INT i_n, // 分
	INT i_s  // 秒
)
{
	DOUBLE cjd = idate_ymdhnsToCjd(i_y, i_m, i_d, i_h, i_n, i_s);
	return idate_cjdToiAryYmdhns(cjd);
}
//-------------------------------------------
// cjd通日から曜日(日 = 0, 月 = 1...)を返す
//-------------------------------------------
// v2013-03-21
INT
idate_cjd_iWday(
	DOUBLE cjd
)
{
	return (INT)((INT)cjd + 1) % 7;
}
//------------------------------------------
// cjd通日から 日="Su", 月="Mo", ...を返す
//------------------------------------------
// v2022-08-20
WS
*idate_cjd_Wday(
	DOUBLE cjd
)
{
	return WDAYS[idate_cjd_iWday(cjd)];
}
//------------------------------
// cjd通日から年内の通日を返す
//------------------------------
// v2023-07-13
INT
idate_cjd_yeardays(
	DOUBLE cjd
)
{
	INT *ai = idate_cjdToiAryYmdhns(cjd);
	INT i1 = ai[0];
	ifree(ai);
	return (INT)(cjd - idate_ymdhnsToCjd(i1, 1, 0, 0, 0, 0));
}
//--------------------------------------
// 日付の前後 [6] = {y, m, d, h, n, s}
//--------------------------------------
/* (例)
	INT *ai = idate_add(2012, 1, 31, 0, 0, 0, 0, 1, 0, 0, 0, 0);
	for(INT i1 = 0; i1 < 6; i1++)
	{
		PL3(ai[i1]); //=> 2012, 2, 29, 0, 0, 0
	}
*/
// v2023-07-13
INT
*idate_add(
	INT i_y,   // 年
	INT i_m,   // 月
	INT i_d,   // 日
	INT i_h,   // 時
	INT i_n,   // 分
	INT i_s,   // 秒
	INT add_y, // 年
	INT add_m, // 月
	INT add_d, // 日
	INT add_h, // 時
	INT add_n, // 分
	INT add_s  // 秒
)
{
	INT *ai1 = 0;
	INT *ai2 = idate_reYmdhns(i_y, i_m, i_d, i_h, i_n, i_s);
	INT i1 = 0, i2 = 0, flg = 0;
	DOUBLE cjd = 0.0;
	// 個々に計算
	// 手を抜くと「1582-11-10 -1m, -1d」のとき、1582-10-04(期待値は1582-10-03)となる
	if(add_y != 0 || add_m != 0)
	{
		i1 = (INT)idate_month_end(ai2[0] + add_y, ai2[1] + add_m);
		if(ai2[2] > (INT)i1)
		{
			ai2[2] = (INT)i1;
		}
		ai1 = idate_reYmdhns(ai2[0] + add_y, ai2[1] + add_m, ai2[2], ai2[3], ai2[4], ai2[5]);
		i2 = 0;
		while(i2 < 6)
		{
			ai2[i2] = ai1[i2];
			++i2;
		}
		ifree(ai1);
		++flg;
	}
	if(add_d != 0)
	{
		cjd = idate_ymdhnsToCjd(ai2[0], ai2[1], ai2[2], ai2[3], ai2[4], ai2[5]);
		ai1 = idate_cjdToiAryYmdhns(cjd + add_d);
		i2 = 0;
		while(i2 < 6)
		{
			ai2[i2] = ai1[i2];
			++i2;
		}
		ifree(ai1);
		++flg;
	}
	if(add_h != 0 || add_n != 0 || add_s != 0)
	{
		ai1 = idate_reYmdhns(ai2[0], ai2[1], ai2[2], ai2[3] + add_h, ai2[4] + add_n, ai2[5] + add_s);
		i2 = 0;
		while(i2 < 6)
		{
			ai2[i2] = ai1[i2];
			++i2;
		}
		ifree(ai1);
		++flg;
	}
	if(flg)
	{
		ai1 = icalloc_INT(6);
		i2 = 0;
		while(i2 < 6)
		{
			ai1[i2] = ai2[i2];
			++i2;
		}
	}
	else
	{
		ai1 = idate_reYmdhns(ai2[0], ai2[1], ai2[2], ai2[3], ai2[4], ai2[5]);
	}
	ifree(ai2);
	return ai1;
}
//-------------------------------------------------------
// 日付の差 [8] = {sign, y, m, d, h, n, s, days}
//   (注)便宜上、(日付1<=日付2)に変換して計算するため、
//       結果は以下のとおりとなるので注意。
//       ・5/31⇒6/30 : + 1m
//       ・6/30⇒5/31 : -30d
//-------------------------------------------------------
/* (例)
	INT *ai = idate_diff(2012, 1, 31, 0, 0, 0, 2012, 2, 29, 0, 0, 0); //=> sign=1, y=0, m=1, d=0, h=0, n=0, s=0, days=29
	for(INT i1 = 0; i1 < 7; i1++)
	{
		PL3(ai[i1]); //=> 2012, 2, 29, 0, 0, 0, 29
	}
*/
// v2023-07-13
INT
*idate_diff(
	INT i_y1, // 年1
	INT i_m1, // 月1
	INT i_d1, // 日1
	INT i_h1, // 時1
	INT i_n1, // 分1
	INT i_s1, // 秒1
	INT i_y2, // 年2
	INT i_m2, // 月2
	INT i_d2, // 日2
	INT i_h2, // 時2
	INT i_n2, // 分2
	INT i_s2  // 秒2
)
{
	INT *rtn = icalloc_INT(8);
	INT i1 = 0, i2 = 0, i3 = 0, i4 = 0;
	DOUBLE cjd = 0.0;
	/*
		正規化1
	*/
	DOUBLE cjd1 = idate_ymdhnsToCjd(i_y1, i_m1, i_d1, i_h1, i_n1, i_s1);
	DOUBLE cjd2 = idate_ymdhnsToCjd(i_y2, i_m2, i_d2, i_h2, i_n2, i_s2);
	if(cjd1 > cjd2)
	{
		rtn[0] = -1; // sign(-)
		cjd  = cjd1;
		cjd1 = cjd2;
		cjd2 = cjd;
	}
	else
	{
		rtn[0] = 1; // sign(+)
	}
	/*
		days
	*/
	rtn[7] = (INT)(cjd2 - cjd1);
	/*
		正規化2
	*/
	INT *ai1 = idate_cjdToiAryYmdhns(cjd1);
	INT *ai2 = idate_cjdToiAryYmdhns(cjd2);
	/*
		ymdhns
	*/
	rtn[1] = ai2[0] - ai1[0];
	rtn[2] = ai2[1] - ai1[1];
	rtn[3] = ai2[2] - ai1[2];
	rtn[4] = ai2[3] - ai1[3];
	rtn[5] = ai2[4] - ai1[4];
	rtn[6] = ai2[5] - ai1[5];
	/*
		m 調整
	*/
	INT *ai3 = idate_cnv_month2(rtn[1], rtn[2]);
		rtn[1] = ai3[0];
		rtn[2] = ai3[1];
	ifree(ai3);
	/*
		hns 調整
	*/
	while(rtn[6] < 0)
	{
		rtn[6] += 60;
		rtn[5] -= 1;
	}
	while(rtn[5] < 0)
	{
		rtn[5] += 60;
		rtn[4] -= 1;
	}
	while(rtn[4] < 0)
	{
		rtn[4] += 24;
		rtn[3] -= 1;
	}
	/*
		d 調整
		前処理
	*/
	if(rtn[3] < 0)
	{
		rtn[2] -= 1;
		if(rtn[2] < 0)
		{
			rtn[2] += 12;
			rtn[1] -= 1;
		}
	}
	/*
		本処理
	*/
	if(rtn[0] > 0)
	{
		ai3 = idate_add(ai1[0], ai1[1], ai1[2], ai1[3], ai1[4], ai1[5], rtn[1], rtn[2], 0, 0, 0, 0);
			i1 = (INT)idate_ymdhnsToCjd(ai2[0], ai2[1], ai2[2], 0, 0, 0);
			i2 = (INT)idate_ymdhnsToCjd(ai3[0], ai3[1], ai3[2], 0, 0, 0);
		ifree(ai3);
	}
	else
	{
		ai3 = idate_add(ai2[0], ai2[1], ai2[2], ai2[3], ai2[4], ai2[5], -rtn[1], -rtn[2], 0, 0, 0, 0);
			i1 = (INT)idate_ymdhnsToCjd(ai3[0], ai3[1], ai3[2], 0, 0, 0);
			i2 = (INT)idate_ymdhnsToCjd(ai1[0], ai1[1], ai1[2], 0, 0, 0);
		ifree(ai3);
	}
	i3 = idate_ymd_num(ai1[3], ai1[4], ai1[5]);
	i4 = idate_ymd_num(ai2[3], ai2[4], ai2[5]);
	/* 変換例
		"05-31" "06-30" のとき m = 1, d = 0
		"06-30" "05-31" のとき m = 0, d = -30
		※分岐条件は非常にシビアなので安易に変更するな!!
	*/
	if(rtn[0] > 0                                             // +のときのみ
		&& i3 <= i4                                           // (例) "12:00:00 =< 23:00:00"
		&& idate_chk_month_end(ai2[0], ai2[1], ai2[2], FALSE) // (例) 06-"30" は月末日？
		&& ai1[2] > ai2[2]                                    // (例) 05-"31" > 06-"30"
	)
	{
		rtn[2] += 1;
		rtn[3] = 0;
	}
	else
	{
		rtn[3] = i1 - i2 - (INT)(i3 > i4 ? 1 : 0);
	}
	ifree(ai2);
	ifree(ai1);
	return rtn;
}
//--------------------------
// diff()／add()の動作確認
//--------------------------
/* (例) 西暦1年から2000年迄のサンプル100例について評価
	idate_diff_checker(1, 2000, 100);
*/
// v2023-07-10
/*
VOID
irand_init()
{
	srand((UINT)time(NULL));
}

INT
irand_INT(
	INT min,
	INT max
)
{
	return (min + (INT)(rand() * (max - min + 1.0) / (1.0 + RAND_MAX)));
}

VOID
idate_diff_checker(
	INT from_year, // 何年から
	INT to_year,   // 何年まで
	INT iSample    // サンプル抽出数
)
{
	if(from_year > to_year)
	{
		INT i1 = to_year;
		to_year = from_year;
		from_year = i1;
	}
	INT rnd_y = to_year - from_year;
	INT *ai1 = 0, *ai2 = 0, *ai3 = 0, *ai4 = 0;
	INT y1 = 0, y2 = 0, m1 = 0, m2 = 0, d1 = 0, d2 = 0;
	MS s1[16] = "", s2[16] = "";
	MS *err = 0;
	P2("\033[94m--Cnt----From----------To------------[Sign,    Y,  M,  D]----DateAdd-------Chk----\033[0m");
	irand_init();
	for(INT i1 = 1; i1 <= iSample; i1++)
	{
		y1 = from_year + irand_INT(0, rnd_y);
		y2 = from_year + irand_INT(0, rnd_y);
		m1 = 1 + irand_INT(0, 11);
		m2 = 1 + irand_INT(0, 11);
		d1 = 1 + irand_INT(0, 30);
		d2 = 1 + irand_INT(0, 30);
		// 再計算
		ai1 = idate_reYmdhns(y1, m1, d1, 0, 0, 0);
		ai2 = idate_reYmdhns(y2, m2, d2, 0, 0, 0);
		// diff & add
		ai3 = idate_diff(ai1[0], ai1[1], ai1[2], 0, 0, 0, ai2[0], ai2[1], ai2[2], 0, 0, 0);
		ai4 = (
			ai3[0] > 0 ?
			idate_add(ai1[0], ai1[1], ai1[2], 0, 0, 0, ai3[1], ai3[2], ai3[3], 0, 0, 0) :
			idate_add(ai1[0], ai1[1], ai1[2], 0, 0, 0, -(ai3[1]), -(ai3[2]), -(ai3[3]), 0, 0, 0)
		);
		// 計算結果の照合
		sprintf(s1, "%d%02d%02d", ai2[0], ai2[1], ai2[2]);
		sprintf(s2, "%d%02d%02d", ai4[0], ai4[1], ai4[2]);
		err = (strcmp(s1, s2) ? "\033[91mNG\033[0m" : "\033[93mok\033[0m");
		P(
			"%5d   %5d-%02d-%02d   \033[96m%5d-%02d-%02d\033[0m    [%4d, %4d, %2d, %2d]   \033[96m%5d-%02d-%02d\033[0m    %s\n"
			,
			i1,
			ai1[0], ai1[1], ai1[2], ai2[0], ai2[1], ai2[2],
			ai3[0], ai3[1], ai3[2], ai3[3], ai4[0], ai4[1], ai4[2],
			err
		);
		ifree(ai4);
		ifree(ai3);
		ifree(ai2);
		ifree(ai1);
	}
}
*/
/*
	// Ymdhns
	%a : 曜日(例:Su)
	%A : 曜日数(例:0)
	%c : 年内の通算日
	%C : CJD通算日
	%J : JD通算日
	%e : 年内の通算週

	// Diff
	%Y : 通算年
	%M : 通算月
	%D : 通算日
	%H : 通算時
	%N : 通算分
	%S : 通算秒
	%W : 通算週
	%w : 通算週の余日

	// 共通
	%g : Sign "-" "+"
	%G : Sign "-" のみ
	%y : 年(0000)
	%m : 月(00)
	%d : 日(00)
	%h : 時(00)
	%n : 分(00)
	%s : 秒(00)
	%% : "%"
	\a
	\n
	\t
*/
/* (例) ymdhns
	INT *ai1 = idate_reYmdhns(1970, 12, 10, 0, 0, 0);
	WS *p1 = idate_format_ymdhns(L"%g%y-%m-%d(%a) %c / %C", ai1[0], ai1[1], ai1[2], ai1[3], ai1[4], ai1[5]);
		PL2W(p1); //=> "+1970-12-10(Th) 344 / 2440931.000"
	ifree(p1);
	ifree(ai1);
*/
/* (例) diff
	INT *ai1 = idate_diff(1970, 12, 10, 0, 0, 0, 2021, 4, 18, 0, 0, 0);
	WS *p1 = idate_format_diff(L"%g%y-%m-%d %Wweeks %Ddays %Sseconds", ai1[0], ai1[1], ai1[2], ai1[3], ai1[4], ai1[5], ai1[6], ai1[7]);
		PL2W(p1); //=> "+0050-04-08 2627weeks 18392days 1589068800seconds"
	ifree(p1);
	ifree(ai1);
*/
// v2023-12-22
WS
*idate_format_diff(
	WS *format,  //
	INT i_sign,  // 符号／-1="-", 1="+"
	INT i_y,     // 年
	INT i_m,     // 月
	INT i_d,     // 日
	INT i_h,     // 時
	INT i_n,     // 分
	INT i_s,     // 秒
	INT64 i_days // 通算日／idate_diff()で使用
)
{
	if(! format)
	{
		return icalloc_WS(0);
	}
	CONST UINT BufSizeMax = 1024;
	WS *rtn = icalloc_WS(BufSizeMax);
	WS *pEnd = rtn;
	UINT uPos = 0;
	// Ymdhns で使用
	DOUBLE cjd = (i_days ? 0.0 : idate_ymdhnsToCjd(i_y, i_m, i_d, i_h, i_n, i_s));
	DOUBLE jd = cjd - CJD_TO_JD;
	// 符号チェック
	if(i_y < 0)
	{
		i_sign = -1;
		i_y = -(i_y);
	}
	while(*format)
	{
		if(*format == '%')
		{
			++format;
			switch(*format)
			{
				// Ymdhns
				case 'a': // 曜日(例:Su)
					pEnd += iwn_cpy(pEnd, idate_cjd_Wday(cjd));
					break;
				case 'A': // 曜日数
					pEnd += swprintf(pEnd, BufSizeMax, L"%d", idate_cjd_iWday(cjd));
					break;
				case 'c': // 年内の通算日
					pEnd += swprintf(pEnd, BufSizeMax, L"%d", idate_cjd_yeardays(cjd));
					break;
				case 'C': // CJD通算日
					pEnd += swprintf(pEnd, BufSizeMax, L"%.8f", cjd);
					break;
				case 'J': // JD通算日
					pEnd += swprintf(pEnd, BufSizeMax, L"%.8f", jd);
					break;
				case 'e': // 年内の通算週
					pEnd += swprintf(pEnd, BufSizeMax, L"%d", idate_cjd_yearweeks(cjd));
					break;
				// Diff
				case 'Y': // 通算年
					pEnd += swprintf(pEnd, BufSizeMax, L"%d", i_y);
					break;
				case 'M': // 通算月
					pEnd += swprintf(pEnd, BufSizeMax, L"%d", (i_y * 12) + i_m);
					break;
				case 'D': // 通算日
					pEnd += swprintf(pEnd, BufSizeMax, L"%lld", i_days);
					break;
				case 'H': // 通算時
					pEnd += swprintf(pEnd, BufSizeMax, L"%lld", (i_days * 24) + i_h);
					break;
				case 'N': // 通算分
					pEnd += swprintf(pEnd, BufSizeMax, L"%lld", (i_days * 24 * 60) + (i_h * 60) + i_n);
					break;
				case 'S': // 通算秒
					pEnd += swprintf(pEnd, BufSizeMax, L"%lld", (i_days * 24 * 60 * 60) + (i_h * 60 * 60) + (i_n * 60) + i_s);
					break;
				case 'W': // 通算週
					pEnd += swprintf(pEnd, BufSizeMax, L"%lld", (i_days / 7));
					break;
				case 'w': // 通算週の余日
					pEnd += swprintf(pEnd, BufSizeMax, L"%d", (INT)(i_days % 7));
					break;
				// 共通
				case 'g': // Sign "-" "+"
					*pEnd = (i_sign < 0 ? '-' : '+');
					++pEnd;
					break;
				case 'G': // Sign "-" のみ
					if(i_sign < 0)
					{
						*pEnd = '-';
						++pEnd;
					}
					break;
				case 'y': // 年
					pEnd += swprintf(pEnd, BufSizeMax, L"%04d", i_y);
					break;
				case 'm': // 月
					pEnd += swprintf(pEnd, BufSizeMax, L"%02d", i_m);
					break;
				case 'd': // 日
					pEnd += swprintf(pEnd, BufSizeMax, L"%02d", i_d);
					break;
				case 'h': // 時
					pEnd += swprintf(pEnd, BufSizeMax, L"%02d", i_h);
					break;
				case 'n': // 分
					pEnd += swprintf(pEnd, BufSizeMax, L"%02d", i_n);
					break;
				case 's': // 秒
					pEnd += swprintf(pEnd, BufSizeMax, L"%02d", i_s);
					break;
				case '%':
					*pEnd = '%';
					++pEnd;
					break;
				default:
					--format; // else に処理を振る
					break;
			}
			uPos = pEnd - rtn;
		}
		else if(*format == '\\')
		{
			switch(format[1])
			{
				case('a'):
					*pEnd = '\a';
					break;
				case('n'):
					*pEnd = '\n';
					break;
				case('t'):
					*pEnd = '\t';
					break;
				default:
					*pEnd = format[1];
					break;
			}
			++format;
			++pEnd;
			++uPos;
		}
		else
		{
			*pEnd = *format;
			++pEnd;
			++uPos;
		}
		++format;
		if(BufSizeMax < uPos)
		{
			break;
		}
	}
	return rtn;
}
/* (例)
	PL2W(idate_format_cjdToWS(NULL, idate_nowToCjd_localtime()));
*/
// v2023-07-13
WS
*idate_format_cjdToWS(
	WS *format, // NULL=IDATE_FORMAT_STD
	DOUBLE cjd
)
{
	if(! format)
	{
		format = IDATE_FORMAT_STD;
	}
	INT *ai1 = idate_cjdToiAryYmdhns(cjd);
	WS *rtn = idate_format_ymdhns(format, ai1[0], ai1[1], ai1[2], ai1[3], ai1[4], ai1[5]);
	ifree(ai1);
	return rtn;
}
//---------------------
// CJD を文字列に変換
//---------------------
/*
	大文字 => "yyyy-mm-dd 00:00:00"
	小文字 => "yyyy-mm-dd hh:nn:ss"
		Y, y : 年
		M, m : 月
		W, w : 週
		D, d : 日
		H, h : 時
		N, n : 分
		S, s : 秒
*/
/* (例)
	WS *str = L" 1 = []\n 2 = [%]\n 3 = [*]\n 4 = [-10d]\n 5 = [-10D]\n 6 = [-10d%]\n 7 = [-10D%]\n 8 = [0]\n 9 = [-0]\n10 = [-0d]\n11 = [-0%]";
	LN(80);
	P2W(str);
	INT *ai = idate_nowToiAryYmdhns_localtime();
		WS *p1 = idate_replace_format_ymdhns(
			str,
			L"[", L"]",
			L"'",
			ai[0], ai[1], ai[2], ai[3], ai[4], ai[5]
		);
			LN(80);
			P2W(p1);
			LN(80);
		ifree(p1);
	ifree(ai);
	// 現在時 "2022-09-25 20:30:45" のとき
	//   "[]"      => "2022-09-25 20:30:45"
	//   "[%]"     => "2022-09-25 %"
	//   "[*]"     => "2022-09-25 *"
	//   "[-10d]"  => "2022-09-15 20:30:45"
	//   "[-10D]"  => "2022-09-15 00:00:00"
	//   "[-10d%]" => "2022-09-15 %"
	//   "[-10D%]" => "2022-09-15 %"
	//   "[0]"     => "2022-09-25 20:30:45"
	//   "[-0]"    => "2022-09-25 20:30:45"
	//   "[-0d]"   => "2022-09-25 20:30:45"
	//   "[-0%]"   => "2022-09-25 %"
*/
// v2023-09-03
WS
*idate_replace_format_ymdhns(
	WS *str,       // 変換対象文字列
	WS *quoteBgn,  // 囲文字列 (例) "["
	WS *quoteEnd,  // 囲文字列 (例) "]"
	WS *add_quote, // 出力文字に追加する囲文字列 (例) "'"
	INT i_y,       // ベース年
	INT i_m,       // ベース月
	INT i_d,       // ベース日
	INT i_h,       // ベース時
	INT i_n,       // ベース分
	INT i_s        // ベース秒
)
{
	if(! str || ! *str || ! quoteBgn || ! *quoteBgn || ! quoteEnd || ! *quoteEnd)
	{
		return iws_clone(str);
	}
	if(! add_quote)
	{
		add_quote = L"";
	}
	WS *rtn = 0;
	UINT u1 = iwn_search(str, quoteBgn);
	UINT u2 = iwn_search(str, quoteEnd);
	// quoteBgn or quoteEnd がないとき str のクローンを返す
	if(u1 && u2)
	{
		rtn = icalloc_WS(wcslen(str) + (u1 * 20));
	}
	else
	{
		return iws_clone(str);
	}
	WS *pRtnE = rtn;
	// add_quoteの除外文字
	WS *pAddQuote = (
		(*add_quote >= '0' && *add_quote <= '9') || *add_quote == '+' || *add_quote == '-' ?
		L"" :
		add_quote
	);
	UINT uQuoteBgn = wcslen(quoteBgn);
	UINT uQuoteEnd = wcslen(quoteEnd);
	WS *pStrBgn = str;
	WS *pStrEnd = str;
	while(*pStrEnd)
	{
		// "[" を探す
		//   str = "[[", quoteBgn = "[" を想定しておく
		if(*quoteBgn && iwb_cmpf(pStrEnd, quoteBgn) && ! iwb_cmpf((pStrEnd + uQuoteBgn), quoteBgn))
		{
			pStrEnd += uQuoteBgn;
			pStrBgn = pStrEnd;
			// "]" を探す
			if(*quoteEnd)
			{
				for(WS *_p1 = pStrBgn; _p1; _p1++)
				{
					if(iwb_cmpf(_p1, quoteEnd))
					{
						pStrEnd = _p1;
						break;
					}
				}
				// 解析用文字列
				WS *pDt = iws_pclone(pStrBgn, pStrEnd);
				WS *pDtEnd = pDt;
				// 差異取得
				INT iDt = _wtoi(pDtEnd);
				if(! iDt)
				{
					// "+0", "-0", "0" をスルー
					while(*pDtEnd == '0' || *pDtEnd == '-' || *pDtEnd == '+')
					{
						++pDtEnd;
					}
					switch(*pDtEnd)
					{
						case('%'):
							pDtEnd = L"y%";
							break;
						case('*'):
							pDtEnd = L"y*";
							break;
						default:
							pDtEnd = L"y";
							break;
					}
				}
				INT add_y = 0, add_m = 0, add_d = 0, add_h = 0, add_n = 0, add_s = 0;
				BOOL bHnsZero = FALSE; // TRUE = "00:00:00"
				BOOL bAdd     = FALSE; // TRUE = "Y".."s"
				BOOL bAddEnd  = FALSE; // TRUE = bAdd が現れたとき
				while(*pDtEnd)
				{
					switch(*pDtEnd)
					{
						// 年
						case 'Y': // "yyyy-mm-dd 00:00:00"
							bHnsZero = TRUE;
							[[fallthrough]];
						case 'y': // "yyyy-mm-dd hh:nn:ss"
							add_y = iDt;
							bAddEnd = TRUE;
							bAdd = TRUE;
							break;
						// 月
						case 'M': // "yyyy-mm-dd 00:00:00"
							bHnsZero = TRUE;
							[[fallthrough]];
						case 'm': // "yyyy-mm-dd hh:nn:ss"
							add_m = iDt;
							bAddEnd = TRUE;
							bAdd = TRUE;
							break;
						// 週
						case 'W': // "yyyy-mm-dd 00:00:00"
							bHnsZero = TRUE;
							[[fallthrough]];
						case 'w': // "yyyy-mm-dd hh:nn:ss"
							add_d = iDt * 7;
							bAddEnd = TRUE;
							bAdd = TRUE;
							break;
						// 日
						case 'D': // "yyyy-mm-dd 00:00:00"
							bHnsZero = TRUE;
							[[fallthrough]];
						case 'd': // "yyyy-mm-dd hh:nn:ss"
							add_d = iDt;
							bAddEnd = TRUE;
							bAdd = TRUE;
							break;
						// 時
						case 'H': // "yyyy-mm-dd 00:00:00"
							bHnsZero = TRUE;
							[[fallthrough]];
						case 'h': // "yyyy-mm-dd hh:nn:ss"
							add_h = iDt;
							bAddEnd = TRUE;
							bAdd = TRUE;
							break;
						// 分
						case 'N': // "yyyy-mm-dd 00:00:00"
							bHnsZero = TRUE;
							[[fallthrough]];
						case 'n': // "yyyy-mm-dd hh:nn:ss"
							add_n = iDt;
							bAddEnd = TRUE;
							bAdd = TRUE;
							break;
						// 秒
						case 'S': // "yyyy-mm-dd 00:00:00"
							bHnsZero = TRUE;
							[[fallthrough]];
						case 's': // "yyyy-mm-dd hh:nn:ss"
							add_s = iDt;
							bAddEnd = TRUE;
							bAdd = TRUE;
							break;
					}
					// [1y6m] のようなとき [1y] で解釈する
					if(bAddEnd)
					{
						break;
					}
					++pDtEnd;
				}
				UINT uCntPercent = 0; // str 中の "%" 個数
				UINT uCntStar    = 0; // str 中の "*" 個数
				INT *ai = 0;
					if(bAdd)
					{
						// "Y".."s" 末尾の "%", "*" を検索
						while(*pDtEnd)
						{
							switch(*pDtEnd)
							{
								case '%':
									++uCntPercent;
									break;
								case '*':
									++uCntStar;
									break;
							}
							++pDtEnd;
						}
						// ±日時
						ai = idate_add(i_y, i_m, i_d, i_h, i_n, i_s, add_y, add_m, add_d, add_h, add_n, add_s);
						// "00:00:00"
						if(bHnsZero)
						{
							ai[3] = ai[4] = ai[5] = 0;
						}
					}
					else
					{
						ai = icalloc_INT(6);
					}
					WS *p1 = idate_format_ymdhns(IDATE_FORMAT_STD, ai[0], ai[1], ai[2], ai[3], ai[4], ai[5]);
						WS *p2 = 0;
						// "yyyy-mm-dd hh:nn:ss" => "yyyy-mm-dd %", "yyyy-mm-dd *"
						if(uCntPercent || uCntStar)
						{
							p2 = p1;
							while(*p2 && *p2 != ' ')
							{
								++p2;
							}
							if(uCntPercent)
							{
								*(++p2) = '%';
							}
							else if(uCntStar)
							{
								*(++p2) = '*';
							}
							*(++p2) = 0;
						}
						pRtnE += iwn_cpy(pRtnE, pAddQuote);
						pRtnE += iwn_cpy(pRtnE, p1);
						pRtnE += iwn_cpy(pRtnE, pAddQuote);
					ifree(p1);
				ifree(ai);
				ifree(pDt);
			}
			pStrEnd += uQuoteEnd;
			pStrBgn = pStrEnd;
		}
		else
		{
			*pRtnE++ = *pStrEnd++;
		}
	}
	*pRtnE = 0;
	return rtn;
}
//---------------------
// 今日のymdhnsを返す
//---------------------
/* (例)
	// 今日 = 2012-06-19 00:00:00 のとき、
	INT *ai1 = idate_nowToiAryYmdhns(0); // System(-9h) => 2012, 6, 18, 15, 0, 0
	INT *ai2 = idate_nowToiAryYmdhns(1); // Local       => 2012, 6, 19, 0, 0, 0
		P("%d, %d, %d, %d, %d, %d, %d\n", ai1[0], ai1[1], ai1[2], ai1[3], ai1[4], ai1[5], ai1[6]);
		P("%d, %d, %d, %d, %d, %d, %d\n", ai2[0], ai2[1], ai2[2], ai2[3], ai2[4], ai2[5], ai2[6]);
	ifree(ai2);
	ifree(ai1);
*/
// v2023-07-13
INT
*idate_nowToiAryYmdhns(
	BOOL area // TRUE=LOCAL／FALSE=SYSTEM
)
{
	SYSTEMTIME st;
	if(area)
	{
		GetLocalTime(&st);
	}
	else
	{
		GetSystemTime(&st);
	}
	/* [Pending] 2021-11-15
		下記コードでビープ音を発生することがある。
			INT *rtn = icalloc_INT(n) ※INT系全般／DOUBL系は問題なし
			rtn[n] = 1793..2047
		2021-11-10にコンパイラを MInGW(32bit) から Mingw-w64(64bit) に変更した影響か？
		当面、様子を見る。
	*/
	INT *rtn = icalloc_INT(7);
		rtn[0] = st.wYear;
		rtn[1] = st.wMonth;
		rtn[2] = st.wDay;
		rtn[3] = st.wHour;
		rtn[4] = st.wMinute;
		rtn[5] = st.wSecond;
		rtn[6] = st.wMilliseconds;
	return rtn;
}
//------------------
// 今日のcjdを返す
//------------------
/* (例)
	idate_nowToCjd(0); // System(-9h)
	idate_nowToCjd(1); // Local
*/
// v2023-07-13
DOUBLE
idate_nowToCjd(
	BOOL area // TRUE=LOCAL／FALSE=SYSTEM
)
{
	INT *ai = idate_nowToiAryYmdhns(area);
	INT y = ai[0], m = ai[1], d = ai[2], h = ai[3], n = ai[4], s = ai[5];
	ifree(ai);
	return idate_ymdhnsToCjd(y, m, d, h, n, s);
}
