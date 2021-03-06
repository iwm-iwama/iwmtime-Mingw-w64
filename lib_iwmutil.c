#include "lib_iwmutil.h"
/* 2011-04-16
	入口でのチェックは、利用者に『安全』を提供する。
	知り得ている危険に手を抜いてはいけない。たとえ、コード量が増加しようとも。
	『安全』と『速度』は反比例しない。
*/
/* 2013-01-31
	マクロで関数を書くな。デバッグが難しくなる。
	「コードの短さ」より「コードの生産性」を優先せよ。
*/
/* 2014-01-03
	今更ながら・・・、
	自作関数の戻値は以下のルールに拠る。
		◆BOOL系
			TRUE || FALSE を返す。
		◆MBS*系
			基本、複製文字列を返す。
			(例)
				MBS *function(MBS *引数)
				{
					if(失敗)           return NULL;
					if(引数 == 空文字) return "";
					if(引数 == 戻値)   return ims_clone(引数); // 複製
					MBS *rtn = icalloc_MBS(Byte数); // 新規
					...
					return rtn;
				}
*/
/* 2014-01-03
	動的メモリ確保( = icalloc_XXX()等)と解放を丁寧に行えば、
	十分な『安全』と『速度』を今時のハードウェアは提供する。
*/
/* 2014-02-13 (2022-04-02修正)
	変数ルール／Minqw-w64
		◆INT    : (+-31bit) 標準
		◆UINT   : (+ 32bit) size_t, NTFS関係
		◇INT64  : (+-63bit) 暦数
		◇DOUBLE : 小数
*/
/* 2016-08-19 (2021-11-11修正)
	大域変数・定数表記
		◆iwmutil共通の変数 // "$" + 大文字
			$CMD, $ARGV など
		◆特殊な大域変数 // "$" + 小文字
			$struct_func() <=> $struct_var
			$union_func() <=> $union_var
		◆特殊な大域変数から派生した大域変数
		◆関数に付随した大域変数
			__func_str
		◆通常の大域変数 // "_"の次は小文字
			_str
		◆#define(定数のみ) // １文字目は大文字
			STR／Str
*/
/* 2016-01-27 (2022-04-01修正)
	基本関数名ルール
		[1] i = iwm-iwama
		[2] m = MBS(byte基準)／j = MBS(word文字基準)／w = WCS
		[3] a = array／b = boolean／n = number／p = pointer／s = strings
		[4] _
*/
/* 2016-09-09
	戻り値について
		関数において、使用しない戻り値は設定せず VOID型 とすること。
		速度低下の根本原因。
*/
/* 2021-11-18
	ポインタ？ *(p + n)
	配列？     p[n]
		Mingw-w64 において大幅な速度低下がない。
		従来、処理が速いとされていた「ポインタ記述」にしていたが、
		今後、可読性を考慮した「配列記述」とする。
*/
/////////////////////////////////////////////////////////////////////////////////////////
/*---------------------------------------------------------------------------------------
	大域変数
---------------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////
MBS      *$CMD         = "";    // コマンド名を格納
UINT     $ARGC         = 0;     // 引数配列数
MBS      **$ARGV       = { 0 }; // 引数配列
MBS      **$ARGS       = { 0 }; // $ARGVからダブルクォーテーションを消去したもの
HANDLE   $StdoutHandle = 0;     // 画面制御用ハンドル
UINT     $ExecSecBgn   = 0;     // 実行開始時間

/////////////////////////////////////////////////////////////////////////////////////////
/*---------------------------------------------------------------------------------------
	実行開始時間
---------------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////
/* Win32 SDK Reference Help「win32.hlp(1996/11/26)」より
	経過時間はDWORD型で保存されています。
	システムを49.7日間連続して動作させると、
	経過時間は 0 に戻ります。
*/
/* (例)
	iExecSec_init(); //=> $ExecSecBgn
	Sleep(2000);
	P("-- %.6fsec\n\n", iExecSec_next());
	Sleep(1000);
	P("-- %.6fsec\n\n", iExecSec_next());
*/
// v2021-03-19
UINT
iExecSec(
	CONST UINT microSec // 0 のとき Init
)
{
	UINT microSec2 = GetTickCount();
	if(!microSec)
	{
		$ExecSecBgn = microSec2;
	}
	return (microSec2 < microSec ? 0 : (microSec2 - microSec)); // Err = 0
}
/////////////////////////////////////////////////////////////////////////////////////////
/*---------------------------------------------------------------------------------------
	メモリ確保
---------------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////
/*
	icalloc() 用に確保される配列
	IcallocDiv は必要に応じて変更
*/
typedef struct
{
	VOID *ptr; // ポインタ位置
	UINT ary;  // 配列個数（配列以外 = 0）
	UINT size; // アロケート長
	UINT id;   // 順番
}
$struct_icallocMap;

$struct_icallocMap *__icallocMap; // 可変長
UINT __icallocMapSize = 0;        // *__icallocMap のサイズ＋1
UINT __icallocMapEOD = 0;         // *__icallocMap の現在位置＋1
UINT __icallocMapFreeCnt = 0;     // *__icallocMap 中の空白領域
UINT __icallocMapId = 0;          // *__icallocMap の順番
CONST UINT __sizeof_icallocMap = sizeof($struct_icallocMap);
// *__icallocMap の基本区画サイズ(適宜変更 > 0)
#define  IcallocDiv          (1 << 5)
// 確保したメモリ後方に最低4byteの空白を確保
#define  ceilX(n)            ((((n - 5) >> 3) << 3) + (1 << 4))

//---------
// calloc
//---------
/* (例)
	MBS *p1 = icalloc_MBS(100);
	INT *ai = icalloc_INT(100);
*/
/* (注)
	calloc()可能値は、1プロセス／32bitOSで1.5GB程度(OS依存)
*/
// v2022-04-29
VOID
*icalloc(
	UINT n,    // 個数
	UINT size, // 宣言子サイズ
	BOOL aryOn // TRUE = 配列
)
{
	UINT size2 = sizeof($struct_icallocMap);

	// 初回 __icallocMap を更新
	if(!__icallocMapSize)
	{
		__icallocMapSize = IcallocDiv;
		__icallocMap = ($struct_icallocMap*)calloc(__icallocMapSize, size2);
		icalloc_err(__icallocMap);
		__icallocMapId = 0;
	}
	else if(__icallocMapSize <= __icallocMapEOD)
	{
		__icallocMapSize += IcallocDiv;
		__icallocMap = ($struct_icallocMap*)realloc(__icallocMap, __icallocMapSize * size2);
		icalloc_err(__icallocMap);
	}

	// 引数にポインタ割当
	UINT uSize = ceilX(n * size);
	VOID *rtn = calloc(uSize, 1);
	icalloc_err(rtn);

	// ポインタ
	(__icallocMap + __icallocMapEOD)->ptr = rtn;

	// 配列
	(__icallocMap + __icallocMapEOD)->ary = (aryOn ? n : 0);

	// サイズ
	(__icallocMap + __icallocMapEOD)->size = uSize;

	// 順番
	++__icallocMapId;
	(__icallocMap + __icallocMapEOD)->id = __icallocMapId;
	++__icallocMapEOD;

	return rtn;
}
//----------
// realloc
//----------
/* (例)
	// icalloc() で領域確保した後使用。
	MBS *pM = icalloc_MBS(1000);
	pM = irealloc_MBS(pM, 2000);

	// 以下、結果が同じ例。
	//   (例1)はデフラグメントに注意
	//   (例2)は要素数が多いほど効率が良い
	//   (例3)はprintf()系の割に速い
	// よって、
	//   可能であれば(例2)を推奨。
	//   文字・数字混在は(例3)一択。

	// (例1)
	MBS *p11 = ims_clone("ABCDEFGH");
	MBS *p12 = "12345678";
	UINT u11 = imn_len(p11);
	UINT u12 = imn_len(p12);
	p11 = irealloc_MBS(p11, (u11 + u12));
	imn_cpy((p11 + u11), p12);
	PL2(p11);
	ifree(p11);

	// (例2) (例1)と同程度。
	MBS *p21 = "ABCDEFGH";
	MBS *p22 = ims_cats(2, p21, "12345678");
	PL2(p22);
	ifree(p22);

	// (例3) (例1)より少し遅い程度。
	MBS *p31 = "ABCDEFGH";
	MBS *p32 = ims_sprintf("%s%s", p31, "12345678");
	PL2(p32);
	ifree(p32);
*/
// v2022-04-29
VOID
*irealloc(
	VOID *ptr, // icalloc()ポインタ
	UINT n,    // 個数
	UINT size  // 宣言子サイズ
)
{
	// 縮小のとき処理しない
	if((n * size) <= imn_len(ptr))
	{
		return ptr;
	}
	VOID *rtn = 0;
	UINT u1 = ceilX(n * size);
	// __icallocMap を更新
	UINT u2 = 0;
	while(u2 < __icallocMapEOD)
	{
		if(ptr == (__icallocMap + u2)->ptr)
		{
			rtn = (VOID*)realloc(ptr, u1);
			icalloc_err(rtn);
			(__icallocMap + u2)->ptr = rtn;
			(__icallocMap + u2)->ary = ((__icallocMap + u2)->ary ? n : 0);
			(__icallocMap + u2)->size = u1;
			break;
		}
		++u2;
	}
	return rtn;
}
//--------------------------------
// icalloc, ireallocのエラー処理
//--------------------------------
/* (例)
	// 通常
	MBS *p1 = icalloc_MBS(1000);
	icalloc_err(p1);
	// 強制的にエラーを発生させる
	icalloc_err(NULL);
*/
// v2016-08-30
VOID
icalloc_err(
	VOID *ptr // icalloc()ポインタ
)
{
	if(!ptr)
	{
		ierr_end("Can't allocate memories!");
	}
}
//-------------------------
// (__icallocMap+n)をfree
//-------------------------
// v2022-04-29
VOID
icalloc_free(
	VOID *ptr // icalloc()ポインタ
)
{
	$struct_icallocMap *map = 0;
	UINT u1 = 0, u2 = 0;
	while(u1 < __icallocMapEOD)
	{
		map = (__icallocMap + u1);
		if(ptr == (map->ptr))
		{
			// 配列から先に free
			if(map->ary)
			{
				// 1次元削除
				u2 = 0;
				while(u2 < (map->ary))
				{
					if(!(*((MBS**)(map->ptr) + u2)))
					{
						break;
					}
					icalloc_free(*((MBS**)(map->ptr) + u2));
					++u2;
				}
				++__icallocMapFreeCnt;

				// memset() + NULL代入 で free() の代替
				// 2次元削除
				// ポインタ配列を消去
				memset(map->ptr, 0, map->size);
				free(map->ptr);
				map->ptr = 0;
				memset(map, 0, __sizeof_icallocMap);
				return;
			}
			else
			{
				free(map->ptr);
				map->ptr = 0;
				memset(map, 0, __sizeof_icallocMap);
				++__icallocMapFreeCnt;
				return;
			}
		}
		++u1;
	}
}
//---------------------
// __icallocMapをfree
//---------------------
// v2016-01-10
VOID
icalloc_freeAll()
{
	// [0]はポインタなので残す
	// [1..]をfree
	while(__icallocMapEOD)
	{
		icalloc_free(__icallocMap->ptr);
		--__icallocMapEOD;
	}
	__icallocMap = ($struct_icallocMap*)realloc(__icallocMap, 0); // free()不可
	__icallocMapSize = 0;
	__icallocMapFreeCnt = 0;
}
//---------------------
// __icallocMapを掃除
//---------------------
// v2016-09-09
VOID
icalloc_mapSweep()
{
	// 毎回呼び出しても影響ない
	UINT uSweep = 0;
	$struct_icallocMap *map1 = 0, *map2 = 0;
	UINT u1 = 0, u2 = 0;

	// 隙間を詰める
	while(u1 < __icallocMapEOD)
	{
		map1 = (__icallocMap + u1);
		if(!(MBS**)(map1->ptr))
		{
			++uSweep; // sweep数
			u2 = u1 + 1;
			while(u2 < __icallocMapEOD)
			{
				map2 = (__icallocMap + u2);
				if((MBS**)(map2->ptr))
				{
					*map1 = *map2; // 構造体コピー
					memset(map2, 0, __sizeof_icallocMap);
					--uSweep; // sweep数
					break;
				}
				++u2;
			}
		}
		++u1;
	}
	// 初期化
	__icallocMapFreeCnt -= uSweep;
	__icallocMapEOD -= uSweep;
	/// PL23("__icallocMapFreeCnt=", __icallocMapFreeCnt);
	/// PL23("__icallocMapEOD=", __icallocMapEOD);
	/// PL23("SweepCnt=", uSweep);
}
//---------------------------
// __icallocMapをリスト出力
//---------------------------
// v2022-04-03
VOID
icalloc_mapPrint1()
{
	if(!__icallocMapSize)
	{
		return;
	}

	iConsole_EscOn();

	P0("\033[38;2;100;100;255m");
	P0("-1 ----------- 8 ------------ 16 ------------ 24 ------------ 32--------");
	P2("\033[38;2;50;255;50m");

	CONST UINT _rowsCnt = 32;
	UINT uRowsCnt = _rowsCnt;
	UINT u1 = 0, u2 = 0;
	while(u1 < __icallocMapSize)
	{
		while(u1 < uRowsCnt)
		{
			if((__icallocMap + u1)->ptr)
			{
				P0("■");
				++u2;
			}
			else
			{
				P0("□");
			}
			++u1;
		}
		P(" %7u", u2);
		uRowsCnt += _rowsCnt;
		NL();
	}

	P0("\033[0m");
}
// v2022-04-29
VOID
icalloc_mapPrint2()
{
	iConsole_EscOn();

	P0("\033[38;2;100;100;255m");
	P0("------- id ---- pointer ---------- array --- size ----------------------");
	P2("\033[38;2;255;255;255m");

	$struct_icallocMap *map = 0;
	UINT uUsedCnt = 0, uUsedSize = 0;
	UINT u1 = 0;
	while(u1 < __icallocMapEOD)
	{
		map = (__icallocMap + u1);
		if((map->ptr))
		{
			++uUsedCnt;
			uUsedSize += (map->size);

			if((map->ary))
			{
				// 背景色変更
				P0("\033[48;2;150;0;0m");
			}

			P(
				"%-7u %07u [%p] %4u %9u => '%s'",
				(u1 + 1),
				(map->id),
				(map->ptr),
				(map->ary),
				(map->size),
				(map->ptr)
			);

			// 背景色リセット
			P2("\033[49m");
		}
		++u1;
	}

	P0("\033[38;2;100;100;255m");
	P(
		"------- Usage %-9u ---------- %14u byte -----------------",
		uUsedCnt,
		uUsedSize
	);
	P2("\033[0m");
	NL();
}
/////////////////////////////////////////////////////////////////////////////////////////
/*---------------------------------------------------------------------------------------
	Print関係
---------------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////
//-----------
// printf()
//-----------
/* (例)
	P("abc");         //=> "abc"
	P("%s\n", "abc"); //=> "abc\n"

	// printf()系は遅い。可能であれば P0(), P2() を使用する。
	P0("abc");   //=> "abc"
	P2("abc");   //=> "abc\n"
	QP("abc\n"); //=> "abc\n"
*/
// v2015-01-24
VOID
P(
	MBS *format,
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
	iConsole_EscOn();

	INT iMax = 100;
	MBS *rtn = icalloc_MBS(10 * iMax);
	MBS *pEnd = rtn;
	INT iCnt = 0;

	for(INT _i1 = 1; _i1 <= iMax; _i1++)
	{
		pEnd += sprintf(pEnd, "%d\n", _i1);

		++iCnt;
		if(iCnt == 30)
		{
			iCnt = 0;

			QP(rtn);
			pEnd = rtn;

			Sleep(1000);
		}
	}
	QP(rtn);
*/
// v2022-04-02
VOID
QP(
	MBS *pM // 文字列
)
{
	// Buffer があれば先に出力
	fflush(stdout);
	// "\n" は "\r\n" に自動変換されないが問題ない
	WriteFile($StdoutHandle, pM, strlen(pM), NULL, NULL);
}
//-----------------------
// EscapeSequenceへ変換
//-----------------------
/* (例)
	MBS *p1 = "あいうえお\\nかき\\rくけこ";
	// "\\n" を '\n' に変換
	PL2(ims_conv_escape(p1)); //=> "あいうえお\nかき\rくけこ"
	// ソノママ
	PL2(p1); //=> "あいうえお\\nかき\\rくけこ"
*/
// v2021-11-17
MBS
*ims_conv_escape(
	MBS *pM // 文字列
)
{
	if(!pM)
	{
		return NULL;
	}
	MBS *rtn = ims_clone(pM);
	INT i1 = 0;
	while(*pM)
	{
		if(*pM == '\\')
		{
			++pM;
			switch(*pM)
			{
				case('a'):
					rtn[i1] = '\a';
					break;

				case('b'):
					rtn[i1] = '\b';
					break;

				case('t'):
					rtn[i1] = '\t';
					break;

				case('n'):
					rtn[i1] = '\n';
					break;

				case('v'):
					rtn[i1] = '\v';
					break;

				case('f'):
					rtn[i1] = '\f';
					break;

				case('r'):
					rtn[i1] = '\r';
					break;

				default:
					rtn[i1] = '\\';
					++i1;
					rtn[i1] = *pM;
					break;
			}
		}
		else
		{
			rtn[i1] = *pM;
		}
		++pM;
		++i1;
	}
	rtn[i1] = 0;
	return rtn;
}
//--------------------
// sprintf()の拡張版
//--------------------
/* (例)
	MBS *p1 = ims_sprintf("%s-%s%05d", "ABC", "123", 456);
		PL2(p1); //=> "ABC-12300456"
	ifree(p1);
*/
// v2021-11-14
MBS
*ims_sprintf(
	MBS *format,
	...
)
{
	FILE *oFp = fopen(NULL_DEVICE, "wb");
		va_list va;
		va_start(va, format);
			MBS *rtn = icalloc_MBS(vfprintf(oFp, format, va));
			vsprintf(rtn, format, va);
		va_end(va);
	fclose(oFp);
	return rtn;
}
/////////////////////////////////////////////////////////////////////////////////////////
/*---------------------------------------------------------------------------------------
	MBS／WCS／U8N変換
---------------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////
// v2022-04-02
WCS
*icnv_M2W(
	MBS *pM
)
{
	if(!pM)
	{
		return NULL;
	}
	UINT uW = MultiByteToWideChar(CP_OEMCP, 0, pM, -1, NULL, 0);
	WCS *pW = icalloc_WCS(uW);
	MultiByteToWideChar(CP_OEMCP, 0, pM, -1, pW, uW);
	return pW;
}
// v2022-04-02
U8N
*icnv_W2U(
	WCS *pW
)
{
	if(!pW)
	{
		return NULL;
	}
	UINT uU = WideCharToMultiByte(CP_UTF8, 0, pW, -1, NULL, 0, NULL, NULL);
	U8N *pU = icalloc_MBS(uU);
	WideCharToMultiByte(CP_UTF8, 0, pW, -1, pU, uU, NULL, NULL);
	return pU;
}
// v2022-04-02
U8N
*icnv_M2U(
	MBS *pM
)
{
	if(!pM)
	{
		return NULL;
	}
	UINT uW = MultiByteToWideChar(CP_OEMCP, 0, pM, -1, NULL, 0);
	WCS *pW = icalloc_WCS(uW);
	MultiByteToWideChar(CP_OEMCP, 0, pM, -1, pW, uW);
	UINT uU = WideCharToMultiByte(CP_UTF8, 0, pW, -1, NULL, 0, NULL, NULL);
	U8N *pU = icalloc_MBS(uU);
	WideCharToMultiByte(CP_UTF8, 0, pW, -1, pU, uU, NULL, NULL);
	ifree(pW);
	return pU;
}
// v2022-04-02
WCS
*icnv_U2W(
	U8N *pU
)
{
	if(!pU)
	{
		return NULL;
	}
	UINT uW = MultiByteToWideChar(CP_UTF8, 0, pU, -1, NULL, 0);
	WCS *pW = icalloc_WCS(uW);
	MultiByteToWideChar(CP_UTF8, 0, pU, -1, pW, uW);
	return pW;
}
// v2022-04-02
MBS
*icnv_W2M(
	WCS *pW
)
{
	if(!pW)
	{
		return NULL;
	}
	UINT uM = WideCharToMultiByte(CP_OEMCP, 0, pW, -1, NULL, 0, NULL, NULL);
	MBS *pM = icalloc_MBS(uM);
	WideCharToMultiByte(CP_OEMCP, 0, pW, -1, pM, uM, NULL, NULL);
	return pM;
}
// v2022-04-02
MBS
*icnv_U2M(
	U8N *pU
)
{
	if(!pU)
	{
		return NULL;
	}
	UINT uW = MultiByteToWideChar(CP_UTF8, 0, pU, -1, NULL, 0);
	WCS *pW = icalloc_WCS(uW);
	MultiByteToWideChar(CP_UTF8, 0, pU, -1, pW, uW);
	UINT uM = WideCharToMultiByte(CP_OEMCP, 0, pW, -1, NULL, 0, NULL, NULL);
	MBS *pM = icalloc_MBS(uM);
	WideCharToMultiByte(CP_OEMCP, 0, pW, -1, pM, uM, NULL, NULL);
	ifree(pW);
	return pM;
}
/////////////////////////////////////////////////////////////////////////////////////////
/*---------------------------------------------------------------------------------------
	文字列処理
		p : return Pointer
		s : return String
		1byte     MBS : imp_xxx(), imp_xxx()
		1 & 2byte MBS : ijp_xxx(), ijs_xxx()
		UTF-8     U8N : iup_xxx(), ius_xxx()
		UTF-16    WCS : iwp_xxx(), iws_xxx()
---------------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////
/* (例)
	MBS *mp1 = "あいうえおaiueo";
	PL3(imn_len(mp1)); //=> 15
	PL3(ijn_len(mp1)); //=> 10
	WCS *wp1 = M2W(mp1);
	PL3(iwn_len(wp1)); //=> 10
*/
// v2022-04-02
UINT
imn_len(
	MBS *pM
)
{
	if(!pM)
	{
		return 0;
	}
	return strlen(pM);
}
// v2022-04-02
UINT
ijn_len(
	MBS *pM
)
{
	if(!pM)
	{
		return 0;
	}
	UINT rtn = 0;
	while(*pM)
	{
		++rtn;
		pM = CharNextA(pM);
	}
	return rtn;
}
// v2022-04-02
UINT
iun_len(
	U8N *pU
)
{
	if(!pU)
	{
		return 0;
	}
	UINT rtn = 0;
	// BOMを読み飛ばす(UTF-8N は該当しない)
	if(*pU == (CHAR)0xEF && pU[1] == (CHAR)0xBB && pU[2] == (CHAR)0xBF)
	{
		pU += 3;
	}
	INT c = 0;
	while(*pU)
	{
		if(*pU & 0x80)
		{
			// 多バイト文字
			// [0]
			c = (*pU & 0xfc);
			c <<= 1;
			++pU;
			// [1]
			if(c & 0x80)
			{
				c <<= 1;
				++pU;
				// [2]
				if(c & 0x80)
				{
					c <<= 1;
					++pU;
					// [3]
					if(c & 0x80)
					{
						++pU;
					}
				}
			}
			/* 以下より10%前後速い
				c = (*pU & 0xfc);
				while(c & 0x80)
				{
					++pU;
					c <<= 1;
				}
			*/
		}
		else
		{
			// 1バイト文字
			++pU;
		}
		++rtn;
	}
	return rtn;
}
// v2022-04-02
UINT
iwn_len(
	WCS *pW
)
{
	if(!pW)
	{
		return 0;
	}
	return wcslen(pW);
}
//-------------------------
// size先のポインタを返す
//-------------------------
/* (例)
	MBS *p1 = "ABあいう";
	PL2(ijp_forwardN(p1, 3)); //=> "いう"
*/
// v2020-05-30
MBS
*ijp_forwardN(
	MBS *pM,   // 開始位置
	UINT sizeJ // 文字数
)
{
	if(!pM)
	{
		return pM;
	}
	while(*pM && sizeJ > 0)
	{
		pM = CharNextA(pM);
		--sizeJ;
	}
	return pM;
}
// v2021-11-17
U8N
*iup_forwardN(
	U8N *pU,   // 開始位置
	UINT sizeU // 文字数
)
{
	if(!pU)
	{
		return pU;
	}
	// BOMを読み飛ばす(UTF-8N は該当しない)
	if(*pU == (CHAR)0xEF && pU[1] == (CHAR)0xBB && pU[2] == (CHAR)0xBF)
	{
		pU += 3;
	}
	INT c = 0;
	while(*pU && sizeU > 0)
	{
		if(*pU & 0x80)
		{
			// 多バイト文字
			c = (*pU & 0xfc);
			while(c & 0x80)
			{
				++pU;
				c <<= 1;
			}
		}
		else
		{
			// 1バイト文字
			++pU;
		}
		--sizeU;
	}
	return pU;
}
//---------------
// 大小文字置換
//---------------
/* (例)
	MBS *p1 = "aBC";
	PL2(ims_upper(p1)); //=> "ABC"
	PL2(ims_lower(p1)); //=> "abc"
*/
// v2021-09-21
MBS
*ims_UpperLower(
	MBS *pM,
	INT option // 1=Upper／2=Lower
)
{
	MBS *rtn = ims_clone(pM);
	switch(option)
	{
		case(1): return CharUpperA(rtn); break;
		default: return CharLowerA(rtn); break;
	}
}
//-------------------------
// コピーした文字長を返す
//-------------------------
/* (例)
	MBS *to = icalloc_MBS(100);
	PL3(imn_cpy(to, "abcde")); //=> 5
	ifree(to);
*/
// v2022-04-02
UINT
imn_cpy(
	MBS *to,
	MBS *from
)
{
	if(!from)
	{
		return 0;
	}
	MBS *pEnd = from;
	while((*to++ = *pEnd++));
	*to = 0;
	return (pEnd - from - 1);
}
// v2022-04-02
UINT
iwn_cpy(
	WCS *to,
	WCS *from
)
{
	if(!from)
	{
		return 0;
	}
	WCS *pEnd = from;
	while((*to++ = *pEnd++));
	*to = 0;
	return (pEnd - from - 1);
}
/* (例)
	MBS *to = icalloc_MBS(100);
	MBS *p1 = "abcde12345";
	PL3(imn_pcpy(to, p1 + 3, p1 + 8)); //=> 5
	ifree(to);
*/
// v2022-04-02
UINT
imn_pcpy(
	MBS *to,
	MBS *from1,
	MBS *from2
)
{
	if(!from1 || !from2)
	{
		return 0;
	}
	MBS *pEnd = from1;
	while((pEnd < from2) && (*to++ = *pEnd++));
	*to = 0;
	return (pEnd - from1);
}
//-----------------------
// 新規部分文字列を生成
//-----------------------
/* (例)
	PL2(ims_clone("abcde")); //=> "abcde"
*/
// v2022-04-01
MBS
*ims_clone(
	MBS *from
)
{
	MBS *to = icalloc_MBS(imn_len(from));
	MBS *pEnd = to;
	while((*pEnd++ = *from++));
	*pEnd = 0;
	return to;
}
// v2022-04-01
WCS
*iws_clone(
	WCS *from
)
{
	WCS *to = icalloc_WCS(iwn_len(from));
	WCS *pEnd = to;
	while((*pEnd++ = *from++));
	*pEnd = 0;
	return to;
}
/* (例)
	MBS *from = "abcde";
	MBS *p1 = ims_pclone(from, from + 3);
		PL2(p1); //=> "abc"
	ifree(p1);
*/
// v2022-04-01
MBS
*ims_pclone(
	MBS *from1,
	MBS *from2
)
{
	MBS *to = icalloc_MBS(from2 - from1);
	MBS *pEnd = to;
	while((from1 < from2) && (*pEnd++ = *from1++));
	*pEnd = 0;
	return to;
}
/* (例)
	// 要素を呼び出す度 realloc する方がスマートだが速度に不安があるので icalloc １回で済ませる。
	MBS *p1 = ims_cats(3, "123", "abcde", "岩間");
		PL2(p1); //=> "123abcde岩間"
	ifree(p1);
*/
// v2022-04-02
MBS
*ims_cats(
	UINT size, // 要素数(n+1)
	...        // ary[0..n]
)
{
	UINT u1 = 0, u2 = 0;

	va_list va;
	va_start(va, size);
		u1 = size;
		while(u1--)
		{
			u2 += imn_len(va_arg(va, MBS*));
		}
	va_end(va);

	MBS *rtn = icalloc_MBS(u2);
	MBS *pEnd = rtn;

	va_start(va, size);
		u1 = size;
		while(u1--)
		{
			pEnd += imn_cpy(pEnd, va_arg(va, MBS*));
		}
	va_end(va);

	return rtn;
}
//--------------------------------
// lstrcmp()／lstrcmpi()より安全
//--------------------------------
/*
	lstrcmp() は大小比較しかしない(TRUE = 0, FALSE = 1 or -1)ので、
	比較する文字列長を揃えてやる必要がある。
*/
/* (例)
	PL3(imb_cmp("", "abc", FALSE, FALSE));   //=> FALSE
	PL3(imb_cmp("abc", "", FALSE, FALSE));   //=> TRUE
	PL3(imb_cmp("", "", FALSE, FALSE));      //=> TRUE
	PL3(imb_cmp(NULL, "abc", FALSE, FALSE)); //=> FALSE
	PL3(imb_cmp("abc", NULL, FALSE, FALSE)); //=> FALSE
	PL3(imb_cmp(NULL, NULL, FALSE, FALSE));  //=> FALSE
	PL3(imb_cmp(NULL, "", FALSE, FALSE));    //=> FALSE
	PL3(imb_cmp("", NULL, FALSE, FALSE));    //=> FALSE
	NL();
	// imb_cmpf(p, search)  <= imb_cmp(p, search, FALSE, FALSE)
	PL3(imb_cmp("abc", "AB", FALSE, FALSE)); //=> FALSE
	// imb_cmpfi(p, search) <= imb_cmp(p, search, FALSE, TRUE)
	PL3(imb_cmp("abc", "AB", FALSE, TRUE));  //=> TRUE
	// imb_cmpp(p, search)  <= imb_cmp(p, search, TRUE, FALSE)
	PL3(imb_cmp("abc", "AB", TRUE, FALSE));  //=> FALSE
	// imb_cmppi(p, search) <= imb_cmp(p, search, TRUE, TRUE)
	PL3(imb_cmp("abc", "AB", TRUE, TRUE));   //=> FALSE
	NL();
	// searchに１文字でも合致すればTRUEを返す
	PL3(imb_cmp_leq(""   , "..", TRUE));     //=> TRUE
	PL3(imb_cmp_leq("."  , "..", TRUE));     //=> TRUE
	PL3(imb_cmp_leq(".." , "..", TRUE));     //=> TRUE
	PL3(imb_cmp_leq("...", "..", TRUE));     //=> FALSE
	PL3(imb_cmp_leq("...", ""  , TRUE));     //=> FALSE
	NL();
*/
// v2021-04-20
BOOL
imb_cmp(
	MBS *pM,      // 検索対象
	MBS *search,  // 検索文字列
	BOOL perfect, // TRUE=長さ一致／FALSE=前方一致
	BOOL icase    // TRUE=大文字小文字区別しない
)
{
	// NULL は存在しないので FALSE
	if(!pM || !search)
	{
		return FALSE;
	}
	// 例外 "" == "
	if(!*pM && !*search)
	{
		return TRUE;
	}
	// 検索対象 == "" のときは FALSE
	if(!*pM)
	{
		return FALSE;
	}
	// int型で比較した方が速い
	INT i1 = 0, i2 = 0;
	if(icase)
	{
		while(*pM && *search)
		{
			i1 = tolower(*pM);
			i2 = tolower(*search);
			if(i1 != i2)
			{
				break;
			}
			++pM;
			++search;
		}
	}
	else
	{
		while(*pM && *search)
		{
			i1 = *pM;
			i2 = *search;
			if(i1 != i2)
			{
				break;
			}
			++pM;
			++search;
		}
	}
	if(perfect)
	{
		// 末尾が共に \0 なら 完全一致
		return (!*pM && !*search ? TRUE : FALSE);
	}
	// searchE の末尾が \0 なら 前方一致
	return (!*search ? TRUE : FALSE);
}
// v2021-04-20
BOOL
iwb_cmp(
	WCS *pW,      // 検索対象
	WCS *search,  // 検索文字列
	BOOL perfect, // TRUE=長さ一致／FALSE=前方一致
	BOOL icase    // TRUE=大文字小文字区別しない
)
{
	// NULL は存在しないので FALSE
	if(!pW || !search)
	{
		return FALSE;
	}
	// 例外
	if(!*pW && !*search)
	{
		return TRUE;
	}
	// 検索対象 == "" のときは FALSE
	if(!*pW)
	{
		return FALSE;
	}
	// int型で比較した方が速い
	INT i1 = 0, i2 = 0;
	if(icase)
	{
		while(*pW && *search)
		{
			i1 = towlower(*pW);
			i2 = towlower(*search);
			if(i1 != i2)
			{
				break;
			}
			++pW;
			++search;
		}
	}
	else
	{
		while(*pW && *search)
		{
			i1 = *pW;
			i2 = *search;
			if(i1 != i2)
			{
				break;
			}
			++pW;
			++search;
		}
	}
	if(perfect)
	{
		// 末尾が共に \0 なら 完全一致
		return (!*pW && !*search ? TRUE : FALSE);
	}
	// searchE の末尾が \0 なら 前方一致
	return (!*search ? TRUE : FALSE);
}
//-------------------------------
// 無視する範囲の終了位置を返す
//-------------------------------
/* (例)
	MBS *p1 = "<-123, <-4, 5->, 6->->";
	PL2(ijp_bypass(p1, "<-", "->")); //=> "->, 6->->"
*/
// v2014-08-16
MBS
*ijp_bypass(
	MBS *pM,   // 文字列
	MBS *from, // 無視開始
	MBS *to    // 無視終了
)
{
	if(!imb_cmpf(pM, from))
	{
		return pM;
	}
	MBS *rtn = ijp_searchL(CharNextA(pM), to); // *from == *to 対策
	return (*rtn ? rtn : pM);
}
//-------------------
// 一致文字数を返す
//-------------------
/* (例)
	PL3(ijn_searchCnti("表\表\123表\", "表\"));  //=> 3 Word
	PL3(ijn_searchCntLi("表\表\123表\", "表\")); //=> 2 Word
	PL3(ijn_searchLenLi("表\表\123表\", "表\")); //=> 2 Word
	PL3(imn_searchLenLi("表\表\123表\", "表\")); //=> 4 byte
	PL3(ijn_searchCntRi("表\表\123表\", "表\")); //=> 1 Word
	PL3(ijn_searchLenRi("表\表\123表\", "表\")); //=> 1 Word
	PL3(imn_searchLenRi("表\表\123表\", "表\")); //=> 2 byte
*/
// v2022-04-02
UINT
ijn_searchCntA(
	MBS *pM,     // 文字列
	MBS *search, // 検索文字列
	BOOL icase   // TRUE=大文字小文字区別しない
)
{
	WCS *pW = icnv_M2W(pM);
	WCS *wp1 = icnv_M2W(search);
	UINT rtn = iwn_searchCntW(pW, wp1, icase);
	ifree(wp1);
	ifree(pW);
	return rtn;
}
// v2022-04-02
UINT
iwn_searchCntW(
	WCS *pW,     // 文字列
	WCS *search, // 検索文字列
	BOOL icase   // TRUE=大文字小文字区別しない
)
{
	if(!pW || !search)
	{
		return 0;
	}
	UINT rtn = 0;
	CONST UINT _searchLen = iwn_len(search);
	while(*pW)
	{
		if(iwb_cmp(pW, search, FALSE, icase))
		{
			pW += _searchLen;
			++rtn;
		}
		else
		{
			++pW;
		}
	}
	return rtn;
}
// v2022-04-02
UINT
ijn_searchCntLA(
	MBS *pM,     // 文字列
	MBS *search, // 検索文字列
	BOOL icase,  // TRUE=大文字小文字区別しない
	INT option   // 0=個数／1=Byte数／2=Word数
)
{
	if(!pM || !search)
	{
		return 0;
	}
	UINT rtn = 0;
	CONST UINT _searchLen = imn_len(search);
	while(*pM)
	{
		if(imb_cmp(pM, search, FALSE, icase))
		{
			pM += _searchLen;
			++rtn;
		}
		else
		{
			break;
		}
	}
	switch(option)
	{
		case(1): rtn *= imn_len(search); break; // Byte数
		case(2): rtn *= ijn_len(search); break; // Word数
		default: break;                         // 個数
	}
	return rtn;
}
// v2022-04-02
UINT
ijn_searchCntRA(
	MBS *pM,     // 文字列
	MBS *search, // 検索文字列
	BOOL icase,  // TRUE=大文字小文字区別しない
	INT option   // 0=個数／1=Byte数／2=Word数
)
{
	if(!pM || !search)
	{
		return 0;
	}
	UINT rtn = 0;
	CONST UINT _searchLen = imn_len(search);
	MBS *pEnd = pM + imn_len(pM) - _searchLen;
	while(pM <= pEnd)
	{
		if(imb_cmp(pEnd, search, FALSE, icase))
		{
			pEnd -= _searchLen;
			++rtn;
		}
		else
		{
			break;
		}
	}
	switch(option)
	{
		case(1): rtn *= imn_len(search); break; // Byte数
		case(2): rtn *= ijn_len(search); break; // Word数
		default: break;                         // 個数
	}
	return rtn;
}
//---------------------------------
// 一致した文字列のポインタを返す
//---------------------------------
/*
	      "\0あいうえお\0"
	         <= TRUE =>
	R_FALSE↑          ↑L_FALSE
*/
/* (例)
	PL2(ijp_searchLA("ABCABCDEFABC", "ABC", FALSE)); //=> "ABCABCDEFABC"
*/
// v2014-11-29
MBS
*ijp_searchLA(
	MBS *pM,     // 文字列
	MBS *search, // 検索文字列
	BOOL icase   // TRUE=大文字小文字区別しない
)
{
	if(!pM)
	{
		return pM;
	}
	while(*pM)
	{
		if(imb_cmp(pM, search, FALSE, icase))
		{
			break;
		}
		pM = CharNextA(pM);
	}
	return pM;
}
//-------------------------
// 比較指示子を数字に変換
//-------------------------
/*
	[-2] "<"  | "!>="
	[-1] "<=" | "!>"
	[ 0] "="  | "!<>" | "!><"
	[ 1] ">=" | "!<"
	[ 2] ">"  | "!<="
	[ 3] "!=" | "<>"  | "><"
*/
// v2021-11-17
INT
icmpOperator_extractHead(
	MBS *pM
)
{
	INT rtn = INT_MAX; // Errのときは MAX を返す
	if(!pM || !*pM || !(*pM == ' ' || *pM == '<' || *pM == '=' || *pM == '>' || *pM == '!'))
	{
		return rtn;
	}

	// 先頭の空白のみ特例
	while(*pM == ' ')
	{
		++pM;
	}
	BOOL bNot = FALSE;
	if(*pM == '!')
	{
		++pM;
		bNot = TRUE;
	}
	switch(*pM)
	{
		// [2]">" | [1]">=" | [3]"><"
		case('>'):
			if(pM[1] == '<')
			{
				rtn = 3;
			}
			else
			{
				rtn = (pM[1] == '=' ? 1 : 2);
			}
			break;

		// [0]"="
		case('='):
			rtn = 0;
			break;

		// [-2]"<" | [-1]"<=" | [3]"<>"
		case('<'):
			if(pM[1] == '>')
			{
				rtn = 3;
			}
			else
			{
				rtn = (pM[1] == '=' ? -1 : -2);
			}
			break;
	}
	if(bNot)
	{
		rtn += (rtn > 0 ? -3 : 3);
	}
	return rtn;
}
//---------------------------------------------------
// icmpOperator_extractHead()で取得した文字列を返す
//---------------------------------------------------
// v2016-02-11
MBS
*icmpOperator_toHeadA(
	INT operator
)
{
	if(operator > 3 || operator < -2)
	{
		return NULL;
	}
	if(operator == -2)
	{
		return "<";
	}
	if(operator == -1)
	{
		return "<=";
	}
	if(operator ==  0)
	{
		return "=" ;
	}
	if(operator ==  1)
	{
		return ">=";
	}
	if(operator ==  2)
	{
		return ">" ;
	}
	if(operator ==  3)
	{
		return "!=";
	}
	return NULL;
}
//-------------------------------------------------------
// icmpOperator_extractHead()で取得した比較指示子で比較
//-------------------------------------------------------
// v2015-12-31
BOOL
icmpOperator_chk_INT64(
	INT64 i1,
	INT64 i2,
	INT operator // [-2..3]
)
{
	if(operator == -2 && i1 < i2)
	{
		return TRUE;
	}
	if(operator == -1 && i1 <= i2)
	{
		return TRUE;
	}
	if(operator == 0 && i1 == i2)
	{
		return TRUE;
	}
	if(operator == 1 && i1 >= i2)
	{
		return TRUE;
	}
	if(operator == 2 && i1 > i2)
	{
		return TRUE;
	}
	if(operator == 3 && i1 != i2)
	{
		return TRUE;
	}
	return FALSE;
}
// v2015-12-26
BOOL
icmpOperator_chkDBL(
	DOUBLE d1,   //
	DOUBLE d2,   //
	INT operator // [-2..3]
)
{
	if(operator == -2 && d1 < d2)
	{
		return TRUE;
	}
	if(operator == -1 && d1 <= d2)
	{
		return TRUE;
	}
	if(operator == 0 && d1 == d2)
	{
		return TRUE;
	}
	if(operator == 1 && d1 >= d2)
	{
		return TRUE;
	}
	if(operator == 2 && d1 > d2)
	{
		return TRUE;
	}
	if(operator == 3 && d1 != d2)
	{
		return TRUE;
	}
	return FALSE;
}
//---------------------------
// 文字列を分割し配列を作成
//---------------------------
/* (例)
	MBS *pM = "2014年 4月29日　18時42分00秒";
	MBS *tokensM = "年月日時分秒　 ";
	MBS **as1 =
		ija_split(pM, tokensM); //=> [0]2014, [1]4, [2]29, [3]18, [4]42, [5]00
	//	ija_split(pM, "");      // 1文字ずつ返す
	//	ija_split_zero(pM);     // ↑
	//	ija_split(pM, NULL);    // ↑
		iary_print(as1);
	ifree(as1);
*/
// v2022-04-02
MBS
**ija_split(
	MBS *pM,     // 元文字列
	MBS *tokensM // 区切文字／複数 (例) "\t\r\n"
)
{
	MBS **rtn = {0};

	if(!pM || !*pM)
	{
		rtn = icalloc_MBS_ary(1);
		rtn[0] = ims_clone(pM);
		return rtn;
	}

	if(!tokensM || !*tokensM)
	{
		return ija_split_zero(pM);
	}

	WCS *pW = icnv_M2W(pM);
	WCS *tokensW = icnv_M2W(tokensM);

	UINT uAry = 0;
	rtn = icalloc_MBS_ary(uAry + 1);

	WCS *wp1 = wcstok(pW, tokensW);
	rtn[uAry] = icnv_W2M(wp1);
	while(wp1)
	{
		wp1 = wcstok(NULL, tokensW);
		++uAry;
		rtn = irealloc_MBS_ary(rtn, (uAry + 1));
		rtn[uAry] = icnv_W2M(wp1);
	}

	ifree(tokensW);
	ifree(pW);

	return rtn;
}
//---------------------------
// １文字づつ区切って配列化
//---------------------------
/* (例)
	MBS **as1 = ija_split_zero("ABCあいう");
		iary_print(as1); //=> A, B, C, あ, い, う
	ifree(as1);
*/
// v2022-04-02
MBS
**ija_split_zero(
	MBS *pM
)
{
	if(!pM)
	{
		return ima_null();
	}
	UINT pLen = ijn_len(pM);
	MBS **rtn = icalloc_MBS_ary(pLen);
	MBS *pBgn = pM;
	MBS *pEnd = 0;
	UINT u1 = 0;
	while(u1 < pLen)
	{
		pEnd = CharNextA(pBgn);
		rtn[u1] = ims_pclone(pBgn, pEnd);
		pBgn = pEnd;
		++u1;
	}
	return rtn;
}
//----------------------
// quote文字を消去する
//----------------------
/* (例)
	PL2(ijs_rm_quote("[[ABC]", "[", "]", TRUE, TRUE));            //=> "[ABC"
	PL2(ijs_rm_quote("[[ABC]", "[", "]", TRUE, FALSE));           //=> "ABC"
	PL2(ijs_rm_quote("<A>123</A>", "<A>", "</A>", TRUE,  TRUE));  //=> "123"
	PL2(ijs_rm_quote("<A>123</A>", "<A>", "</A>", FALSE, TRUE));  //=> "123"
	PL2(ijs_rm_quote("<A>123</A>", "<A>", "</a>", TRUE,  TRUE));  //=> "123"
	PL2(ijs_rm_quote("<A>123</A>", "<A>", "</a>", FALSE, FALSE)); //=> "123</A>"
*/
// v2022-04-02
MBS
*ijs_rm_quote(
	MBS *pM,      // 文字列
	MBS *quoteL,  // 消去する先頭文字列
	MBS *quoteR,  // 消去する末尾文字列
	BOOL icase,   // TRUE=大文字小文字区別しない
	BOOL oneToOne // TRUE=quote数をグループとして処理
)
{
	if(!pM || !*pM)
	{
		return pM;
	}
	MBS *rtn = 0, *quoteL2 = 0, *quoteR2 = 0;
	// 大小区別
	if(icase)
	{
		rtn = ims_lower(pM);
		quoteL2 = ims_lower(quoteL);
		quoteR2 = ims_lower(quoteR);
	}
	else
	{
		rtn = ims_clone(pM);
		quoteL2 = ims_clone(quoteL);
		quoteR2 = ims_clone(quoteR);
	}
	// 先頭のquote数
	CONST UINT quoteL2Len = imn_len(quoteL2);
	UINT quoteL2Cnt = ijn_searchCntL(rtn, quoteL2);
	// 末尾のquote数
	CONST UINT quoteR2Len = imn_len(quoteR2);
	UINT quoteR2Cnt = ijn_searchCntR(rtn, quoteR2);
	// ifree()
	ifree(quoteR2);
	ifree(quoteL2);
	// 対のとき、低い方のquote数を取得
	if(oneToOne)
	{
		quoteL2Cnt = quoteR2Cnt = (quoteL2Cnt < quoteR2Cnt ? quoteL2Cnt : quoteR2Cnt);
	}
	// 大小区別
	if(icase)
	{
		ifree(rtn);
		rtn = ims_clone(pM); // 元の文字列に置換
	}
	// 先頭と末尾のquoteを対で消去
	imb_shiftL(rtn, (quoteL2Len * quoteL2Cnt));        // 先頭位置をシフト
	rtn[imn_len(rtn) - (quoteR2Len * quoteR2Cnt)] = 0; // 末尾にNULL代入
	return rtn;
}
//---------------------
// 数字文字列を区切る
//---------------------
/* (例)
	PL2(ims_addTokenNStr("+000123456.7890"));    //=> "+123,456.7890"
	PL2(ims_addTokenNStr(".000123456.7890"));    //=> "0.000123456.7890"
	PL2(ims_addTokenNStr("+.000123456.7890"));   //=> "+0.000123456.7890"
	PL2(ims_addTokenNStr("0000abcdefg.7890"));   //=> "0abcdefg.7890"
	PL2(ims_addTokenNStr("1234abcdefg.7890"));   //=> "1,234abcdefg.7890"
	PL2(ims_addTokenNStr("+0000abcdefg.7890"));  //=> "+0abcdefg.7890"
	PL2(ims_addTokenNStr("+1234abcdefg.7890"));  //=> "+1,234abcdefg.7890"
	PL2(ims_addTokenNStr("+abcdefg.7890"));      //=> "+abcdefg0.7890"
	PL2(ims_addTokenNStr("±1234567890.12345")); //=> "±1,234,567,890.12345"
	PL2(ims_addTokenNStr("aiuあいう@岩間"));     //=> "aiuあいう@岩間"
*/
// v2022-04-02
MBS
*ims_addTokenNStr(
	MBS *pM
)
{
	if(!pM || !*pM)
	{
		return pM;
	}
	UINT u1 = imn_len(pM);
	UINT u2 = 0;
	MBS *rtn = icalloc_MBS(u1 * 2);
	MBS *pRtnE = rtn;
	MBS *p1 = 0;

	// "-000123456.7890" のとき
	// (1) 先頭の [\S*] を探す
	MBS *pBgn = pM;
	MBS *pEnd = pM;
	while(*pEnd)
	{
		if((*pEnd >= '0' && *pEnd <= '9') || *pEnd == '.')
		{
			break;
		}
		++pEnd;
	}
	pRtnE += imn_pcpy(pRtnE, pBgn, pEnd);

	// (2) [0-9] 間を探す => "000123456"
	pBgn = pEnd;
	while(*pEnd)
	{
		if(*pEnd < '0' || *pEnd > '9')
		{
			break;
		}
		++pEnd;
	}

	// (2-11) 先頭は [.] か？
	if(*pBgn == '.')
	{
		*pRtnE = '0';
		++pRtnE;
		imn_cpy(pRtnE, pBgn);
	}

	// (2-21) 連続する 先頭の[0] を調整 => "123456"
	else
	{
		while(*pBgn)
		{
			if(*pBgn != '0')
			{
				break;
			}
			++pBgn;
		}
		if(*(pBgn - 1) == '0' && (*pBgn < '0' || *pBgn > '9'))
		{
			--pBgn;
		}

		// (2-22) ", " 付与 => "123,456"
		p1 = ims_pclone(pBgn, pEnd);
			u1 = pEnd - pBgn;
			if(u1 > 3)
			{
				u2 = u1 % 3;
				if(u2)
				{
					pRtnE += imn_pcpy(pRtnE, p1, p1 + u2);
				}
				while(u2 < u1)
				{
					if(u2 > 0 && u2 < u1)
					{
						*pRtnE = ',';
						++pRtnE;
					}
					pRtnE += imn_pcpy(pRtnE, p1 + u2, p1 + u2 + 3);
					u2 += 3;
				}
			}
			else
			{
				pRtnE += imn_cpy(pRtnE, p1);
			}
		ifree(p1);

		// (2-23) 残り => ".7890"
		imn_cpy(pRtnE, pEnd);
	}
	return rtn;
}
//-----------------------
// 左右の文字を消去する
//-----------------------
/* (例)
	PL2(ijs_cut(" \tABC\t ", " \t", " \t")); //=> "ABC"
*/
// v2021-09-24
MBS
*ijs_cut(
	MBS *pM,
	MBS *rmLs,
	MBS *rmRs
)
{
	MBS *rtn = 0;
	MBS **aryLs = ija_split_zero(rmLs);
	MBS **aryRs = ija_split_zero(rmRs);
		rtn = ijs_cutAry(pM, aryLs, aryRs);
	ifree(aryRs);
	ifree(aryLs);
	return rtn;
}
// v2022-04-02
MBS
*ijs_cutAry(
	MBS *pM,
	MBS **aryLs,
	MBS **aryRs
)
{
	if(!pM)
	{
		return NULL;
	}
	BOOL execL = (aryLs && *aryLs ? TRUE : FALSE);
	BOOL execR = (aryRs && *aryRs ? TRUE : FALSE);
	UINT u1 = 0;
	MBS *pBgn = pM;
	MBS *pEnd = pBgn + imn_len(pBgn);
	MBS *p1 = 0;
	// 先頭
	if(execL)
	{
		while(*pBgn)
		{
			u1 = 0;
			while((p1 = aryLs[u1]))
			{
				if(imb_cmpf(pBgn, p1))
				{
					break;
				}
				++u1;
			}
			if(!p1)
			{
				break;
			}
			pBgn = CharNextA(pBgn);
		}
	}
	// 末尾
	if(execR)
	{
		pEnd = CharPrevA(0, pEnd);
		while(*pEnd)
		{
			u1 = 0;
			while((p1 = aryRs[u1]))
			{
				if(imb_cmpf(pEnd, p1))
				{
					break;
				}
				++u1;
			}
			if(!p1)
			{
				break;
			}
			pEnd = CharPrevA(0, pEnd);
		}
		pEnd = CharNextA(pEnd);
	}
	return ims_pclone(pBgn, pEnd);
}
// v2021-04-20
MBS
*ARY_SPACE[] = {
	"\n",
	"\r",
	"\t",
	"\x20",     // " "
	"\x81\x40", // "　"
	NULL
};
// v2014-11-05
MBS
*ijs_trim(
	MBS *pM
)
{
	return ijs_cutAry(pM, ARY_SPACE, ARY_SPACE);
}
// v2014-11-05
MBS
*ijs_trimL(
	MBS *pM
)
{
	return ijs_cutAry(pM, ARY_SPACE, NULL);
}
// v2014-11-05
MBS
*ijs_trimR(
	MBS *pM
)
{
	return ijs_cutAry(pM, NULL, ARY_SPACE);
}
// v2014-11-05
MBS
*ARY_CRLF[] = {
	"\r",
	"\n",
	NULL
};
// v2014-11-05
MBS
*ijs_chomp(
	MBS *pM
)
{
	return ijs_cutAry(pM, NULL, ARY_CRLF);
}
//-------------
// 文字列置換
//-------------
/* (例)
	PL2(ijs_replace("100YEN yen", "YEN", "円", TRUE));  //=> "100円 円"
	PL2(ijs_replace("100YEN yen", "YEN", "円", FALSE)); //=> "100円 yen"
*/
// v2022-03-31
MBS
*ijs_replace(
	MBS *from,   // 文字列
	MBS *before, // 変換前の文字列
	MBS *after,  // 変換後の文字列
	BOOL icase   // TRUE=大文字小文字区別しない
)
{
	// "\\" "表\" に対応するため MBS => WCS => MBS の順で変換
	WCS *fromW = icnv_M2W(from);
	WCS *beforeW = icnv_M2W(before);
	WCS *afterW = icnv_M2W(after);
	WCS *rtnW = iws_replace(fromW, beforeW, afterW, icase);

	MBS *rtn = icnv_W2M(rtnW);

	ifree(rtnW);
	ifree(afterW);
	ifree(beforeW);
	ifree(fromW);

	return rtn;
}
// v2022-04-02
WCS
*iws_replace(
	WCS *from,   // 文字列
	WCS *before, // 変換前の文字列
	WCS *after,  // 変換後の文字列
	BOOL icase   // TRUE=大文字小文字区別しない
)
{
	if(!from || !*from || !before || !*before)
	{
		return iws_clone(from);
	}

	WCS *fW = 0;
	WCS *bW = 0;

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

	UINT fromLen = iwn_len(from);
	UINT beforeLen = iwn_len(before);
	UINT afterLen = iwn_len(after);

	// ゼロ長対策
	WCS *rtn = icalloc_WCS(fromLen * (1 + (afterLen / beforeLen)));

	WCS *fWB = fW;
	WCS *rtnE = rtn;

	while(*fWB)
	{
		if(!wcsncmp(fWB, bW, beforeLen))
		{
			rtnE += iwn_cpy(rtnE, after);
			fWB += beforeLen;
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
//-----------------------------
// 動的引数の文字位置をシフト
//-----------------------------
/* (例)
	MBS *p1 = ims_clone("123456789");
	PL2(p1); //=> "123456789"
	imb_shiftL(p1, 3);
	PL2(p1); //=> "456789"
	imb_shiftR(p1, 3);
	PL2(p1); //=> "456"
*/
// v2022-04-02
BOOL
imb_shiftL(
	MBS *pM,
	UINT byte
)
{
	if(!byte || !pM || !*pM)
	{
		return FALSE;
	}
	UINT u1 = imn_len(pM);
	if(byte > u1)
	{
		byte = u1;
	}
	memcpy(pM, (pM + byte), (u1 - byte + 1)); // NULLもコピー
	return TRUE;
}
// v2022-04-02
BOOL
imb_shiftR(
	MBS *pM,
	UINT byte
)
{
	if(!byte || !pM || !*pM)
	{
		return FALSE;
	}
	UINT u1 = imn_len(pM);
	if(byte > u1)
	{
		byte = u1;
	}
	pM[u1 - byte] = 0;
	return TRUE;
}
/////////////////////////////////////////////////////////////////////////////////////////
/*---------------------------------------------------------------------------------------
	数字関係
---------------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////
//-------------------------------------
// 文字を無視した位置で数値に変換する
//-------------------------------------
/* (例)
	PL3(inum_atoi64("-0123.45")); //=> -123
*/
// v2015-12-31
INT64
inum_atoi(
	MBS *pM // 文字列
)
{
	if(!pM || !*pM)
	{
		return 0;
	}
	while(*pM)
	{
		if(inum_chkM(pM))
		{
			break;
		}
		++pM;
	}
	return _atoi64(pM);
}
/* (例)
	PL4(inum_atof("-0123.45")); //=> -123.45000000
*/
// v2015-12-31
DOUBLE
inum_atof(
	MBS *pM // 文字列
)
{
	if(!pM || !*pM)
	{
		return 0;
	}
	while(*pM)
	{
		if(inum_chkM(pM))
		{
			break;
		}
		++pM;
	}
	return atof(pM);
}
//-----------------------------------------------------------------------------------------
// Copyright (C) 1997 - 2002, Makoto Matsumoto and Takuji Nishimura, All rights reserved.
//  A C-program for MT19937, with initialization improved 2002/1/26.
//  Coded by Takuji Nishimura and Makoto Matsumoto.
//-----------------------------------------------------------------------------------------
/*
	http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/MT2002/CODES/mt19937ar.c
	上記コードを元に以下の関数についてカスタマイズを行った。
	MT関連の最新情報（派生版のSFMT、TinyMTなど）については下記を参照のこと
		http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/mt.html
*/
/* (例)
INT
main()
{
	CONST INT Output = 10; // 出力数
	CONST INT Min    = -5; // 最小値(>=INT_MIN)
	CONST INT Max    =  5; // 最大値(<=INT_MAX)

	MT_init(TRUE); // 初期化

	for(INT _i1 = 0; _i1 < Output; _i1++)
	{
		P4(MT_irand_DBL(Min, Max, 10));
	}

	MT_free(); // 解放

	return 0;
}
*/
/*
	Period parameters
*/
#define  MT_N 624
#define  MT_M 397
#define  MT_MATRIX_A         0x9908b0dfUL // constant vector a
#define  MT_UPPER_MASK       0x80000000UL // most significant w-r bits
#define  MT_LOWER_MASK       0x7fffffffUL // least significant r bits
static UINT MT_u1 = (MT_N + 1);           // MT_u1 == MT_N + 1 means ai1[MT_N] is not initialized
static UINT *MT_au1 = 0;                  // the array forthe state vector
// v2021-11-15
VOID
MT_init(
	BOOL fixOn
)
{
	// 二重Alloc回避
	MT_free();
	MT_au1 = icalloc(MT_N, sizeof(UINT), FALSE);
	// Seed設定
	UINT init_key[4];
	INT *ai1 = idate_now_to_iAryYmdhns_localtime();
		init_key[0] = ai1[3] * ai1[5];
		init_key[1] = ai1[3] * ai1[6];
		init_key[2] = ai1[4] * ai1[5];
		init_key[3] = ai1[4] * ai1[6];
	ifree(ai1);
	// fixOn == FALSE のとき時間でシャッフル
	if(!fixOn)
	{
		init_key[3] &= (INT)GetTickCount();
	}
	INT i = 1, j = 0, k = 4;
	while(k)
	{
		MT_au1[i] = (MT_au1[i] ^ ((MT_au1[i - 1] ^ (MT_au1[i - 1] >> 30)) * 1664525UL)) + init_key[j] + j; // non linear
		MT_au1[i] &= 0xffffffffUL; // for WORDSIZE>32 machines
		++i, ++j;
		if(i >= MT_N)
		{
			MT_au1[0] = MT_au1[MT_N - 1];
			i = 1;
		}
		if(j >= MT_N)
		{
			j = 0;
		}
		--k;
	}
	k = MT_N - 1;
	while(k)
	{
		MT_au1[i] = (MT_au1[i] ^ ((MT_au1[i - 1] ^ (MT_au1[i - 1] >> 30)) * 1566083941UL)) - i; // non linear
		MT_au1[i] &= 0xffffffffUL; // for WORDSIZE>32 machines
		++i;
		if(i >= MT_N)
		{
			MT_au1[0] = MT_au1[MT_N - 1];
			i = 1;
		}
		--k;
	}
	MT_au1[0] = 0x80000000UL; // MSB is 1;assuring non-zero initial array
}
/*
	generates a random number on [0, 0xffffffff]-interval
	generates a random number on [0, 0xffffffff]-interval
*/
// v2022-04-01
UINT
MT_genrand_UINT()
{
	UINT y = 0;
	static UINT mag01[2] = {0x0UL, MT_MATRIX_A};
	if(MT_u1 >= MT_N)
	{
		// generate N words at one time
		INT kk = 0;
		while(kk < MT_N - MT_M)
		{
			y = (MT_au1[kk] & MT_UPPER_MASK) | (MT_au1[kk + 1] & MT_LOWER_MASK);
			MT_au1[kk] = MT_au1[kk + MT_M] ^ (y >> 1) ^ mag01[y & 0x1UL];
			++kk;
		}
		while(kk < MT_N - 1)
		{
			y = (MT_au1[kk] & MT_UPPER_MASK) | (MT_au1[kk + 1] & MT_LOWER_MASK);
			MT_au1[kk] = MT_au1[kk + (MT_M - MT_N)] ^ (y >> 1) ^ mag01[y & 0x1UL];
			++kk;
		}
		y = (MT_au1[MT_N - 1] & MT_UPPER_MASK) | (MT_au1[0] & MT_LOWER_MASK);
		MT_au1[MT_N - 1] = MT_au1[MT_M - 1] ^ (y >> 1) ^ mag01[y & 0x1UL];
		MT_u1 = 0;
	}
	y = MT_au1[++MT_u1];
	// Tempering
	y ^= (y >> 11);
	y ^= (y <<  7) & 0x9d2c5680UL;
	y ^= (y << 15) & 0xefc60000UL;
	y ^= (y >> 18);
	return y;
}
// v2015-11-15
VOID
MT_free()
{
	ifree(MT_au1);
}
//----------------
// INT乱数を発生
//----------------
/* (例)
	MT_init(TRUE);                        // 初期化
	P("%lld\n", MT_irand_INT64(0, 1000)); // [0..100]
	MT_free();                            // 解放
*/
// v2022-04-01
INT64
MT_irand_INT64(
	INT posMin,
	INT posMax
)
{
	if(posMin > posMax)
	{
		return 0;
	}
	return (MT_genrand_UINT() % (posMax - posMin + 1)) + posMin;
}
//-------------------
// DOUBLE乱数を発生
//-------------------
/* (例)
	MT_init(TRUE);                        // 初期化
	P("%0.5f\n", MT_irand_DBL(0, 10, 5)); // [0.00000..10.00000]
	MT_free();                            // 解放
*/
// v2022-04-01
DOUBLE
MT_irand_DBL(
	INT posMin,
	INT posMax,
	INT decRound // [0..10]／[0]"1", [1]"0.1", .., [10]"0.0000000001"
)
{
	if(posMin > posMax)
	{
		return 0.0;
	}
	if(decRound > 10)
	{
		decRound = 0;
	}
	INT i1 = 1;
	while(decRound > 0)
	{
		i1 *= 10;
		--decRound;
	}
	return (DOUBLE)MT_irand_INT64(posMin, (posMax - 1)) + (DOUBLE)MT_irand_INT64(0, i1) / i1;
}
/////////////////////////////////////////////////////////////////////////////////////////
/*---------------------------------------------------------------------------------------
	Command Line
---------------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////
//-------------------------
// コマンド名／引数を取得
//-------------------------
/* (例)
	iCLI_getCommandLine();
	PL2($CMD);
	PL3($ARGC);
	iary_print($ARGV);
	iary_print($ARGS); // $ARGVからダブルクォーテーションを除去したもの
*/
// v2022-04-29
VOID
iCLI_getCommandLine()
{
	MBS *pM = ijs_trim(GetCommandLineA());

	CONST UINT AryMax = 32;
	MBS **argv = icalloc_MBS_ary(AryMax);
	MBS **args = icalloc_MBS_ary(AryMax);

	MBS *pBgn = NULL;
	MBS *pEnd = NULL;

	MBS *p1 = NULL;
	BOOL bArgc = FALSE;
	$ARGC = 0;

	for(pEnd = pM; pEnd < (pM + imn_len(pM)) && $ARGC < AryMax; pEnd++)
	{
		for(; *pEnd == ' '; pEnd++);

		if(*pEnd == '\"')
		{
			pBgn = pEnd;
			++pEnd;
			for(; *pEnd && *pEnd != '\"'; pEnd++);
			++pEnd;
			p1 = ims_pclone(pBgn, pEnd);
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
			p1 = ims_pclone(pBgn, pEnd);
		}

		if(bArgc)
		{
			argv[$ARGC] = p1;
			args[$ARGC] = ijs_replace(p1, "\"", "", FALSE);
			++$ARGC;
		}
		else
		{
			$CMD = p1;
			bArgc = TRUE;
		}
	}

	$ARGV = argv;
	$ARGS = args;
}
//-----------------------------------------
// 引数 [Key]+[Value] から [Value] を取得
//-----------------------------------------
/* (例)
	iCLI_getCommandLine();
	// $ARGS[0] => "-w=size <= 1000"
	P2(iCLI_getOptValue(0, "-w=", NULL)); //=> "size <= 1000"
*/
// v2022-04-03
MBS
*iCLI_getOptValue(
	UINT argc, // $ARGS[argc]
	MBS *opt1, // (例) "-w="
	MBS *opt2  // (例) "-where=", NULL
)
{
	if(argc >= $ARGC)
	{
		return NULL;
	}

	if(!imn_len(opt1))
	{
		return NULL;
	}
	else if(!imn_len(opt2))
	{
		opt2 = opt1;
	}

	MBS *p1 = $ARGS[argc];

	// 前方一致
	if(imb_cmpf(p1, opt1))
	{
		return (p1 + imn_len(opt1));
	}
	else if(imb_cmpf(p1, opt2))
	{
		return (p1 + imn_len(opt2));
	}

	return NULL;
}
//--------------------------
// 引数 [Key] と一致するか
//--------------------------
/* (例)
	iCLI_getCommandLine();
	// $ARGV[0] => "-repeat"
	P3(iCLI_getOptMatch(0, "-repeat", NULL)); //=> TRUE
	// $ARGV[0] => "-w=size <= 1000"
	P3(iCLI_getOptMatch(0, "-w=", NULL));     //=> FALSE
*/
// v2022-04-03
BOOL
iCLI_getOptMatch(
	UINT argc, // $ARGV[argc]
	MBS *opt1, // (例) "-r"
	MBS *opt2  // (例) "-repeat", NULL
)
{
	if(argc >= $ARGC)
	{
		return FALSE;
	}

	if(!imn_len(opt1))
	{
		return FALSE;
	}
	else if(!imn_len(opt2))
	{
		opt2 = opt1;
	}

	MBS *p1 = $ARGV[argc];

	// 完全一致
	if(imb_cmpp(p1, opt1) || imb_cmpp(p1, opt2))
	{
		return TRUE;
	}

	return FALSE;
}
/////////////////////////////////////////////////////////////////////////////////////////
/*---------------------------------------------------------------------------------------
	Array
---------------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////
//-----------------
// NULL配列を返す
//-----------------
/* (例)
	// if(!p) return (MBS**)ima_null();
	PP(ima_null()); NL();  //=> 有効アドレス
	PP(*ima_null()); NL(); //=> NULL
*/
// v2016-01-19
MBS
**ima_null()
{
	static MBS *ary[1] = {0};
	return (MBS**)ary;
}
//-------------------
// 配列サイズを取得
//-------------------
/* (例)
	iCLI_getCommandLine();
	PL3(iary_size($ARGV));
*/
// v2021-09-23
UINT
iary_size(
	MBS **ary // 引数列
)
{
	UINT rtn = 0;
	while(*ary++)
	{
		++rtn;
	}
	return rtn;
}
//---------------------
// 配列の合計長を取得
//---------------------
/* (例)
	iCLI_getCommandLine();
	PL3(iary_Mlen($ARGV));
	PL3(iary_Jlen($ARGV));
*/
// v2022-04-02
UINT
iary_Mlen(
	MBS **ary
)
{
	UINT rtn = 0;
	while(*ary)
	{
		rtn += imn_len(*ary++);
	}
	return rtn;
}
// v2022-04-02
UINT
iary_Jlen(
	MBS **ary
)
{
	UINT rtn = 0;
	while(*ary)
	{
		rtn += ijn_len(*ary++);
	}
	return rtn;
}
//--------------
// 配列をqsort
//--------------
/* (例)
	iCLI_getCommandLine();
	// 元データ
	LN();
	iary_print($ARGV);
	// 順ソート
	LN();
	iary_sortAsc($ARGV);
	iary_print($ARGV);
	// 逆順ソート
	LN();
	iary_sortDesc($ARGV);
	iary_print($ARGV);
	LN();
*/
// v2014-02-07
INT
iary_qsort_cmp(
	CONST VOID *p1, //
	CONST VOID *p2, //
	BOOL asc        // TRUE=昇順／FALSE=降順
)
{
	MBS **p11 = (MBS**)p1;
	MBS **p21 = (MBS**)p2;
	INT rtn = lstrcmpA(*p11, *p21); // 大小区別する
	return rtn *= (asc > 0 ? 1 : -1);
}
// v2014-02-07
INT
iary_qsort_cmpAsc(
	CONST VOID *p1,
	CONST VOID *p2
)
{
	return iary_qsort_cmp(p1, p2, TRUE);
}
// v2014-02-07
INT
iary_qsort_cmpDesc(
	CONST VOID *p1,
	CONST VOID *p2
)
{
	return iary_qsort_cmp(p1, p2, FALSE);
}
//---------------------
// 配列を文字列に変換
//---------------------
/* (例)
	iCLI_getCommandLine();
	PL2(iary_join($ARGV, " | "));
	PL2(iary_njoin($ARGV, " | ", 0, $ARGC));
	PL2(iary_njoin($ARGV, " | ", 0, iary_size($ARGV)));
	PL2(iary_njoin($ARGV, " | ", 0, 2));
	PL2(iary_njoin($ARGV, " | ", 1, 2));
*/
// v2022-04-02
MBS
*iary_njoin(
	MBS **ary,  // 配列
	MBS *token, // 区切文字
	UINT start, // 取得位置
	UINT count  // 個数
)
{
	UINT arySize = iary_size(ary);
	UINT tokenSize = imn_len(token);
	MBS *rtn = icalloc_MBS(iary_Mlen(ary) + (arySize * tokenSize));
	MBS *pEnd = rtn;
	UINT u1 = 0;
	while(u1 < arySize && count > 0)
	{
		if(u1 >= start)
		{
			--count;
			pEnd += imn_cpy(pEnd, ary[u1]);
			pEnd += imn_cpy(pEnd, token);
		}
		++u1;
	}
	*(pEnd - tokenSize) = 0;
	return rtn;
}
//---------------------------
// 配列から空白／重複を除く
//---------------------------
/*
	MBS *args[] = {"aaa", "AAA", "BBB", "", "bbb", NULL};
	MBS **as1 = {0};
	INT i1 = 0;
	//
	// TRUE = 大小区別しない
	//
	as1 = iary_simplify(args, TRUE);
	i1 = 0;
	while((as1[i1]))
	{
		PL2(as1[i1]); //=> "aaa", "BBB"
		++i1;
	}
	ifree(as1);
	//
	// FALSE = 大小区別する
	//
	as1 = iary_simplify(args, FALSE);
	i1 = 0;
	while((as1[i1]))
	{
		PL2(as1[i1]); //=> "aaa", "AAA", "BBB", "bbb"
		++i1;
	}
	ifree(as1);
*/
// v2021-11-17
MBS
**iary_simplify(
	MBS **ary,
	BOOL icase // TRUE=大文字小文字区別しない
)
{
	CONST UINT uArySize = iary_size(ary);
	UINT u1 = 0, u2 = 0;
	// iAryFlg 生成
	INT *iAryFlg = icalloc_INT(uArySize); // 初期値 = 0
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
					if(imb_cmppi(ary[u1], ary[u2]))
					{
						iAryFlg[u2] = -1; // ×
					}
				}
				else
				{
					if(imb_cmpp(ary[u1], ary[u2]))
					{
						iAryFlg[u2] = -1; // ×
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
	MBS **rtn = icalloc_MBS_ary(uAryUsed);
	u1 = u2 = 0;
	while(u1 < uArySize)
	{
		if(iAryFlg[u1] == 1)
		{
			rtn[u2] = ims_clone(ary[u1]);
			++u2;
		}
		++u1;
	}
	ifree(iAryFlg);
	return rtn;
}
//----------------
// 上位Dirを抽出
//----------------
/*
	// 存在しないDirは無視する
	// 大文字小文字を区別しない
	MBS *args[] = {"", "D:", "c:\\Windows\\", "a:", "C:", "d:\\TMP", NULL};
	MBS **as1 = iary_higherDir(args);
		iary_print(as1); //=> 'C:\' 'D:\'
	ifree(as1);
*/
// v2022-04-02
MBS
**iary_higherDir(
	MBS **ary
)
{
	UINT uArySize = iary_size(ary);
	MBS **rtn = icalloc_MBS_ary(uArySize);
	UINT u1 = 0, u2 = 0;
	// Dir名整形／存在するDirのみ抽出
	while(u1 < uArySize)
	{
		if(iFchk_typePathA(ary[u1]) == 1)
		{
			rtn[u2] = iFget_AdirA(ary[u1]);
			++u2;
		}
		++u1;
	}
	uArySize = iary_size(rtn);
	// 順ソート
	iary_sortAsc(rtn);
	// 上位Dirを取得
	u1 = 0;
	while(u1 < uArySize)
	{
		u2 = u1 + 1;
		while(u2 < uArySize)
		{
			// 前方一致／大小区別しない
			if(imb_cmpfi(rtn[u2], rtn[u1]))
			{
				rtn[u2] = "";
				++u2;
			}
			else
			{
				break;
			}
		}
		u1 = u2;
	}
	// 順ソート
	iary_sortAsc(rtn);
	// 先頭の "" を読み飛ばす
	// 有効配列を返す
	u1 = 0;
	while(u1 < uArySize)
	{
		if(*rtn[u1])
		{
			break;
		}
		++u1;
	}
	return (rtn + u1);
}
//-----------
// 配列一覧
//-----------
/* (例)
	// コマンドライン引数
	iary_print($ARGV);
	iary_print($ARGS);

	// 文字配列
	MBS *ary[] = {"ABC", "", "12345", NULL};
	iary_print(ary);
*/
// v2022-04-02
VOID
iary_print(
	MBS **ary // 引数列
)
{
	if(!ary)
	{
		return;
	}
	UINT u1 = 0;
	while(*ary)
	{
		P("[%04u] '%s'\n", ++u1, *ary++);
	}
}
/////////////////////////////////////////////////////////////////////////////////////////
/*---------------------------------------------------------------------------------------
	File/Dir処理(WIN32_FIND_DATAA)
---------------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////
/*
	typedef struct _WIN32_FIND_DATAA
	{
		DWORD dwFileAttributes;
		FILETIME ftCreationTime;   // ctime
		FILETIME ftLastAccessTime; // mtime
		FILETIME ftLastWriteTime;  // atime
		DWORD nFileSizeHigh;
		DWORD nFileSizeLow;
		MBS cFileName[MAX_PATH];
	}
	WIN32_FIND_DATAA, *PWIN32_FIND_DATAA, *LPWIN32_FIND_DATAA;

	typedef struct _FILETIME
	{
		DWORD dwLowDateTime;
		DWORD dwHighDateTime;
	}
	FILETIME;

	typedef struct _SYSTEMTIME
	{
		INT wYear;
		INT wMonth;
		INT wDayOfWeek;
		INT wDay;
		INT wHour;
		INT wMinute;
		INT wSecond;
		INT wMilliseconds;
	}
	SYSTEMTIME;
*/
//-------------------
// ファイル情報取得
//-------------------
/* (例1) 複数取得する場合
VOID
ifindA(
	$struct_iFinfoA *FI,
	MBS *dir,
	UINT dirLenA
)
{
	WIN32_FIND_DATAA F;
	MBS *p1 = FI->fullnameA + imn_cpy(FI->fullnameA, dir);
		*p1 = '*';
		*(++p1) = 0;
	HANDLE hfind = FindFirstFileA(FI->fullnameA, &F);
		// Dir
		iFinfo_initA(FI, &F, dir, dirLenA, NULL);
			P2(FI->fullnameA);
		// File
		do
		{
			/// PL2(F.cFileName);
			if(iFinfo_initA(FI, &F, dir, dirLenA, F.cFileName))
			{
				// Dir
				if((FI->uFtype) == 1)
				{
					p1 = ims_nclone(FI->fullnameA, FI->uEnd);
						ifindA(FI, p1, FI->uEnd); // Dir(下位)
					ifree(p1);
				}
				// File
				else
				{
					P2(FI->fullnameA);
				}
			}
		}
		while(FindNextFileA(hfind, &F));
	FindClose(hfind);
}
// main()
	$struct_iFinfoA *FI = iFinfo_allocA();
		MBS *p1 = ".\\";
		MBS *dir = iFget_AdirA(p1);
		if(dir)
		{
			ifindA(FI, dir, imn_len(dir));
		}
		ifree(dir);
	iFinfo_freeA(FI);
*/
/* (例2) 単一ファイルから情報取得する場合
// main()
	$struct_iFinfoA *FI = iFinfo_allocA();
	MBS *fn = "w32.s";
	if(iFinfo_init2M(FI, fn))
	{
		PL32(FI->iFsize, FI->fullnameA);
		PL2(ijp_forwardN(FI->fullnameA, FI->uFname));
	}
	iFinfo_freeA(FI);
*/
// v2016-08-09
$struct_iFinfoA
*iFinfo_allocA()
{
	return icalloc(1, sizeof($struct_iFinfoA), FALSE);
}
// v2016-08-09
$struct_iFinfoW
*iFinfo_allocW()
{
	return icalloc(1, sizeof($struct_iFinfoW), FALSE);
}
// v2016-08-09
VOID
iFinfo_clearA(
	$struct_iFinfoA *FI
)
{
	*FI->fullnameA = 0;
	FI->uFname = 0;
	FI->uExt = 0;
	FI->uEnd = 0;
	FI->uAttr = 0;
	FI->uFtype = 0;
	FI->cjdCtime = 0.0;
	FI->cjdMtime = 0.0;
	FI->cjdAtime = 0.0;
	FI->iFsize = 0;
}
// v2016-08-09
VOID
iFinfo_clearW(
	$struct_iFinfoW *FI
)
{
	*FI->fullnameW = 0;
	FI->uFname = 0;
	FI->uExt = 0;
	FI->uEnd = 0;
	FI->uAttr = 0;
	FI->uFtype = 0;
	FI->cjdCtime = 0.0;
	FI->cjdMtime = 0.0;
	FI->cjdAtime = 0.0;
	FI->iFsize = 0;
}
//---------------------------
// ファイル情報取得の前処理
//---------------------------
// v2022-04-02
BOOL
iFinfo_initA(
	$struct_iFinfoA *FI,
	WIN32_FIND_DATAA *F,
	MBS *dir,            // "\"を付与して呼ぶ／iFget_AdirA()で絶対値にしておく
	UINT dirLenA,
	MBS *name
)
{
	// "\." "\.." は除外
	if(name && imb_cmp_leqf(name, ".."))
	{
		return FALSE;
	}
	// FI->uAttr
	FI->uAttr = (UINT)F->dwFileAttributes; // DWORD => UINT

	// <32768
	if((FI->uAttr) >> 15)
	{
		iFinfo_clearA(FI);
		return FALSE;
	}

	// FI->fullnameW
	// FI->uFname
	// FI->uEnd
	MBS *p1 = FI->fullnameA + imn_cpy(FI->fullnameA, dir);
	UINT u1 = imn_cpy(p1, name);

	// FI->uFtype
	// FI->uExt
	// FI->iFsize
	if((FI->uAttr & FILE_ATTRIBUTE_DIRECTORY))
	{
		if(u1)
		{
			dirLenA += u1 + 1;
			*((FI->fullnameA) + dirLenA - 1) = '\\'; // "\" 付与
			*((FI->fullnameA) + dirLenA) = 0;        // NULL 付与
		}
		(FI->uFtype) = 1;
		(FI->uFname) = (FI->uExt) = (FI->uEnd) = dirLenA;
		(FI->iFsize) = 0;
	}
	else
	{
		(FI->uFtype) = 2;
		(FI->uFname) = dirLenA;
		(FI->uEnd) = (FI->uFname) + u1;
		(FI->uExt) = PathFindExtensionA(FI->fullnameA) - (FI->fullnameA);
		if((FI->uExt) < (FI->uEnd))
		{
			++(FI->uExt);
		}
		(FI->iFsize) = (INT64)F->nFileSizeLow + (F->nFileSizeHigh ? (INT64)(F->nFileSizeHigh) * MAXDWORD + 1 : 0);
	}

	// JST変換
	// FI->cftime
	// FI->mftime
	// FI->aftime
	if((FI->uEnd) <= 3)
	{
		(FI->cjdCtime) = (FI->cjdMtime) = (FI->cjdAtime) = 2444240.0; // 1980-01-01
	}
	else
	{
		FILETIME ft;
		FileTimeToLocalFileTime(&F->ftCreationTime, &ft);
			(FI->cjdCtime) = iFinfo_ftimeToCjd(ft);
		FileTimeToLocalFileTime(&F->ftLastWriteTime, &ft);
			(FI->cjdMtime) = iFinfo_ftimeToCjd(ft);
		FileTimeToLocalFileTime(&F->ftLastAccessTime, &ft);
			(FI->cjdAtime) = iFinfo_ftimeToCjd(ft);
	}

	return TRUE;
}
// v2022-04-02
BOOL
iFinfo_initW(
	$struct_iFinfoW *FI,
	WIN32_FIND_DATAW *F,
	WCS *dir,            // "\"を付与して呼ぶ
	UINT dirLenW,
	WCS *name
)
{
	// "\." "\.." は除外
	if(name && *name && iwb_cmp_leqf(name, L".."))
	{
		return FALSE;
	}

	// FI->uAttr
	FI->uAttr = (UINT)F->dwFileAttributes; // DWORD => UINT

	// <32768
	if((FI->uAttr) >> 15)
	{
		iFinfo_clearW(FI);
		return FALSE;
	}

	// FI->fullnameW
	// FI->uFname
	// FI->uEnd
	WCS *p1 = FI->fullnameW + iwn_cpy(FI->fullnameW, dir);
	UINT u1 = iwn_cpy(p1, name);

	// FI->uFtype
	// FI->uExt
	// FI->iFsize
	if((FI->uAttr & FILE_ATTRIBUTE_DIRECTORY))
	{
		if(u1)
		{
			dirLenW += u1 + 1;
			*((FI->fullnameW) + dirLenW - 1) = L'\\'; // "\" 付与
			*((FI->fullnameW) + dirLenW) = 0;         // NULL 付与
		}
		(FI->uFtype) = 1;
		(FI->uFname) = (FI->uExt) = (FI->uEnd) = dirLenW;
		(FI->iFsize) = 0;
	}
	else
	{
		(FI->uFtype) = 2;
		(FI->uFname) = dirLenW;
		(FI->uEnd) = (FI->uFname) + u1;
		(FI->uExt) = PathFindExtensionW(FI->fullnameW) - (FI->fullnameW);
		if((FI->uExt) < (FI->uEnd))
		{
			++(FI->uExt);
		}
		(FI->iFsize) = (INT64)F->nFileSizeLow + (F->nFileSizeHigh ? (INT64)(F->nFileSizeHigh) * MAXDWORD + 1 : 0);
	}

	// JST変換
	// FI->cftime
	// FI->mftime
	// FI->aftime
	if((FI->uEnd) <= 3)
	{
		(FI->cjdCtime) = (FI->cjdMtime) = (FI->cjdAtime) = 2444240.0; // 1980-01-01
	}
	else
	{
		FILETIME ft;
		FileTimeToLocalFileTime(&F->ftCreationTime, &ft);
			(FI->cjdCtime) = iFinfo_ftimeToCjd(ft);
		FileTimeToLocalFileTime(&F->ftLastWriteTime, &ft);
			(FI->cjdMtime) = iFinfo_ftimeToCjd(ft);
		FileTimeToLocalFileTime(&F->ftLastAccessTime, &ft);
			(FI->cjdAtime) = iFinfo_ftimeToCjd(ft);
	}
	return TRUE;
}
// v2022-04-02
BOOL
iFinfo_init2M(
	$struct_iFinfoA *FI, //
	MBS *path            // ファイルパス
)
{
	// 存在チェック
	/// PL3(iFchk_existPathA(path));
	if(!iFchk_existPathA(path))
	{
		return FALSE;
	}
	MBS *path2 = iFget_AdirA(path); // 絶対pathを返す
		INT iFtype = iFchk_typePathA(path2);
		UINT uDirLen = (iFtype == 1 ? imn_len(path2) : (UINT)(PathFindFileNameA(path2) - path2));
		MBS *pDir = (FI->fullnameA); // tmp
			imn_pcpy(pDir, path2, path2 + uDirLen);
		MBS *sName = 0;
			if(iFtype == 1)
			{
				// Dir
				imn_cpy(path2 + uDirLen, "."); // Dir検索用 "." 付与
			}
			else
			{
				// File
				sName = ims_clone(path2 + uDirLen);
			}
			WIN32_FIND_DATAA F;
			HANDLE hfind = FindFirstFileA(path2, &F);
				iFinfo_initA(FI, &F, pDir, uDirLen, sName);
			FindClose(hfind);
		ifree(sName);
	ifree(path2);
	return TRUE;
}
// v2016-08-09
VOID
iFinfo_freeA(
	$struct_iFinfoA *FI
)
{
	ifree(FI);
}
// v2016-08-09
VOID
iFinfo_freeW(
	$struct_iFinfoW *FI
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
// v2022-04-02
MBS
*iFinfo_attrToA(
	UINT uAttr
)
{
	MBS *rtn = icalloc_MBS(5);
	if(!rtn)
	{
		return NULL;
	}
	rtn[0] = (uAttr & FILE_ATTRIBUTE_DIRECTORY ? 'd' : '-'); // 16: Dir
	rtn[1] = (uAttr & FILE_ATTRIBUTE_READONLY  ? 'r' : '-'); //  1: ReadOnly
	rtn[2] = (uAttr & FILE_ATTRIBUTE_HIDDEN    ? 'h' : '-'); //  2: Hidden
	rtn[3] = (uAttr & FILE_ATTRIBUTE_SYSTEM    ? 's' : '-'); //  4: System
	rtn[4] = (uAttr & FILE_ATTRIBUTE_ARCHIVE   ? 'a' : '-'); // 32: Archive
	return rtn;
}
// v2021-09-24
UINT
iFinfo_attrAtoUINT(
	MBS *sAttr // "r, h, s, d, a" => 55
)
{
	if(!sAttr || !*sAttr)
	{
		return 0;
	}
	MBS **ap1 = ija_split_zero(sAttr);
	MBS **ap2 = iary_simplify(ap1, TRUE);
		MBS *p1 = iary_join(ap2, "");
	ifree(ap2);
	ifree(ap1);
	// 小文字に変換
	CharLowerA(p1);
	UINT rtn = 0;
	MBS *pE = p1;
	while(*pE)
	{
		// 頻出順
		switch(*pE)
		{
			// 32: ARCHIVE
			case('a'):
				rtn += FILE_ATTRIBUTE_ARCHIVE;
				break;

			// 16: DIRECTORY
			case('d'):
				rtn += FILE_ATTRIBUTE_DIRECTORY;
				break;

			// 4: SYSTEM
			case('s'):
				rtn += FILE_ATTRIBUTE_SYSTEM;
				break;

			// 2: HIDDEN
			case('h'):
				rtn += FILE_ATTRIBUTE_HIDDEN;
				break;

			// 1: READONLY
			case('r'):
				rtn += FILE_ATTRIBUTE_READONLY;
				break;
		}
		++pE;
	}
	ifree(p1);
	return rtn;
}
//* 2022-04-02
MBS
*iFinfo_ftypeToA(
	INT iFtype
)
{
	MBS *rtn = icalloc_MBS(1);
	switch(iFtype)
	{
		case(1): *rtn = 'd'; break;
		case(2): *rtn = 'f'; break;
		default: *rtn = '-'; break;
	}
	return rtn;
}
/*
	(Local)"c:\" => 0
	(Network)"\\localhost\" => 0
*/
// v2022-05-26
INT
iFinfo_depthA(
	$struct_iFinfoA *FI
)
{
	if(!*FI->fullnameA)
	{
		return -1;
	}
	return ijn_searchCnt(FI->fullnameA + 2, "\\") - 1;
}
// v2022-05-26
INT
iFinfo_depthW(
	$struct_iFinfoW *FI
)
{
	if(!*FI->fullnameW)
	{
		return -1;
	}
	return iwn_searchCnt(FI->fullnameW + 2, L"\\") - 1;
}
//---------------------------
// ファイルサイズ取得に特化
//---------------------------
// v2016-08-09
INT64
iFinfo_fsizeA(
	MBS *Fn // 入力ファイル名
)
{
	$struct_iFinfoA *FI = iFinfo_allocA();
	iFinfo_init2M(FI, Fn);
	INT64 rtn = FI->iFsize;
	iFinfo_freeA(FI);
	return rtn;
}
//---------------
// FileTime関係
//---------------
/*
	基本、FILETIME(UTC)で処理。
	必要に応じて、JST(UTC+9h)に変換した値を渡すこと。
*/
// v2015-12-23
MBS
*iFinfo_ftimeToA(
	FILETIME ft
)
{
	MBS *rtn = icalloc_MBS(32);
	SYSTEMTIME st;
	FileTimeToSystemTime(&ft, &st);
	if(st.wYear <= 1980 || st.wYear >= 2099)
	{
		rtn = 0;
	}
	else
	{
		sprintf(
			rtn,
			ISO_FORMAT_DATETIME,
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
// v2015-01-03
DOUBLE
iFinfo_ftimeToCjd(
	FILETIME ftime
)
{
	INT64 i1 = ((INT64)ftime.dwHighDateTime << 32) + ftime.dwLowDateTime;
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
		INT *ai = idate_reYmdhns(i_y, i_m, i_d, i_h, i_n, i_s); // 正規化
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
/////////////////////////////////////////////////////////////////////////////////////////
/*---------------------------------------------------------------------------------------
	File/Dir処理
---------------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////
//-----------------------------------
// ファイルが存在するときTRUEを返す
//-----------------------------------
/* (例)
	PL3(iFchk_existPathA("."));    //=> 1
	PL3(iFchk_existPathA(""));     //=> 0
	PL3(iFchk_existPathA("\\"));   //=> 1
	PL3(iFchk_existPathA("\\\\")); //=> 0
*/
// v2015-11-26
BOOL
iFchk_existPathA(
	MBS *path // ファイルパス
)
{
	if(!path || !*path)
	{
		return FALSE;
	}
	return (PathFileExistsA(path) ? TRUE : FALSE);
}
//---------------------------------
// Dir／File が実在するかチェック
//---------------------------------
/* (例)
	// 返り値
	//  Err  : 0
	//  Dir  : 1
	//  File : 2
	//
	// 存在チェックはしない
	// 必要に応じて iFchk_existPathA() で前処理
	PL3(iFchk_typePathA("."));                       //=> 1
	PL3(iFchk_typePathA(".."));                      //=> 1
	PL3(iFchk_typePathA("\\"));                      //=> 1
	PL3(iFchk_typePathA("c:\\windows\\"));           //=> 1
	PL3(iFchk_typePathA("c:\\windows\\system.ini")); //=> 2
	PL3(iFchk_typePathA("\\\\Network\\"));           //=> 2(不明なときも)
*/
// v2022-03-21
INT
iFchk_typePathA(
	MBS *path // ファイルパス
)
{
	if(!path || !*path)
	{
		return 0;
	}
	return (PathIsDirectoryA(path) ? 1 : 2);
}
//-------------------------------
// Binary File のときTRUEを返す
//-------------------------------
/* (例)
	PL3(iFchk_Bfile("aaa.exe")); //=> TRUE
	PL3(iFchk_Bfile("aaa.txt")); //=> FALSE
	PL3(iFchk_Bfile("???"));     //=> FALSE (存在しないとき)
*/
// v2022-04-29
BOOL
iFchk_Bfile(
	MBS *Fn
)
{
	FILE *Fp = fopen(Fn, "rb");
	if(!Fp)
	{
		return FALSE;
	}
	UINT cnt = 0, c = 0, u1 = 0;
	// 64byteでは不完全
	while((c = getc(Fp)) != (UINT)EOF && u1 < 128)
	{
		if(!c)
		{
			++cnt;
			break;
		}
		++u1;
	}
	fclose(Fp);
	return (0 < cnt ? TRUE : FALSE);
}
//---------------------
// ファイル名等を抽出
//---------------------
/* (例)
	// 存在しなくても機械的に抽出
	// 必要に応じて iFchk_existPathA() で前処理
	MBS *p1 = "c:\\windows\\win.ini";
	PL2(iFget_extPathname(p1, 0)); //=>"c:\windows\win.ini"
	PL2(iFget_extPathname(p1, 1)); //=>"win.ini"
	PL2(iFget_extPathname(p1, 2)); //=>"win"
*/
// v2022-04-02
MBS
*iFget_extPathname(
	MBS *path,
	INT option
)
{
	if(!path || !*path)
	{
		return 0;
	}
	MBS *rtn = icalloc_MBS(imn_len(path) + 3); // CRLF+NULL
	MBS *pBgn = 0;
	MBS *pEnd = 0;

	// Dir or File ?
	if(PathIsDirectoryA(path))
	{
		if(option < 1)
		{
			pEnd = rtn + imn_cpy(rtn, path);
			// "表\"対策
			if(*CharPrevA(0, pEnd) != '\\')
			{
				*pEnd = '\\'; // "\"
				++pEnd;
				*pEnd = 0;
			}
		}
	}
	else
	{
		switch(option)
		{
			// path
			case(0):
				imn_cpy(rtn, path);
				break;

			// name + ext
			case(1):
				pBgn = PathFindFileNameA(path);
				imn_cpy(rtn, pBgn);
				break;

			// name
			case(2):
				pBgn = PathFindFileNameA(path);
				pEnd = PathFindExtensionA(pBgn);
				imn_pcpy(rtn, pBgn, pEnd);
				break;
		}
	}
	return rtn;
}
//-------------------------------------
// 相対Dir を 絶対Dir("\"付き) に変換
//-------------------------------------
/* (例)
	// _fullpath() の応用
	PL2(iFget_AdirA(".\\"));
*/
// v2021-12-01
MBS
*iFget_AdirA(
	MBS *path // ファイルパス
)
{
	MBS *p1 = icalloc_MBS(IMAX_PATH);
	MBS *p2 = 0;
	switch(iFchk_typePathA(path))
	{
		// Dir
		case(1):
			p2 = ims_cats(2, path, "\\");
				_fullpath(p1, p2, IMAX_PATH);
			ifree(p2);
			break;
		// File
		case(2):
			_fullpath(p1, path, IMAX_PATH);
			break;
	}
	MBS *rtn = ims_clone(p1);
	ifree(p1);
	return rtn;
}
//----------------------------
// 相対Dir を "\" 付きに変換
//----------------------------
/* (例)
	// _fullpath() の応用
	PL2(iFget_RdirA(".")); => ".\\"
*/
// v2022-04-02
MBS
*iFget_RdirA(
	MBS *path // ファイルパス
)
{
	MBS *rtn = 0;
	if(PathIsDirectoryA(path))
	{
		rtn = ims_clone(path);
		UINT u1 = ijn_searchLenR(rtn, "\\");
		MBS *pEnd = rtn + imn_len(rtn) - u1;
		pEnd[0] = '\\';
		pEnd[1] = 0;
	}
	else
	{
		rtn = ijs_replace(path, "\\\\", "\\", FALSE);
	}
	return rtn;
}
//--------------------
// 複階層のDirを作成
//--------------------
/* (例)
	PL3(imk_dir("aaa\\bbb"));
*/
// v2022-03-21
BOOL
imk_dir(
	MBS *path // ファイルパス
)
{
	INT flg = 0;
	MBS *p1 = 0;
	MBS *pBgn = ijs_cut(path, "\\", "\\"); // 前後の'\'を消去
	MBS *pEnd = pBgn;
	while(*pEnd)
	{
		pEnd = ijp_searchL(pEnd, "\\");
		p1 = ims_pclone(pBgn, pEnd);
			if(CreateDirectory(p1, 0))
			{
				++flg;
			}
		ifree(p1);
		++pEnd;
	}
	ifree(pBgn);
	return (flg ? TRUE : FALSE);
}
/////////////////////////////////////////////////////////////////////////////////////////
/*---------------------------------------------------------------------------------------
	Console
---------------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////
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
	//    0 = 黒    1 = 赤    2 = 黄緑  3 = 黄
	//    4 = 青    5 = 紅紫  6 = 水    7 = 白
	P2("\033[91m 文字[赤] \033[0m");
	P2("\033[101m 背景[赤] \033[0m");
	P2("\033[91;107m 文字[赤]／背景[白] \033[0m");

	// RGBによる指定例
	//  文字色   \033[38;2;R;G;Bm
	//  背景色   \033[48;2;R;G;Bm
	P2("\033[38;2;255;255;255m\033[48;2;0;0;255m 文字[白]／背景[青] \033[0m");
*/
// v2022-03-23
VOID
iConsole_EscOn()
{
	$StdoutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD consoleMode = 0;
	GetConsoleMode($StdoutHandle, &consoleMode);
	consoleMode = (consoleMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
	SetConsoleMode($StdoutHandle, consoleMode);
}
/////////////////////////////////////////////////////////////////////////////////////////
/*---------------------------------------------------------------------------------------
	暦
---------------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////
//--------------------------
// 空白暦：1582/10/5-10/14
//--------------------------
// {-4712-1-1からの通算日, yyyymmdd}
INT NS_BEFORE[2] = {2299160, 15821004};
INT NS_AFTER[2]  = {2299161, 15821015};
//-------------------------
// 曜日表示設定 [7]=Err値
//-------------------------
MBS *WDAYS[8] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa", "**"};
//-----------------------
// 月末日(-1y12m - 12m)
//-----------------------
INT MDAYS[13] = {31, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
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
		if(!(i_y % 400))
		{
			return TRUE;
		}
		if(!(i_y % 100))
		{
			return FALSE;
		}
	}
	return (!(i_y % 4) ? TRUE : FALSE);
}
//-------------
// 月を正規化
//-------------
/* (例)
	INT *ai = idate_cnv_month(2011, 14, 1, 12);
	for(INT _i1 = 0; _i1 < 2; _i1++)
	{
		PL3(ai[_i1]); //=> 2012, 2
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
		INT *ai1 = idate_reYmdhns(i_y, i_m, i_d, 0, 0, 0); // 正規化
			i_y = ai1[0];
			i_m = ai1[1];
			i_d = ai1[2];
		ifree(ai1);
	}
	return (i_d == idate_month_end(i_y, i_m) ? TRUE : FALSE);
}
//-----------------------
// strを強引にCJDに変換
//-----------------------
/* (例)
	PL4(idate_MBStoCjd(">[1970-12-10] and <[2015-12-10]")); //=> 2440931.00000000
	PL4(idate_MBStoCjd(">[1970-12-10]"));                   //=> 2440931.00000000
	PL4(idate_MBStoCjd(">[+1970-12-10]"));                  //=> 2440931.00000000
	PL4(idate_MBStoCjd(">[0000-00-00]"));                   //=> 1721026.00000000
	PL4(idate_MBStoCjd(">[-1970-12-10]"));                  //=> 1001859.00000000
	PL4(idate_MBStoCjd(">[2016-01-02]"));                   //=> 2457390.00000000
	PL4(idate_MBStoCjd(">[0D]"));                           // "2016-01-02 10:44:08" => 2457390.00000000
	PL4(idate_MBStoCjd(">[]"));                             // "2016-01-02 10:44:08" => 2457390.44731481
	PL4(idate_MBStoCjd(""));                                //=> DBL_MAX
*/
// v2021-11-15
DOUBLE
idate_MBStoCjd(
	MBS *pM
)
{
	if(!pM || !*pM)
	{
		return DBL_MAX; // Errのときは MAX を返す
	}
	INT *ai = idate_now_to_iAryYmdhns_localtime();
	MBS *p1 = idate_replace_format_ymdhns(
		pM,
		"[", "]",
		"\t", // flg
		ai[0], ai[1], ai[2], ai[3], ai[4], ai[5]
	);
	ifree(ai);
	// flgチェック
	MBS *p2 = p1;
	while(*p2)
	{
		if(*p2 == '\t')
		{
			++p2;
			break;
		}
		++p2;
	}
	ai = idate_MBS_to_iAryYmdhns(p2);
	DOUBLE rtn = idate_ymdhnsToCjd(ai[0], ai[1], ai[2], ai[3], ai[4], ai[5]);
	ifree(p1);
	ifree(ai);
	return rtn;
}
//--------------------
// strを年月日に分割
//--------------------
/* (例)
	MBS **as1 = idate_MBS_to_mAryYmdhns("-2012-8-12 12:45:00");
		iary_print(as1); //=> -2012, 8, 12, 12, 45, 00
	ifree(as1);
*/
// v2022-04-02
MBS
**idate_MBS_to_mAryYmdhns(
	MBS *pM // (例) "2012-03-12 13:40:00"
)
{
	BOOL bMinus = (pM && *pM == '-' ? TRUE : FALSE);
	MBS **rtn = ija_split(pM, "/.-: ");
	if(bMinus)
	{
		MBS *p1 = rtn[0];
		memmove(p1 + 1, p1, imn_len(p1) + 1);
		p1[0] = '-';
	}
	return rtn;
}
//-------------------------
// strを年月日に分割(int)
//-------------------------
/* (例)
	INT *ai1 = idate_MBS_to_iAryYmdhns("-2012-8-12 12:45:00");
	for(INT _i1 = 0; _i1 < 6; _i1++)
	{
		PL3(ai1[_i1]); //=> -2012, 8, 12, 12, 45, 0
	}
	ifree(ai1);
*/
// v2021-11-17
INT
*idate_MBS_to_iAryYmdhns(
	MBS *pM // (例) "2012-03-12 13:40:00"
)
{
	INT *rtn = icalloc_INT(6);
	MBS **as1 = idate_MBS_to_mAryYmdhns(pM);
	INT i1 = 0;
	while(as1[i1])
	{
		rtn[i1] = atoi(as1[i1]);
		++i1;
	}
	ifree(as1);
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
// v2021-11-15
INT
*idate_cjd_to_iAryHhs(
	DOUBLE cjd
)
{
	INT *rtn = icalloc_INT(3);
	INT i_h = 0, i_n = 0, i_s = 0;

	// 小数点部分を抽出
	// [sec][補正前]  [cjd]
	//   0  =  0   -  0.00000000000000000
	//   1  =  1   -  0.00001157407407407
	//   2  =  2   -  0.00002314814814815
	//   3  >  2  NG  0.00003472222222222
	//   4  =  4   -  0.00004629629629630
	//   5  =  5   -  0.00005787037037037
	//   6  >  5  NG  0.00006944444444444
	//   7  =  7   -  0.00008101851851852
	//   8  =  8   -  0.00009259259259259
	//   9  =  9   -  0.00010416666666667
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
// v2021-11-15
INT
*idate_cjd_to_iAryYmdhns(
	DOUBLE cjd // cjd通日
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
	INT *ai2 = idate_cjd_to_iAryHhs(cjd);
		rtn[0] = i_y;
		rtn[1] = i_m;
		rtn[2] = i_d;
		rtn[3] = ai2[0];
		rtn[4] = ai2[1];
		rtn[5] = ai2[2];
	ifree(ai2);
	return rtn;
}
//----------------------
// CJDをFILETIMEに変換
//----------------------
// v2021-11-15
FILETIME
idate_cjdToFtime(
	DOUBLE cjd // cjd通日
)
{
	INT *ai1 = idate_cjd_to_iAryYmdhns(cjd);
	INT i_y = ai1[0], i_m = ai1[1], i_d = ai1[2], i_h = ai1[3], i_n = ai1[4], i_s = ai1[5];
	ifree(ai1);
	return iFinfo_ymdhnsToFtime(i_y, i_m, i_d, i_h, i_n, i_s, FALSE);
}
//---------
// 再計算
//---------
// v2013-03-21
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
	return idate_cjd_to_iAryYmdhns(cjd);
}
//-------------------------------------------
// cjd通日から曜日(日 = 0, 月 = 1...)を返す
//-------------------------------------------
// v2013-03-21
INT
idate_cjd_iWday(
	DOUBLE cjd // cjd通日
)
{
	return (INT)((INT)cjd + 1) % 7;
}
//-----------------------------------------
// cjd通日から日="Su", 月="Mo", ...を返す
//-----------------------------------------
// v2021-11-17
MBS
*idate_cjd_mWday(
	DOUBLE cjd // cjd通日
)
{
	return WDAYS[idate_cjd_iWday(cjd)];
}
//------------------------------
// cjd通日から年内の通日を返す
//------------------------------
// v2021-11-15
INT
idate_cjd_yeardays(
	DOUBLE cjd // cjd通日
)
{
	INT *ai = idate_cjd_to_iAryYmdhns(cjd);
	INT i1 = ai[0];
	ifree(ai);
	return (INT)(cjd - idate_ymdhnsToCjd(i1, 1, 0, 0, 0, 0));
}
//--------------------------------------
// 日付の前後 [6] = {y, m, d, h, n, s}
//--------------------------------------
/* (例)
	INT *ai = idate_add(2012, 1, 31, 0, 0, 0, 0, 1, 0, 0, 0, 0);
	for(INT _i1 = 0; _i1 < 6; _i1++)
	{
		PL3(ai[_i1]); //=> 2012, 2, 29, 0, 0, 0
	}
*/
// v2021-11-15
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
	INT *ai2 = idate_reYmdhns(i_y, i_m, i_d, i_h, i_n, i_s); // 正規化
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
		ai1 = idate_cjd_to_iAryYmdhns(cjd + add_d);
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
	INT *ai = idate_diff(2012, 1, 31, 0, 0, 0, 2012, 2, 29, 0, 0, 0); //=> sign = 1, y = 0, m = 1, d = 0, h = 0, n = 0, s = 0, days = 29
	for(INT _i1 = 0; _i1 < 7; _i1++)
	{
		PL3(ai[_i1]); //=> 2012, 2, 29, 0, 0, 0, 29
	}
*/
// v2021-11-15
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
	INT *ai1 = idate_cjd_to_iAryYmdhns(cjd1);
	INT *ai2 = idate_cjd_to_iAryYmdhns(cjd2);
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
	if(rtn[0] > 0                                                 // +のときのみ
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
// v2022-04-02
/*
VOID
idate_diff_checker(
	INT from_year, // 何年から
	INT to_year,   // 何年まで
	UINT repeat    // サンプル抽出数(乱数)
)
{
	INT rnd_y = to_year - from_year;
	if(rnd_y < 0)
	{
		rnd_y = -(rnd_y);
	}
	INT *ai1 = 0, *ai2 = 0, *ai3 = 0, *ai4 = 0;
	INT y1 = 0, y2 = 0, m1 = 0, m2 = 0, d1 = 0, d2 = 0;
	MBS s1[16] = "", s2[16] = "";
	MBS *err = 0;
	P2("-cnt--From----------To----------sign,    y,  m,  d---DateAdd------chk---");
	MT_init(TRUE);
	for(INT _i1 = 1; _i1 <= repeat; _i1++)
	{
		y1 = from_year + MT_irand_INT(0, rnd_y);
		y2 = from_year + MT_irand_INT(0, rnd_y);
		m1 = 1 + MT_irand_INT(0, 11);
		m2 = 1 + MT_irand_INT(0, 11);
		d1 = 1 + MT_irand_INT(0, 30);
		d2 = 1 + MT_irand_INT(0, 30);
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
		err = (imb_cmpp(s1, s2) ? "ok" : "NG");
		P(
			"%4d  %5d-%02d-%02d : %5d-%02d-%02d  [%2d, %4d, %2d, %2d]  %5d-%02d-%02d  %s\n",
			_i1,
			ai1[0], ai1[1], ai1[2], ai2[0], ai2[1], ai2[2],
			ai3[0], ai3[1], ai3[2], ai3[3], ai4[0], ai4[1], ai4[2],
			err
		);
		ifree(ai4);
		ifree(ai3);
		ifree(ai2);
		ifree(ai1);
	}
	MT_free();
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
	INT *ai = idate_reYmdhns(1970, 12, 10, 0, 0, 0);
	MBS *s1 = 0;
		s1 = idate_format_ymdhns("%g%y-%m-%d(%a), %c/%C", ai[0], ai[1], ai[2], ai[3], ai[4], ai[5]);
		PL2(s1);
	ifree(s1);
	ifree(ai);
*/
/* (例) diff
	INT *ai = idate_diff(1970, 12, 10, 0, 0, 0, 2021, 4, 18, 0, 0, 0);
	MBS *s1 = idate_format_diff("%g%y-%m-%d\t%W週\t%D日\t%S秒", ai[0], ai[1], ai[2], ai[3], ai[4], ai[5], ai[6], ai[7]);
		PL2(s1);
	ifree(s1);
	ifree(ai);
*/
// v2022-04-02
MBS
*idate_format_diff(
	MBS   *format, //
	INT   i_sign,  // 符号／-1="-", 0<="+"
	INT   i_y,     // 年
	INT   i_m,     // 月
	INT   i_d,     // 日
	INT   i_h,     // 時
	INT   i_n,     // 分
	INT   i_s,     // 秒
	INT64 i_days   // 通算日／idate_diff()で使用
)
{
	if(!format)
	{
		return "";
	}

	CONST UINT BufSizeMax = 512;
	CONST UINT BufSizeDmz = 64;
	MBS *rtn = icalloc_MBS(BufSizeMax + BufSizeDmz);
	MBS *pEnd = rtn;
	UINT uPos = 0;

	// Ymdhns で使用
	DOUBLE cjd = (i_days ? 0.0 : idate_ymdhnsToCjd(i_y, i_m, i_d, i_h, i_n, i_s));
	DOUBLE jd = idate_cjdToJd(cjd);

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
					pEnd += imn_cpy(pEnd, idate_cjd_mWday(cjd));
					break;

				case 'A': // 曜日数
					pEnd += sprintf(pEnd, "%d", idate_cjd_iWday(cjd));
					break;

				case 'c': // 年内の通算日
					pEnd += sprintf(pEnd, "%d", idate_cjd_yeardays(cjd));
					break;

				case 'C': // CJD通算日
					pEnd += sprintf(pEnd, CJD_FORMAT, cjd);
					break;

				case 'J': // JD通算日
					pEnd += sprintf(pEnd, CJD_FORMAT, jd);
					break;

				case 'e': // 年内の通算週
					pEnd += sprintf(pEnd, "%d", idate_cjd_yearweeks(cjd));
					break;

				// Diff
				case 'Y': // 通算年
					pEnd += sprintf(pEnd, "%d", i_y);
					break;

				case 'M': // 通算月
					pEnd += sprintf(pEnd, "%d", (i_y * 12) + i_m);
					break;

				case 'D': // 通算日
					pEnd += sprintf(pEnd, "%lld", i_days);
					break;

				case 'H': // 通算時
					pEnd += sprintf(pEnd, "%lld", (i_days * 24) + i_h);
					break;

				case 'N': // 通算分
					pEnd += sprintf(pEnd, "%lld", (i_days * 24 * 60) + (i_h * 60) + i_n);
					break;

				case 'S': // 通算秒
					pEnd += sprintf(pEnd, "%lld", (i_days * 24 * 60 * 60) + (i_h * 60 * 60) + (i_n * 60) + i_s);
					break;

				case 'W': // 通算週
					pEnd += sprintf(pEnd, "%lld", (i_days / 7));
					break;

				case 'w': // 通算週の余日
					pEnd += sprintf(pEnd, "%d", (INT)(i_days % 7));
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
					pEnd += sprintf(pEnd, "%04d", i_y);
					break;

				case 'm': // 月
					pEnd += sprintf(pEnd, "%02d", i_m);
					break;

				case 'd': // 日
					pEnd += sprintf(pEnd, "%02d", i_d);
					break;

				case 'h': // 時
					pEnd += sprintf(pEnd, "%02d", i_h);
					break;

				case 'n': // 分
					pEnd += sprintf(pEnd, "%02d", i_n);
					break;

				case 's': // 秒
					pEnd += sprintf(pEnd, "%02d", i_s);
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
				case('a'): *pEnd = '\a';      break;
				case('n'): *pEnd = '\n';      break;
				case('t'): *pEnd = '\t';      break;
				default:   *pEnd = format[1]; break;
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
	MBS *p1 = "1970/12/10 00:00:00";
	INT *ai1 = idate_MBS_to_iAryYmdhns(p1);
	PL2(idate_format_iAryToA(IDATE_FORMAT_STD, ai1));
*/
// v2021-11-15
MBS
*idate_format_iAryToA(
	MBS *format, //
	INT *ymdhns  // {y, m, d, h, n, s}
)
{
	INT *ai1 = ymdhns;
	MBS *rtn = idate_format_ymdhns(format, ai1[0], ai1[1], ai1[2], ai1[3], ai1[4], ai1[5]);
	return rtn;
}
// v2021-11-15
MBS
*idate_format_cjdToA(
	MBS *format,
	DOUBLE cjd
)
{
	INT *ai1 = idate_cjd_to_iAryYmdhns(cjd);
	MBS *rtn = idate_format_ymdhns(format, ai1[0], ai1[1], ai1[2], ai1[3], ai1[4], ai1[5]);
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
/*
	"[-20d]"  "2015-12-10 00:25:00"
	"[-20D]"  "2015-12-10 00:00:00"
	"[-20d%]" "2015-12-10 %"
	"[]"      "2015-12-30 00:25:00"
	"[%]"     "2015-12-30 %"
	"[Err]"   ""
	"[Err%]"  ""
*/
/* (例)
	MBS *pM = "date>[-1m%] and date<[1M]";
	INT *ai1 = idate_now_to_iAryYmdhns_localtime();
	MBS *p1 = idate_replace_format_ymdhns(
		pM,
		"[", "]",
		"'",
		ai1[0], ai1[1], ai1[2], ai1[3], ai1[4], ai1[5]
	);
	PL2(p1);
	ifree(p1);
	ifree(ai1);
*/
// v2022-04-02
MBS
*idate_replace_format_ymdhns(
	MBS *pM,        // 変換対象文字列
	MBS *quoteBgn,  // 囲文字 1文字 (例) "["
	MBS *quoteEnd,  // 囲文字 1文字 (例) "]"
	MBS *add_quote, // 出力文字に追加するquote (例) "'"
	INT i_y,        // ベース年
	INT i_m,        // ベース月
	INT i_d,        // ベース日
	INT i_h,        // ベース時
	INT i_n,        // ベース分
	INT i_s         // ベース秒
)
{
	if(!pM)
	{
		return NULL;
	}
	MBS *p1 = 0, *p2 = 0, *p3 = 0, *p4 = 0, *p5 = 0;
	UINT u1 = ijn_searchCnt(pM, quoteBgn);
	MBS *rtn = icalloc_MBS(imn_len(pM) + (32 * u1));
	MBS *pEnd = rtn;

	// quoteBgn がないとき、pのクローンを返す
	if(!u1)
	{
		imn_cpy(rtn, pM);
		return rtn;
	}

	// add_quoteの除外文字
	MBS *pAddQuote = (
		(*add_quote >= '0' && *add_quote <= '9') || *add_quote == '+' || *add_quote == '-' ?
		"" :
		add_quote
	);
	UINT uQuote1 = imn_len(quoteBgn), uQuote2 = imn_len(quoteEnd);
	INT add_y = 0, add_m = 0, add_d = 0, add_h = 0, add_n = 0, add_s = 0;
	INT cntPercent = 0;
	INT *ai = 0;
	BOOL bAdd = FALSE;
	BOOL flg = FALSE;
	BOOL zero = FALSE;
	MBS *ts1 = icalloc_MBS(imn_len(pM));

	// quoteBgn - quoteEnd を日付に変換
	p1 = p2 = pM;
	while(*p2)
	{
		// "[" を探す
		// pM = "[[", quoteBgn = "["のときを想定しておく
		if(*quoteBgn && imb_cmpf(p2, quoteBgn) && !imb_cmpf(p2 + uQuote1, quoteBgn))
		{
			bAdd = FALSE;  // 初期化
			p2 += uQuote1; // quoteBgn.len
			p1 = p2;

			// "]" を探す
			if(*quoteEnd)
			{
				p2 = ijp_searchL(p1, quoteEnd);
				imn_pcpy(ts1, p1, p2); // 解析用文字列

				// "[]" の中に数字が含まれているか
				u1 = 0;
				p3 = ts1;
				while(*p3)
				{
					if(*p3 >= '0' && *p3 <= '9')
					{
						u1 = 1;
						break;
					}
					++p3;
				}

				// 例外
				p3 = 0;
				switch((p2 - p1))
				{
					case(0): // "[]"
						p3 = "y";
						u1 = 1;
						break;

					case(1): // "[%]"
						if(*ts1 == '%')
						{
							p3 = "y%";
							u1 = 1;
						}
						break;

					default:
						p3 = ts1;
						break;
				}
				if(u1)
				{
					zero = FALSE; // "00:00:00" かどうか
					u1 = (INT)inum_atoi(p3); // 引数から数字を抽出
					while(*p3)
					{
						switch(*p3)
						{
							case 'Y': // 年 => "yyyy-mm-dd 00:00:00"
								zero = TRUE;
								/* Falls through. */ // -Wimplicit-fallthrough=3
							case 'y': // 年 => "yyyy-mm-dd hh:nn:ss"
								add_y = u1;
								flg = TRUE;
								bAdd = TRUE;
								break;

							case 'M': // 月 => "yyyy-mm-dd 00:00:00"
								zero = TRUE;
								/* Falls through. */ // -Wimplicit-fallthrough=3
							case 'm': // 月 => "yyyy-mm-dd hh:nn:ss"
								add_m = u1;
								flg = TRUE;
								bAdd = TRUE;
								break;

							case 'W': // 週 => "yyyy-mm-dd 00:00:00"
								zero = TRUE;
								/* Falls through. */ // -Wimplicit-fallthrough=3
							case 'w': // 週 => "yyyy-mm-dd hh:nn:ss"
								add_d = u1 * 7;
								flg = TRUE;
								bAdd = TRUE;
								break;

							case 'D': // 日 => "yyyy-mm-dd 00:00:00"
								zero = TRUE;
								/* Falls through. */ // -Wimplicit-fallthrough=3
							case 'd': // 日 => "yyyy-mm-dd hh:nn:ss"
								add_d = u1;
								flg = TRUE;
								bAdd = TRUE;
								break;

							case 'H': // 時 => "yyyy-mm-dd 00:00:00"
								zero = TRUE;
								/* Falls through. */ // -Wimplicit-fallthrough=3
							case 'h': // 時 => "yyyy-mm-dd hh:nn:ss"
								add_h = u1;
								flg = TRUE;
								bAdd = TRUE;
								break;

							case 'N': // 分 => "yyyy-mm-dd 00:00:00"
								zero = TRUE;
								/* Falls through. */ // -Wimplicit-fallthrough=3
							case 'n': // 分 => "yyyy-mm-dd hh:nn:ss"
								add_n = u1;
								flg = TRUE;
								bAdd = TRUE;
								break;

							case 'S': // 秒 => "yyyy-mm-dd 00:00:00"
								zero = TRUE;
								/* Falls through. */ // -Wimplicit-fallthrough=3
							case 's': // 秒 => "yyyy-mm-dd hh:nn:ss"
								add_s = u1;
								flg = TRUE;
								bAdd = TRUE;
								break;
						}
						// [1y6m] のようなとき [1y] で処理する
						if(flg)
						{
							break;
						}
						++p3;
					}

					// 略記か日付か
					cntPercent = 0;
					if(bAdd)
					{
						// "Y-s" 末尾の "%" を検索
						while(*p3)
						{
							if(*p3 == '%')
							{
								++cntPercent;
							}
							++p3;
						}

						// ±日時
						ai = idate_add(
							i_y, i_m, i_d, i_h, i_n, i_s,
							add_y, add_m, add_d, add_h, add_n, add_s
						);
						// その日時の 00:00 を求める
						if(zero)
						{
							ai[3] = ai[4] = ai[5] = 0;
						}

						// 初期化
						flg = FALSE;
						add_y = add_m = add_d = add_h = add_n = add_s = 0;
					}
					else
					{
						// "%" が含まれていれば「見なし」処理する
						p4 = ts1;
						while(*p4)
						{
							if(*p4 == '%')
							{
								++cntPercent;
							}
							++p4;
						}
						ai = idate_MBS_to_iAryYmdhns(ts1);
					}

					// bAddの処理を引き継ぎ
					p5 = idate_format_ymdhns(
						IDATE_FORMAT_STD,
						ai[0], ai[1], ai[2], ai[3], ai[4], ai[5]
					);
						// "2015-12-30 00:00:00" => "2015-12-30 %"
						if(cntPercent)
						{
							p4 = p5;
							while(*p4 != ' ')
							{
								++p4;
							}
							*(++p4) = '%'; // SQLiteの"%"を付与
							*(++p4) = 0;
						}
						pEnd += imn_cpy(pEnd, pAddQuote);
						pEnd += imn_cpy(pEnd, p5);
						pEnd += imn_cpy(pEnd, pAddQuote);
					ifree(p5);
					ifree(ai);
				}
				p2 += (uQuote2); // quoteEnd.len
				p1 = p2;
			}
		}
		else
		{
			pEnd += imn_pcpy(pEnd, p2, p2 + 1);
			++p2;
		}
		add_y = add_m = add_d = add_h = add_n = add_s = 0;
	}
	ifree(ts1);
	return rtn;
}
//---------------------
// 今日のymdhnsを返す
//---------------------
/* (例)
	// 今日 = 2012-06-19 00:00:00 のとき、
	idate_now_to_iAryYmdhns(0); // System(-9h) => 2012, 6, 18, 15, 0, 0
	idate_now_to_iAryYmdhns(1); // Local       => 2012, 6, 19,  0, 0, 0
*/
// v2021-11-15
INT
*idate_now_to_iAryYmdhns(
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
// v2021-11-15
DOUBLE
idate_nowToCjd(
	BOOL area // TRUE=LOCAL, FALSE=SYSTEM
)
{
	INT *ai = idate_now_to_iAryYmdhns(area);
	INT y = ai[0], m = ai[1], d = ai[2], h = ai[3], n = ai[4], s = ai[5];
	ifree(ai);
	return idate_ymdhnsToCjd(y, m, d, h, n, s);
}
