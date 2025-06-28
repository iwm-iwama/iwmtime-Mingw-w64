/*
[2025-03-26] + [2025-04-13]
	--------------------------
	| このライブラリについて
	--------------------------
	2009年、コマンドラインプログラム開発簡素化のため書き始める。
	オブジェクト指向やガベージコレクションを参考にしつつ、古典的手続き型プログラミングの拡張によるメモリ安全かつ高速な実装が目標。
	当初の目的は次のとおり。
		関数戻り値の基本設計
			元データ（のポインタ）でなく、新たに生成されたデータ（のポインタ）を戻り値とする。
		動的メモリ管理（icalloc(), irealloc(), ifree() 系実装）
			・インデックスによる管理
			・管理されているポインタは終了時に自動解放
			・基本は手動解放
			・セキュリティを考慮し 割り当て／解放時 ゼロクリア（memset(p,0,n)）
			・デバッグ用のメモリアドレス表示（idebug_map()）
		日付計算（iwmdateadd.exe, iwmdatediff.exe）
			・ユリウス暦 -4712/01/01..1582/10/04 とグレゴリオ暦 1582/10/15..9999/12/31 を使用
			・空白暦 1582/10/05..1582/10/14 を考慮
			・数ヶ月前後の計算
				05/31 の１ヶ月後は 06/30
				06/30 の１ヶ月前は 05/30
	Ruby, UNIXライク環境などから得たアイデアは次のとおり。
		・引数の切り分け（$CMD, $ARG, $ARGC, $ARGV[]）
		・パイプ経由標準入力の改装（iwmesc.exe, iwmclipboard.exe）
		・エスケープシーケンス文字の表示（iwmesc.exe）
	2022年、lib_iwmutil（Shiftt_JIS／CP932）から、lib_iwmutil2（UTF-8／CP65001）に移行。
		2018年頃？からMicrosoft社はUTF-8による開発を推奨しているが、DOSプロンプトにおける日本語処理は難解。
		当面、試行的に以下の実装とする。
			標準出力は、UTF-8／BOMなし／改行コード=LF'\n'。
			ただし、標準入力の改行コード（例：CRLF'\r\n'）を優先する。
		imn_CodePage(), SetConsoleOutputCP(65001) 前後の実装は試行解。

[2016-08-19] + [2024-09-09]
	-------------------------------
	| lib_iwmutil2 で規定する表記
	-------------------------------
	・大域変数の "１文字目は $"
		$ARGV, $icallocMap など
	・#define定数（関数は含まない）は "すべて大文字"
		IMAX_PATHW, WS など

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

[2021-11-18] + [2025-03-27]
	ポインタ *(p + n) と、配列 p[n] どちらが速い？
		Mingw-w64においては最適化するとどちらも同じになる。
		基本、可読性を考慮した配列 p[n] でコーディングする。

[2024-01-19] + [2025-04-09]
	主に使うデータ型
		BOOL   = TRUE, FALSE
		INT    = 32bit integer（INT8, INT16, UINT8, UINT16の代替）
		INT64  = 64bit integer（日計算）
		UINT   = 32bit unsigned integer（NTFSファイル個数）
		UINT64 = 64bit unsigned integer（size_t／秒計算／ファイルサイズ）
		DOUBLE = double（日計算）
		MS     = char
		WS     = wchar
		VOID   = void
		DWORD  = typedef unsigned long DWORD（Win32API）
	ペンディング
		size_t
			64bit環境では size_t（SIZE_MAX）とUINT64（UINT64_MAX）は同値だが、実行上の制約を考慮し、UINT（UINT_MAX）として実装した箇所がある。
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
WS     *$CMD         = NULL; // コマンド名を格納
WS     *$ARG         = NULL; // 引数からコマンド名を消去したもの
UINT   $ARGC         = 0;    // 引数配列数
WS     **$ARGV       = { };  // 引数配列／ダブルクォーテーションを消去したもの
HANDLE $StdinHandle  = 0;    // STDIN ハンドル
HANDLE $StdoutHandle = 0;    // STDOUT ハンドル
HANDLE $StderrHandle = 0;    // STDERR ハンドル
UINT64 $ExecSecBgn   = 0;    // 実行開始時間
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
// v2025-03-31
VOID
iCLI_begin()
{
	// [Ctrl]+[C]
	signal(SIGINT, (__p_sig_fn_t)iCLI_signal);
	// CodePage
	SetConsoleCP(65001);
	SetConsoleOutputCP(65001);
	// $icallocMap
	icalloc_initMap();
	// $CMD
	// $ARG
	// $ARGC
	// $ARGV
	WS *pBgn = GetCommandLineW();
	INT64 iPos = iwn_searchPos(pBgn, (WS*)L" ", 1);
	if(iPos < 0)
	{
		iPos = wcslen(pBgn);
	}
	$CMD = iws_nclone(pBgn, iPos);
	pBgn += iPos;
	for(; *pBgn == L' '; pBgn++);
	if(! *pBgn)
	{
		$ARG = icalloc_WS(0);
		$ARGC = 0;
		$ARGV = icalloc_WS_ary($ARGC);
		return;
	}
	$ARG = iws_clone(pBgn);
		INT pNumArgs = 0;
	WS **lpCmdLine = CommandLineToArgvW($ARG, &pNumArgs);
		$ARGC = (UINT)pNumArgs;
		$ARGV = icalloc_WS_ary($ARGC);
		UINT u1 = 0;
		while(u1 < $ARGC)
		{
			$ARGV[u1] = iws_clone(lpCmdLine[u1]);
			++u1;
		}
	LocalFree(lpCmdLine);
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
// v2025-03-23
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
	WS *wp1 = $ARGV[argc];
	UINT u1 = iwn_len(opt1);
	if(u1 && ! wcsncmp(wp1, opt1, u1))
	{
		return (wp1 + u1);
	}
	UINT u2 = iwn_len(opt2);
	if(u2 && ! wcsncmp(wp1, opt2, u2))
	{
		return (wp1 + u2);
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
// v2025-03-22
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
	WS *wp1 = $ARGV[argc];
	if(opt1 && ! wcscmp(wp1, opt1))
	{
		return TRUE;
	}
	if(opt2 && ! wcscmp(wp1, opt2))
	{
		return TRUE;
	}
	return FALSE;
}
// v2025-03-26
VOID
iCLI_VarList()
{
	P1(
		"\033[1G"
		"\033[44;97m $CMD \033[0m"
		"\n"
		"\033[5G"
	);
	P2W($CMD);
	P1(
		"\033[1G"
		"\033[44;97m $ARG \033[0m"
		"\n"
		"\033[5G"
	);
	P2W($ARG);
	P1(
		"\033[1G"
		"\033[44;97m $ARGC \033[0m"
		"\n"
		"\033[5G"
	);
	P(
		"\033[92m[%d]\n"
		, $ARGC
	);
	P2(
		"\033[1G"
		"\033[44;97m $ARGV \033[0m"
	);
	iwav_print($ARGV);
	P2("\033[0m");
}
//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	実行時間
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
/* (例)
	iExecSec_init();
	Sleep(2500);
	P("-- %.3fsec\n\n", iExecSec_next());
	Sleep(1500);
	P("-- %.3fsec\n\n", iExecSec_next());
*/
// v2025-02-14
DOUBLE
iExecSec(
	UINT64 microSec // 0=$ExecSecBgnに保存
)
{
	UINT64 u1 = GetTickCount64();
	if(! microSec)
	{
		$ExecSecBgn = u1;
		return 0.0;
	}
	return (u1 < microSec ? 0.0 : (u1 - microSec) / 1000.0);
}
//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	メモリ確保
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
/*
	icalloc() 用に確保される配列
*/
typedef struct
{
	VOID *ptr;    // ポインタ位置
	UINT uAry;    // 配列個数（配列以外=0）
	UINT uSizeOf; // sizeof
	UINT uAlloc;  // アロケート長
	UINT uId;     // 順番
}
$struct_icallocMap;

$struct_icallocMap *$icallocMap = 0; // 可変長
UINT $icallocMapSize = 0;            // $icallocMap のサイズ [0..n]
UINT $icallocMapEOD = 0;             // $icallocMap の末尾'\0'位置 [0..(n-1)]
UINT $icallocMapId = 1;              // $icallocMap の順番 [1..(n+1)]
UINT $icallocMapSweepWait = 0;

// $icallocMap の基本サイズ
#define   IcallocDiv          16

// ダブルヌル追加／アラインメント調整はコンパイラ依存
#define   MemX(n, sizeOf)     (UINT)((n + 2) * sizeOf)

//-----------------------
// $icallocMap 新規作成
//-----------------------
// v2025-03-19
VOID
icalloc_initMap()
{
	if(! $icallocMapSize)
	{
		$icallocMapSize = IcallocDiv;
		$icallocMap = ($struct_icallocMap*)calloc($icallocMapSize, sizeof($struct_icallocMap));
		icalloc_err($icallocMap);
		$icallocMapId = 1;
		$icallocMapEOD = 0;
	}
}
//---------
// calloc
//---------
/* (例)
	WS *wp1 = NULL;
	UINT wp1Pos = 0;
	wp1 = icalloc_WS(100);
		wp1Pos += iwn_cpy((wp1 + wp1Pos), L"12345");
		wp1Pos += iwn_cpy((wp1 + wp1Pos), L"ABCDE");
		P2W(wp1); //=> "12345ABCDE"
	wp1 = irealloc_WS(wp1, 200);
		wp1Pos += iwn_cpy((wp1 + wp1Pos), L"あいうえお");
		P2W(wp1); //=> "12345ABCDEあいうえお"
	ifree(wp1);
*/
// v2025-03-23
VOID
*icalloc(
	UINT n,      // 個数
	UINT sizeOf, // sizeof
	BOOL aryOn   // TRUE=配列
)
{
	// $icallocMap 拡張
	if($icallocMapEOD >= $icallocMapSize)
	{
		$icallocMapSize += IcallocDiv;
		$struct_icallocMap *pOld = $icallocMap;
			$icallocMap = ($struct_icallocMap*)calloc($icallocMapSize, sizeof($struct_icallocMap));
			icalloc_err($icallocMap);
			UINT uOld = $icallocMapEOD * sizeof($struct_icallocMap);
				memcpy($icallocMap, pOld, uOld);
				memset(pOld, 0, uOld);
		free(pOld);
		pOld = 0;
	}
	// 引数にポインタ割当
	UINT uAlloc = MemX(n, sizeOf);
	VOID *rtn = calloc(uAlloc, 1);
	icalloc_err(rtn);
	$struct_icallocMap *map1 = $icallocMap + $icallocMapEOD;
	map1->ptr = rtn;
	if(aryOn)
	{
		// 配列ゼロ回避
		map1->uAry = (! n ? 1 : n);
	}
	map1->uSizeOf = sizeOf;
	map1->uAlloc = uAlloc;
	map1->uId = $icallocMapId;
	++$icallocMapId;
	++$icallocMapEOD;
	return rtn;
}
//----------
// realloc
//----------
// v2025-03-19
VOID
*irealloc(
	VOID *ptr,  // icalloc()ポインタ
	UINT n,     // 個数
	UINT sizeOf // sizeof
)
{
	// 該当なしのときは ptr を返す
	VOID *rtn = ptr;
	UINT uAlloc = MemX(n, sizeOf);
	// $icallocMap を更新／サイズに変更なし
	UINT u1 = 0;
	while(u1 < $icallocMapEOD)
	{
		$struct_icallocMap *map1 = ($icallocMap + u1);
		if(ptr == map1->ptr)
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
				map1->uId = $icallocMapId;
				++$icallocMapId;
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
// v2025-04-03
VOID
icalloc_err(
	VOID *ptr
)
{
	if(ptr)
	{
		return;
	}
	P2(
		IESC_FALSE1
		"[Err] Can't allocate memories!"
		IESC_RESET
	);
	imain_err();
}
//-----------------------
// $icallocMap[n]をfree
//-----------------------
// v2025-03-16
VOID
icalloc_free(
	VOID *ptr
)
{
	// 定期清掃
	++$icallocMapSweepWait;
	if($icallocMapSweepWait >= (IcallocDiv >> 1))
	{
		$icallocMapSweepWait = 0;
		icalloc_sweepMap();
	}
	// 削除される可能性のあるデータ＝最近作成されたデータは後方に集中するため末尾から走査
	// NULLが排除されているのでチェックなしで走査可能
	INT i1 = $icallocMapEOD - 1;
	while(i1 >= 0)
	{
		$struct_icallocMap *map1 = ($icallocMap + i1);
		if(ptr == map1->ptr)
		{
			// 配列のとき
			if(map1->uAry)
			{
				// 子ポインタのリンク先を解放
				VOID **va1 = (VOID**)(map1->ptr);
				// (i1 + 1)以降を総当たりで走査／「子」は必ずしもアドレス順でないことに留意
				for(UINT _u1 = 0; _u1 < map1->uAry; _u1++)
				{
					for(UINT _u2 = i1 + 1; _u2 < $icallocMapEOD; _u2++)
					{
						$struct_icallocMap *map2 = ($icallocMap + _u2);
						if(va1[_u1] && va1[_u1] == map2->ptr)
						{
							memset(map2->ptr, 0, map2->uAlloc);
							free(map2->ptr);
							memset(map2, 0, sizeof($struct_icallocMap));
							break;
						}
					}
				}
			}
			// ポインタのリンク先を解放
			memset(map1->ptr, 0, map1->uAlloc);
			free(map1->ptr);
			memset(map1, 0, sizeof($struct_icallocMap));
			return;
		}
		--i1;
	}
}
//--------------------
// $icallocMapをfree
//--------------------
// v2025-03-16
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
	memset($icallocMap, 0, ($icallocMapSize * sizeof($struct_icallocMap)));
	free($icallocMap);
	$icallocMap = 0;
	$icallocMapSize = 0;
	$icallocMapEOD = 0;
	$icallocMapId = 0;
}
//--------------------
// $icallocMapを掃除
//--------------------
// v2025-03-15
VOID
icalloc_sweepMap()
{
	// ゼロクリア
	memset(($icallocMap + $icallocMapEOD), 0, (sizeof($struct_icallocMap) * ($icallocMapSize - $icallocMapEOD)));
	// 隙間を詰める
	UINT uTo = 0;
	UINT uFrom = 0;
	while(uTo < $icallocMapSize)
	{
		$struct_icallocMap *map1 = ($icallocMap + uTo);
		if(! map1->ptr)
		{
			if(uTo >= uFrom)
			{
				uFrom = uTo + 1;
			}
			while(uFrom < $icallocMapSize)
			{
				$struct_icallocMap *map2 = ($icallocMap + uFrom);
				if(map2->ptr)
				{
					memcpy(map1, map2, sizeof($struct_icallocMap));
					map2->ptr = 0;
					break;
				}
				++uFrom;
			}
			if(uFrom >= $icallocMapSize)
			{
				break;
			}
		}
		++uTo;
	}
	// $icallocMapEOD を取得
	INT i1= $icallocMapEOD - 1;
	while(i1 >= 0)
	{
		$struct_icallocMap *map1 = $icallocMap + i1;
		if(map1->ptr)
		{
			$icallocMapEOD = i1 + 1;
			break;
		}
		--i1;
	}
}
//---------------------
// icalloc 情報を取得
//---------------------
/* (例)
	UINT uId     = 0; // 順番
	UINT uAry    = 0; // 配列個数（配列以外=0）
	UINT uSizeOf = 0; // sizeof
	UINT uAlloc  = 0; // アロケート長

	INT *ai1 = icalloc_INT(0);
		BOOL b1 = icalloc_getInfo(ai1, &uId, &uAry, &uSizeOf, &uAlloc);
		P1("icalloc:  ");
		P("[%d] uId=%3u  uAry=%3u  uSizeOf=%3u  uAlloc=%3u\n", b1, uId, uAry, uSizeOf, uAlloc);

	ai1 = irealloc_INT(ai1, 10);
		b1 = icalloc_getInfo(ai1, &uId, &uAry, &uSizeOf, &uAlloc);
		P1("irealloc: ");
		P("[%d] uId=%3u  uAry=%3u  uSizeOf=%3u  uAlloc=%3u\n", b1, uId, uAry, uSizeOf, uAlloc);

	ifree(ai1);
		b1 = icalloc_getInfo(ai1, &uId, &uAry, &uSizeOf, &uAlloc);
		P1("ifree:    ");
		P("[%d] uId=%3u  uAry=%3u  uSizeOf=%3u  uAlloc=%3u\n", b1, uId, uAry, uSizeOf, uAlloc);
*/
// v2025-04-09
BOOL
icalloc_getInfo(
	CONST VOID *ptr, // ポインタ位置
	UINT *uId,       // 順番
	UINT *uAry,      // 配列個数（配列以外=0）
	UINT *uSizeOf,   // sizeof
	UINT *uAlloc     // アロケート長
)
{
	INT i1 = $icallocMapEOD - 1;
	while(i1 >= 0)
	{
		$struct_icallocMap *map1 = ($icallocMap + i1);
		if(ptr == map1->ptr)
		{
			if(uId)
			{
				*uId = map1->uId;
			}
			if(uAry)
			{
				*uAry = map1->uAry;
			}
			if(uSizeOf)
			{
				*uSizeOf = map1->uSizeOf;
			}
			if(uAlloc)
			{
				*uAlloc = map1->uAlloc;
			}
			return TRUE;
		}
		--i1;
	}
	UINT u0 = 0;
	*uId     = *&u0;
	*uAry    = *&u0;
	*uSizeOf = *&u0;
	*uAlloc  = *&u0;
	return FALSE;
}
//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	Debug
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
//--------------------------
// $icallocMapをリスト出力
//--------------------------
// v2025-04-04
VOID
idebug_printMap()
{
	iConsole_EscOn();
	P2(
		"\033[94m"
		"- count - id ------- pointer -------- array - sizeof ---------- alloc -------------"
	);
	UINT64 uAllocUsed = 0;
	UINT u1 = 0, u2 = 0;
	while(u1 < $icallocMapSize)
	{
		$struct_icallocMap *map = ($icallocMap + u1);
		uAllocUsed += map->uAlloc;
		if(map->uAry)
		{
			P1("\033[37;44m");
		}
		else if(map->ptr)
		{
			P1("\033[97m");
		}
		else
		{
			P1("\033[31m");
		}
		++u2;
		MS ms1[72];
		sprintf(
			ms1,
			"  %-7u %-10u %p %5u %8u %16u "
			, u2
			, map->uId
			, map->ptr
			, map->uAry
			, map->uSizeOf
			, map->uAlloc
		);
		QP(ms1, 72);
		if(map->ptr)
		{
			P1("\033[32m");
			if(map->uSizeOf == sizeof(WS))
			{
				QP1W((WS*)map->ptr);
			}
			else if(map->uSizeOf == sizeof(MS))
			{
				QP1((MS*)map->ptr);
			}
		}
		P2("\033[0m");
		++u1;
	}
	P(
		"\033[94m"
		"---------------------------------------------------- %16llu byte --------"
		"\033[0m\n\n"
		, uAllocUsed
	);
}
//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	Print関係
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
//--------
// Print
//--------
// v2023-07-11
VOID
P1W(
	CONST WS *str
)
{
	MS *mp1 = W2M(str);
		fputs(mp1, STDOUT);
	ifree(mp1);
}
//--------------
// Quick Print
//--------------
/* (例)
	MS *ms1 = "Quick Print!";
	QP(ms1, strlen(ms1));
*/
// v2025-04-10
VOID
QP(
	CONST MS *str,
	UINT strLen
)
{
	if(! str)
	{
		return;
	}
	fflush(STDOUT);
	WriteFile($StdoutHandle, str, strLen, NULL, NULL);
	FlushFileBuffers($StdoutHandle);
}
// v2025-04-10
VOID
QP1W(
	CONST WS *str
)
{
	if(! str)
	{
		return;
	}
	MS *mp1 = W2M(str);
		QP(mp1, strlen(mp1));
	ifree(mp1);
}
//---------------
// Print Repeat
//---------------
// v2025-03-17
VOID
PR(
	CONST MS *str,
	UINT strRepeat
)
{
	MS *mp1 = ims_repeat(str, strRepeat);
		UINT u1 = strlen(str) * strRepeat;
		QP(mp1, u1);
	ifree(mp1);
}
//-------------------------
// Escape Sequence へ変換
//-------------------------
/* (例)
	WS *wp1 = L"A\\nB\\rC";
	WS *wp2 = iws_cnv_escape(wp1);
		P2W(wp1); //=> "A\\nB\\rC"
		P2W(wp2); //=> "A\nC"
	ifree(wp2);
*/
// v2025-04-02
WS
*iws_cnv_escape(
	WS *str
)
{
	if(! str)
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
				case('\"'):
					*pEnd = '\"';
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
		rtn = iws_replace(wp1, (WS*)L"\\033[", (WS*)L"\033[", FALSE);
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
// v2025-02-15
VOID
iConsole_EscOn()
{
	$StdinHandle = GetStdHandle(STD_INPUT_HANDLE);
	$StdoutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
	$StderrHandle = GetStdHandle(STD_ERROR_HANDLE);
	DWORD consoleMode = 0;
	GetConsoleMode($StdoutHandle, &consoleMode);
	SetConsoleMode($StdoutHandle, (consoleMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING));
}
//-----------------
// キーボード入力
//-----------------
/* 例
	P1("一行入力 > ");
	WS *wp1 = iCLI_getKeyInput(FALSE);
		P2W(wp1);
	ifree(wp1);

	P2("複数行入力 > ");
	wp1 = iCLI_getKeyInput(TRUE);
		P2W(wp1);
	ifree(wp1);
*/
// v2025-02-24
WS
*iCLI_getKeyInput(
	BOOL bKeyInputMultiLine
)
{
	UINT rtnMax = 256;
	WS *rtn = icalloc_WS(rtnMax);
		UINT uEnd = 0;
		WS RCW_buf[2];
		DWORD RCW_len;
		// 複数行入力
		//   4  = [Ctrl]+[D]
		//   26 = [Ctrl]+[Z]
		if(bKeyInputMultiLine)
		{
			while(ReadConsoleW($StdinHandle, RCW_buf, 1, &RCW_len, NULL))
			{
				if(RCW_buf[0] == 4 || RCW_buf[0] == 26)
				{
					break;
				}
				rtn[uEnd] = RCW_buf[0];
				++uEnd;
				if(uEnd >= rtnMax)
				{
					rtnMax <<= 1;
					rtn = irealloc_WS(rtn, rtnMax);
				}
			}
		}
		// 一行入力
		//   13 = [Enter]
		else
		{
			while(ReadConsoleW($StdinHandle, RCW_buf, 1, &RCW_len, NULL))
			{
				if(RCW_buf[0] == 13)
				{
					break;
				}
				else if(RCW_buf[0] <= 26)
				{
					RCW_buf[0] = ' ';
				}
				rtn[uEnd] = RCW_buf[0];
				++uEnd;
				if(uEnd >= rtnMax)
				{
					rtnMax <<= 1;
					rtn = irealloc_WS(rtn, rtnMax);
				}
			}
		}
	return rtn;
}
//-----------------
// STDIN から読込
//-----------------
/* 例
	// STDIN に
	//   データがあれば読み取る（パイプからの標準入力を想定）
	//   空ならば手入力に移行する
	WS *wp1 = iCLI_getStdin(FALSE);
		if(*wp1)
		{
			LN(60);
			P1W(wp1);
		}
		else
		{
			P1("文字を入力してください。\n[Ctrl]+[D]＋[Enter] で終了\n\n");
			ifree(wp1);
			wp1 = iCLI_getStdin(TRUE);
				LN(60);
				P1W(wp1);
		}
		LN(60);
		P("%llu 文字（改行文字含む）\n\n", wcslen(wp1));
	ifree(wp1);
*/
// v2025-04-13
WS
*iCLI_getStdin(
	BOOL bKeyInput // STDINが空のとき TRUE=手入力モード／FALSE=空文字を返す
)
{
	WS *rtn = NULL;
	INT iStdin = fseeko64(STDIN, 0, SEEK_END);
	if(iStdin)
	{
		rtn = (
			bKeyInput ?
			iCLI_getKeyInput(TRUE) :
			icalloc_WS(0)
		);
	}
	// STDIN から読込
	else
	{
		rtn = iF_read(STDIN);
		// 別プログラムがコードページ／ESC制御を変更することがあるので再設定
		SetConsoleOutputCP(65001);
		iConsole_EscOn();
	}
	return rtn;
}
//---------------------
// コマンド実行／VOID
//---------------------
/* (例)
	// MSはCP932／65001間で文字化けするので使用不可
	iCLI_systemW(L"dir");
*/
// v2025-04-05
VOID
iCLI_systemW(
	WS *cmd
)
{
	STARTUPINFOW si;
		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(si);
	PROCESS_INFORMATION pi;
		ZeroMemory(&pi, sizeof(pi));
		if(CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
		{
			WaitForSingleObject(pi.hProcess, INFINITE);
		}
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
}
//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	Clipboard
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
/* (例)
	iClipboard_setText(L"クリップボードにコピー");
*/
// v2025-04-03
VOID
iClipboard_setText(
	CONST WS *str
)
{
	if(! str)
	{
		return;
	}
	UINT uLen = wcslen(str);
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
	P2W(iClipboard_getText());
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
// v2025-04-10
MS
*icnv_W2M(
	CONST WS *str,
	UINT uCP
)
{
	INT i1 = WideCharToMultiByte(uCP, 0, str, -1, NULL, 0, NULL, NULL);
	if(i1 > 0)
	{
		MS *rtn = icalloc_MS(i1);
		WideCharToMultiByte(uCP, 0, str, -1, rtn, i1, NULL, NULL);
		return rtn;
	}
	else
	{
		return icalloc_MS(0);
	}
}
// v2025-04-10
WS
*icnv_M2W(
	CONST MS *str,
	UINT uCP
)
{
	INT i1 = MultiByteToWideChar(uCP, 0, str, -1, NULL, 0);
	if(i1 > 0)
	{
		WS *rtn = icalloc_WS(i1);
		MultiByteToWideChar(uCP, 0, str, -1, rtn, i1);
		return rtn;
	}
	else
	{
		return icalloc_WS(0);
	}
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
	// strlen(p1) => NG
	P3(imn_len(p1)); //=> 0
*/
// v2023-07-29
UINT
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
UINT
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
// v2025-04-12
UINT
iun_bomLen(
	CONST MS *str
)
{
	if(imn_len(str) >= 3 && str[0] == (MS)0xEF && str[1] == (MS)0xBB && str[2] == (MS)0xBF)
	{
		return 3;
	}
	return 0;
}
// v2025-04-12
UINT
iun_len(
	CONST MS *str
)
{
	if(! str || ! *str)
	{
		return 0;
	}
	// BOM
	str += iun_bomLen(str);
	UINT rtn = 0;
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
// v2025-04-13
UINT
imn_CodePage(
	MS *str
)
{
	// UTF-8 ?
	if(! str || ! *str)
	{
		return 65001;
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
	// UTF-8 ?
	return 65001;
}
//---------------
// 文字列コピー
//---------------
/* (例)
	WS *from = L"あいうえお";
	WS *to = icalloc_WS(wcslen(from));
		iwv_cpy(to, from);
		P2W(to); //=> "あいうえお"
	ifree(to);
*/
// v2025-03-08
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
	while((*to++ = *from++));
	*to = '\0';
}
// v2025-03-08
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
	while((*to++ = *from++));
	*to = L'\0';
}
//-------------------------
// コピーした文字長を返す
//-------------------------
/* (例)
	// 力技による実装／"wcslen() + wmemcpy()" と差がない
	WS *to = iws_clone(L"ABCDEFGHIJKLMNOPQRSTUVWXYZ");
		UINT uEnd = iwn_cpy(to, L"c:\\windows");
			P3(uEnd); //=> 10
			P2W(to);  //=> "c:\windows"
		uEnd += iwn_ncpy((to + uEnd), L"\\123", 1);
			P3(uEnd); //=> 11
			P2W(to);  //=> "c:\windows\"
	ifree(to);
*/
// v2025-03-08
UINT
imn_cpy(
	MS *to,
	CONST MS *from
)
{
	if(! from)
	{
		return 0;
	}
	UINT rtn = 0;
	while((*to++ = *from++))
	{
		++rtn;
	}
	*to = '\0';
	return rtn;
}
// v2025-03-08
UINT
iwn_cpy(
	WS *to,
	CONST WS *from
)
{
	if(! from)
	{
		return 0;
	}
	UINT rtn = 0;
	while((*to++ = *from++))
	{
		++rtn;
	}
	*to = L'\0';
	return rtn;
}
// v2025-03-08
UINT
imn_ncpy(
	MS *to,
	CONST MS *from,
	UINT fromLen
)
{
	if(! from || ! fromLen)
	{
		return 0;
	}
	UINT rtn = 0;
	while(fromLen-- && (*to++ = *from++))
	{
		++rtn;
	}
	*to = '\0';
	return rtn;
}
// v2025-03-08
UINT
iwn_ncpy(
	WS *to,
	CONST WS *from,
	UINT fromLen
)
{
	if(! from || ! fromLen)
	{
		return 0;
	}
	UINT rtn = 0;
	while(fromLen-- && (*to++ = *from++))
	{
		++rtn;
	}
	*to = L'\0';
	return rtn;
}
//-----------------------
// 新規部分文字列を生成
//-----------------------
/* (例)
	WS *wFrom = L"あいうえお";
	WS *wp1 = iws_clone(wFrom);
		P2W(wp1); //=> "あいうえお"
	ifree(wp1);
	wp1 = iws_nclone(wFrom, 3);
		P2W(wp1); //=> "あいう"
	ifree(wp1);
*/
// v2025-04-03
MS
*ims_nclone(
	CONST MS *from,
	UINT fromLen
)
{
	MS *rtn = icalloc_MS(fromLen);
	if(from)
	{
		memcpy(rtn, from, fromLen);
	}
	return rtn;
}
// v2025-04-03
WS
*iws_nclone(
	CONST WS *from,
	UINT fromLen
)
{
	WS *rtn = icalloc_WS(fromLen);
	if(from)
	{
		wmemcpy(rtn, from, fromLen);
	}
	return rtn;
}
/* (例)
	WS *wp1 = iws_cats(5, L"123", NULL, L"abcde", L"", L"あいうえお");
		P2W(wp1); //=> "123abcdeあいうえお"
	ifree(wp1);
*/
// v2025-03-06
MS
*ims_cats(
	UINT size, // 要素数(n+1)
	...        // ary[0..n]
)
{
	MS **rtnAry = icalloc_MS_ary(size);
		UINT rtnLen = 0;
		va_list va;
		va_start(va, size);
			for(UINT _u1 = 0; _u1 < size; _u1++)
			{
				rtnAry[_u1] = va_arg(va, MS*);
				rtnLen += imn_len(rtnAry[_u1]);
			}
		va_end(va);
		MS *rtn = icalloc_MS(rtnLen);
			UINT uEnd = 0;
			for(UINT _u1 = 0; _u1 < size; _u1++)
			{
				uEnd += imn_cpy((rtn + uEnd), rtnAry[_u1]);
			}
	ifree(rtnAry);
	return rtn;
}
// v2025-03-06
WS
*iws_cats(
	UINT size, // 要素数(n+1)
	...        // ary[0..n]
)
{
	WS **rtnAry = icalloc_WS_ary(size);
		UINT rtnLen = 0;
		va_list va;
		va_start(va, size);
			for(UINT _u1 = 0; _u1 < size; _u1++)
			{
				rtnAry[_u1] = va_arg(va, WS*);
				rtnLen += iwn_len(rtnAry[_u1]);
			}
		va_end(va);
		WS *rtn = icalloc_WS(rtnLen);
			UINT uEnd = 0;
			for(UINT _u1 = 0; _u1 < size; _u1++)
			{
				uEnd += iwn_cpy((rtn + uEnd), rtnAry[_u1]);
			}
	ifree(rtnAry);
	return rtn;
}
//--------------------
// sprintf()の拡張版
//--------------------
/* (例)
	MS *mp1 = ims_sprintf("%s-%s%05d\n", "ABC", "123", 456);
		P1(mp1); //=> "ABC-12300456\n"
	ifree(mp1);
*/
// v2025-01-30
MS
*ims_sprintf(
	CONST MS *format,
	...
)
{
	FILE *oFp = fopen("NUL", "wb");
		va_list va;
		va_start(va, format);
			INT len = vfprintf(oFp, format, va) + 1; // '\0' 追加
			MS *rtn = icalloc_MS(len);
			vsnprintf(rtn, len, format, va);
		va_end(va);
	fclose(oFp);
	return rtn;
}
/* (例)
	WS *wp1 = iws_sprintf(L"ワイド文字：%S\n", L"あいうえお");
		P1W(wp1); //=> "ワイド文字：あいうえお\n"
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
	// PR("あいうえお", 3); NL();
	MS *mp1 = ims_repeat("あいうえお", 3);
		P2(mp1); //=> "あいうえおあいうえおあいうえお"
	ifree(mp1);
*/
// v2025-03-08
MS
*ims_repeat(
	CONST MS *str,
	UINT strRepeat
)
{
	UINT uStr = imn_len(str);
	UINT uSize = uStr * strRepeat;
	MS *rtn = icalloc_MS(uSize);
	UINT u1 = 0;
	while(u1 < uSize)
	{
		memcpy((rtn + u1), str, uStr);
		u1 += uStr;
	}
	return rtn;
}
/* (例)
	WS *wp1 = L"あいうえお@@@@かき@@くけこ";
	WS *wp2 = L"@@";
	UINT wp1Len = wcslen(wp1);
	UINT wp2Len = wcslen(wp2);
	UINT uEnd = 0;
	LN(60);
	P2W(wp1);
	P2W(wp2);
	LN(60);
	P2("[通常分割]");
	uEnd = 0;
	while(uEnd < wp1Len)
	{
		INT64 iPos = iwn_searchPos((wp1 + uEnd), wp2, wp2Len);
		if(iPos >= 0)
		{
			P("    +%-4lld", iPos);
			P2W((wp1 + uEnd));
			uEnd += iPos + wp2Len;
		}
		else
		{
			P("    +%-4u", (wp1Len - uEnd));
			P2W((wp1 + uEnd));
			break;
		}
	}
	LN(60);
	P2("[重複を排除して分割]");
	uEnd = 0;
	while(uEnd < wp1Len)
	{
		INT64 iPos = iwn_searchPos((wp1 + uEnd), wp2, wp2Len);
		if(iPos > 0)
		{
			P("    +%-4lld", iPos);
			P2W((wp1 + uEnd));
			uEnd += iPos + wp2Len;
		}
		else if(iPos == 0)
		{
			uEnd += wp2Len;
		}
		else
		{
			P("    +%-4u", (wp1Len - uEnd));
			P2W((wp1 + uEnd));
			break;
		}
	}
*/
// v2025-03-24
INT64
iwn_searchPos(
	WS *str,       // 文字列
	WS *search,    // 検索文字列
	UINT searchLen // wcslen(search)
)
{
	INT64 rtn = 0;
	while(*(str + rtn))
	{
		if(! wcsncmp((str + rtn), search, searchLen))
		{
			return rtn;
		}
		++rtn;
	}
	return -1;
}
/* (例)
	WS *wp1 = L"あい\r\nうえお\r\nかき\r\nくけこ";
	WS *wp2 = L"\r\n";
	P3(iwn_searchCnt(wp1, wp2)); //=> 3
*/
// v2024-03-25
UINT
iwn_searchCnt(
	WS *str,   // 文字列
	WS *search // 検索文字列
)
{
	UINT rtn = 0;
	UINT uStr = iwn_len(str);
	UINT uSearch = iwn_len(search);
	UINT uEnd = 0;
	while(uEnd < uStr)
	{
		if(! wcsncmp((str + uEnd), search, uSearch))
		{
			++rtn;
			uEnd += uSearch;
		}
		else
		{
			++uEnd;
		}
	}
	return rtn;
}
/* (例)
	//---------------
	// 戻り値／配列
	//---------------
	// [0]    検索数 n個
	// [1..n] 検索位置
	// [n+1]  文字列長
	//
	// 例えば、
	//   [0]=4, [1..4], [5]
	//   "" or NULL のとき [0]=0, [1]=wcslen(str)
	//
	WS *wp1 = L"ABC文字aBc文字abc";
	WS *wp2 = L"aBc";
	UINT64 *au1 = iwsa_searchPos(wp1, wp2, TRUE);
		// [0]
		P("[0] 検索数   %llu\n", au1[0]);
		for(UINT _u1 = 1; _u1 <= au1[0]; _u1++)
		{
			// [1..n]
			P("[%u] 検索位置 %llu\n", _u1, au1[_u1]);
		}
		// [n+1]
		P("[%llu] 文字列長 %llu\n", (au1[0] + 1), au1[(au1[0] + 1)]);
	ifree(au1);
*/
// v2025-03-25
UINT64
*iwsa_searchPos(
	WS *str,    // 文字列
	WS *search, // 検索文字列
	BOOL icase  // TRUE=大文字小文字区別しない
)
{
	CONST UINT strLen = iwn_len(str);
	CONST UINT searchLen = iwn_len(search);
	if(! strLen || ! searchLen)
	{
		UINT64 *rtn = icalloc_UINT64(2);
		rtn[0] = 0;
		rtn[1] = strLen;
		return rtn;
	}
	WS *str2 = NULL;
	WS *search2 = NULL;
	if(icase)
	{
		str2 = iws_clone(str);
			CharUpperW(str2);
		search2 = iws_clone(search);
			CharUpperW(search2);
	}
	else
	{
		str2 = str;
		search2 = search;
	}
	WS *wp1 = str2;
	// +2 => rtn[0], rtn[n+1]
	UINT64 *rtn = icalloc_UINT64(iwn_searchCnt(wp1, search2) + 2);
		UINT uIndex = 0;
		UINT64 uPos = 0;
		while(uPos < strLen)
		{
			if(! wcsncmp((wp1 + uPos), search2, searchLen))
			{
				// rtn[1..n]
				++uIndex;
				rtn[uIndex] = uPos;
				uPos += searchLen;
			}
			else
			{
				++uPos;
			}
		}
		// rtn[n+1]
		rtn[(uIndex + 1)] = strLen;
		// rtn[0]
		rtn[0] = (UINT64)uIndex;
		if(icase)
		{
			ifree(search2);
			ifree(str2);
		}
	return rtn;
}
//---------------------------
// 文字列を分割し配列を作成
//---------------------------
/* (例)
	WS **aw1, **aw2;
	WS *str = L" 2025////3/26  11:19:50   ";

	LN(60);

	// 重複排除
	aw1 = iwsa_split(str, TRUE, 3, L"/", L":", L" ");
		iwav_print(aw1);
	ifree(aw1);

	LN(60);

	// 通常分割
	aw2 = iwsa_split(str, FALSE, 3, L"/", L":", L" ");
		iwav_print(aw2);
	ifree(aw2);

	LN(60);
*/
// v2025-03-29
WS
**iwsa_nsplit(
	WS *str,         // 文字列
	UINT strLen,     // 文字列サイズ／部分抽出可能
	BOOL ignoreNull, // TRUE=重複排除
	UINT size,       // 要素数(n+1)
	...              // ary[0..n]
)
{
	if(! str || ! *str)
	{
		return icalloc_WS_ary(0);
	}
	WS **sepAry = icalloc_WS_ary(size);
		va_list va;
		va_start(va, size);
			for(UINT _u1 = 0; _u1 < size; _u1++)
			{
				sepAry[_u1] = va_arg(va, WS*);
			}
		va_end(va);
		WS *rtnBase = iws_nclone(str, strLen);
		UINT _u1 = 0;
		while(_u1 < size)
		{
			WS *wTmp = iws_replace(rtnBase, sepAry[_u1], (WS*)L"\a", FALSE);
			ifree(rtnBase);
			rtnBase = wTmp;
			++_u1;
		}
		WS *pBgn = rtnBase;
		WS **rtn = icalloc_WS_ary(iwn_searchCnt(pBgn, (WS*)L"\a") + 1);
			UINT uBgn = wcslen(pBgn);
			UINT uEnd = 0;
			_u1 = 0;
			while(uEnd < uBgn)
			{
				INT64 iPos = iwn_searchPos((pBgn + uEnd), (WS*)L"\a", 1);
				if(iPos >= 0)
				{
					if(! iPos && ignoreNull)
					{
						// 重複排除
					}
					else
					{
						rtn[_u1] = iws_nclone((pBgn + uEnd), iPos);
						++_u1;
					}
					uEnd += iPos + 1;
				}
				else
				{
					rtn[_u1] = iws_clone((pBgn + uEnd));
					break;
				}
			}
		ifree(rtnBase);
	ifree(sepAry);
	return rtn;
}
//-------------
// 文字列置換
//-------------
/* (例)
	P2W(iws_replace(L"100YEN yen", L"YEN", L"円", TRUE));  //=> "100円 円"
	P2W(iws_replace(L"100YEN yen", L"YEN", L"円", FALSE)); //=> "100円 yen"
	P2W(iws_replace(L"100YEN yen", L"YEN", L"", FALSE));   //=> "100 yen"
	P2W(iws_replace(L"100YEN yen", L"YEN", NULL, FALSE));  //=> "100"
*/
// v2025-03-29
WS
*iws_replace(
	WS *from,   // 文字列
	WS *before, // 変換前の文字列
	WS *after,  // 変換後の文字列／NULL（0, L'\0'）への変換も可
	BOOL icase  // TRUE=大文字小文字区別しない
)
{
	UINT uBefore = iwn_len(before);
	UINT uAfter = iwn_len(after);
	if(! after)
	{
		uAfter = 1;
	}
	UINT64 *au1 = iwsa_searchPos(from, before, icase);
		WS *rtn = icalloc_WS(wcslen(from) + ((uAfter - uBefore) * au1[0]));
		UINT fromEnd = 0;
		UINT rtnEnd = 0;
		// au1[1..n]
		UINT u1 = 1;
		while(u1 <= au1[0])
		{
			if(au1[u1] == fromEnd)
			{
				if(! after)
				{
					*(rtn + rtnEnd) = L'\0';
				}
				else
				{
					wmemcpy((rtn + rtnEnd), after, uAfter);
				}
				rtnEnd += uAfter;
				fromEnd += uBefore;
				++u1;
			}
			else
			{
				UINT u2 = au1[u1] - fromEnd;
				wmemcpy((rtn + rtnEnd), (from + fromEnd), u2);
				rtnEnd += u2;
				fromEnd += u2;
			}
		}
		wcscpy((rtn + rtnEnd), (from + fromEnd));
	ifree(au1);
	return rtn;
}
//-------------------------
// 数値を３桁区切りで出力
//-------------------------
/* (例)
	DOUBLE d1 = -1234567.890;
	MS *p1 = NULL;
		p1 = ims_IntToMs(d1); //=> -1,234,567／切り捨て
		P2(p1);
	ifree(p1);
	p1 = ims_DblToMs(d1, 4);  //=> -1,234,567.8900
		P2(p1);
	ifree(p1);
	p1 = ims_DblToMs(d1, 2);  //=> -1,234,567.89
		P2(p1);
	ifree(p1);
	p1 = ims_DblToMs(d1, 1);  //=> -1,234,567.9
		P2(p1);
	ifree(p1);
	p1 = ims_DblToMs(d1, 0);  //=> -1,234,568／四捨五入
		P2(p1);
	ifree(p1);
*/
// v2025-03-08
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
		pEnd += imn_ncpy(pEnd, p1, u2);
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
		pEnd += imn_ncpy(pEnd, p1, 3);
		p1 += 3;
		u1 -= 3;
	}
	ifree(pNum);
	return rtn;
}
// v2025-03-08
MS
*ims_DblToMs(
	DOUBLE num, // 小数
	UINT uDigit // 小数点桁数
)
{
	if(! uDigit)
	{
		num = round(num);
	}
	DOUBLE dDecimal = fabs(fmod(num, 1.0));
	MS *format = ims_sprintf("%%.%uf", uDigit);
		MS *p1 = ims_IntToMs(num);
		MS *p2 = ims_sprintf(format, dDecimal);
			MS *rtn = ims_cats(2, p1, (p2 + 1));
		ifree(p2);
		ifree(p1);
	ifree(format);
	return rtn;
}
//-------------------------------------------------------
// 文字列前後の空白 '\n' '\r' ' ' '\t' '\f' '\v' を消去
//-------------------------------------------------------
// v2025-04-05
MS
*ims_strip(
	MS *str,
	BOOL bStripLeft,
	BOOL bStripRight
)
{
	if(! str)
	{
		return icalloc_MS(0);
	}
	INT iAll = strlen(str);
	// 末尾 -> 先頭
	INT iIndexR = iAll - 1;
	if(bStripRight)
	{
		while(str[iIndexR] == '\n' || str[iIndexR] == '\r' || str[iIndexR] == ' ' || str[iIndexR] == '\t' || str[iIndexR] == '\f' || str[iIndexR] == '\v')
		{
			if(! iIndexR)
			{
				return icalloc_MS(0);
			}
			--iIndexR;
		}
	}
	// 先頭 -> 末尾
	INT iIndexL = 0;
	if(bStripLeft)
	{
		while(str[iIndexL] == ' ' || str[iIndexL] == '\t' || str[iIndexL] == '\n' || str[iIndexL] == '\r' || str[iIndexL] == '\f' || str[iIndexL] == '\v')
		{
			++iIndexL;
			if(! str[iIndexL])
			{
				return icalloc_MS(0);
			}
		}
	}
	return ims_nclone((str + iIndexL), (iIndexR - iIndexL + 1));
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
		P2W(wp1);
	WS *wp2 = iws_withoutESC(wp1);
		P2W(wp2);
	ifree(wp2);
*/
// v2024-05-26
WS
*iws_withoutESC(
	WS *str
)
{
	WS *rtn = icalloc_WS(iwn_len(str));
	WS *uEnd = rtn;
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
		*uEnd++ = *str++;
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
	P3(iwan_size($ARGV));
*/
// v2025-03-08
UINT
iwan_size(
	WS **ary
)
{
	UINT rtn = 0;
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
	P3(iwan_strlen($ARGV));
*/
// v2025-03-08
UINT
iwan_strlen(
	WS **ary
)
{
	UINT rtn = 0;
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
	P2W(iwas_njoin($ARGV, L", ", 0, $ARGC));
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
	WS **wa1 = { };
	INT i1 = 0;
	//
	// TRUE=大小文字区別しない
	//
	wa1 = iwaa_uniq(args, TRUE);
		i1 = 0;
		while((wa1[i1]))
		{
			P2W(wa1[i1]); //=> "aaa", "BBB"
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
			P2W(wa1[i1]); //=> "aaa", "AAA", "BBB", "bbb"
			++i1;
		}
	ifree(wa1);
*/
// v2025-03-22
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
					if(! _wcsicmp(ary[u1], ary[u2]))
					{
						iAryFlg[u2] = -1; // NG
					}
				}
				else
				{
					if(! wcscmp(ary[u1], ary[u2]))
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
				if(iF_chkDirName(wa1[u1]) && PathFileExistsW(wa1[u1]))
				{
					rtn[u2] = iF_getAPath(wa1[u1]);
					++u2;
				}
			}
			if(iType == 0 || iType == 2)
			{
				if(! iF_chkDirName(wa1[u1]) && PathFileExistsW(wa1[u1]))
				{
					WS *_wp1 = icalloc_WS(IMAX_PATHW);
					_wfullpath(_wp1, wa1[u1], IMAX_PATHW);
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
// v2025-03-22
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
			if(! _wcsnicmp(rtn[u2], rtn[u1], wcslen(rtn[u1])))
			{
				ifree(rtn[u2]);
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
// v2025-01-31
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
		P("\033[5G\033[92m[%u]\033[0m '%s'\n", u1, ary[u1]);
		++u1;
	}
}
// v2025-01-31
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
		P("\033[5G\033[92m[%u]\033[0m '", u1);
		P1W(ary[u1]);
		P2("'");
		++u1;
	}
}
/* (例)
	iwav_print2($ARGV, L"-- ", L"\n");
*/
// v2025-03-22
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
	MS *pL = W2M(sLeft);
	MS *pR = W2M(sRight);
		UINT u1 = 0;
		while(ary[u1])
		{
			P1(pL);
			QP1W(ary[u1]);
			P1(pR);
			++u1;
		}
	ifree(pR);
	ifree(pL);
}
//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	Variable Buffer
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
/* (例)
	#define _printUsed() PL();P("%3d / %3d\n", iVBW_getLength(iVBW), iVBW_getSize(iVBW))
	$struct_iVBW *iVBW = iVBW_alloc();
		iVBW_push2(iVBW, L"1234567890");
			P2W(iVBW_getStr(iVBW)); //=> "1234567890"
			_printUsed();
		iVBW_clear(iVBW);
			_printUsed();
		iVBW_push2(iVBW, L"あいうえお");
			P2W(iVBW_getStr(iVBW)); //=> "あいうえお"
			_printUsed();
		iVBW_push_sprintf(iVBW, L"%.5f", 123456.7890);
		iVBW_push_sprintf(iVBW, L"%lc", 97);
		iVBW_push_sprintf(iVBW, L"%d", 2025);
			P2W(iVBW_getStr(iVBW)); //=> "あいうえお123456.78900a2025"
			_printUsed();
		iVBW_pop(iVBW, 17);
			P2W(iVBW_getStr(iVBW)); //=> "あいうえお"
			_printUsed();
	WS *wp1 = iVBW_free(iVBW);
		P2W(wp1);                   //=> "あいうえお"
	ifree(wp1);
*/
// v2025-03-17
$struct_iVB
*iVB_alloc(
	UINT sizeOf // sizeof(MS), sizeof(WS)
)
{
	UINT strLen = 1024;
	$struct_iVB *iVB = ($struct_iVB*)icalloc(1, sizeof($struct_iVB), FALSE);
		iVB->sizeOf = sizeOf;
		iVB->length = 0;
		iVB->freeSize = strLen;
		iVB->str = icalloc(iVB->freeSize, iVB->sizeOf, FALSE);
	return iVB;
}
// v2025-03-10
VOID
iVBM_push(
	$struct_iVBM *iVBM, // 格納場所
	CONST MS *str,      // 追記する文字列
	UINT strLen         // 追記する文字列長／strlen(str), wcslen(str)
)
{
	if(strLen >= iVBM->freeSize)
	{
		iVBM->freeSize = iVBM->length + strLen;
		iVBM->str = irealloc_MS(iVBM->str, (iVBM->length + iVBM->freeSize));
	}
	memcpy(((MS*)iVBM->str + iVBM->length), str, (strLen + 1));
	iVBM->length += strLen;
	iVBM->freeSize -= strLen;
}
// v2025-03-10
VOID
iVBW_push(
	$struct_iVBW *iVBW, // 格納場所
	CONST WS *str,      // 追記する文字列
	UINT strLen         // 追記する文字列長／strlen(str), wcslen(str)
)
{
	if(strLen >= iVBW->freeSize)
	{
		iVBW->freeSize = iVBW->length + strLen;
		iVBW->str = irealloc_WS(iVBW->str, (iVBW->length + iVBW->freeSize));
	}
	wmemcpy(((WS*)iVBW->str + iVBW->length), str, (strLen + 1));
	iVBW->length += strLen;
	iVBW->freeSize -= strLen;
}
// v2025-03-03
VOID
iVBM_push_sprintf(
	$struct_iVBM *iVBM,
	CONST MS *format,
	...
)
{
	FILE *oFp = fopen("NUL", "wb");
		va_list va;
		va_start(va, format);
			INT iLen = vfprintf(oFp, format, va);
			if(iLen > 0 && (UINT)iLen >= iVBM->freeSize)
			{
				iVBM->freeSize += iLen;
				iVBM->str = irealloc(iVBM->str, (iVBM->length + iVBM->freeSize), iVBM->sizeOf);
			}
			vsnprintf(((MS*)iVBM->str + iVBM->length), (iLen + 1), format, va);
			iVBM->length += iLen;
			iVBM->freeSize -= iLen;
		va_end(va);
	fclose(oFp);
}
// v2025-03-03
VOID
iVBW_push_sprintf(
	$struct_iVBW *iVBW,
	CONST WS *format,
	...
)
{
	FILE *oFp = fopen("NUL", "wb");
		va_list va;
		va_start(va, format);
			INT iLen = vfwprintf(oFp, format, va);
			if(iLen > 0 && (UINT)iLen >= iVBW->freeSize)
			{
				iVBW->freeSize += iLen;
				iVBW->str = irealloc(iVBW->str, (iVBW->length + iVBW->freeSize), iVBW->sizeOf);
			}
			vswprintf(((WS*)iVBW->str + iVBW->length), (iLen + 1), format, va);
			iVBW->length += iLen;
			iVBW->freeSize -= iLen;
		va_end(va);
	fclose(oFp);
}
// v2025-03-04
VOID
iVBM_pop(
	$struct_iVBM *iVBM,
	UINT strLen
)
{
	if(! strLen)
	{
		return;
	}
	if(strLen > iVBM->length)
	{
		strLen = iVBM->length;
	}
	iVBM->length -= strLen;
	iVBM->freeSize += strLen;
	memset(((MS*)iVBM->str + iVBM->length), 0, strLen);
}
// v2025-03-04
VOID
iVBW_pop(
	$struct_iVBW *iVBW,
	UINT strLen
)
{
	if(! strLen)
	{
		return;
	}
	if(strLen > iVBW->length)
	{
		strLen = iVBW->length;
	}
	iVBW->length -= strLen;
	iVBW->freeSize += strLen;
	wmemset(((WS*)iVBW->str + iVBW->length), 0, strLen);
}
// v2025-03-03
VOID
*iVB_free(
	$struct_iVB *iVB,
	BOOL bReturnStr
)
{
	VOID *rtn = 0;
	if(bReturnStr)
	{
		rtn = iVB->str;
	}
	else
	{
		ifree(iVB->str);
	}
	ifree(iVB);
	return rtn;
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
	WS *dir = iF_getRPath(L".");
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
			iwv_cpy(FI->sPath, path);
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
					WS *wp1 = NULL;
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
// v2025-04-02
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
	if(! fname)
	{
		// 後述 FI->uFname で対応
	}
	// Dir "." ".." は除外
	//   else if(fname[0] == L'.' && (! fname[1] || (fname[1] == L'.' && ! fname[2])))
	//   else if((! fname[2] && fname[1] == L'.' && fname[0] == L'.') || (! fname[1] && fname[0] == L'.'))
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
			*(FI->sPath + dirLen) = L'\0';     // '\0' 付与
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
[2024-01-13]
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
	P3(iFchk_BfileW(L"aaa.exe")); //=> TRUE
	P3(iFchk_BfileW(L"aaa.txt")); //=> FALSE
	P3(iFchk_BfileW(L"???"));     //=> FALSE (存在しないとき)
*/
// v2025-04-12
BOOL
iF_chkBinfile(
	WS *Fn
)
{
	BOOL rtn = FALSE;
	FILE *iFp = _wfopen(Fn, L"rb");
	if(iFp)
	{
		INT c = 0;
		while((c = getc(iFp)) != EOF)
		{
			if(! c)
			{
				rtn = TRUE;
				break;
			}
		}
		fclose(iFp);
	}
	return rtn;
}
//---------------------
// ファイル名等を抽出
//---------------------
/* (例)
	WS *p1 = L"c:\\windows\\win.ini";
	P2W(iF_getExtPathname(p1, 0)); //=> "c:\windows\win.ini"
	P2W(iF_getExtPathname(p1, 1)); //=> "win.ini"
	P2W(iF_getExtPathname(p1, 2)); //=> "win"
*/
// v2025-03-08
WS
*iF_getExtPathname(
	WS *path,
	INT option // 1=拡張子付きファイル名／2=拡張子なしファイル名
)
{
	UINT uPath = iwn_len(path);
	if(! uPath)
	{
		return icalloc_WS(0);
	}
	WS *rtn = icalloc_WS(uPath);
	// Dir or File ?
	if(iF_chkDirName(path))
	{
		if(option == 0)
		{
			wmemcpy(rtn, path, uPath);
		}
	}
	else
	{
		switch(option)
		{
			// path
			case(0):
				wmemcpy(rtn, path, uPath);
				break;
			// name + ext
			case(1):
				iwv_cpy(rtn, PathFindFileNameW(path));
				break;
			// name
			case(2):
				WS *pBgn = PathFindFileNameW(path);
				WS *pEnd = PathFindExtensionW(pBgn);
				iwn_ncpy(rtn, pBgn, (pEnd - pBgn));
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
	P2W(iF_getAPath(L".")); //=> "d:\foo\"
*/
// v2024-03-08
WS
*iF_getAPath(
	WS *path
)
{
	WS *p1 = iws_cutYenR(path);
	if(! *p1)
	{
		return p1;
	}
	WS *rtn = NULL;
	// "c:" のような表記は特別処理
	if(p1[1] == ':' && wcslen(p1) == 2 && iF_chkDirName(p1))
	{
		rtn = wcscat(p1, L"\\");
	}
	else
	{
		rtn = icalloc_WS(IMAX_PATHW);
		if(iF_chkDirName(p1) && _wfullpath(rtn, p1, IMAX_PATHW))
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
	P2W(iF_getRPath(L".")); //=> ".\"
*/
// v2024-03-08
WS
*iF_getRPath(
	WS *path
)
{
	WS *rtn = iws_cutYenR(path);
	if(! *rtn)
	{
		return rtn;
	}
	if(iF_chkDirName(path))
	{
		wcscat(rtn, L"\\");
	}
	return rtn;
}
//--------------------
// 複階層のDirを作成
//--------------------
/* (例)
	P3(iF_mkdir(L"aaa\\bbb")); //=> 2
*/
// v2025-03-08
UINT
iF_mkdir(
	WS *path
)
{
	UINT rtn = 0;
	WS *pBgn = iws_cats(2, path, L"\\");
		UINT uEnd = 0;
		while(*(pBgn + uEnd))
		{
			if(*(pBgn + uEnd) == '\\')
			{
				WS *wp1 = iws_nclone(pBgn, uEnd);
					if(CreateDirectoryW(wp1, NULL))
					{
						++rtn;
					}
				ifree(wp1);
			}
			++uEnd;
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
// v2025-04-05
WS
**iF_trash(
	WS *path
)
{
	WS **rtn = icalloc_WS_ary(0);
	if(! path)
	{
		return rtn;
	}
	// '\t' '\r' '\n' で分割
	WS **awp1 = iwsa_split(path, TRUE, 3, L"\t", L"\r", L"\n");
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
					// フォルダごと移動されたファイルは、ここでは存在しない。
					if(iF_chkExistPath(awp2[_u1]))
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
//---------------------
// FILEポインタを開く
//---------------------
/* (例)
	// $ARGV[0] に指定したファイルを表示
	FILE *iFp = _wfopen($ARGV[0], L"rb");
	if(iFp)
	{
		WS *wp1 = iF_read(iFp);
			P2W(wp1);
		ifree(wp1);
		fclose(iFp);
	}
*/
// v2025-04-13
WS
*iF_read(
	FILE *iFp
)
{
	CONST UINT BufLen = 1024;
	UINT BufSize = BufLen;
	MS *Buf = icalloc_MS(BufSize);
		UINT uEnd = 0;
		UINT uCnt = 0;
		// Win32API ReadFile() は日本語表示しないので使用しない
		while((uCnt = fread((Buf + uEnd), sizeof(MS), BufLen, iFp)))
		{
			uEnd += uCnt;
			if(uEnd >= BufSize)
			{
				BufSize <<= 1;
				Buf = irealloc_MS(Buf, BufSize);
			}
		}
		// 文字コードは直前のSTDOUT（CP65001／CP932）に依存するため都度解析
		// UTF-8 BOM はスルー
		WS *rtn = icnv_M2W((Buf + iun_bomLen(Buf)), imn_CodePage(Buf));
	ifree(Buf);
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
	P3(idate_chk_ymdhnsW(NULL));                   //=> FALSE
	P3(idate_chk_ymdhnsW(L""));                    //=> FALSE
	P3(idate_chk_ymdhnsW(L"abc"));                 //=> FALSE
	P3(idate_chk_ymdhnsW(L"-1234.56"));            //=> FALSE
	P3(idate_chk_ymdhnsW(L"+2022-09-22"));         //=> TRUE
	P3(idate_chk_ymdhnsW(L"2022/09/22 13:40:10")); //=> TRUE
	P3(idate_chk_ymdhnsW(L"-100/1/1"));            //=> TRUE
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
		P3(i_y); //=> 2012
		P3(i_m); //=> 2
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
	P3(idate_chk_month_end(2012, 2, 28)); //=> FALSE
	P3(idate_chk_month_end(2012, 2, 29)); //=> TRUE
	P3(idate_chk_month_end(2012, 1, 59)); //=> FALSE
	P3(idate_chk_month_end(2012, 1, 60)); //=> TRUE
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
		P3(ai1[i1]); //=> -2012, 8, 12, 12, 45, 0
	}
	ifree(ai1);
*/
// v2025-03-28
INT
*idate_WsToiAryYmdhns(
	WS *str // (例) "2012-03-12 13:40:00"
)
{
	CONST UINT uArySize = 6;
	INT *rtn = icalloc_INT(uArySize);
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
	WS **as1 = iwsa_split(str, TRUE, 5, L"/", L".", L"-", L":", L" ");
		UINT u1 = 0;
		while(u1 < uArySize && as1[u1])
		{
			rtn[u1] = _wtoi(as1[u1]);
			++u1;
		}
	ifree(as1);
	rtn[0] *= i1;
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
	DOUBLE cjd
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
		P3(IDV->y); //=> 1582
		P3(IDV->m); //=> 10
		P3(IDV->d); //=> 3
		P3(IDV->h); //=> 0
		P3(IDV->n); //=> 0
		P3(IDV->s); //=> 0
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
// v2025-03-19
/*
INT
irand_INT(
	INT min,
	INT max
)
{
	return (min + (INT)(rand() * (max - min + 1.0) / (1.0 + RAND_MAX)));
}
// v2025-03-29
VOID
iDV_checker(
	INT yearFrom, // 何年から
	INT yearTo,   // 何年まで
	UINT uSample  // サンプル抽出数
)
{
	$struct_idate_value *IDV1 = iDV_alloc();
	$struct_idate_value *IDV2 = iDV_alloc();
	$struct_idate_value *IDV3 = iDV_alloc();
	$struct_idate_value *IDV4 = iDV_alloc();
		CONST UINT BufLen = 128;
		MS Buf[BufLen];
		INT rndYear = yearTo - yearFrom;
		P2("\033[94m----Cnt----From----------To------------Sign  Y  M  D----DateAdd-------Check----\033[0m");
		srand((UINT)time(NULL));
		for(UINT _u1 = 1; _u1 <= uSample; _u1++)
		{
			INT iFrom = yearFrom + irand_INT(0, rndYear);
			INT iTo = yearTo - irand_INT(0, rndYear);
			if(! (iFrom & 1))
			{
				INT i1 = iFrom;
				iFrom = iTo;
				iTo = i1;
			}
			iDV_set(
				IDV1,
				iFrom,
				1 + irand_INT(0, 11),
				1 + irand_INT(0, 30),
				0, 0, 0
			);
			iDV_set(
				IDV2,
				iTo,
				1 + irand_INT(0, 11),
				1 + irand_INT(0, 30),
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
			memset(Buf, 0, (BufLen * sizeof(MS)));
			INT i1 = sprintf(
				Buf,
				"\033[37m"	"%7u"
				"\033[32m"	"%8d-%02d-%02d"
				"\033[33m"	"%8d-%02d-%02d"
				"\033[32m\033[4C"	"%s %5d %2d %2d"
				"\033[33m"	"%8d-%02d-%02d"
				"\033[4C"	"%s"	"\033[0m\n"
				, _u1
				, IDV1->y, IDV1->m, IDV1->d
				, IDV2->y, IDV2->m, IDV2->d
				, (IDV3->sign ? "+" : "-"), IDV3->y, IDV3->m, IDV3->d
				, IDV4->y, IDV4->m, IDV4->d
				, (
					idate_ymdToINT(IDV2->y, IDV2->m, IDV2->d) == idate_ymdToINT(IDV4->y, IDV4->m, IDV4->d) ?
					"\033[36mok" :
					"\033[91mNG"
				)
			);
			///P3(i1);
			QP(Buf, i1);
		}
	iDV_free(IDV4);
	iDV_free(IDV3);
	iDV_free(IDV2);
	iDV_free(IDV1);
	NL();
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
			P2W(wp1); //=> "+2024-01-21(Su) 2460331.500000"
		ifree(wp1);
	iDV_free(IDV);
*/
// v2025-03-03
WS
*idate_format(
	CONST WS *format,
	BOOL bSign,  // TRUE='+'／FALSE='-'
	INT iY,      // 年
	INT iM,      // 月
	INT iD,      // 日
	INT iH,      // 時
	INT iN,      // 分
	INT iS,      // 秒
	DOUBLE dDays // 通算日／idate_diff()で使用
)
{
	if(! format)
	{
		return icalloc_WS(0);
	}
	$struct_iVBW *iVBW = iVBW_alloc();
		DOUBLE cjd = (dDays ? dDays : idate_ymdhnsToCjd(iY, iM, iD, iH, iN, iS));
		INT64 iDays = (INT64)cjd;
		// 符号チェック
		if(iY < 0)
		{
			bSign = FALSE;
			iY = -(iY);
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
						iVBW_push2(iVBW, idate_cjd_Wday(cjd));
						break;
					case 'A': // 曜日数
						iVBW_push_sprintf(iVBW, L"%d", idate_cjd_iWday(cjd));
						break;
					case 'c': // 年内の通算日
						iVBW_push_sprintf(iVBW, L"%d", idate_cjd_yeardays(cjd));
						break;
					case 'C': // CJD通算日
						iVBW_push_sprintf(iVBW, L"%.6f", cjd);
						break;
					case 'J': // JD通算日
						iVBW_push_sprintf(iVBW, L"%.6f", (cjd - CJD_TO_JD));
						break;
					case 'e': // 年内の通算週
						iVBW_push_sprintf(iVBW, L"%d", idate_cjd_yearweeks(cjd));
						break;
					// Diff
					case 'Y': // 通算年
						iVBW_push_sprintf(iVBW, L"%d", iY);
						break;
					case 'M': // 通算月
						iVBW_push_sprintf(iVBW, L"%d", (iY * 12) + iM);
						break;
					case 'D': // 通算日
						iVBW_push_sprintf(iVBW, L"%lld", iDays);
						break;
					case 'H': // 通算時
						iVBW_push_sprintf(iVBW, L"%lld", (iDays * 24) + iH);
						break;
					case 'N': // 通算分
						iVBW_push_sprintf(iVBW, L"%lld", (iDays * 24 * 60) + (iH * 60) + iN);
						break;
					case 'S': // 通算秒
						iVBW_push_sprintf(iVBW, L"%lld", (iDays * 24 * 60 * 60) + (iH * 60 * 60) + (iN * 60) + iS);
						break;
					case 'W': // 通算週
						iVBW_push_sprintf(iVBW, L"%lld", (iDays / 7));
						break;
					case 'w': // 通算週の余日
						iVBW_push_sprintf(iVBW, L"%lld", (iDays / 7));
						break;
					// 共通
					case 'g': // Sign "-" "+"
						iVBW_push2(iVBW, (bSign ? L"+" : L"-"));
						break;
					case 'G': // Sign "-" のみ
						if(! bSign)
						{
							iVBW_push2(iVBW, L"-");
						}
						break;
					case 'y': // 年
						iVBW_push_sprintf(iVBW, L"%04d", iY);
						break;
					case 'm': // 月
						iVBW_push_sprintf(iVBW, L"%02d", iM);
						break;
					case 'd': // 日
						iVBW_push_sprintf(iVBW, L"%02d", iD);
						break;
					case 'h': // 時
						iVBW_push_sprintf(iVBW, L"%02d", iH);
						break;
					case 'n': // 分
						iVBW_push_sprintf(iVBW, L"%02d", iN);
						break;
					case 's': // 秒
						iVBW_push_sprintf(iVBW, L"%02d", iS);
						break;
					case '%':
						iVBW_push2(iVBW, L"%");
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
						iVBW_push2(iVBW, L"\a");
						break;
					case('n'):
						iVBW_push2(iVBW, L"\n");
						break;
					case('t'):
						iVBW_push2(iVBW, L"\t");
						break;
					default:
						--format;
						break;
				}
			}
			else
			{
				iVBW_push_sprintf(iVBW, L"%lc", *format);
			}
			++format;
		}
	return iVBW_free(iVBW);
}
/* (例)
	P2W(idate_format_cjdToWS(NULL, CJD_NOW_LOCAL()));
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
	LN(60);
	P2W(str);
	INT *ai = idate_nowToiAryYmdhns_localtime();
		WS *p1 = idate_replace_format_ymdhns(
			str,
			L"[", L"]",
			L"'",
			ai[0], ai[1], ai[2], ai[3], ai[4], ai[5]
		);
			LN(60);
			P2W(p1);
			LN(60);
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
// v2025-03-23
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
	WS *rtn = NULL;
	UINT u1 = iwn_searchCnt(str, quoteBgn);
	UINT u2 = iwn_searchCnt(str, quoteEnd);
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
		if(*quoteBgn && ! wcsncmp(pStrEnd, quoteBgn, wcslen(quoteBgn)) && wcsncmp((pStrEnd + uQuoteBgn), quoteBgn, wcslen(quoteBgn)))
		{
			pStrEnd += uQuoteBgn;
			pStrBgn = pStrEnd;
			// "]" を探す
			if(*quoteEnd)
			{
				for(WS *_p1 = pStrBgn; _p1; _p1++)
				{
					if(! wcsncmp(_p1, quoteEnd, wcslen(quoteEnd)))
					{
						pStrEnd = _p1;
						break;
					}
				}
				// 解析用文字列
				WS *pDt = iws_nclone(pStrBgn, (pStrEnd - pStrBgn + 1));
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
						WS *p2 = NULL;
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
	INT *ai1 = idate_nowToiAryYmdhns(0); // System(-9h) => 2012, 6, 18, 15, 0, 0, 0
	INT *ai2 = idate_nowToiAryYmdhns(1); // Local       => 2012, 6, 19, 0, 0, 0, 0
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
