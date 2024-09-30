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

[2016-08-19] + [2024-09-09]
	-------------------------------
	| lib_iwmutil2 で規定する表記
	-------------------------------
	・大域変数の "１文字目は $"
		$ARGV, $icallocMap など
	・#define定数（関数は含まない）は "すべて大文字"
		IMAX_PATH, WS など

	--------------------------
	| ソース別に期待する表記
	--------------------------
	・大域変数, #define定数（関数は含まない）の "１文字目は大文字"
		Buf, BufSIze など
	・オプション変数の "１文字目は _" かつ "２文字目は大文字"
		_NL, _Format など
	・局所変数の "１文字目は _" かつ "２文字目は小文字"
		_i1, _u1, _mp1, _wp1 など

[2016-01-27] + [2024-09-09]
	基本関数名ルール
		i = iwm-iwama によって書かれた
		m = MS(1byte) | u = MS(UTF-8) | w = WS(UTF-16) | v = VOID
		a = string[]  | b = bool      | n = length
		p = pointer   | s = string
			"p" は "引数自身の"、"s" は "引数とは別の" ポインタを返す。

[2021-11-18] + [2022-09-23]
	ポインタ *(p + n) と、配列 p[n] どちらが速い？
		Mingw-w64においては最適化するとどちらも同じになる。
		今後は可読性を考慮した配列 p[n] でコーディングする。

[2024-01-19]
	主に使うデータ型
		BOOL   = TRUE / FALSE
		INT    = 32bit integer
		UINT   = 32bit unsigned integer (例：NTFSファイル数)
		INT64  = 64bit integer (例：年月日計算)
		UINT64 = 64bit unsigned integer (例：size_t)
		DOUBLE = double (例：年月日通算日)
		MS     = char
		WS     = wchar
		VOID   = void
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
	idebug_map(); ifree_all(); idebug_map();
	// 最終処理
	imain_end();
*/
WS     *$CMD         = (WS*)L""; // コマンド名を格納
WS     *$ARG         = 0;        // 引数からコマンド名を消去したもの
UINT   $ARGC         = 0;        // 引数配列数
WS     **$ARGV       = 0;        // 引数配列／ダブルクォーテーションを消去したもの
HANDLE $StdoutHandle = 0;        // 画面制御用ハンドル
UINT64 $ExecSecBgn   = 0;        // 実行開始時間
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
// v2024-05-25
VOID
iCLI_begin()
{
	// [Ctrl]+[C]
	signal(SIGINT, (__p_sig_fn_t)iCLI_signal);

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
	WS *wp1 = 0;
	BOOL bArgc = FALSE;

	// TrimR
	for(pEnd = (str + wcslen(str) - 1); *pEnd == ' '; pEnd--)
	{
		*pEnd = 0;
	}

	for(pEnd = str; pEnd < (str + wcslen(str)); pEnd++)
	{
		for(; *pEnd == ' '; pEnd++);

		if(*pEnd == '\"')
		{
			pBgn = pEnd;
			++pEnd;
			for(; *pEnd && *pEnd != '\"'; pEnd++);
			++pEnd;
			wp1 = iws_pclone(pBgn, pEnd);
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
			wp1 = iws_pclone(pBgn, pEnd);
		}

		if(bArgc)
		{
			uArgSize += wcslen(wp1) + 2;
			$ARG = irealloc_WS($ARG, uArgSize);
			UINT u1 = wcslen($ARG);
			if(u1)
			{
				iwv_cpy(($ARG + u1), L" ");
				++u1;
			}
			iwv_cpy(($ARG + u1), wp1);

			$ARGV = irealloc_WS_ary($ARGV, ($ARGC + 1));
			$ARGV[$ARGC] = iws_replace(wp1, L"\"", L"", FALSE);
			++$ARGC;

			ifree(wp1);
		}
		else
		{
			$CMD = wp1;
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
	UINT argc,      // $ARGV[argc]
	CONST WS *opt1, // (例) "-w=", NULL
	CONST WS *opt2  // (例) "-where=", NULL
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
	UINT argc,      // $ARGV[argc]
	CONST WS *opt1, // (例) "-r", NULL
	CONST WS *opt2  // (例) "-repeat", NULL
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
// v2024-03-10
VOID
iCLI_VarList()
{
	P1("\033[97m");
	P1("\033[44m $CMD \033[49m\n" "    ");
	P2W($CMD);
	P1("\033[44m $ARG \033[49m\n" "    ");
	P2W($ARG);
	P("\033[44m $ARGC \033[49m\n" "    [%d]\n", $ARGC);
	P2("\033[44m $ARGV \033[49m");
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
UINT $icallocMapSize = 0;            // *$icallocMap のサイズ [0..n]
UINT $icallocMapEOD = 0;             // *$icallocMap の末尾 [0..(n+1)]
UINT $icallocMapId = 0;              // *$icallocMap の順番 [1..n]
CONST UINT $sizeof_icallocMap = sizeof($struct_icallocMap);

// $icallocMap の基本サイズ（８以上）
//   irealloc() の可能性が低くなる16を基本値としている
#define   IcallocDiv          16

// ダブルヌル追加
#define   MemX(n, sizeOf)     (UINT64)((n + 2) * sizeOf)

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
// v2024-02-25
VOID
*icalloc(
	UINT64 n,      // 個数
	UINT64 sizeOf, // sizeof
	BOOL aryOn     // TRUE=配列
)
{
	if($icallocMapEOD >= $icallocMapSize)
	{
		// $icallocMap を新規作成
		if(! $icallocMapSize)
		{
			$icallocMapSize = IcallocDiv;
			$icallocMap = ($struct_icallocMap*)calloc($icallocMapSize, $sizeof_icallocMap);
			icalloc_err($icallocMap);
			$icallocMapId = 0;
			$icallocMapEOD = 0;
		}
		else
		{
			// 掃除
			icalloc_mapSweep();
			// 掃除後も余白がないならば realloc()
			if($icallocMapEOD >= $icallocMapSize)
			{
				$icallocMapSize += IcallocDiv;
				$icallocMap = ($struct_icallocMap*)realloc($icallocMap, ($icallocMapSize * $sizeof_icallocMap));
				icalloc_err($icallocMap);
			}
		}
	}
	// 引数にポインタ割当
	UINT64 uAlloc = MemX(n, sizeOf);
	VOID *rtn = calloc(uAlloc, 1);
	icalloc_err(rtn);
	$struct_icallocMap *map1 = $icallocMap + $icallocMapEOD;
	map1->ptr = rtn;
	map1->uAry = (aryOn ? n : 0);
	map1->uAlloc = uAlloc;
	map1->uSizeOf = sizeOf;
	// 順番
	map1->id = $icallocMapId;
	++$icallocMapId;
	++$icallocMapEOD;
	return rtn;
}
//----------
// realloc
//----------
// v2024-02-25
VOID
*irealloc(
	VOID *ptr,    // icalloc()ポインタ
	UINT64 n,     // 個数
	UINT64 sizeOf // sizeof
)
{
	// 該当なしのときは ptr を返す
	VOID *rtn = ptr;
	UINT64 uAlloc = MemX(n, sizeOf);
	// $icallocMap を更新
	UINT u1 = 0;
	while(u1 < $icallocMapEOD)
	{
		$struct_icallocMap *map1 = ($icallocMap + u1);
		if(ptr == (map1->ptr))
		{
			if(map1->uAry)
			{
				map1->uAry = n;
			}
			if(map1->uAlloc < uAlloc)
			{
				rtn = (VOID*)calloc(uAlloc, 1);
				icalloc_err(rtn);
				memcpy(rtn, ptr, map1->uAlloc);
				memset(ptr, 0, map1->uAlloc);
				free(ptr);
				ptr = 0;
				map1->ptr = rtn;
				map1->uAlloc = uAlloc;
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
// v2024-02-24
VOID
icalloc_free(
	VOID *ptr
)
{
	// 削除される可能性のあるデータ＝最近作成されたデータは後方に集中するため末尾から走査
	// NULLが排除されているのでチェックなしで走査可能
	INT i1 = $icallocMapEOD - 1;
	while(i1 >= 0)
	{
		$struct_icallocMap *map1 = ($icallocMap + i1);
		if(ptr == (map1->ptr))
		{
			// 配列のとき
			if(map1->uAry)
			{
				// 子ポインタのリンク先を解放
				VOID **va1 = (VOID**)(map1->ptr);
				// (i1 + 1)以降を総当たりで走査／「子」は必ずしもアドレス順でないことに留意
				for(UINT _u1 = 0; _u1 < (map1->uAry); _u1++)
				{
					for(UINT _u2 = i1 + 1; _u2 < $icallocMapEOD; _u2++)
					{
						$struct_icallocMap *map2 = ($icallocMap + _u2);
						if(va1[_u1] && va1[_u1] == (map2->ptr))
						{
							memset(map2->ptr, 0, map2->uAlloc);
							free(map2->ptr);
							map2->ptr = 0;
							map2->uAry = 0;
							map2->uAlloc = 0;
							map2->uSizeOf = 0;
							break;
						}
					}
				}
			}
			// ポインタのリンク先を解放
			memset(map1->ptr, 0, map1->uAlloc);
			free(map1->ptr);
			map1->ptr = 0;
			map1->uAry = 0;
			map1->uAlloc = 0;
			map1->uSizeOf = 0;
			return;
		}
		--i1;
	}
}
//--------------------
// $icallocMapをfree
//--------------------
// v2024-02-23
VOID
icalloc_freeAll()
{
	if(! $icallocMapSize)
	{
		return;
	}
	for(UINT _u1 = 0; _u1 < $icallocMapSize; _u1++)
	{
		$struct_icallocMap *map1 = ($icallocMap + _u1);
		if(map1->ptr)
		{
			memset(map1->ptr, 0, map1->uAlloc);
			free(map1->ptr);
		}
	}
	memset($icallocMap, 0, ($icallocMapSize * $sizeof_icallocMap));
	free($icallocMap);
	$icallocMapEOD = 0;
	$icallocMapSize = 0;
}
//--------------------
// $icallocMapを掃除
//--------------------
// v2024-02-24
VOID
icalloc_mapSweep()
{
	UINT uTo = 0, uFrom = 0;
	while(uTo < $icallocMapSize)
	{
		$struct_icallocMap *map1 = ($icallocMap + uTo);
		// 隙間を詰める
		if(! (map1->ptr))
		{
			if(uTo >= uFrom)
			{
				uFrom = uTo + 1;
			}
			while(uFrom < $icallocMapSize)
			{
				$struct_icallocMap *map2 = ($icallocMap + uFrom);
				if((map2->ptr))
				{
					// 構造体コピー
					*map1 = *map2;
					map2->ptr = 0;
					map2->uAry = 0;
					map2->uAlloc = 0;
					map2->uSizeOf = 0;
					break;
				}
				++uFrom;
			}
			if(uFrom >= $icallocMapSize)
			{
				$icallocMapEOD = uTo;
				return;
			}
		}
		++uTo;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	Debug
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
//--------------------------
// $icallocMapをリスト出力
//--------------------------
// v2024-09-15
VOID
idebug_printMap()
{
	iConsole_EscOn();
	P2(
		"\033[94m"
		"- count - id ---- pointer -------- array - sizeof -------------- alloc ---------"
	);
	UINT uAllocUsed = 0;
	UINT u1 = 0, u2 = 0;
	while(u1 < $icallocMapSize)
	{
		$struct_icallocMap *map = ($icallocMap + u1);
		uAllocUsed += (map->uAlloc);
		if((map->uAry))
		{
			P1("\033[37;44m");
		}
		else if((map->ptr))
		{
			P1("\033[97m");
		}
		else
		{
			P1("\033[31m");
		}
		++u2;
		P(
			"  %-7u %-7u %p %5u %8u %20u ",
			u2,
			(map->id),
			(map->ptr),
			(map->uAry),
			(map->uSizeOf),
			(map->uAlloc)
		);
		if((map->ptr))
		{
			P1("\033[32m");
			if((map->uSizeOf) == 2)
			{
				P1W((WS*)map->ptr);
			}
			else
			{
				P1((MS*)map->ptr);
			}
		}
		P2("\033[0m");
		++u1;
	}
	P(
		"\033[94m"
		"------------------------------------------------- %20lld byte ----"
		"\033[0m\n\n"
		,
		uAllocUsed
	);
}
//-----------------------------------
// ポインタのアドレス／文字列を出力
//-----------------------------------
/* (例)
	MS **ma1 = icalloc_MS_ary(4);
		// ma1[2]を故意に放置し断片化させる
		ma1[0] = ims_clone("AAA");
		ma1[3] = ims_clone("DDD");
		ma1[1] = ims_clone("BBB");

		idebug_map();
		idebug_pointer(ma1);
		idebug_pointer(*(ma1 + 0));
		idebug_pointer(*(ma1 + 1));
		idebug_pointer(*(ma1 + 2));
		idebug_pointer(*(ma1 + 3));
		NL();
	ifree(ma1);

	// 再利用されているポインタは'(null)'になっていないことに留意
	idebug_map();
	idebug_pointer(ma1);
	idebug_pointer(*(ma1 + 0));
	idebug_pointer(*(ma1 + 1));
	idebug_pointer(*(ma1 + 2));
	idebug_pointer(*(ma1 + 3));
	NL();
*/
// v2024-05-07
VOID
idebug_printPointer0(
	CONST VOID *ptr,
	UINT sizeOf      // sizeof(MS), sizeof(WS)
)
{
	// 64bit有効アドレス空間 = 1<<44(16TB)未満??
	// 当環境では'43'以上を無効範囲としている
	if(((UINT64)ptr >> 43))
	{
		P("%p    * Invalid pointer", ptr);
		return;
	}
	if(! ptr)
	{
		P("%p    * (null)", ptr);
		return;
	}
	MS *pEnd = (MS*)ptr;
	for(; *pEnd; (pEnd += sizeOf));
	UINT64 uLen = pEnd - (MS*)ptr;
	// WS
	if(sizeOf == 2)
	{
		P("%p %4u '", ptr, (uLen << 1));
		P1W((WS*)ptr);
		P1("'");
	}
	// MS
	else
	{
		P("%p %4u '%s'", ptr, uLen, (MS*)ptr);
	}
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
	CONST MS *format,
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
	CONST MS *str,
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
	CONST WS *str
)
{
	MS *p1 = W2M(str);
		fputs(p1, stdout);
	ifree(p1);
}
// v2024-04-16
VOID
PR1(
	CONST MS *str,
	UINT uRepeat
)
{
	MS *rtn = ims_repeat(str, uRepeat);
		P1(rtn);
	ifree(rtn);
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
// コマンド実行／VOID
//---------------------
/* (例)
	imv_systemW(L"dir");
*/
// v2024-03-10
VOID
imv_systemW(
	WS *cmd
)
{
	WS *wpCmd = iws_cats(2, L"cmd.exe /c ", cmd);
		STARTUPINFOW si;
			ZeroMemory(&si, sizeof(si));
			si.cb = sizeof(si);
		PROCESS_INFORMATION pi;
			ZeroMemory(&pi, sizeof(pi));
			if(CreateProcessW(NULL, wpCmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
			{
				WaitForSingleObject(pi.hProcess, INFINITE);
			}
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	ifree(wpCmd);
}
//--------------------
// コマンド実行／MS*
//--------------------
/* (例)
	MS *mp1 = ims_popenW(L"dir");
		P1(mp1);
	ifree(mp1);
*/
// v2024-03-02
MS
*ims_popenW(
	WS *cmd
)
{
	UINT uSize = 2048;
	MS *rtn = icalloc_MS(uSize);
		UINT uPos = 0;
		// STDERR 排除
		WS *wp1 = iws_cats(2, cmd, L" 2>NUL");
			FILE *fp = _wpopen(wp1, L"rb");
				INT c = 0;
				while((c = fgetc(fp)) != EOF)
				{
					rtn[uPos] = c;
					++uPos;
					if(uPos >= uSize)
					{
						uSize <<= 1;
						rtn = irealloc_MS(rtn, uSize);
					}
				}
				rtn[uPos] = 0;
			pclose(fp);
		ifree(wp1);
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
	DWORD consoleMode = 0;
	GetConsoleMode($StdoutHandle, &consoleMode);
	consoleMode = (consoleMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
	SetConsoleMode($StdoutHandle, consoleMode);
}
//-----------------
// STDIN から読込
//-----------------
/* 例
	P2("何か入力してください。\n[Ctrl]+[D]＋[Enter] で終了");
	WS *wp1 = iCLI_GetStdin(TRUE);
		P2W(wp1);
		P("\n> %lld 文字（'\\n'含む）\n", wcslen(wp1));
	ifree(wp1);
*/
// v2024-05-24
WS
*iCLI_GetStdin(
	BOOL bInputKey // TRUE=手入力モードへ移行
)
{
	INT iStdin = fseeko64(stdin, 0, SEEK_END);

	if(iStdin != 0 && ! bInputKey)
	{
		return icalloc_WS(1);
	}

	WS *rtn = 0;

	// STDIN から読込
	if(iStdin == 0)
	{
		UINT mp1Size = 4096;
		UINT mp1End = 0;
		MS *mp1 = icalloc_MS(mp1Size);
			INT c = 0;
			while((c = getchar()) != EOF)
			{
				mp1[mp1End] = (MS)c;
				++mp1End;
				if(mp1End >= mp1Size)
				{
					mp1Size <<= 1;
					mp1 = irealloc_MS(mp1, mp1Size);
				}
			}
			rtn = icnv_M2W(mp1, imn_Codepage(mp1));
		ifree(mp1);
	}
	// 手入力
	// getchar() では日本語が欠落するので ReadConsole() を使用
	else
	{
		$struct_iVBW *iVBW = iVBW_alloc();
			CONST DWORD BufLen = 1;
			WS *Buf = icalloc_WS(BufLen);
			DWORD RCW_len;
				while(TRUE)
				{
					ReadConsoleW(
						GetStdHandle(STD_INPUT_HANDLE),
						Buf,
						BufLen,
						&RCW_len,
						NULL
					);
					// 終了時 [Ctrl]+[D] or [Ctrl]+[Z]
					if(Buf[0] == 4 || Buf[0] == 26)
					{
						break;
					}
					iVBW_add(iVBW, Buf);
				}
			ifree(Buf);
			rtn = iws_clone(iVBW_getStr(iVBW));
		iVBW_free(iVBW);
	}
	// "\r\n" を "\n" に変換
	WS *wp1 = rtn;
		rtn = iws_replace(wp1, L"\r\n", L"\n", FALSE);
	ifree(wp1);

	// 重要
	SetConsoleOutputCP(65001);
	iConsole_EscOn();

	return rtn;
}
//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	Clipboard
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
/* (例)
	iClipboard_setText(L"あいうえお");
*/
// v2024-05-07
VOID
iClipboard_setText(
	CONST WS *str
)
{
	UINT uLen = iwn_len(str);
	if(! uLen)
	{
		return;
	}
	HGLOBAL hg = GlobalAlloc(GMEM_DDESHARE | GMEM_MOVEABLE, ((uLen + 1) * sizeof(WS)));
	if(hg)
	{
		wcscpy((WS*)GlobalLock(hg), str);
		GlobalUnlock(hg);
		if(OpenClipboard(NULL))
		{
			EmptyClipboard();
			SetClipboardData(CF_UNICODETEXT, hg);
			CloseClipboard();
		}
	}
}
/* (例)
	PL2W(iClipboard_getText());
*/
// v2024-05-07
WS
*iClipboard_getText()
{
	if(! OpenClipboard(NULL))
	{
		return icalloc_WS(0);
	}
	HANDLE hg = GetClipboardData(CF_UNICODETEXT);
	if(! hg)
	{
		return icalloc_WS(0);
	}
	WS *rtn = iws_clone((WS*)GlobalLock(hg));
		GlobalUnlock(hg);
		CloseClipboard();
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
	CONST WS *str,
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
	CONST MS *str,
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
	CONST MS *str
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
	CONST WS *str
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
	CONST MS *str
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
// v2023-12-14
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
//---------------
// 文字列コピー
//---------------
/* (例)
	WS *from = L"あいうえお";
	WS *to = icalloc_WS(iwn_len(from));
		iwv_cpy(to, from);
		PL2W(to); //=> "あいうえお"
	ifree(to);
*/
// v2024-03-23
VOID
imv_cpy(
	MS *to,
	CONST MS *from
)
{
	if(! from)
	{
		return;
	}
	// GNU glibc とは相違
	while((*to++ = *from++) != 0);
}
// v2024-03-23
VOID
iwv_cpy(
	WS *to,
	CONST WS *from
)
{
	if(! from)
	{
		return;
	}
	// GNU glibc とは相違
	while((*to++ = *from++) != 0);
}
//-------------------------
// コピーした文字長を返す
//-------------------------
/* (例)
	WS *to = icalloc_WS(IMAX_PATH);
		UINT u1 = iwn_cpy(to, L"c:\\windows");
			PL3(u1);  //=> 10
			PL2W(to); //=> "c:\windows"
		u1 += iwn_cpy((to + u1), L"\\");
			PL3(u1);  //=> 11
			PL2W(to); //=> "c:\windows\"
	ifree(to);
*/
// v2024-03-23
UINT
imn_cpy(
	MS *to,
	CONST MS *from
)
{
	UINT rtn = 0;
	if(from)
	{
		// GNU glibc とは相違
		while((*to++ = *from++) != 0)
		{
			++rtn;
		}
	}
	return rtn;
}
// v2024-03-23
UINT
iwn_cpy(
	WS *to,
	CONST WS *from
)
{
	UINT rtn = 0;
	if(from)
	{
		// GNU glibc とは相違
		while((*to++ = *from++) != 0)
		{
			++rtn;
		}
	}
	return rtn;
}
/* (例)
	WS *p1 = iws_clone(L"123456789");
	WS *p2 = iws_clone(L"AB\0CDE");
	PL3(iwn_pcpy(p1, p2, (p2 + 5))); //=> 2
	PL2W(p1);                        //=> "AB"
*/
// v2024-03-02
UINT
ivn_pcpy(
	VOID *to,
	CONST VOID *from1,
	CONST VOID *from2,
	UINT sizeOf // sizeof(MS), sizeof(WS)
)
{
	if(! from1 || ! from2 || from1 >= from2)
	{
		return 0;
	}
	UINT rtn = (MS*)from2 - (MS*)from1;
	memcpy(to, from1, rtn);
	memset(((MS*)to + rtn), 0, sizeOf);
	return (rtn / sizeOf);
}
//-----------------------
// 新規部分文字列を生成
//-----------------------
/* (例)
	PL2W(iws_clone(L"あいうえお")); //=> "あいうえお"
*/
// v2024-02-27
MS
*ims_clone(
	CONST MS *from
)
{
	UINT64 uLen = imn_len(from);
	MS *rtn = icalloc_MS(uLen);
	if(uLen)
	{
		memcpy(rtn, from, (uLen + 1));
	}
	return rtn;
}
// v2024-03-23
WS
*iws_clone(
	CONST WS *from
)
{
	UINT64 uLen = iwn_len(from);
	WS *rtn = icalloc_WS(uLen);
	if(uLen)
	{
		wmemcpy(rtn, from, (uLen + 1));
	}
	return rtn;
}
/* (例)
	WS *from = L"あいうえお";
	WS *p1 = iws_pclone(from, (from + 3));
		PL2W(p1); //=> "あいう"
	ifree(p1);
*/
// v2024-02-25
VOID
*ivs_pclone(
	CONST VOID *from1,
	CONST VOID *from2,
	INT sizeOf         // sizeof(MS), sizeof(WS)
)
{
	if(! from1 || ! from2 || from1 >= from2)
	{
		return icalloc(0, sizeOf, FALSE);
	}
	UINT64 uLen = (MS*)from2 - (MS*)from1;
	VOID *rtn = icalloc((uLen / sizeOf), sizeOf, FALSE);
	memcpy(rtn, from1, uLen);
	memset(((MS*)rtn + uLen), 0, sizeOf);
	return rtn;
}
/* (例)
	WS *wp1 = iws_cats(5, L"123", NULL, L"abcde", L"", L"あいうえお");
		PL2W(wp1); //=> "123abcdeあいうえお"
	ifree(wp1);
*/
// v2024-03-23
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
				UINT uLen = strlen(_mp1);
				rtnSize += uLen;
				rtn = irealloc_MS(rtn, rtnSize);
				imv_cpy((rtn + pEnd), _mp1);
				pEnd += uLen;
			}
		}
	va_end(va);
	return rtn;
}
// v2024-03-23
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
				UINT uLen = wcslen(_wp1);
				rtnSize += uLen;
				rtn = irealloc_WS(rtn, rtnSize);
				iwv_cpy((rtn + pEnd), _wp1);
				pEnd += uLen;
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
// v2024-02-26
MS
*ims_sprintf(
	CONST MS *format,
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
	WS *wp1 = iws_sprintf(L"ワイド文字：%S", L"あいうえお");
		P2W(wp1);
	ifree(wp1);
*/
// v2024-02-26
WS
*iws_sprintf(
	CONST WS *format,
	...
)
{
	FILE *oFp = fopen("NUL", "wb");
		va_list va;
		va_start(va, format);
			INT len = vfwprintf(oFp, format, va) + 1; // '\0' 追加
			WS *rtn = icalloc_WS(len);
			vswprintf(rtn, len, format, va);
		va_end(va);
	fclose(oFp);
	return rtn;
}
/* (例)
	// PR1("あいうえお", 3); NL();
	MS *mp1 = ims_repeat("あいうえお", 3);
		P2(mp1); //=> "あいうえおあいうえおあいうえお"
	ifree(mp1);
*/
// v2024-04-16
MS
*ims_repeat(
	CONST MS *str,
	UINT uRepeat
)
{
	UINT uLen = imn_len(str);
	UINT uSize = uLen * uRepeat;
	MS *rtn = icalloc_MS(uSize);
	for(UINT _u1 = 0; _u1 < uSize; _u1 += uLen)
	{
		strcpy((rtn + _u1), str);
	}
	return rtn;
}
/* (例)
	PL3(iwb_cmp(L"", L"abc", FALSE, FALSE));  //=> FALSE
	PL3(iwb_cmp(L"abc", L"", FALSE, FALSE));  //=> FALSE
	PL3(iwb_cmp(L"", L"", FALSE, FALSE));     //=> TRUE
	PL3(iwb_cmp(NULL, L"abc", FALSE, FALSE)); //=> FALSE
	PL3(iwb_cmp(L"abc", NULL, FALSE, FALSE)); //=> FALSE
	PL3(iwb_cmp(NULL, NULL, FALSE, FALSE));   //=> FALSE
	PL3(iwb_cmp(NULL, L"", FALSE, FALSE));    //=> FALSE
	PL3(iwb_cmp(L"", NULL, FALSE, FALSE));    //=> FALSE
	NL();

	PL3(iwb_cmpf(L"abc", L"AB"));             //=> FALSE
	PL3(iwb_cmpfi(L"abc", L"AB"));            //=> TRUE
	PL3(iwb_cmpp(L"abc", L"AB"));             //=> FALSE
	PL3(iwb_cmppi(L"abc", L"AB"));            //=> FALSE
	NL();

	// searchに１文字でも合致すればTRUEを返す
	PL3(iwb_cmp_leqfi(L""   , L".."));        //=> TRUE
	PL3(iwb_cmp_leqfi(L"."  , L".."));        //=> TRUE
	PL3(iwb_cmp_leqfi(L".." , L".."));        //=> TRUE
	PL3(iwb_cmp_leqfi(L"...", L".."));        //=> FALSE
	PL3(iwb_cmp_leqfi(L"...", L""  ));        //=> FALSE
	NL();
*/
// v2024-03-08
BOOL
iwb_cmp(
	CONST WS *str,    // 検索対象
	CONST WS *search, // 検索文字列
	BOOL perfect,     // TRUE=長さ一致／FALSE=前方一致
	BOOL icase        // TRUE=大小文字区別しない
)
{
	// 存在しないものは FALSE
	if(! str || ! search)
	{
		return FALSE;
	}
	// 長さ一致
	if(perfect)
	{
		// 大文字小文字区別しない
		if(icase)
		{
			return (_wcsicmp(str, search) ? FALSE : TRUE);
		}
		else
		{
			if(*str == *search)
			{
				return (wcscmp(str, search) ? FALSE : TRUE);
			}
			return FALSE;
		}
	}
	else
	{
		// 大文字小文字区別しない
		if(icase)
		{
			return (_wcsnicmp(str, search, wcslen(search)) ? FALSE : TRUE);
		}
		else
		{
			if(*str == *search)
			{
				return (wcsncmp(str, search, wcslen(search)) ? FALSE : TRUE);
			}
			return FALSE;
		}
	}
}
/* (例)
	WS *wp1 = L"ABC文字aBc文字abc";
	WS *wp2 = L"abc";
	WS *pEnd = wp1;
	while(TRUE)
	{
		// 検索ヒット位置／該当ないときは末尾'\0' のポインタを返す
		pEnd = iwp_searchPos(pEnd, wp2, TRUE);
		if(*pEnd)
		{
			PL3((pEnd - wp1));
			P2W(pEnd);
			// 次の文字へ移動
			++pEnd;
		}
		else
		{
			break;
		}
	}
*/
// v2024-03-09
WS
*iwp_searchPos(
	WS *str,          // 文字列
	CONST WS *search, // 検索文字列
	BOOL icase        // TRUE=大文字小文字区別しない
)
{
	WS *rtn = str;
	while(*rtn)
	{
		if(iwb_cmp(rtn, search, FALSE, icase))
		{
			return rtn;
		}
		++rtn;
	}
	return rtn;
}
/* (例)
	WS *wp1 = L"ABC文字aBc文字abc";
	WS *wp2 = L"abc";
	PL3(iwn_searchCnt(wp1, wp2, TRUE)); //=> 3
*/
// v2024-03-09
UINT
iwn_searchCnt(
	CONST WS *str,    // 文字列
	CONST WS *search, // 検索文字列
	BOOL icase        // TRUE=大文字小文字区別しない
)
{
	if(! str || ! *str || ! search || ! *search)
	{
		return 0;
	}
	UINT rtn = 0;
	CONST UINT uSearch = wcslen(search);
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
	WS *str,          // 文字列
	CONST WS *tokens, // 区切り文字群 (例)"-/: " => "-", "/", ":", " "
	BOOL bRmEmpty     // TRUE=配列から空白を除く
)
{
	WS **rtn = { 0 };
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
// v2024-05-07
WS
*iws_replace(
	WS *from,         // 文字列
	CONST WS *before, // 変換前の文字列
	CONST WS *after,  // 変換後の文字列
	BOOL icase        // TRUE=大文字小文字区別しない
)
{
	if(! from || ! *from || ! before || ! *before || ! after)
	{
		return iws_clone(from);
	}
	$struct_iVBW *iVBW = iVBW_alloc();
		CONST UINT uBeforeLen = wcslen(before);
		CONST UINT uAfterLen = wcslen(after);
		WS *pBgn = from;
		WS *pEnd = pBgn;
		while(TRUE)
		{
			pEnd = iwp_searchPos(pBgn, before, icase);
			if(*pEnd)
			{
				iVBW_add2(iVBW, pBgn, (pEnd - pBgn));
				iVBW_add2(iVBW, after, uAfterLen);
				pBgn = pEnd + uBeforeLen;
			}
			else
			{
				iVBW_add2(iVBW, pBgn, (pEnd - pBgn));
				break;
			}
		}
		WS *rtn = iws_clone((WS*)iVBW->str);
	iVBW_free(iVBW);
	return rtn;
}
//-------------------------
// 数値を３桁区切りで出力
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
	INT64 num // 整数
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
	MS *rtn = icalloc_MS(uNum << 1);
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
//--------------------------------------------------------
// 文字列前後の空白 ' ' L'　' '\t' '\r' '\n' '\0' を消去
//--------------------------------------------------------
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
//-----------------------
// Dir末尾の "\" を消去
//-----------------------
// v2024-03-08
WS
*iws_cutYenR(
	WS *path
)
{
	WS *rtn = iws_clone(path);
	if(! *rtn)
	{
		return rtn;
	}
	WS *pEnd = rtn + wcslen(rtn) - 1;
	while(*pEnd == L'\\' || *pEnd == L'/')
	{
		--pEnd;
	}
	++pEnd;
	*pEnd = 0;
	return rtn;
}
//----------------
// ESC文字を消去
//----------------
/* 例
	WS *wp1 = L"\033[97;104mあいうえお\033[0m\n\033[93mかきくけこ\033[0m\n";
		PL2W(wp1);
		WS *wp2 = iws_withoutESC(wp1);
			PL2W(wp2);
		ifree(wp2);
	ifree(wp1);
*/
// v2024-05-26
WS
*iws_withoutESC(
	WS *str
)
{
	WS *rtn = icalloc_WS(iwn_len(str));
	WS *rtnEnd = rtn;
	while(*str)
	{
		// \033[(NUM;NUM)(BYTE)
		while(*str == '\033' && *(str + 1) == '[')
		{
			// '\033' + '['
			str += 2;
			// (NUM;NUM)
			while((*str >= '0' && *str <= '9') || *str == ';')
			{
				++str;
			}
			// (BYTE)
			if((*str >= 'A' && *str <= 'Z') || (*str >= 'a' && *str <= 'z'))
			{
				++str;
			}
		}
		*rtnEnd++ = *str++;
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
UINT
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
/* (例)
	// 引数を", "で結合
	PL2W(iwas_njoin($ARGV, L", ", 0, $ARGC));
*/
// v2024-03-22
WS
*iwas_njoin(
	WS **ary,        // 配列
	CONST WS *token, // 区切文字
	UINT start,      // 取得位置
	UINT count       // 個数
)
{
	UINT uAry = iwan_size(ary);
	if(! uAry || start >= uAry || ! count)
	{
		return icalloc_WS(0);
	}
	if(! token)
	{
		token = L"";
	}
	UINT uToken = wcslen(token);
	WS *rtn = icalloc_WS(iwan_strlen(ary) + (uAry * uToken));
	WS *pEnd = rtn;
	UINT u1 = 0;
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
// v2024-09-15
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
	u1 = 0;
	u2 = 0;
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
				rtn[u2] = (WS*)L"";
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
					rtn[u2] = (WS*)L"";
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
// v2024-03-10
VOID
imav_print(
	MS **ary
)
{
	if(! ary)
	{
		return;
	}
	UINT u1 = 0;
	while(ary[u1])
	{
		P("    [%u] '%s'\n", u1, ary[u1]);
		++u1;
	}
}
// v2024-03-10
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
	while(ary[u1])
	{
		P("    [%u] '", u1);
		P1W(ary[u1]);
		P2("'");
		++u1;
	}
}
/* (例)
	iwav_print2($ARGV, L"'", L"'\n");
*/
// v2024-02-23
VOID
iwav_print2(
	WS **ary,
	CONST WS *sLeft,
	CONST WS *sRight
)
{
	if(! ary)
	{
		return;
	}
	MS *mp1L = W2M(sLeft);
	MS *mp1R = W2M(sRight);
		UINT u1 = 0;
		while(ary[u1])
		{
			P1(mp1L);
			P1W(ary[u1]);
			P1(mp1R);
			++u1;
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
	$struct_iVBW *iVBW = iVBW_alloc();
			PL3(iVBW_getSizeof(iVBW));   //=> 2
		iVBW_add(iVBW, L"1234567890");
			PL2W(iVBW_getStr(iVBW));     //=> "1234567890"
			PL3(iVBW_getLength(iVBW));   //=> 10
			PL3(iVBW_getFreesize(iVBW)); //=> 246
			PL3(iVBW_getSize(iVBW));     //=> 256
		iVBW_clear(iVBW);
			PL2W(iVBW_getStr(iVBW));     //=> ""
			PL3(iVBW_getLength(iVBW));   //=> 0
			PL3(iVBW_getFreesize(iVBW)); //=> 256
			PL3(iVBW_getSize(iVBW));     //=> 256
		iVBW_add2(iVBW, L"あいうえおかきくけこ", 7);
		iVBW_add_sprintf(iVBW, L"%.5f", 123456.7890);
			PL2W(iVBW_getStr(iVBW));     //=> "あいうえおかき123456.78900"
			PL3(iVBW_getLength(iVBW));   //=> 19
			PL3(iVBW_getFreesize(iVBW)); //=> 237
			PL3(iVBW_getSize(iVBW));     //=> 256
	iVBW_free(iVBW);
*/
// v2024-05-07
$struct_iVB
*iVB_alloc0(
	UINT sizeOf, // sizeof(MS), sizeof(WS)
	UINT strLen
)
{
	$struct_iVB *iVB = ($struct_iVB*)icalloc(1, sizeof($struct_iVB), FALSE);
		iVB->sizeOf = sizeOf;
		iVB->length = 0;
		iVB->freesize = strLen;
		iVB->str = icalloc(iVB->freesize, iVB->sizeOf, FALSE);
	return iVB;
}
// v2024-05-07
VOID
iVB_add0(
	$struct_iVB *iVB, // 格納場所
	CONST VOID *str,  // 追記する文字列
	UINT strLen       // 追記する文字列長／strlen(str), wcslen(str)
)
{
	if(strLen >= iVB->freesize)
	{
		///PL();P("Length=%-8lld  FreeSize=%-8lld\n", iVB->length, iVB->freesize);
		iVB->freesize = iVB->length + strLen;
		iVB->str = irealloc(iVB->str, (iVB->length + iVB->freesize), iVB->sizeOf);
	}
	// 速度を優先し文字列長をチェックしない／要自己管理
	memcpy(((MS*)iVB->str + (iVB->length * iVB->sizeOf)), str, (strLen * iVB->sizeOf));
	iVB->length += strLen;
	iVB->freesize -= strLen;
}
// v2024-05-07
VOID
iVBM_add_sprintf(
	$struct_iVBM *iVBM,
	CONST MS *format,
	...
)
{
	FILE *oFp = fopen("NUL", "wb");
		va_list va;
		va_start(va, format);
			UINT uLen = vfprintf(oFp, format, va);
			if(uLen >= iVBM->freesize)
			{
				iVBM->freesize = iVBM->length + uLen;
				iVBM->str = irealloc(iVBM->str, (iVBM->length + iVBM->freesize), iVBM->sizeOf);
			}
			uLen = vsprintf(((MS*)iVBM->str + iVBM->length), format, va);
			iVBM->length += uLen;
			iVBM->freesize -= uLen;
		va_end(va);
	fclose(oFp);
}
// v2024-05-07
VOID
iVBW_add_sprintf(
	$struct_iVBW *iVBW,
	CONST WS *format,
	...
)
{
	FILE *oFp = fopen("NUL", "wb");
		va_list va;
		va_start(va, format);
			UINT uLen = vfwprintf(oFp, format, va);
			if(uLen >= iVBW->freesize)
			{
				iVBW->freesize = iVBW->length + uLen;
				iVBW->str = irealloc(iVBW->str, (iVBW->length + iVBW->freesize), iVBW->sizeOf);
			}
			uLen = vswprintf(((WS*)iVBW->str + iVBW->length), (uLen + 1), format, va);
			iVBW->length += uLen;
			iVBW->freesize -= uLen;
		va_end(va);
	fclose(oFp);
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
						wp1 = idate_format_cjdToWS(NULL, FI->ctime_cjd);
							P2W(wp1);
						ifree(wp1);
					P1("Mtime:   ");
						wp1 = idate_format_cjdToWS(NULL, FI->mtime_cjd);
							P2W(wp1);
						ifree(wp1);
					P1("Atime:   ");
						wp1 = idate_format_cjdToWS(NULL, FI->atime_cjd);
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
	return ($struct_iFinfo*)icalloc(1, sizeof($struct_iFinfo), FALSE);
}
//---------------------------
// ファイル情報取得の前処理
//---------------------------
// v2024-03-12
BOOL
iFinfo_init(
	$struct_iFinfo *FI,
	WIN32_FIND_DATAW *F,
	WS *dir, // "\"を付与して呼ぶ
	WS *fname
)
{
	// 初期化
	*FI->sPath    = 0;
	FI->uFnPos    = 0;
	FI->uAttr     = 0;
	FI->bType     = FALSE;
	FI->ctime_cjd = 0.0;
	FI->mtime_cjd = 0.0;
	FI->atime_cjd = 0.0;
	FI->uFsize    = 0;
	if(! fname || ! *fname)
	{
		// 後述 iwn_cpy(... , fname) で対応
	}
	// Dir "." ".." は除外
	else if(! wcscmp(fname, L"..") || ! wcscmp(fname, L"."))
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
	// FI->ctime_cjd
	// FI->mtime_cjd
	// FI->atime_cjd
	FILETIME ft;
	FileTimeToLocalFileTime(&F->ftCreationTime, &ft);
		FI->ctime_cjd = iFinfo_ftimeToCjd(ft);
	FileTimeToLocalFileTime(&F->ftLastWriteTime, &ft);
		FI->mtime_cjd = iFinfo_ftimeToCjd(ft);
	FileTimeToLocalFileTime(&F->ftLastAccessTime, &ft);
		FI->atime_cjd = iFinfo_ftimeToCjd(ft);
	return TRUE;
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
// v2024-03-23
WS
*iFget_extPathname(
	WS *path,
	INT option // 1=拡張子付きファイル名／2=拡張子なしファイル名
)
{
	UINT64 uPath = iwn_len(path);
	if(! uPath)
	{
		return icalloc_WS(0);
	}
	WS *rtn = icalloc_WS(uPath);
	// Dir or File ?
	if(iFchk_DirName(path))
	{
		if(option == 0)
		{
			wmemcpy(rtn, path, (uPath + 1));
		}
	}
	else
	{
		switch(option)
		{
			// path
			case(0):
				wmemcpy(rtn, path, (uPath + 1));
				break;
			// name + ext
			case(1):
				iwv_cpy(rtn, PathFindFileNameW(path));
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
// v2024-03-08
WS
*iFget_APath(
	WS *path
)
{
	WS *p1 = iws_cutYenR(path);
	if(! *p1)
	{
		return p1;
	}
	WS *rtn = 0;
	// "c:" のような表記は特別処理
	if(p1[1] == ':' && wcslen(p1) == 2 && iFchk_DirName(p1))
	{
		rtn = wcscat(p1, L"\\");
	}
	else
	{
		rtn = icalloc_WS(IMAX_PATH);
		if(iFchk_DirName(p1) && _wfullpath(rtn, p1, IMAX_PATH))
		{
			wcscat(rtn, L"\\");
		}
		ifree(p1);
	}
	return rtn;
}
//------------------------
// 相対Path に '\\' 付与
//------------------------
/* (例)
	PL2W(iFget_RPath(L".")); //=> ".\"
*/
// v2024-03-08
WS
*iFget_RPath(
	WS *path
)
{
	WS *rtn = iws_cutYenR(path);
	if(! *rtn)
	{
		return rtn;
	}
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
	iwav_print(iF_trash(L"新しいテキスト ドキュメント.txt\n新しいフォルダー\n"));
*/
// v2024-03-23
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
				iwv_cpy(awp1[_u1], wp1);
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
WS *WDAYS[8] = {(WS*)L"Su", (WS*)L"Mo", (WS*)L"Tu", (WS*)L"We", (WS*)L"Th", (WS*)L"Fr", (WS*)L"Sa", (WS*)L"**"};
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
	INT i_y = 2011, i_m = 14;
	idate_cnv_month(&i_y, &i_m, 1, 12);
		PL3(i_y); //=> 2012
		PL3(i_m); //=> 2
*/
// v2024-01-21
VOID
idate_cnv_month(
	INT *i_y,   // 年
	INT *i_m,   // 月
	INT from_m, // 開始月
	INT to_m    // 終了月
)
{
	while(*i_m < from_m)
	{
		*i_m += 12;
		*i_y -= 1;
	}
	while(*i_m > to_m)
	{
		*i_m -= 12;
		*i_y += 1;
	}
}
//---------------
// 月末日を返す
//---------------
/* (例)
	idate_month_end(2012, 2); //=> 閏29
	idate_month_end(2013, 2); //=> 28
*/
// v2024-01-21
INT
idate_month_end(
	INT i_y, // 年
	INT i_m  // 月
)
{
	idate_cnv_month1(&i_y, &i_m);
	INT i_d = MDAYS[i_m];
	// 閏２月末日
	if(i_m == 2 && idate_chk_uruu(i_y))
	{
		i_d = 29;
	}
	return i_d;
}
//-----------------
// 月末日かどうか
//-----------------
/* (例)
	PL3(idate_chk_month_end(2012, 2, 28)); //=> FALSE
	PL3(idate_chk_month_end(2012, 2, 29)); //=> TRUE
	PL3(idate_chk_month_end(2012, 1, 59)); //=> FALSE
	PL3(idate_chk_month_end(2012, 1, 60)); //=> TRUE
*/
// v2024-01-21
BOOL
idate_chk_month_end(
	INT i_y, // 年
	INT i_m, // 月
	INT i_d  // 日
)
{
	$struct_iDV *IDV = iDV_alloc();
		iDV_set(IDV, i_y, i_m, i_d, 0, 0, 0);
		i_y = IDV->y;
		i_m = IDV->m;
		i_d = IDV->d;
	iDV_free(IDV);
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
	idate_ymdToINT(2012, 6, 17); //=> 20120617
*/
// v2024-01-21
INT
idate_ymdToINT(
	INT i_y, // 年
	INT i_m, // 月
	INT i_d  // 日
)
{
	INT i1 = 1;
	if(i_y < 0)
	{
		i1 = -1;
		i_y = -(i_y);
	}
	return (i1 * (i_y * 10000) + (i_m * 100) + i_d);
}
//-----------------------------------------------
// 年月日をCJDに変換
//   (注)空白日のとき、一律 NS_BEFORE[0] を返す
//-----------------------------------------------
// v2024-01-21
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
	idate_cnv_month1(&i_y, &i_m);
	// 1m=>13m, 2m=>14m
	if(i_m <= 2)
	{
		i_y -= 1;
		i_m += 12;
	}
	// 空白日
	i1 = idate_ymdToINT(i_y, i_m, i_d);
	if(i1 > NS_BEFORE[1] && i1 < NS_AFTER[1])
	{
		return NS_BEFORE[0];
	}
	// ユリウス通日
	cjd = floor((DOUBLE)(365.25 * (i_y + 4716)) + floor(30.6001 * (i_m + 1)) + i_d - 1524.0);
	if((INT)cjd >= NS_AFTER[0])
	{
		i1 = (INT)floor(i_y / 100.0);
		i2 = 2 - i1 + (INT)floor(i1 / 4);
		cjd += (DOUBLE)i2;
	}
	return (cjd + ((i_h * 3600 + i_n * 60 + i_s) / 86400.0));
}
//--------------------
// CJDをYMDHNSに変換
//--------------------
// v2024-01-21
VOID
idate_cjdToYmdhns(
	$struct_iDV *IDV,
	CONST DOUBLE cjd
)
{
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
	DOUBLE dCjd = (cjd - (INT64)cjd);
		dCjd += 0.00001; // 補正
		dCjd *= 24.0;
	i_h = (INT)dCjd;
		dCjd -= i_h;
		dCjd *= 60.0;
	i_n = (INT)dCjd;
		dCjd -= i_n;
		dCjd *= 60.0;
	i_s = (INT)dCjd;
	IDV->h = i_h;
	IDV->n = i_n;
	IDV->s = i_s;

	INT i_y = 0, i_m = 0, i_d = 0;
	INT64 iCJD = (INT64)cjd;
	INT64 i1 = 0, i2 = 0, i3 = 0, i4 = 0;
	if((INT64)cjd >= NS_AFTER[0])
	{
		i1 = (INT64)floor((cjd - 1867216.25) / 36524.25);
		iCJD += (i1 - (INT64)floor(i1 / 4.0) + 1);
	}
	i1 = iCJD + 1524;
	i2 = (INT64)floor((i1 - 122.1) / 365.25);
	i3 = (INT64)floor(365.25 * i2);
	i4 = (INT64)((i1 - i3) / 30.6001);
	i_d = (i1 - i3 - (INT64)floor(30.6001 * i4));
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
	IDV->y = i_y;
	IDV->m = i_m;
	IDV->d = i_d;
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
// v2024-01-21
INT
idate_cjd_yeardays(
	DOUBLE cjd
)
{
	$struct_iDV *IDV = iDV_alloc();
		iDV_set2(IDV, cjd);
		INT rtn = (INT)(cjd - idate_ymdhnsToCjd(IDV->y, 1, 0, 0, 0, 0));
	iDV_free(IDV);
	return rtn;
}
//--------------------------------
// 日付の加算 = y, m, d, h, n, s
//--------------------------------
/* (例)
	$struct_iDV *IDV = iDV_alloc();
		idate_add(
			IDV,
			1582, 11, 10, 0, 0, 0,
			0, -1, -1, 0, 0, 0
		);
		PL3(IDV->y); //=> 1582
		PL3(IDV->m); //=> 10
		PL3(IDV->d); //=> 3
		PL3(IDV->h); //=> 0
		PL3(IDV->n); //=> 0
		PL3(IDV->s); //=> 0
	iDV_free(IDV);
*/
// v2024-01-22
VOID
idate_add(
	$struct_iDV *IDV,
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
	iDV_set(IDV, i_y, i_m, i_d, i_h, i_n, i_s);
	INT i1 = 0;
	// 個々に計算
	// 手を抜くと「1582-11-10 -1m, -1d」のとき、エラー1582-10-04（期待値は1582-10-03）となる
	if(add_y != 0 || add_m != 0)
	{
		i1 = (INT)idate_month_end((IDV->y + add_y), (IDV->m + add_m));
		if(IDV->d > (INT)i1)
		{
			IDV->d = (INT)i1;
		}
		iDV_set(IDV, (IDV->y + add_y), (IDV->m + add_m), IDV->d, IDV->h, IDV->n, IDV->s);
	}
	if(add_d)
	{
		// 【重要】CJDに変換してから加算
		iDV_set2(IDV, add_d + idate_ymdhnsToCjd(IDV->y, IDV->m, IDV->d, IDV->h, IDV->n, IDV->s));
	}
	if(add_h != 0 || add_n != 0 || add_s != 0)
	{
		iDV_set(IDV, IDV->y, IDV->m, IDV->d, (IDV->h + add_h), (IDV->n + add_n), (IDV->s + add_s));
	}
	IDV->sign = TRUE;
	IDV->days = 0.0;
}
//------------------------------------------
// 日付の差 = sign, y, m, d, h, n, s, days
//   (注)「月差」と「日差」を区別する
//     ・5/31 => 6/30 : +1m
//     ・6/30 => 5/31 : -30d
//------------------------------------------
/* (例)
	$struct_iDV *IDV = iDV_alloc();
		idate_diff(
			IDV,
			2012, 5, 31, 0, 0, 0,
			2012, 6, 30, 0, 0, 0
		);
		//=> sign=1, y=0, m=1, d=0, h=0, n=0, s=0, days=30.000000
		P("sign=%d, y=%d, m=%d, d=%d, h=%d, n=%d, s=%d, days=%.6f\n", IDV->sign, IDV->y, IDV->m, IDV->d, IDV->h, IDV->n, IDV->s, IDV->days);
		idate_diff(
			IDV,
			2012, 6, 30, 0, 0, 0,
			2012, 5, 31, 0, 0, 0
		);
		//=> sign=0, y=0, m=0, d=30, h=0, n=0, s=0, days=30.000000
		P("sign=%d, y=%d, m=%d, d=%d, h=%d, n=%d, s=%d, days=%.6f\n", IDV->sign, IDV->y, IDV->m, IDV->d, IDV->h, IDV->n, IDV->s, IDV->days);
	iDV_free(IDV);
*/
// v2024-01-21
VOID
idate_diff(
	$struct_iDV *IDV,
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
	iDV_clear(IDV);

	INT i1 = 0, i2 = 0, i3 = 0, i4 = 0;

	// 正規化1
	DOUBLE cjd1 = idate_ymdhnsToCjd(i_y1, i_m1, i_d1, i_h1, i_n1, i_s1);
	DOUBLE cjd2 = idate_ymdhnsToCjd(i_y2, i_m2, i_d2, i_h2, i_n2, i_s2);
	if(cjd1 > cjd2)
	{
		IDV->sign = FALSE;
		DOUBLE cjd  = cjd1;
		cjd1 = cjd2;
		cjd2 = cjd;
	}
	else
	{
		IDV->sign = TRUE;
	}

	// days
	IDV->days = (DOUBLE)(cjd2 - cjd1);

	// 正規化2
	$struct_iDV *IDV1 = iDV_alloc();
		iDV_set2(IDV1, cjd1);
	$struct_iDV *IDV2 = iDV_alloc();
		iDV_set2(IDV2, cjd2);

		// ymdhns
		IDV->y = IDV2->y - IDV1->y;
		IDV->m = IDV2->m - IDV1->m;
		IDV->d = IDV2->d - IDV1->d;
		IDV->h = IDV2->h - IDV1->h;
		IDV->n = IDV2->n - IDV1->n;
		IDV->s = IDV2->s - IDV1->s;

		// m 調整
		idate_cnv_month2(&IDV->y, &IDV->m);

		// hns 調整
		while(IDV->s < 0)
		{
			IDV->s += 60;
			IDV->n -= 1;
		}
		while(IDV->n < 0)
		{
			IDV->n += 60;
			IDV->h -= 1;
		}
		while(IDV->h < 0)
		{
			IDV->h += 24;
			IDV->d -= 1;
		}

		// d 調整
		// 前処理
		if(IDV->d < 0)
		{
			IDV->m -= 1;
			if(IDV->m < 0)
			{
				IDV->m += 12;
				IDV->y -= 1;
			}
		}

		// 本処理
		$struct_iDV *IDV3 = iDV_alloc();
			if(IDV->sign)
			{
				idate_add(IDV3, IDV1->y, IDV1->m, IDV1->d, IDV1->h, IDV1->n, IDV1->s, IDV->y, IDV->m, 0, 0, 0, 0);
					i1 = (INT)idate_ymdhnsToCjd(IDV2->y, IDV2->m, IDV2->d, 0, 0, 0);
					i2 = (INT)idate_ymdhnsToCjd(IDV3->y, IDV3->m, IDV3->d, 0, 0, 0);
			}
			else
			{
				idate_add(IDV3, IDV2->y, IDV2->m, IDV2->d, IDV2->h, IDV2->n, IDV2->s, -(IDV->y), -(IDV->m), 0, 0, 0, 0);
					i1 = (INT)idate_ymdhnsToCjd(IDV3->y, IDV3->m, IDV3->d, 0, 0, 0);
					i2 = (INT)idate_ymdhnsToCjd(IDV1->y, IDV1->m, IDV1->d, 0, 0, 0);
			}
		iDV_free(IDV3);
		i3 = idate_ymdToINT(IDV1->h, IDV1->n, IDV1->s);
		i4 = idate_ymdToINT(IDV2->h, IDV2->n, IDV2->s);

		/* 変換例
			"05-31" "06-30" のとき m = 1, d = 0
			"06-30" "05-31" のとき m = 0, d = -30
			※分岐条件は非常にシビアなので安易に変更するな!!
		*/
		if(IDV->sign                                          // ＋のとき
			&& i3 <= i4                                       // (例) "12:00:00 =< 23:00:00"
			&& idate_chk_month_end(IDV2->y, IDV2->m, IDV2->d) // (例) "06-30" は月末日？
			&& IDV1->d > IDV2->d                              // (例) "05-31" > "06-30"
		)
		{
			IDV->m += 1;
			IDV->d = 0;
		}
		else
		{
			IDV->d = i1 - i2 - (INT)(i3 > i4 ? 1 : 0);
		}
	iDV_free(IDV2);
	iDV_free(IDV1);
}
//--------------------------
// diff()／add()の動作確認
//--------------------------
/* (例) 西暦1年から2000年迄のサンプル100例について評価
	iDV_checker(1, 2000, 100);
*/
// v2024-01-21
/*
INT
irand_INT(
	INT min,
	INT max
)
{
	return (min + (INT)(rand() * (max - min + 1.0) / (1.0 + RAND_MAX)));
}
VOID
iDV_checker(
	INT from_year, // 何年から
	INT to_year,   // 何年まで
	INT iSample    // サンプル抽出数
)
{
	$struct_idate_value *IDV1 = iDV_alloc();
	$struct_idate_value *IDV2 = iDV_alloc();
	$struct_idate_value *IDV3 = iDV_alloc();
	$struct_idate_value *IDV4 = iDV_alloc();
		if(from_year > to_year)
		{
			INT _i1 = to_year;
			to_year = from_year;
			from_year = _i1;
		}
		INT rnd_y = to_year - from_year;
		MS buf[256] = "";
		P2("\033[94m--Cnt----From-----------To-------------Sign  Y  M  D----DateAdd--------Check--\033[0m");
		srand((UINT)time(NULL));
		for(INT _i1 = 1; _i1 <= iSample; _i1++)
		{
			iDV_set(
				IDV1,
				from_year + irand_INT(0, rnd_y),
				(1 + irand_INT(0, 11)),
				(1 + irand_INT(0, 30)),
				0, 0, 0
			);
			iDV_set(
				IDV2,
				to_year + irand_INT(0, rnd_y),
				(1 + irand_INT(0, 11)),
				(1 + irand_INT(0, 30)),
				0, 0, 0
			);
			idate_diff(
				IDV3,
				IDV1->y, IDV1->m, IDV1->d, 0, 0, 0,
				IDV2->y, IDV2->m, IDV2->d, 0, 0, 0
			);
			if(IDV3->sign)
			{
				idate_add(
					IDV4,
					IDV1->y, IDV1->m, IDV1->d, 0, 0, 0,
					IDV3->y, IDV3->m, IDV3->d, 0, 0, 0
				);
			}
			else
			{
				idate_add(
					IDV4,
					IDV1->y, IDV1->m, IDV1->d, 0, 0, 0,
					-(IDV3->y), -(IDV3->m), -(IDV3->d), 0, 0, 0
				);
			}
			UINT _uLen = sprintf(
				buf,
				"%5d"
				" \033[92m%8d-%02d-%02d"
				" \033[96m%8d-%02d-%02d"
				"    \033[94m%c %5d %2d %2d"
				" \033[96m%8d-%02d-%02d"
				"    %s\033[0m\n"
				,
				_i1,
				IDV1->y, IDV1->m, IDV1->d,
				IDV2->y, IDV2->m, IDV2->d,
				(IDV3->sign ? '+' : '-'), IDV3->y, IDV3->m, IDV3->d,
				IDV4->y, IDV4->m, IDV4->d,
				(
					idate_ymdToINT(IDV2->y, IDV2->m, IDV2->d) == idate_ymdToINT(IDV4->y, IDV4->m, IDV4->d) ?
					"\033[93mok" :
					"\033[91mNG"
				)
			);
			QP(buf, _uLen);
		}
	iDV_free(IDV4);
	iDV_free(IDV3);
	iDV_free(IDV2);
	iDV_free(IDV1);
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
	$struct_iDV *IDV = iDV_alloc();
		iDV_set(IDV, 2024, 0, 50, 60, 0, 0);
		WS *wp1 = idate_format(L"%g%y-%m-%d(%a) %C\n", IDV->sign, IDV->y, IDV->m, IDV->d, IDV->h, IDV->n, IDV->s, IDV->days);
			PL2W(wp1); //=> "+2024-01-21(Su) 2460331.500000"
		ifree(wp1);
	iDV_free(IDV);
*/
// v2024-01-24
WS
*idate_format(
	WS *format,
	BOOL b_sign,  // TRUE='+'／FALSE='-'
	INT i_y,      // 年
	INT i_m,      // 月
	INT i_d,      // 日
	INT i_h,      // 時
	INT i_n,      // 分
	INT i_s,      // 秒
	DOUBLE d_days // 通算日／idate_diff()で使用
)
{
	if(! format)
	{
		return icalloc_WS(0);
	}
	CONST UINT BufSizeBase = 32; // %lld長以上
	UINT BufSize = BufSizeBase;
	UINT BufSizeMax = BufSize + BufSizeBase;
	UINT BufEnd = 0;
	WS *rtn = icalloc_WS(BufSizeMax);
	WS *pEnd = rtn;
	// Ymdhns で使用
	DOUBLE cjd = (d_days ? d_days : idate_ymdhnsToCjd(i_y, i_m, i_d, i_h, i_n, i_s));
	INT64 i_days = (INT64)cjd;
	// 符号チェック
	if(i_y < 0)
	{
		b_sign = FALSE;
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
					pEnd += swprintf(pEnd, BufSizeMax, L"%.6f", cjd);
					break;
				case 'J': // JD通算日
					pEnd += swprintf(pEnd, BufSizeMax, L"%.6f", (cjd - CJD_TO_JD));
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
					*pEnd = (! b_sign ? '-' : '+');
					++pEnd;
					break;
				case 'G': // Sign "-" のみ
					if(! b_sign)
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
					--format;
					break;
			}
		}
		else if(*format == '\\')
		{
			++format;
			switch(*format)
			{
				case('a'):
					*pEnd = '\a';
					++pEnd;
					break;
				case('n'):
					*pEnd = '\n';
					++pEnd;
					break;
				case('t'):
					*pEnd = '\t';
					++pEnd;
					break;
				default:
					--format;
					break;
			}
		}
		else
		{
			*pEnd = *format;
			++pEnd;
		}
		BufEnd = pEnd - rtn;
		if(BufEnd >= BufSize)
		{
			BufSize <<= 1;
			BufSizeMax = BufSize + BufSizeBase;
			rtn = irealloc_WS(rtn, BufSizeMax);
			pEnd = rtn + BufEnd;
		}
		++format;
	}
	return rtn;
}
/* (例)
	PL2W(idate_format_cjdToWS(NULL, CJD_NOW_LOCAL()));
*/
// v2024-01-21
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
	$struct_iDV *IDV = iDV_alloc();
		iDV_set2(IDV, cjd);
		WS *rtn = idate_format_ymdhns(format, IDV->y, IDV->m, IDV->d, IDV->h, IDV->n, IDV->s);
	iDV_free(IDV);
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
// v2024-09-15
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
		add_quote = (WS*)L"";
	}
	WS *rtn = 0;
	UINT u1 = iwn_searchCnt(str, quoteBgn, FALSE);
	UINT u2 = iwn_searchCnt(str, quoteEnd, FALSE);
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
		(WS*)L"" :
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
							pDtEnd = (WS*)L"y%";
							break;
						case('*'):
							pDtEnd = (WS*)L"y*";
							break;
						default:
							pDtEnd = (WS*)L"y";
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
				INT *ai = icalloc_INT(6);
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
						$struct_iDV *IDV = iDV_alloc();
							idate_add(IDV, i_y, i_m, i_d, i_h, i_n, i_s, add_y, add_m, add_d, add_h, add_n, add_s);
							ai[0] = IDV->y;
							ai[1] = IDV->m;
							ai[2] = IDV->d;
							ai[3] = IDV->h;
							ai[4] = IDV->n;
							ai[5] = IDV->s;
						iDV_free(IDV);
						// "00:00:00"
						if(bHnsZero)
						{
							ai[3] = 0;
							ai[4] = 0;
							ai[5] = 0;
						}
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
	/* [2021-11-15] Pending
		下記コードでビープ音を発生することがある。
			INT *rtn = icalloc_INT(n) ※INT系全般／DOUBLE系は問題なし
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
