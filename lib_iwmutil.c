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
/* 2014-02-13
	変数ルール
		◆INT    ： 当面の標準
		◆UINT   ： (要注意)「FILE関係」「正数」のみ ⇒ 頃合みて INT64 に移行する
		◆DOUBLE ： 小数点の扱い全て
*/
/* 2016-08-19
	大域変数・定数表記
		◆iwmutil共通の変数 // "$IWM_"+型略記+大文字で始まる
			$IWM_(b|i|p|u)Str
		◆特殊な大域変数 // "$"の次は小文字
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
/* 2016-01-27
	関数名ルール
		[1] i = iwm-iwama
		[2] m = MBS(byte基準)／j = MBS(word文字基準)／w = WCS
		[3] a = array／b = boolean／i = integer(unsigned, double含む)／n = null／p = pointer／s = strings／v = void
		[4] _
*/
/* 2016-09-09
	戻り値について
		関数において、使用しない戻り値は設定せず VOID型 とすること。
		速度低下の根本原因。
*/
/////////////////////////////////////////////////////////////////////////////////////////
/*---------------------------------------------------------------------------------------
	大域変数
---------------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////
MBS      *$IWM_CMD         = "";  // コマンド名を格納
MBS      **$IWM_ARGV       = {0}; // ARGVを格納
UINT     $IWM_ARGC         = 0;   // ARGCを格納
HANDLE   $IWM_StdoutHandle = 0;   // 画面制御用ハンドル
UINT     $IWM_ColorDefault = 0;   // コンソール色
UINT     $IWM_ExecSecBgn   = 0;   // 実行開始時間
/////////////////////////////////////////////////////////////////////////////////////////
/*---------------------------------------------------------------------------------------
	実行開始時間
---------------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////
/*
	Win32 SDK Reference Help「win32.hlp(1996/11/26)」より
		経過時間はDWORD型で保存されています。
		システムを49.7日間連続して動作させると、
		経過時間は 0 に戻ります。
*/
/* (例)
	iExecSec_init(); //=> $IWM_ExecSecBgn // 実行開始時間
	Sleep(2000);
	P("-- %.6fsec\n\n", iExecSec_next());
	Sleep(1000);
	P("-- %.6fsec\n\n", iExecSec_next());
*/
// v2021-03-19
UINT
iExecSec(
	CONST UINT microSec // 0のとき Init
)
{
	UINT microSec2 = GetTickCount();
	if(!microSec)
	{
		$IWM_ExecSecBgn = microSec2;
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
	UINT num;  // 配列個数（配列以外 = 0）
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
// *__icallocMap の基本区画サイズ(適宜変更>0)
#define   IcallocDiv          32
// 8の倍数に丸める
#define   ceilX8(n)           (UINT)((((n) >> 3) + 2) << 3)
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
// v2016-01-31
VOID
*icalloc(
	UINT n,    // 個数
	UINT size, // 宣言子サイズ
	BOOL aryOn // TRUE=配列
)
{
	UINT size2 = sizeof($struct_icallocMap);

	// 初回 __icallocMap を更新
	if(__icallocMapSize == 0)
	{
		__icallocMapSize = IcallocDiv;
		__icallocMap = ($struct_icallocMap*)calloc(__icallocMapSize, size2);
			icalloc_err(__icallocMap);
		__icallocMapId = 0;
	}
	else if(__icallocMapSize <= __icallocMapEOD)
	{
		UINT uOldSize = __icallocMapSize;
		__icallocMapSize += IcallocDiv;
		__icallocMap = ($struct_icallocMap*)realloc(__icallocMap, __icallocMapSize * size2);
			icalloc_err(__icallocMap);
		memset(__icallocMap + uOldSize, 0, (IcallocDiv * size2)); // realloc() 部分を初期化
	}

	// 引数にポインタ割当
	VOID *rtn = calloc(ceilX8(n), size);
		icalloc_err(rtn); // 戻値が 0 なら exit()

	// ポインタ
	(__icallocMap + __icallocMapEOD)->ptr = rtn;

	// 配列
	(__icallocMap + __icallocMapEOD)->num = (aryOn ? n : 0);

	// サイズ
	(__icallocMap + __icallocMapEOD)->size = ceilX8(n) * size;

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
	// icalloc() で領域確保した後使用.
	MBS *pM = icalloc_MBS(1000);
	pM = irealloc_MBS(pM, 2000);
*/
// v2016-01-31
VOID
*irealloc(
	VOID *ptr, // icalloc()ポインタ
	UINT n,    // 個数
	UINT size  // 宣言子サイズ
)
{
	// 縮小のとき処理しない
	if((n * size) <= imi_len(ptr))
	{
		return ptr;
	}
	VOID *rtn = 0;
	UINT u1 = ceilX8(n * size);
	// __icallocMap を更新
	UINT u2 = 0;
	while(u2 < __icallocMapEOD)
	{
		if(ptr == (__icallocMap + u2)->ptr)
		{
			rtn = (VOID*)realloc(ptr, u1); // 遅いが確実
				icalloc_err(rtn); // 戻値が 0 なら exit()
			(__icallocMap + u2)->ptr = rtn;
			(__icallocMap + u2)->num = ((__icallocMap + u2)->num ? n : 0);
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
// v2016-08-30
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
			if(map->num)
			{
				// 1次元削除
				u2 = 0;
				while(u2 < (map->num))
				{
					if(!(*((MBS**)(map->ptr) + u2)))
					{
						break;
					}
					icalloc_free(*((MBS**)(map->ptr) + u2));
					++u2;
				}
				++__icallocMapFreeCnt;

				// 2次元削除
				memset(map->ptr, 0, map->size);
				free(map->ptr);
				memset(map, 0, __sizeof_icallocMap);
				return;
			}
			else
			{
				memset(map->ptr, 0, map->size);
				free(map->ptr);
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
	u1 = 0;
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
	/// P823("__icallocMapFreeCnt=", __icallocMapFreeCnt);
	/// P823("__icallocMapEOD=", __icallocMapEOD);
	/// P823("SweepCnt=", uSweep);
}
//---------------------------
// __icallocMapをリスト出力
//---------------------------
// v2020-05-12
VOID
icalloc_mapPrint1()
{
	if(!__icallocMapSize)
	{
		return;
	}
	iConsole_getColor();

	iConsole_setTextColor(9 + (0 * 16));
	P2("-1 ----------- 8 ------------ 16 ------------ 24 ------------ 32--------");
	CONST UINT _rowsCnt = 32;
	UINT uRowsCnt = _rowsCnt;
	UINT u1 = 0, u2 = 0;
	while(u1 < __icallocMapSize)
	{
		iConsole_setTextColor(10 + (1 * 16));
		while(u1 < uRowsCnt)
		{
			if((__icallocMap + u1)->ptr)
			{
				P("■");
				++u2;
			}
			else
			{
				P("□");
			}
			++u1;
		}
		P(" %7u", u2);
		uRowsCnt += _rowsCnt;

		iConsole_setTextColor(-1);
		NL();
	}
}
// v2020-05-12
VOID
icalloc_mapPrint2()
{
	iConsole_getColor();

	iConsole_setTextColor(9 + (0 * 16));
	P2("------- id ---- pointer -- array --- byte ------------------------------");
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
			iConsole_setTextColor(15 + (((map->num) ? 4 : 0) * 16));
			P(
				"%-7u %07u [%p] (%2u)%10u => '%s'",
				(u1 + 1),
				(map->id),
				(map->ptr),
				(map->num),
				(map->size),
				(map->ptr)
			);
			iConsole_setTextColor(-1);
			NL();
		}
		++u1;
	}
	iConsole_setTextColor(9 + (0 * 16));
	P(
		"------- Usage %-7u ---- %14u byte -------------------------\n\n",
		uUsedCnt,
		uUsedSize
	);
	iConsole_setTextColor(-1);
}
/////////////////////////////////////////////////////////////////////////////////////////
/*---------------------------------------------------------------------------------------
	printf()系
---------------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////
//-----------
// printf()
//-----------
/* (例)
	P("%s", "abc"); //=> "abc"
	P("abc");       //=> "abc"
*/
// v2015-01-24
VOID
P(
	CONST MBS *format,
	...
)
{
	va_list va;
	va_start(va, format);
		vfprintf(stdout, format, va);
	va_end(va);
}
//---------------
// 繰り返し表示
//---------------
/* (例)
	PR("abc", 3);  //=> "abcabcabc"
*/
// v2020-05-28
VOID
PR(
	MBS *pM,   // 文字列
	INT repeat // 繰り返し回数
)
{
	if(repeat <= 0)
	{
		return;
	}
	while(repeat--)
	{
		P(pM);
	}
}
//---------------
// 色文字を表示
//---------------
/* (例)
	iConsole_getColor(); //=> $IWM_ColorDefault // 元の色を保存
	PZ(15, "カラー文字：%s\n", "白"); //=> "カラー文字：白\n"
	PZ(-1, NULL); // 元の色に戻す
*/
// v2021-03-19
VOID
PZ(
	INT rgb, // iConsole_setTextColor() 参照
	MBS *format,
	...
)
{
	if(rgb < 0)
	{
		rgb = $IWM_ColorDefault;
	}
	SetConsoleTextAttribute($IWM_StdoutHandle, rgb);
	va_list va;
	va_start(va, format);
		vfprintf(stdout, format, va);
	va_end(va);
}
//-----------------------
// EscapeSequenceへ変換
//-----------------------
/* (例)
	// '\a' を '\a' と扱う
	// (補)通常は '\\a' を '\a' とする
*/
// v2013-02-20
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
					*(rtn + i1) = '\a';
					break;

				case('b'):
					*(rtn + i1) = '\b';
					break;

				case('t'):
					*(rtn + i1) = '\t';
					break;

				case('n'):
					*(rtn + i1) = '\n';
					break;

				case('v'):
					*(rtn + i1) = '\v';
					break;

				case('f'):
					*(rtn + i1) = '\f';
					break;

				case('r'):
					*(rtn + i1) = '\r';
					break;

				default:
					*(rtn + i1) = '\\';
					++i1;
					*(rtn + i1) = *pM;
					break;
			}
		}
		else
		{
			*(rtn + i1) = *pM;
		}
		++pM;
		++i1;
	}
	*(rtn + i1) = 0;
	return rtn;
}
//--------------------
// sprintf()の拡張版
//--------------------
/* (例)
	// NULへ出力（何も出力しない）
	FILE *oFp = fopen(NULL_DEVICE, "wb");
		MBS *p1 = ims_fprintf(oFp, "%s-%s", "ABC", "123"); //=> "ABC-123"
			P82(p1);
		ifree(p1);
	fclose(oFp);
	// 画面に出力
	P82(ims_fprintf(stdout, "%s-%s", "ABC", "123")); //=> "ABC-123"
*/
// v2021-03-21
MBS
*ims_fprintf(
	FILE *oFp,
	MBS *format,
	...
)
{
	va_list va;
	va_start(va, format);
		// 長さを求めるだけ
		UINT len = vfprintf(oFp, format, va);
		// フォーマット
		MBS *rtn = icalloc_MBS(len);
		vsprintf(rtn, format, va);
	va_end(va);
	return rtn;
}
/////////////////////////////////////////////////////////////////////////////////////////
/*---------------------------------------------------------------------------------------
	MBS／WCS／U8N変換
---------------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////
// v2016-09-05
WCS
*icnv_M2W(
	MBS *pM
)
{
	if(!pM)
	{
		return NULL;
	}
	UINT uW = iji_len(pM);
	WCS *pW = icalloc_WCS(uW);
	MultiByteToWideChar(CP_OEMCP, 0, pM, -1, pW, uW);
	return pW;
}
// v2020-05-30
U8N
*icnv_W2U(
	WCS *pW
)
{
	if(!pW)
	{
		return NULL;
	}
	UINT uW = iwi_len(pW);
	UINT uU = uW * 3;
	U8N *pU = icalloc_MBS(uU);
	WideCharToMultiByte(CP_UTF8, 0, pW, uW, pU, uU, 0, 0);
	return pU;
}
// v2016-09-05
U8N
*icnv_M2U(
	MBS *pM
)
{
	WCS *pW = icnv_M2W(pM);
	U8N *pU = icnv_W2U(pW);
	ifree(pW);
	return pU;
}
// v2016-09-05
WCS
*icnv_U2W(
	U8N *pU
)
{
	if(!pU)
	{
		return NULL;
	}
	UINT uW = iui_len(pU);
	WCS *pW = icalloc_WCS(uW);
	MultiByteToWideChar(CP_UTF8, 0, pU, -1, pW, uW);
	return pW;
}
// v2020-05-30
MBS
*icnv_W2M(
	WCS *pW
)
{
	if(!pW)
	{
		return NULL;
	}
	UINT uW = iwi_len(pW);
	UINT uM = uW * 2;
	MBS *pM = icalloc_MBS(uM);
	WideCharToMultiByte(CP_OEMCP, 0, pW, uW, pM, uM, 0, 0);
	return pM;
}
// v2016-09-05
MBS
*icnv_U2M(
	U8N *pU // UTF-8
)
{
	WCS *pW = icnv_U2W(pU);
	MBS *pM = icnv_W2M(pW);
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
	P83(imi_len(mp1)); //=> 15
	P83(iji_len(mp1)); //=> 10
	WCS *wp1 = M2W(mp1);
	P83(iwi_len(wp1)); //=> 10
*/
// v2016-09-06
UINT
imi_len(
	MBS *pM
)
{
	if(!pM)
	{
		return 0;
	}
	UINT uCnt = 0;
	while(*pM)
	{
		++pM;
		++uCnt;
	}
	return uCnt;
}
// v2016-09-06
UINT
iji_len(
	MBS *pM
)
{
	if(!pM)
	{
		return 0;
	}
	UINT uCnt = 0;
	while(*pM)
	{
		pM = CharNextA(pM);
		++uCnt;
	}
	return uCnt;
}
// v2020-05-30
UINT
iui_len(
	U8N *p
)
{
	if(!p)
	{
		return 0;
	}
	UINT rtn = 0;
	// BOMを読み飛ばす(UTF-8N は該当しない)
	if(*p == (CHAR)0xEF && *(p + 1) == (CHAR)0xBB && *(p + 2) == (CHAR)0xBF)
	{
		p += 3;
	}
	INT c = 0;
	while(*p)
	{
		if(*p & 0x80)
		{
			// 多バイト文字
			c = (*p & 0xfc);
			while(c & 0x80)
			{
				++p;
				c <<= 1;
			}
		}
		else
		{
			// 1バイト文字
			++p;
		}
		++rtn;
	}
	return rtn;
}
// v2016-09-06
UINT
iwi_len(
	WCS *p
)
{
	if(!p)
	{
		return 0;
	}
	UINT rtn = 0;
	while(*p)
	{
		++p;
		++rtn;
	}
	return rtn;
}
//-------------------------
// size先のポインタを返す
//-------------------------
/* (例)
	MBS *p1 = "ABあいう";
	P82(imp_forwardN(p1, 4)); //=> "いう"
*/
// v2021-03-20
MBS
*imp_forwardN(
	MBS *pM,   // 開始位置
	UINT sizeM // 文字数
)
{
	if(!pM)
	{
		return 0;
	}
	UINT uCnt = imi_len(pM);

	return (uCnt < sizeM ?
		(pM + uCnt) :
		(pM + sizeM)
	);
}
/* (例)
	MBS *p1 = "ABあいう";
	P82(ijp_forwardN(p1, 3)); //=> "いう"
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
		return 0;
	}
	while(*pM && sizeJ > 0)
	{
		pM = CharNextA(pM);
		--sizeJ;
	}
	return pM;
}
// v2020-05-30
U8N
*iup_forwardN(
	U8N *p,    // 開始位置
	UINT sizeU // 文字数
)
{
	if(!p)
	{
		return p;
	}
	// BOMを読み飛ばす(UTF-8N は該当しない)
	if(*p == (CHAR)0xEF && *(p + 1) == (CHAR)0xBB && *(p + 2) == (CHAR)0xBF)
	{
		p += 3;
	}
	INT c = 0;
	while(*p && sizeU > 0)
	{
		if(*p & 0x80)
		{
			// 多バイト文字
			c = (*p & 0xfc);
			while(c & 0x80)
			{
				++p;
				c <<= 1;
			}
		}
		else
		{
			// 1バイト文字
			++p;
		}
		--sizeU;
	}
	return p;
}
// v2014-11-23
MBS
*imp_sod(
	MBS *pM
)
{
	while(*pM)
	{
		--pM;
	}
	return (++pM);
}
// v2014-11-27
WCS
*iwp_sod(
	WCS *pW
)
{
	while(*pW)
	{
		--pW;
	}
	return (++pW);
}
//---------------
// 大小文字置換
//---------------
/* (例)
	MBS *p1 = "aBC";
	P82(ims_upper(p1)); //=> "ABC"
	P82(ims_lower(p1)); //=> "abc"
*/
// v2016-02-15
MBS
*ims_upper(
	MBS *pM
)
{
	MBS *rtn = ims_clone(pM);
	return CharUpperA(rtn);
}
// v2016-02-15
MBS
*ims_lower(
	MBS *pM
)
{
	MBS *rtn = ims_clone(pM);
	return CharLowerA(rtn);
}
//---------------------------
// コピー後、コピー長を返す
//---------------------------
// v2016-05-04
UINT
iji_plen(
	MBS *pBgn,
	MBS *pEnd
)
{
	if(!pBgn || !pEnd)
	{
		return 0;
	}
	UINT rtn = 0;
	while(*pBgn && pBgn < pEnd)
	{
		pBgn = CharNextA(pBgn);
		++rtn;
	};
	return rtn;
}
//-------------------------------------------
// コピー後、終端位置(NULL)のポインタを返す
//-------------------------------------------
/* (例)
	MBS *to = icalloc_MBS(100);
	MBS *pEnd = imp_cpy(to, "Iwama, "); //=> "Iwama, "
		imp_cpy(pEnd, "Yoshiyuki");     //=> "Iwama, Yoshiyuki"
	P82(to);
	ifree(to);
*/
// v2020-05-30
MBS
*imp_cpy(
	MBS *to,
	MBS *from
)
{
	if(!from)
	{
		return to;
	}
	while(*from)
	{
		*(to++) = *(from++);
	}
	*to = 0;
	return to; // 末尾のポインタ(\0)を返す
}
// v2020-05-30
WCS
*iwp_cpy(
	WCS *to,
	WCS *from
)
{
	if(!from)
	{
		return to;
	}
	while(*from)
	{
		*(to++) = *(from++);
	}
	*to = 0;
	return to; // 末尾のポインタ(\0)を返す
}
// v2020-05-30
MBS
*imp_pcpy(
	MBS *to,
	MBS *from1,
	MBS *from2
)
{
	if(!from1 || !from2)
	{
		return to;
	}
	while(*from1 && from1 < from2)
	{
		*(to++) = *(from1++);
	}
	*to = 0;
	return to; // 末尾のポインタ(\0)を返す
}
//-------------------------
// コピーした文字長を返す
//-------------------------
/* (例)
	MBS *to = icalloc_MBS(100);
	P83(imi_cpy(to, "abcde")); //=> 5
	ifree(to);
*/
// v2020-05-30
UINT
imi_cpy(
	MBS *to,
	MBS *from
)
{
	UINT rtn = 0;
	if(!from)
	{
		return 0;
	}
	while(*from)
	{
		*(to++) = *(from++);
		++rtn;
	}
	*to = 0;
	return rtn;
}
//-----------------------
// 新規部分文字列を生成
//-----------------------
/* (例)
	MBS *from="abcde";
	P82(ims_clone(from)); //=> "abcde"
*/
// v2020-05-30
MBS
*ims_clone(
	MBS *from
)
{
	MBS *rtn = icalloc_MBS(imi_len(from));
	imp_cpy(rtn, from);
	return rtn;
}
// v2020-05-30
WCS
*iws_clone(
	WCS *from
)
{
	WCS *rtn = icalloc_WCS(iwi_len(from));
	iwp_cpy(rtn, from);
	return rtn;
}
/* (例)
	MBS *from = "abcde";
	P82(ims_pclone(from, from + 3)); //=> "abc"
*/
// v2020-05-30
MBS
*ims_pclone(
	MBS *from1,
	MBS *from2
)
{
	MBS *rtn = icalloc_MBS(from2 - from1);
	imp_pcpy(rtn, from1, from2);
	return rtn;
}
/* (例)
	MBS *to = "123";
	MBS *from = "abcde";
	P82(ims_cat_pclone(to, from, from + 3)); //=> "123abc"
*/
// v2020-05-30
MBS
*ims_cat_pclone(
	MBS *to,    // 先
	MBS *from1, // 元の開始位置
	MBS *from2  // 元の終了位置(含まれる)
)
{
	MBS *rtn = icalloc_MBS(imi_len(to) + (from2 - from1));
	MBS *pEnd = imp_cpy(rtn, to);
		imp_pcpy(pEnd, from1, from2);
	return rtn;
}
/* (例)
	// 要素を呼び出す度 realloc する方がスマートだが、
	// 速度の点で不利なので calloc １回で済ませる。
	MBS *p1 = "123";
	MBS *p2 = "abcde";
	MBS *p3 = "いわま";
	MBS *rtn = ims_ncat_clone(p1, p2, p3, NULL); //=> "123abcde"
		P82(rtn);
	ifree(rtn);
*/
// v2020-05-30
MBS
*ims_ncat_clone(
	MBS *pM, // ary[0]
	...      // ary[1...]、最後は必ず NULL
)
{
	if(!pM)
	{
		return pM;
	}
	UINT u1 = 0;

	// [0]
	UINT uSize = imi_len(pM);

	// [1..n]
	va_list va;
	va_start(va, pM);
		UINT uCnt = 1;
		while(uCnt < IVA_LIST_MAX)
		{
			if(!(u1 = imi_len((MBS*)va_arg(va, MBS*))))
			{
				break;
			}
			uSize += u1;
			++uCnt;
		}
	va_end(va);

	MBS *rtn = icalloc_MBS(uSize);
	MBS *pEnd = imp_cpy(rtn, pM);

	va_start(va, pM);
		while(--uCnt)
		{
			pEnd = imp_cpy(pEnd, (MBS*)va_arg(va, MBS*)); // [0..n]
		}
	va_end(va);

	return rtn;
}
//-----------------------
// 新規部分文字列を生成
//-----------------------
/* (例)
	//----------------------------
	//  [ A  B  C  D  E ] のとき
	//    0  1  2  3  4
	//   -5 -4 -3 -2 -1
	//----------------------------
	MBS *p1 = "ABCDE";
	P82(ijs_sub_clone(p1,  0, 2)); //=> "AB"
	P82(ijs_sub_clone(p1,  1, 2)); //=> "BC"
	P82(ijs_sub_clone(p1,  2, 2)); //=> "CD"
	P82(ijs_sub_clone(p1,  3, 2)); //=> "DE"
	P82(ijs_sub_clone(p1,  4, 2)); //=> "E"
	P82(ijs_sub_clone(p1,  5, 2)); //=> ""
	P82(ijs_sub_clone(p1, -1, 2)); //=> "E"
	P82(ijs_sub_clone(p1, -2, 2)); //=> "DE"
	P82(ijs_sub_clone(p1, -3, 2)); //=> "CD"
	P82(ijs_sub_clone(p1, -4, 2)); //=> "BC"
	P82(ijs_sub_clone(p1, -5, 2)); //=> "AB"
	P82(ijs_sub_clone(p1, -6, 2)); //=> "A"
	P82(ijs_sub_clone(p1, -7, 2)); //=> ""
*/
// v2019-08-15
MBS
*ijs_sub_clone(
	MBS *pM,  // 文字列
	INT jpos, // 開始位置
	INT sizeJ // 文字数
)
{
	if(!pM)
	{
		return pM;
	}
	INT len = iji_len(pM);
	if(jpos >= len)
	{
		return imp_eod(pM);
	}
	if(jpos < 0)
	{
		jpos += len;
		if(jpos < 0)
		{
			sizeJ += jpos;
			jpos = 0;
		}
	}
	MBS *pBgn = ijp_forwardN(pM, jpos);
	MBS *pEnd = ijp_forwardN(pBgn, sizeJ);
	return ims_cat_pclone(NULL, pBgn, pEnd);
}
//--------------------------------
// lstrcmp()／lstrcmpi()より安全
//--------------------------------
/*
	lstrcmp() は大小比較しかしない(TRUE = 0, FALSE = 1 or -1)ので、
	比較する文字列長を揃えてやる必要がある.
*/
/* (例)
	P83(imb_cmp("", "abc", FALSE, FALSE));   //=> FALSE
	P83(imb_cmp("abc", "", FALSE, FALSE));   //=> TRUE
	P83(imb_cmp("", "", FALSE, FALSE));      //=> TRUE
	P83(imb_cmp(NULL, "abc", FALSE, FALSE)); //=> FALSE
	P83(imb_cmp("abc", NULL, FALSE, FALSE)); //=> FALSE
	P83(imb_cmp(NULL, NULL, FALSE, FALSE));  //=> FALSE
	P83(imb_cmp(NULL, "", FALSE, FALSE));    //=> FALSE
	P83(imb_cmp("", NULL, FALSE, FALSE));    //=> FALSE
	NL();
	// imb_cmpf(p, search)  <= imb_cmp(p, search, FALSE, FALSE)
	P83(imb_cmp("abc", "AB", FALSE, FALSE)); //=> FALSE
	// imb_cmpfi(p, search) <= imb_cmp(p, search, FALSE, TRUE)
	P83(imb_cmp("abc", "AB", FALSE, TRUE));  //=> TRUE
	// imb_cmpp(p, search)  <= imb_cmp(p, search, TRUE, FALSE)
	P83(imb_cmp("abc", "AB", TRUE, FALSE));  //=> FALSE
	// imb_cmppi(p, search) <= imb_cmp(p, search, TRUE, TRUE)
	P83(imb_cmp("abc", "AB", TRUE, TRUE));   //=> FALSE
	NL();
	// searchに１文字でも合致すればTRUEを返す
	P83(imb_cmp_leq(""   , "..", TRUE)); //=> TRUE
	P83(imb_cmp_leq("."  , "..", TRUE)); //=> TRUE
	P83(imb_cmp_leq(".." , "..", TRUE)); //=> TRUE
	P83(imb_cmp_leq("...", "..", TRUE)); //=> FALSE
	P83(imb_cmp_leq("...", ""  , TRUE)); //=> FALSE
	NL();
*/
// v2016-02-27
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
	INT i1 = 0, i2 = 0;
	while(*pM && *search)
	{
		i1 = *pM;
		i2 = *search;
		if(icase)
		{
			i1 = tolower(i1);
			i2 = tolower(i2);
		}
		if(i1 != i2)
		{
			break;
		}
		++pM;
		++search;
	}
	if(perfect)
	{
		// 末尾が共に \0 なら 完全一致した
		return (!*pM && !*search ? TRUE : FALSE);
	}
	// searchEの末尾が \0 なら 前方一致した
	return (!*search ? TRUE : FALSE);
}
// v2016-02-27
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
	// 例外 "" == "
	if(!*pW && !*search)
	{
		return TRUE;
	}
	// 検索対象 == "" のときは FALSE
	if(!*pW)
	{
		return FALSE;
	}
	INT i1 = 0, i2 = 0;
	while(*pW && *search)
	{
		i1 = *pW;
		i2 = *search;
		if(icase)
		{
			i1 = towlower(i1);
			i2 = towlower(i2);
		}
		if(i1 != i2)
		{
			break;
		}
		++pW;
		++search;
	}
	if(perfect)
	{
		// 末尾が共に \0 なら 完全一致した
		return (!*pW && !*search ? TRUE : FALSE);
	}
	// searchEの末尾が \0 なら 前方一致した
	return (!*search ? TRUE : FALSE);
}
//-------------------------------
// 無視する範囲の終了位置を返す
//-------------------------------
/* (例)
	MBS *p1 = "<-123, <-4, 5->, 6->->";
	P82(ijp_bypass(p1, "<-", "->")); //=> "->, 6->->"
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
	P83(iji_searchCnti("d:\\folder1\\表\", "\\"));  //=> 2
	P83(iji_searchCntLi("表\表\123表\表\", "表\")); //=> 2
	P83(iji_searchCntRi("表\表\123表\表\", "表\")); //=> 2
	P83(iji_searchLenLi("表\表\123表\表\", "表\")); //=> 2
	P83(imi_searchLenLi("表\表\123表\表\", "表\")); //=> 4
	P83(iji_searchLenRi("表\表\123表\表\", "表\")); //=> 2
	P83(imi_searchLenRi("表\表\123表\表\", "表\")); //=> 4
*/
// v2014-11-29
UINT
iji_searchCntA(
	MBS *pM,     // 文字列
	MBS *search, // 検索文字列
	BOOL icase   // TRUE=大文字小文字区別しない
)
{
	if(!pM || !search)
	{
		return 0;
	}
	UINT rtn = 0;
	CONST UINT _searchLen = iji_len(search);
	while(*pM)
	{
		if(imb_cmp(pM, search, FALSE, icase))
		{
			pM = ijp_forwardN(pM, _searchLen);
			++rtn;
		}
		else
		{
			pM = CharNextA(pM);
		}
	}
	return rtn;
}
// v2014-11-30
UINT
iwi_searchCntW(
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
	CONST UINT _searchLen = iwi_len(search);
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
// v2016-02-11
UINT
iji_searchCntLA(
	MBS *pM,     // 文字列
	MBS *search, // 検索文字列
	BOOL icase,  // TRUE=大文字小文字区別しない
	INT option   // 0=個数／1=Byte数／2=文字数
)
{
	if(!pM || !search)
	{
		return 0;
	}
	UINT rtn = 0;
	CONST UINT _searchLen = imi_len(search);
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
		case(0):                         break;
		case(1): rtn *= imi_len(search); break;
		case(2): rtn *= iji_len(search); break;
	}
	return rtn;
}
// v2016-02-11
UINT
iji_searchCntRA(
	MBS *pM,     // 文字列
	MBS *search, // 検索文字列
	BOOL icase,  // TRUE=大文字小文字区別しない
	INT option   // 0=個数／1=Byte数／2=文字数
)
{
	if(!pM || !search)
	{
		return 0;
	}
	UINT rtn = 0;
	CONST UINT _searchLen = imi_len(search);
	MBS *pEnd = imp_eod(pM) - _searchLen;
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
		case(0):                         break;
		case(1): rtn *= imi_len(search); break;
		case(2): rtn *= iji_len(search); break;
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
	P82(ijp_searchLA("ABCABCDEFABC", "ABC", FALSE)); //=> "ABCABCDEFABC"
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
/* (例)
	P82(ijp_searchRA("ABCABCDEFABC", "ABC", FALSE)); //=> "ABC"
*/
// v2014-11-29
MBS
*ijp_searchRA(
	MBS *pM,     // 文字列
	MBS *search, // 検索文字列
	BOOL icase   // TRUE=大文字小文字区別しない
)
{
	if(!pM)
	{
		return pM;
	}
	MBS *pEnd = pM + imi_len(pM) - imi_len(search);
	while(pM <= pEnd)
	{
		if(imb_cmp(pEnd, search, FALSE, icase))
		{
			break;
		}
		--pEnd;
	}
	return pEnd;
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
// v2016-02-11
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
			if(*(pM + 1) == '<')
			{
				rtn = 3;
			}
			else
			{
				rtn = (*(pM + 1) == '=' ? 1 : 2);
			}
			break;

		// [0]"="
		case('='):
			rtn = 0;
			break;

		// [-2]"<" | [-1]"<=" | [3]"<>"
		case('<'):
			if(*(pM + 1) == '>')
			{
				rtn = 3;
			}
			else
			{
				rtn = (*(pM + 1) == '=' ? -1 : -2);
			}
			break;
	}
	if(bNot)
	{
		rtn += (rtn>0 ? -3 : 3);
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
icmpOperator_chk_INT(
	INT i1,
	INT i2,
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
	MBS *p1 = "2014年 4月29日  {(18時 42分) 00秒}";
	MBS *tokens = "年月日時分秒 ";
	MBS **ary = {0};
	// ary = ija_split(p1, tokens, "{}[]", FALSE); //=> [0]"2014" [1]"4" [2]"29" [3]"{(18時 42分) 00秒}"
	// ary = ija_split(p1, tokens, "{}[]", TRUE);  //=> [0]"2014" [1]"4" [2]"29" [3]"(18時 42分) 00秒"
	// 第3引数 = NULL | "" のとき、以降の引数は無視される
	// ary = ija_split(p1, tokens, "", FALSE);     //=> [0]"2014" [1]"4" [2]"29" [3]"{<18" [4]"42" [5]")" [6]"00" [7]"}"
	// ary = ija_split(p1, tokens, "", TRUE);      //=> [0]"2014" [1]"4" [2]"29" [3]"{<18" [4]"42" [5]")" [6]"00" [7]"}"
	// ary = ija_split(p1, "", "", FALSE);         // 1文字ずつ返す
	// ary = ija_split(p1, "", "", TRUE);          // 1文字ずつ返す
	// ary = ija_split(p1, NULL, "", FALSE);       // pを返す
	// ary = ija_split(p1, NULL, "", TRUE);        // pを返す
	iary_print(ary);
	ifree(ary);
*/
// v2020-05-30
MBS
**ija_split(
	MBS *pM,       // 元文字列
	MBS *tokens,   // 区切文字（複数）
	MBS *quotes,   // quote文字（2文字1セット／複数可）／NULL | "" のとき無視
	BOOL quote_cut // TRUE=quote文字を消去／FALSE=しない
)
{
	if(!pM || !*pM || !tokens)
	{
		return ija_token_eod(pM);
	}
	if(!*tokens)
	{
		return ija_token_zero(pM);
	}
	// 前後の空白を無視
	MBS *sPtr = ijs_trim(pM);
	UINT arySize = 0, u1 = 0, u2 = 0;
	MBS *pBgn = 0, *pEnd = 0, *p1 = 0, *p2 = 0, *p3 = 0;
	MBS **aToken = ija_token_zero(tokens);
	MBS **aQuote = ija_token_zero((quotes ? quotes: NULL));
	u1 = 0;
	while((p1 = *(aToken + u1)))
	{
		arySize += iji_searchCnt(sPtr, p1);
		++u1;
	}
	++arySize;
	MBS **rtn = icalloc_MBS_ary(arySize);
	pBgn = pEnd = sPtr;
	u1 = 0;
	while(*pEnd)
	{
		// quote検索
		u2 = 0;
		while((p1 = *(aQuote + u2)) && (p2 = *(aQuote + u2 + 1)))
		{
			p3 = pEnd;
			pEnd = ijp_bypass(p3, p1, p2);
			if(pEnd > p3)
			{
				++pEnd;
			}
			u2 += 2;
		}
		// token検索
		u2 = 0;
		while((p1 = *(aToken + u2)))
		{
			if(imb_cmpf(pEnd, p1))
			{
				if(pEnd > pBgn)
				{
					*(rtn + u1) = ims_pclone(pBgn, pEnd);
					pBgn = CharNextA(pEnd);
					++u1;
				}
				pBgn = CharNextA(pEnd);
				break;
			}
			++u2;
		}
		pEnd = CharNextA(pEnd);
	}
	*(rtn + u1) = ims_pclone(pBgn, pEnd);
	// quoteを消去
	if(quote_cut)
	{
		arySize = iary_size(rtn);
		u1 = 0;
		while(u1 < arySize)
		{
			p1 = *(rtn + u1);
			u2 = 0;
			while(*(aQuote + u2) && *(aQuote + u2 + 1))
			{
				if(imb_cmpf(p1, *(aQuote + u2)))
				{
					p2 = ijs_rm_quote(p1, *(aQuote + u2), *(aQuote + u2 + 1), FALSE, TRUE);
						imp_cpy(p1, p2);
					ifree(p2);
					break;
				}
				u2 += 2;
			}
			++u1;
		}
	}
	ifree(aQuote);
	ifree(aToken);
	ifree(sPtr);
	return rtn;
}
//-----------------------------
// NULL(行末)で区切って配列化
//-----------------------------
/* (例)
	INT i1 = 0;
	MBS **ary = ija_token_eod("ABC");
	iary_print(ary); //=> [ABC]
*/
// v2020-05-30
MBS
**ija_token_eod(
	MBS *pM
)
{
	MBS **rtn = icalloc_MBS_ary(1);
	*(rtn + 0) = ims_clone(pM);
	return rtn;
}
//---------------------------
// １文字づつ区切って配列化
//---------------------------
/* (例)
	INT i1 = 0;
	MBS **ary = ija_token_zero("ABC");
	iary_print(ary); //=> "A" "B" "C"
*/
// v2020-05-30
MBS
**ija_token_zero(
	MBS *pM
)
{
	MBS **rtn = 0;
	if(!pM)
	{
		rtn = ima_null();
		return rtn;
	}
	CONST UINT pLen = iji_len(pM);
	rtn = icalloc_MBS_ary(pLen);
	MBS *pBgn = pM;
	MBS *pEnd = 0;
	UINT u1 = 0;
	while(u1 < pLen)
	{
		pEnd = CharNextA(pBgn);
		*(rtn + u1) = ims_pclone(pBgn, pEnd);
		pBgn = pEnd;
		++u1;
	}
	return rtn;
}
//----------------------
// quote文字を消去する
//----------------------
/* (例)
	P82(ijs_rm_quote("[[ABC]", "[", "]", TRUE, TRUE));           //=> "[ABC"
	P82(ijs_rm_quote("[[ABC]", "[", "]", TRUE, FALSE));          //=> "ABC"
	P82(ijs_rm_quote("<A>123</A>", "<a>", "</a>", FALSE, TRUE)); //=> "123"
*/
// v2016-02-16
MBS
*ijs_rm_quote(
	MBS *pM,      // 文字列
	MBS *quoteL,  // 消去する先頭文字列
	MBS *quoteR,  // 消去する末尾文字列
	BOOL icase,   // TRUE=大文字小文字区別しない
	BOOL oneToOne // TRUE=quote数を対で処理
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
	CONST UINT quoteL2Len = imi_len(quoteL2);
	UINT quoteL2Cnt = iji_searchCntL(rtn, quoteL2);
	// 末尾のquote数
	CONST UINT quoteR2Len = imi_len(quoteR2);
	UINT quoteR2Cnt = iji_searchCntR(rtn, quoteR2);
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
	imb_shiftL(rtn, (quoteL2Len*quoteL2Cnt));      // 先頭位置をシフト
	*(imp_eod(rtn) - (quoteR2Len*quoteR2Cnt)) = 0; // 末尾にNULL代入
	return rtn;
}
//---------------------
// 数字文字列を区切る
//---------------------
/* (例)
	P82(ims_addTokenNStr("+000123456.7890"));    //=> "+123,456.7890"
	P82(ims_addTokenNStr(".000123456.7890"));    //=> "0.000123456.7890"
	P82(ims_addTokenNStr("+.000123456.7890"));   //=> "+0.000123456.7890"
	P82(ims_addTokenNStr("0000abcdefg.7890"));   //=> "0abcdefg.7890"
	P82(ims_addTokenNStr("1234abcdefg.7890"));   //=> "1,234abcdefg.7890"
	P82(ims_addTokenNStr("+0000abcdefg.7890"));  //=> "+0abcdefg.7890"
	P82(ims_addTokenNStr("+1234abcdefg.7890"));  //=> "+1,234abcdefg.7890"
	P82(ims_addTokenNStr("+abcdefg.7890"));      //=> "+abcdefg0.7890"
	P82(ims_addTokenNStr("±1234567890.12345")); //=> "±1,234,567,890.12345"
	P82(ims_addTokenNStr("aiuあいう@岩間"));     //=> "aiuあいう@岩間"
*/
// v2016-02-18
MBS
*ims_addTokenNStr(
	MBS *pM
)
{
	if(!pM || !*pM)
	{
		return pM;
	}
	UINT u1 = imi_len(pM);
	UINT u2 = 0;
	MBS *rtn = icalloc_MBS(u1 * 2);
	MBS *pRtnE = rtn;
	MBS *p1 = 0;

	// "-000123456.7890" のとき
	// (1) 先頭の [\S*] を探す
	MBS *pB = pM;
	MBS *pE = pM;
	while(*pE)
	{
		if((*pE >= '0' && *pE <= '9') || *pE == '.')
		{
			break;
		}
		++pE;
	}
	pRtnE = imp_pcpy(pRtnE, pB, pE);

	// (2) [0-9] 間を探す => "000123456"
	pB = pE;
	while(*pE)
	{
		if(*pE < '0' || *pE > '9')
		{
			break;
		}
		++pE;
	}

	// (2-11) 先頭は [.] か？
	if(*pB == '.')
	{
		pRtnE = imp_cpy(pRtnE, "0");
		imp_cpy(pRtnE, pB);
	}

	// (2-21) 連続する 先頭の[0] を調整 => "123456"
	else
	{
		while(*pB)
		{
			if(*pB != '0')
			{
				break;
			}
			++pB;
		}
		if(*(pB - 1) == '0' && (*pB < '0' || *pB > '9'))
		{
			--pB;
		}

		// (2-22) ", " 付与 => "123,456"
		p1 = ims_pclone(pB, pE);
			u1 = pE - pB;
			if(u1 > 3)
			{
				u2 = u1 % 3;
				if(u2)
				{
					pRtnE = imp_pcpy(pRtnE, p1, p1 + u2);
				}
				while(u2 < u1)
				{
					if(u2 > 0 && u2 < u1)
					{
						pRtnE = imp_cpy(pRtnE, ",");
					}
					pRtnE = imp_pcpy(pRtnE, p1 + u2, p1 + u2 + 3);
					u2 += 3;
				}
			}
			else
			{
				pRtnE = imp_cpy(pRtnE, p1);
			}
		ifree(p1);

		// (2-23) 残り => ".7890"
		imp_cpy(pRtnE, pE);
	}
	return rtn;
}
//-----------------------
// 左右の文字を消去する
//-----------------------
/* (例)
	P82(ijs_cut(" \tABC\t ", " \t", " \t")); //=> "ABC"
*/
// v2014-10-13
MBS
*ijs_cut(
	MBS *pM,
	MBS *rmLs,
	MBS *rmRs
)
{
	MBS *rtn = 0;
	MBS **aryLs = ija_token_zero(rmLs);
	MBS **aryRs = ija_token_zero(rmRs);
		rtn = ijs_cutAry(pM, aryLs, aryRs);
	ifree(aryLs);
	ifree(aryRs);
	return rtn;
}
// v2014-11-05
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
	UINT i1 = 0;
	MBS *pBgn = pM;
	MBS *pEnd = imp_eod(pBgn);
	MBS *p1 = 0;
	// 先頭
	if(execL)
	{
		while(*pBgn)
		{
			i1 = 0;
			while((p1 = *(aryLs + i1)))
			{
				if(imb_cmpf(pBgn, p1))
				{
					break;
				}
				++i1;
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
			i1 = 0;
			while((p1 = *(aryRs + i1)))
			{
				if(imb_cmpf(pEnd, p1))
				{
					break;
				}
				++i1;
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
// v2014-11-05
MBS
*ARY_SPACE[] = {
	"\x20",     // " "
	"\x81\x40", // "　"
	"\t",
	EOD
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
	EOD
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
	P82(ijs_replace("100YEN", "YEN", "円")); //=> "100円"
*/
// v2016-08-24
MBS
*ijs_replace(
	MBS *from,   // 文字列
	MBS *before, // 変換前の文字列
	MBS *after   // 変換後の文字列
)
{
	if(!from || !*from || !before || !*before)
	{
		return ims_clone(from);
	}
	UINT beforeLen = iji_len(before);
	UINT cnvCnt = iji_searchCntA(from, before, FALSE);
	MBS *rtn = icalloc_MBS(imi_len(from) + (cnvCnt*(imi_len(after) - imi_len(before))));
	MBS *rtnE = rtn;
	MBS *pBgn = 0, *pEnd = 0;
	pBgn = pEnd = from;
	while((cnvCnt--))
	{
		pEnd = ijp_searchLA(pEnd, before, FALSE);
			rtnE = imp_pcpy(rtnE, pBgn, pEnd);
			rtnE = imp_cpy(rtnE, after);
		pBgn = pEnd = ijp_forwardN(pEnd, beforeLen);
	}
	imp_cpy(rtnE, pBgn);
	return rtn;
}
//-------------------------------
// 文字列の重複文字を一つにする
//-------------------------------
/* (例)
	P82(ijs_simplify("123abcabcabc456", "abc")); //=> "123abc456"
*/
// v2014-11-30
MBS
*ijs_simplify(
	MBS *pM,    // 文字列
	MBS *search // 検索文字列
)
{
	if(!pM)
	{
		return NULL;
	}
	if(!search || !*search)
	{
		return ims_clone(pM);
	}
	MBS *rtn = icalloc_MBS(imi_len(pM));
	MBS *rtnE = rtn;
	UINT iLen = 0;
	MBS *pEnd = pM;
	while(*pEnd)
	{
		iLen = iji_searchLenL(pEnd, search);
		if(iLen > 0)
		{
			--iLen;
			pEnd += iLen;
		}
		rtnE = ijp_cpy(rtnE, pEnd);
		pEnd = CharNextA(pEnd);
	}
	return rtn;
}
//-----------------------------
// 動的引数の文字位置をシフト
//-----------------------------
/* (例)
	MBS *p1 = ims_clone("123456789");
	P82(p1); //=> "123456789"
	imb_shiftL(p1, 3);
	P82(p1); //=> "456789"
	imb_shiftR(p1, 3);
	P82(p1); //=> "456"
*/
// v2016-02-16
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
	UINT u1 = imi_len(pM);
	if(byte > u1)
	{
		byte = u1;
	}
	memcpy(pM, pM + byte, (u1-byte + 1)); // NULLもコピー
	return TRUE;
}
// v2016-02-16
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
	UINT u1 = imi_len(pM);
	if(byte > u1)
	{
		byte = u1;
	}
	*(pM + u1 - byte) = 0;
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
	P83(inum_atoi("-0123.45")); //=> -123
*/
// v2015-12-31
INT
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
	return atoi(pM);
}
/* (例)
	P83(inum_atoi64("-0123.45")); //=> -123
*/
// v2015-12-31
INT64
inum_atoi64(
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
	P84(inum_atof("-0123.45")); //=> -123.45000000
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
	http: //www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/MT2002/CODES/mt19937ar.c
	上記コードを元に以下の関数についてカスタマイズを行った。
	MT関連の最新情報（派生版のSFMT、TinyMTなど）については下記を参照のこと
		http: //www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/mt.html
*/
/* サンプル
INT
main()
{
	#define Output 1000 // 出力数
	#define Min      -5 // 最小値(> = 0)
	#define Max       5 // 最大値(<UINT_MAX)
	#define Row       2 // 行あたり出力数
	INT i1 = 0;
	MT_initByAry(TRUE); // 初期化
	for(i1 = 0; i1 < Output; i1++)
	{
		P(
			"%20.12f%c",
			MT_irandDBL(Min, Max, 10),
			((i1 % Row) == (Row - 1) ? '\n' : ' ')
		);
	}
	MT_freeAry(); // 解放
	#undef Output
	#undef Min
	#undef Max
	#undef Row
	return 0;
}
*/
/*
	Period parameters
*/
#define   MT_N 624
#define   MT_M 397
#define   MT_MATRIX_A         0x9908b0dfUL // constant vector a
#define   MT_UPPER_MASK       0x80000000UL // most significant w-r bits
#define   MT_LOWER_MASK       0x7fffffffUL // least significant r bits
static UINT MT_i1 = (MT_N + 1); // MT_i1 == MT_N + 1 means au1[MT_N] is not initialized
static UINT *MT_au1 = 0;        // the array forthe state vector
// v2015-12-31
VOID
MT_initByAry(
	BOOL fixOn
)
{
	// 二重Alloc回避
	MT_freeAry();
	MT_au1 = icalloc(MT_N, sizeof(UINT), FALSE);
	// Seed設定
	#define InitLen 4
	UINT init_key[InitLen] = {0x217, 0x1226, 0x1210, 0xBBBB};
	// fixOn == FALSE のとき時間でシャッフル
	if(!fixOn)
	{
		init_key[3] &= (INT)GetTickCount();
	}
	INT i = 1, j = 0, k = InitLen;
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
	#undef InitLen
}
/*
	generates a random number on [0, 0xffffffff]-interval
	generates a random number on [0, 0xffffffff]-interval
*/
// v2015-12-31
UINT
MT_genrandUint32()
{
	UINT y = 0;
	static UINT mag01[2] = {0x0UL, MT_MATRIX_A};
	if(MT_i1 >= MT_N)
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
		MT_i1 = 0;
	}
	y = MT_au1[++MT_i1];
	// Tempering
	y ^= (y >> 11);
	y ^= (y <<  7) & 0x9d2c5680UL;
	y ^= (y << 15) & 0xefc60000UL;
	y ^= (y >> 18);
	return y;
}
// v2015-11-15
VOID
MT_freeAry()
{
	ifree(MT_au1);
}
//----------------
// INT乱数を発生
//----------------
/* (例)
	MT_initByAry(TRUE);               // 初期化
	P("%3d\n", MT_irand_INT(0, 100)); // [0..100]
	MT_freeAry();                     // 解放
*/
// v2015-12-30
INT
MT_irand_INT(
	INT posMin,
	INT posMax
)
{
	if(posMin > posMax)
	{
		return 0;
	}
	INT rtn = ((MT_genrandUint32() >> 1) % (posMax - posMin + 1)) + posMin;
	return rtn;
}
//-------------------
// DOUBLE乱数を発生
//-------------------
/* (例)
	MT_initByAry(TRUE);                    // 初期化
	P("%20.12f\n", MT_irandDBL(0, 10, 5)); // [0.00000..10.00000]
	MT_freeAry();                          // 解放
*/
// v2015-12-30
DOUBLE
MT_irandDBL(
	INT posMin,
	INT posMax,
	UINT decRound // [0..10]／[0]"1", [1]"0.1", .., [10]"0.0000000001"
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
	return (DOUBLE)MT_irand_INT(posMin, (posMax - 1)) + (DOUBLE)MT_irand_INT(0, i1) / i1;
}
//---------------
// 乱文字を発生
//---------------
/* (例)
	MT_initByAry(FALSE); // 初期化
	INT i1 = 0;
	for(i1 = 8; i1 <= 128; i1 *= 2)
	{
		P832(i1, MT_irand_words(i1, FALSE));
		P832(i1, MT_irand_words(i1, TRUE));
	}
	MT_freeAry(); // 解放
*/
// v2015-12-30
MBS
*MT_irand_words(
	UINT size, // 文字列長
	BOOL ext   // FALSEのとき英数字のみ／TRUEのとき"!#$%&@"を含む
)
{
	MBS *rtn = icalloc_MBS(size);
	MBS *w = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz!#$%&@"; // FALSE = 61／TRUE = 67
	UINT uMax = (ext ? 67 : 61);
	UINT u1 = 0;
	while(u1 < size)
	{
		*(rtn + u1) = *(w + MT_irand_INT(0, uMax));
		++u1;
	}
	return rtn;
}
/////////////////////////////////////////////////////////////////////////////////////////
/*---------------------------------------------------------------------------------------
	Command Line
---------------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////
//-------------------
// コマンド名を取得
//-------------------
/* (例)
	iCLI_getCMD(); //=> $IWM_CMD
	// "a.exe 123 abc ..."
	P82($IWM_CMD); //=> "a.exe"
*/
// v2021-03-19
MBS
*iCLI_getCMD()
{
	MBS *pBgn = GetCommandLineA();
	MBS *pEnd = pBgn;
	for(; *pEnd && *pEnd != ' '; pEnd++);
	$IWM_CMD = ims_pclone(pBgn, pEnd);
	return $IWM_CMD;
}
//--------------------------------
// 引数を取得（コマンド名は除去）
//--------------------------------
/* (例)
	// "a.exe 123 abc"
	iCLI_getARGS();
		//=> $IWM_ARGV => ["123", "abc"]
		//=> $IWM_ARGC => 2
*/
// v2021-03-20
MBS
**iCLI_getARGS()
{
	MBS *pBgn = ijs_trim(GetCommandLineA());
	for(; *pBgn && *pBgn != ' '; pBgn++); // コマンド部分をスルー
	$IWM_ARGV = (*pBgn ?
		ija_split(pBgn, " ", "\"\"\'\'", TRUE) : // quote = [""]['']のみ対象
		ima_null()
	);
	$IWM_ARGC = iary_size($IWM_ARGV);
	return $IWM_ARGV;
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
// v2016-01-19
WCS
**iwa_null()
{
	static WCS *ary[1] = {0};
	return (WCS**)ary;
}
//-------------------
// 配列サイズを取得
//-------------------
/* (例)
	MBS **ary = iCLI_getARGS(); // {"123", "45", "abc", NULL}
	INT i1 = iary_size(ary);    //=> 3
*/
// v2016-01-19
UINT
iary_size(
	MBS **ary // 引数列
)
{
	UINT rtn = 0;
	while(*(ary + rtn))
	{
		++rtn;
	}
	return rtn;
}
// v2016-01-19
UINT
iwary_size(
	WCS **ary // 引数列
)
{
	UINT rtn = 0;
	while(*(ary + rtn))
	{
		++rtn;
	}
	return rtn;
}
//---------------------
// 配列の合計長を取得
//---------------------
/* (例)
	MBS **ary = iCLI_getARGS(); // {"123", "岩間", NULL}
	INT i1 = iary_Mlen(ary);    //=> 7
	INT i2 = iary_Jlen(ary);    //=> 5
*/
// v2016-01-19
UINT
iary_Mlen(
	MBS **ary
)
{
	UINT size = 0, cnt = 0;
	while(*(ary + cnt))
	{
		size += imi_len(*(ary + cnt));
		++cnt;
	}
	return size;
}
// v2016-01-19
UINT
iary_Jlen(
	MBS **ary
)
{
	UINT size = 0, cnt = 0;
	while(*(ary + cnt))
	{
		size += iji_len(*(ary + cnt));
		++cnt;
	}
	return size;
}
//--------------
// 配列をqsort
//--------------
/* (例)
	MBS **ary = iCLI_getARGS();
	// 元データ
	P8();
	iary_print(ary);
	NL();
	// 順ソート
	P8();
	iary_sortAsc(ary);
	iary_print(ary);
	NL();
	// 逆順ソート
	P8();
	iary_sortDesc(ary);
	iary_print(ary);
	NL();
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
// v2014-02-07
/*
VOID
iary_sort(
	MBS **ary, // 配列
	BOOL asc   // TRUE=昇順／FALSE=降順
)
{
	qsort(
		(MBS*)ary,
		iary_size(ary),
		sizeof(MBS**),
		(asc ? iary_qsort_cmpAsc : iary_qsort_cmpDesc)
	);
}
*/
//---------------------
// 配列を文字列に変換
//---------------------
/* (例)
	MBS **ary = iCLI_getARGS();
	MBS *p1 = iary_join(ary, "\t");
	P82(p1);
*/
// v2020-05-14
MBS
*iary_join(
	MBS **ary, // 配列
	MBS *token // 区切文字
)
{
	UINT arySize = iary_size(ary);
	UINT tokenSize = imi_len(token);
	UINT u1 = iary_Mlen(ary) + (tokenSize*arySize);
	MBS *rtn = icalloc_MBS(u1);
	MBS *p1 = rtn;
	u1 = 0;
	while(u1 < arySize)
	{
		p1 = imp_cpy(p1, *(ary + u1));
		if(token)
		{
			p1 = imp_cpy(p1, token);
		}
		++u1;
	}
	*(p1-tokenSize) = 0;
	return rtn;
}
//---------------------------
// 配列から空白／重複を除く
//---------------------------
/*
	// 有効配列マッピング
	//  (例)TRUE
	//    {"aaa", "AAA", "BBB", "", "bbb"}
	//    { 1   ,  -1  ,  -1  ,  0,  1   } // Flg
	MBS *args[] = {"aaa", "AAA", "BBB", "", "bbb", NULL};

	// 大小区別
	//   TRUE  => "aaa", "bbb"
	//   FALSE => "aaa", "AAA", "bbb", "BBB"
	MBS **pAryUsed = iary_simplify(args, TRUE);
	UINT uAryUsed = iary_size(pAryUsed);
	UINT u1 = 0;
		while(u1 < uAryUsed)
		{
			P832(u1, *(pAryUsed + u1));
			++u1;
		}
	ifree(pAryUsed);
*/
// v2020-05-30
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
		if(**(ary + u1) && iAryFlg[u1] > -1)
		{
			iAryFlg[u1] = 1; // 仮
			u2 = u1 + 1;
			while(u2 < uArySize)
			{
				if(icase)
				{
					if(imb_cmppi(*(ary + u1), *(ary + u2)))
					{
						iAryFlg[u2] = -1; // ×
					}
				}
				else
				{
					if(imb_cmpp(*(ary + u1), *(ary + u2)))
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
			*(rtn + u2) = ims_clone(*(ary + u1));
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
	// 有効配列マッピング
	//  (例)
	//    {"c:\", "", "c:\A\B\", "d:\A\B\", "C:\"}
	//    { 0   ,  0,  2       ,  2       ,  0   } // Depth : 0-MAX_PATH
	//    { 1   ,  0,  -1      ,  -1      ,  -1  } // Flg   : 1=OK , 0="" , -1=ダブリ
	//  (注)実在しないDirは無視される
	MBS *args[] = {"d:", "d:\\A\\B", "d:\\A\\B\\C\\D\\E\\", "", "D:\\A\\B\\", NULL};
	MBS **pAryUsed = iary_higherDir(args, 2); // 階層 = 2
	UINT uAryUsed = iary_size(pAryUsed);

	// depth
	//   0 => "d:\", "d:\A\B\", "d:\A\B\C\D\E\"
	//   1 => "d:\", "d:\A\B\", "d:\A\B\C\D\E\"
	//   2 => "d:\", "d:\A\B\C\D\E\"
	//   3 => "d:\"
	UINT u1 = 0;
		while(u1 < uAryUsed)
		{
			P832(u1, *(pAryUsed + u1));
			++u1;
		}
	ifree(pAryUsed);
*/
// v2020-05-30
MBS
**iary_higherDir(
	MBS **ary,
	UINT depth // 階層 = 0..MAX_PATH
)
{
	CONST UINT uArySize = iary_size(ary);
	UINT u1 = 0, u2 = 0;
	// 実在Dirのみ抽出
	MBS **sAry = icalloc_MBS_ary(uArySize);
	u1 = 0;
	while(u1 < uArySize)
	{
		*(sAry + u1) = (iFchk_typePathA(*(ary + u1)) == 1 ? iFget_AdirA(*(ary + u1)) : "");
		++u1;
	}
	// 順ソート
	iary_sortAsc(sAry);
	// iAryDepth 生成
	INT *iAryDepth = icalloc_INT(uArySize); // 初期値 = 0
	u1 = 0;
	while(u1 < uArySize)
	{
		iAryDepth[u1] = iji_searchCnt(*(sAry + u1), "\\");
		++u1;
	}
	// iAryFlg 生成
	INT *iAryFlg = icalloc_INT(uArySize); // 初期値 = 0
	// 上位へ集約
	u1 = 0;
	while(u1 < uArySize)
	{
		if(iAryDepth[u1])
		{
			if(iAryFlg[u1] > -1)
			{
				iAryFlg[u1] = 1;
			}
			u2 = u1 + 1;
			while(u2 < uArySize)
			{
				// 前方一致／大小区別しない
				if(imb_cmpfi(*(sAry + u2), *(sAry + u1)))
				{
					// 順ソートなので u2, u1
					iAryFlg[u2] = (iAryDepth[u2] <= (iAryDepth[u1] + (INT)depth) ? -1 : 1);
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
			*(rtn + u2) = ims_clone(*(sAry + u1));
			++u2;
		}
		++u1;
	}
	ifree(iAryFlg);
	ifree(iAryDepth);
	ifree(sAry);
	return rtn;
}
//---------------------
// 配列のクローン作成
//---------------------
/* (例)
	MBS **ary1 = iCLI_getARGS();
	MBS **ary2 = iary_clone(ary1);
	P83(iary_size(ary1));
		iary_print(ary1);
	P83(iary_size(ary2));
		iary_print(ary2);
	ifree(ary2);
	ifree(ary1);
*/
// v2015-12-31
MBS
**iary_clone(
	MBS **ary
)
{
	UINT size = iary_size(ary);
	MBS **rtn = icalloc_MBS_ary(size);
	MBS **ps = rtn;
	while(size)
	{
		*(ps++) = ims_clone(*(ary++));
		--size;
	}
	return rtn;
}
//-----------
// 配列一覧
//-----------
/* (例)
	// **ary = {"a", "b", "c"} のとき
	iget_ary_print(ary); //=> [0]"a", [1]"b", [2]"c"
*/
// v2014-10-16
VOID
iary_print(
	MBS **ary // 引数列
)
{
	UINT aSize = iary_size(ary);
	UINT u1 = 0;
	while(u1 < aSize)
	{
		P("[%04d] \"%s\"\n", u1 + 1, *(ary + u1));
		++u1;
	}
}
/////////////////////////////////////////////////////////////////////////////////////////
/*---------------------------------------------------------------------------------------
	File/Dir処理(WIN32_FIND_DATAA)
---------------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////
/*
	typedef struct _WIN32_FIND_DATAA {
		DWORD dwFileAttributes;
		FILETIME ftCreationTime;   // ctime
		FILETIME ftLastAccessTime; // mtime
		FILETIME ftLastWriteTime;  // atime
		DWORD nFileSizeHigh;
		DWORD nFileSizeLow;
		MBS cFileName[MAX_PATH];
	} WIN32_FIND_DATAA, *PWIN32_FIND_DATAA, *LPWIN32_FIND_DATAA;
	typedef struct _FILETIME {
		DWORD dwLowDateTime;
		DWORD dwHighDateTime;
	} FILETIME;
	typedef struct _SYSTEMTIME {
		INT wYear;
		INT wMonth;
		INT wDayOfWeek;
		INT wDay;
		INT wHour;
		INT wMinute;
		INT wSecond;
		INT wMilliseconds;
	} SYSTEMTIME;
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
	MBS *p1 = imp_cpy(FI->fullnameA, dir);
		imp_cpy(p1, "*");
	HANDLE hfind = FindFirstFileA(FI->fullnameA, &F);
		// Dir
		iFinfo_initA(FI, &F, dir, dirLenA, NULL);
			P2(FI->fullnameA);
		// File
		do
		{
			/// P82(F.cFileName);
			if(iFinfo_initA(FI, &F, dir, dirLenA, F.cFileName))
			{
				// Dir
				if((FI->iFtype) == 1)
				{
					p1 = ims_nclone(FI->fullnameA, FI->iEnd);
						ifindA(FI, p1, FI->iEnd); // Dir(下位)
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
			ifindA(FI, dir, imi_len(dir));
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
		P832(FI->iFsize, FI->fullnameA);
		P82(ijp_forwardN(FI->fullnameA, FI->iFname));
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
	FI->iFname = 0;
	FI->iExt = 0;
	FI->iEnd = 0;
	FI->iAttr = 0;
	FI->iFtype = 0;
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
	FI->iFname = 0;
	FI->iExt = 0;
	FI->iEnd = 0;
	FI->iAttr = 0;
	FI->iFtype = 0;
	FI->cjdCtime = 0.0;
	FI->cjdMtime = 0.0;
	FI->cjdAtime = 0.0;
	FI->iFsize = 0;
}
//---------------------------
// ファイル情報取得の前処理
//---------------------------
// v2016-05-06
BOOL
iFinfo_initA(
	$struct_iFinfoA *FI,
	WIN32_FIND_DATAA *F,
	MBS *dir, // "\"を付与して呼ぶ／iFget_AdirA()で絶対値にしておく
	UINT dirLenA,
	MBS *name
)
{
	// "\." "\.." は除外
	if(name && imb_cmp_leqf(name, ".."))
	{
		return FALSE;
	}
	// FI->iAttr
	FI->iAttr = (UINT)F->dwFileAttributes; // DWORD => UINT

	// <32768
	if((FI->iAttr) >> 15)
	{
		iFinfo_clearA(FI);
		return FALSE;
	}

	// FI->fullnameW
	// FI->iFname
	// FI->iEnd
	MBS *p1 = imp_cpy(FI->fullnameA, dir);
	MBS *p2 = imp_cpy(p1, name);
	UINT u1 = p2 - p1;

	// FI->iFtype
	// FI->iExt
	// FI->iFsize
	if((FI->iAttr & FILE_ATTRIBUTE_DIRECTORY))
	{
		if(u1)
		{
			dirLenA += u1 + 1;
			*((FI->fullnameA) + dirLenA - 1) = '\\'; // "\" 付与
			*((FI->fullnameA) + dirLenA) = 0;        // NULL 付与
		}
		(FI->iFtype) = 1;
		(FI->iFname) = (FI->iExt) = (FI->iEnd) = dirLenA;
		(FI->iFsize) = 0;
	}
	else
	{
		(FI->iFtype) = 2;
		(FI->iFname) = dirLenA;
		(FI->iEnd) = (FI->iFname) + u1;
		(FI->iExt) = PathFindExtensionA(FI->fullnameA) - (FI->fullnameA);
		if((FI->iExt) < (FI->iEnd))
		{
			++(FI->iExt);
		}
		(FI->iFsize) = (INT64)F->nFileSizeLow + (F->nFileSizeHigh ? (INT64)(F->nFileSizeHigh) * MAXDWORD + 1 : 0);
	}

	// JST変換
	// FI->cftime
	// FI->mftime
	// FI->aftime
	// (例)"c:\"
	if((FI->iEnd) <= 3)
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
// v2016-05-06
BOOL
iFinfo_initW(
	$struct_iFinfoW *FI, //
	WIN32_FIND_DATAW *F, //
	WCS *dir,            // "\"を付与して呼ぶ
	UINT dirLenW,        //
	WCS *name            //
)
{
	// "\." "\.." は除外
	if(name && *name && iwb_cmp_leqf(name, L".."))
	{
		return FALSE;
	}

	// FI->iAttr
	FI->iAttr = (UINT)F->dwFileAttributes; // DWORD => UINT

	// <32768
	if((FI->iAttr) >> 15)
	{
		iFinfo_clearW(FI);
		return FALSE;
	}

	// FI->fullnameW
	// FI->iFname
	// FI->iEnd
	WCS *p1 = iwp_cpy(FI->fullnameW, dir);
	WCS *p2 = iwp_cpy(p1, name);
	UINT u1 = p2 - p1;

	// FI->iFtype
	// FI->iExt
	// FI->iFsize
	if((FI->iAttr & FILE_ATTRIBUTE_DIRECTORY))
	{
		if(u1)
		{
			dirLenW += u1 + 1;
			*((FI->fullnameW) + dirLenW - 1) = L'\\'; // "\" 付与
			*((FI->fullnameW) + dirLenW) = 0;         // NULL 付与
		}
		(FI->iFtype) = 1;
		(FI->iFname) = (FI->iExt) = (FI->iEnd) = dirLenW;
		(FI->iFsize) = 0;
	}
	else
	{
		(FI->iFtype) = 2;
		(FI->iFname) = dirLenW;
		(FI->iEnd) = (FI->iFname) + u1;
		(FI->iExt) = PathFindExtensionW(FI->fullnameW) - (FI->fullnameW);
		if((FI->iExt) < (FI->iEnd))
		{
			++(FI->iExt);
		}
		(FI->iFsize) = (INT64)F->nFileSizeLow + (F->nFileSizeHigh ? (INT64)(F->nFileSizeHigh) * MAXDWORD + 1 : 0);
	}

	// JST変換
	// FI->cftime
	// FI->mftime
	// FI->aftime
	// (例)"c:\"
	if((FI->iEnd) <= 3)
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
// v2019-08-15
BOOL
iFinfo_init2M(
	$struct_iFinfoA *FI, //
	MBS *path            // ファイルパス
)
{
	// 存在チェック
	/// P83(iFchk_existPathA(path));
	if(!iFchk_existPathA(path))
	{
		return FALSE;
	}
	MBS *path2 = iFget_AdirA(path); // 絶対pathを返す
		INT iFtype = iFchk_typePathA(path2);
		UINT uDirLen = (iFtype == 1 ? imi_len(path2) : (UINT)(PathFindFileNameA(path2) - path2));
		MBS *pDir = (FI->fullnameA); // tmp
			imp_pcpy(pDir, path2, path2 + uDirLen);
		MBS *sName = NULL;
			if(iFtype == 1)
			{
				// Dir
				imp_cpy(path2 + uDirLen, "."); // Dir検索用 "." 付与
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
// v2017-10-11
MBS
*iFinfo_attrToA(
	UINT iAttr
)
{
	MBS *rtn = icalloc_MBS(6);
	if(!rtn)
	{
		return NULL;
	}
	*(rtn + 0) = (iAttr & FILE_ATTRIBUTE_DIRECTORY ? 'd' : '-'); // 16: Dir
	*(rtn + 1) = (iAttr & FILE_ATTRIBUTE_READONLY  ? 'r' : '-'); //  1: ReadOnly
	*(rtn + 2) = (iAttr & FILE_ATTRIBUTE_HIDDEN    ? 'h' : '-'); //  2: Hidden
	*(rtn + 3) = (iAttr & FILE_ATTRIBUTE_SYSTEM    ? 's' : '-'); //  4: System
	*(rtn + 4) = (iAttr & FILE_ATTRIBUTE_ARCHIVE   ? 'a' : '-'); // 32: Archive
	return rtn;
}
// v2016-02-11
UINT
iFinfo_attrAtoUINT(
	MBS *sAttr // "r, h, s, d, a" => 55
)
{
	if(!sAttr || !*sAttr)
	{
		return 0;
	}
	MBS **ap1 = ija_token(sAttr, "");
	MBS **ap2 = iary_simplify(ap1, TRUE);
		MBS *p1 = iary_join(ap2, "");
	ifree(ap2);
	ifree(ap1);
	// 小文字に変換
	CharLowerA(p1);
	UINT rtn = 0;
	MBS *p2 = p1;
	while(*p2)
	{
		// 頻出順
		switch(*p2)
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
		++p2;
	}
	ifree(p1);
	return rtn;
}
//* 2015-12-23
MBS
*iFinfo_ftypeToA(
	UINT iFtype
)
{
	MBS *rtn = icalloc_MBS(2);
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
// v2016-08-09
INT
iFinfo_depthA(
	$struct_iFinfoA *FI
)
{
	if(!FI->fullnameA)
	{
		return -1;
	}
	return iji_searchCnt(FI->fullnameA + 2, "\\") - 1;
}
// v2016-08-09
INT
iFinfo_depthW(
	$struct_iFinfoW *FI
)
{
	if(!FI->fullnameW)
	{
		return -1;
	}
	return iwi_searchCnt(FI->fullnameW + 2, L"\\") - 1;
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
	INT64 I1 = ((INT64)ftime.dwHighDateTime << 32) + ftime.dwLowDateTime;
	I1 /= 10000000; // (重要) MicroSecond 削除
	return ((DOUBLE)I1 / 86400) + 2305814.0;
}
// v2014-11-21
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
			i_y = *(ai + 0);
			i_m = *(ai + 1);
			i_d = *(ai + 2);
			i_h = *(ai + 3);
			i_n = *(ai + 4);
			i_s = *(ai + 5);
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
/*
//-------------------
// FileCopyサンプル
//-------------------
MBS *iFn = "infile.dat";
MBS *oFn = "outfile.dat";
$struct_iFinfoA *FI = iFinfo_allocA();
	if(iFinfo_init2M(FI, iFn))
	{
		FILE *iFp = ifopen(iFn, "r");
		FILE *oFp = ifopen(oFn, "w");
			$struct_ifreadBuf *Buf = ifreadBuf_alloc(FI->iFsize);
				UINT bufSize = 0;
				while((bufSize = ifreadBuf(iFp, Buf)))
				{
					P83(ifwrite(oFp, Buf->ptr, bufSize));
				}
			ifreadBuf_free(Buf);
		ifclose(oFp);
		ifclose(iFp);
	}
iFinfo_freeA(FI);
*/
// v2016-01-18
FILE
*ifopen(
	MBS *Fn,  //
	MBS *mode // fopen()と同じ／常に"b"モードに開く
)
{
	MBS mode2[4] = "";
		imp_pcpy(mode2, mode, mode + 3);
	INT c = 0, chk = 0;
	// (例)"r", "r+"のとき"b"を付与する
	UINT u1 = 1;
	while((c = mode2[u1]))
	{
		if(c == 'b')
		{
			++chk;
		}
		++u1;
	}
	if(!chk)
	{
		strcat(mode2, "b");
	}
	FILE *Fp = fopen(Fn, mode2);
	if(!Fp)
	{
		P("'%s' <= ", Fn);
		ierr_end("Can't open a file!");
	}
	return Fp;
}
/*
	//--------------------------
	// TSVから実データのみ出力
	//--------------------------
	MBS *iFn = "sample.tsv";
	if(iFchk_Tfile(iFn))
	{
		FILE *iFp = ifopen(iFn, "r");
			MBS *rtn = 0;
			MBS **aryX2 = 0;
			UINT u1 = 0;
			while((rtn = ifreadLine(iFp, TRUE)))
			{
				aryX2 = ija_token(rtn, "\t");
				if(**aryX2)
				{
					u1 = 0;
					while(*(aryX2 + u1))
					{
						P82(*(aryX2 + u1));
						++u1;
					}
					NL();
				}
				ifree(aryX2);
				ifree(rtn);
			}
		ifclose(iFp);
	}
	else
	{
		P2("Not a Text File!!");
	}
*/
//v2015-05-15
MBS
*ifreadLine(
	FILE *iFp,
	BOOL rmCrlf // TRUE=行末の"\r\n"を"\0"に変換
)
{
	CONST UINT _buf = 128; // 128Byte="76Byte以下"と"256Byte超"の両方に対応
	UINT uBuf = _buf;
	MBS *rtn = icalloc_MBS(uBuf);
	UINT u1 = 0;
	INT c1 = 0;
	while(TRUE)
	{
		if(u1 < uBuf)
		{
			c1 = getc(iFp);
			if(c1 == EOF)
			{
				break;
			}
			*(rtn + u1) = c1;
			if(*(rtn + u1) == '\n')
			{
				if(rmCrlf)
				{
					*(rtn + u1) = 0;
					if(*(rtn + u1 - 1) == '\r')
					{
						*(rtn + u1 - 1) = 0;
					}
				}
				break;
			}
			++u1;
		}
		else
		{
			uBuf += _buf;
			rtn = irealloc_MBS(rtn, uBuf);
		}
	}
	return (c1 == EOF ? (*rtn ? rtn : NULL) : rtn);
}
// v2015-05-12
$struct_ifreadBuf
*ifreadBuf_alloc(
	INT64 fsize // ifopen()するファイルのサイズ
)
{
	if(fsize < 1)
	{
		fsize = 0;
	}
	MEMORYSTATUS stat;
	GlobalMemoryStatus(&stat);
	CONST INT64 _freeMem = (stat.dwAvailPhys * 0.5); // 最大50%以下
	CONST INT64 _maxByte = 1024 * 1024 * 512;        // 最大512MB
	if(fsize > _freeMem)
	{
		fsize = _freeMem;
	}
	if(fsize > _maxByte)
	{
		fsize = _maxByte;
	}
	$struct_ifreadBuf *Buf = ($struct_ifreadBuf*)icalloc(1, sizeof($struct_ifreadBuf), FALSE);
		icalloc_err(Buf);
	(Buf->size) = (UINT)fsize;
	(Buf->ptr) = icalloc_MBS(Buf->size);
	return Buf;
}
// v2015-01-03
UINT
ifreadBuf(
	FILE *Fp,              // 入力ファイルポインタ
	$struct_ifreadBuf *Buf // ifreadBuf_alloc(Fn)で取得
)
{
	UINT size = (Buf->size);
	if(size<1 || feof(Fp))
	{
		return 0;
	}
	MBS *p1 = (Buf->ptr);
	UINT rtn = fread(p1, 1, size, Fp);
	*(p1 + rtn) = EOF;
	return rtn;
}
// v2016-08-30
VOID
ifreadBuf_free(
	$struct_ifreadBuf *Buf
)
{
	if(Buf)
	{
		ifree(Buf->ptr);
		ifree(Buf);
	}
}
//-----------------------------------
// ファイルが存在するときTRUEを返す
//-----------------------------------
/* (例)
	P83(iFchk_existPathA("."));    //=> 1
	P83(iFchk_existPathA(""));     //=> 0
	P83(iFchk_existPathA("\\"));   //=> 1
	P83(iFchk_existPathA("\\\\")); //=> 0
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
	P83(iFchk_typePathA("."));                       //=> 1
	P83(iFchk_typePathA(".."));                      //=> 1
	P83(iFchk_typePathA("\\"));                      //=> 1
	P83(iFchk_typePathA("c:\\windows\\"));           //=> 1
	P83(iFchk_typePathA("c:\\windows\\system.ini")); //=> 2
	P83(iFchk_typePathA("\\\\Network\\"));           //=> 2(不明なときも)
*/
// v2015-11-26
UINT
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
	P83(iFchk_Bfile("aaa.exe")); //=> TRUE
	P83(iFchk_Bfile("aaa.txt")); //=> FALSE
	P83(iFchk_Bfile("???"));     //=> FALSE (存在しないとき)
*/
// v2019-08-15
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
		if(c == 0)
		{
			++cnt;
			break;
		}
		++u1;
	}
	fclose(Fp);
	return (0<cnt ? TRUE : FALSE);
}
//---------------------
// ファイル名等を抽出
//---------------------
/* (例)
	// 存在しなくても機械的に抽出
	// 必要に応じて iFchk_existPathA() で前処理
	MBS *p1 = "c:\\windows\\win.ini";
	P82(iget_regPathname(p1, 0)); //=>"c:\windows\win.ini"
	P82(iget_regPathname(p1, 1)); //=>"win.ini"
	P82(iget_regPathname(p1, 2)); //=>"win"
*/
// v2016-02-11
MBS
*iFget_extPathname(
	MBS *path,
	UINT option
)
{
	if(!path || !*path)
	{
		return 0;
	}
	MBS *rtn = icalloc_MBS(imi_len(path) + 3); // CRLF+NULL
	MBS *pBgn = 0, *pEnd = 0;
	// Dir or File ?
	if(PathIsDirectoryA(path))
	{
		if(option < 1)
		{
			pEnd = imp_cpy(rtn, path);
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
				imp_cpy(rtn, path);
				break;

			// name+ext
			case(1):
				pBgn = PathFindFileNameA(path);
				imp_cpy(rtn, pBgn);
				break;

			// name
			case(2):
				pBgn = PathFindFileNameA(path);
				pEnd = PathFindExtensionA(pBgn);
				imp_pcpy(rtn, pBgn, pEnd);
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
	P82(iFget_AdirA(".\\"));
*/
// v2020-05-29
MBS
*iFget_AdirA(
	MBS *path // ファイルパス
)
{
	MBS *rtn = icalloc_MBS(IMAX_PATHA);
	MBS *p1 = 0;
	switch(iFchk_typePathA(path))
	{
		// Dir
		case(1):
			p1 = ims_ncat_clone(path, "\\", NULL);
				_fullpath(rtn, p1, IMAX_PATHA);
			ifree(p1);
			break;
		// File
		case(2):
			_fullpath(rtn, path, IMAX_PATHA);
			break;
	}
	return rtn;
}
//---------------------------
// 相対Dir を "\"付きに変換
//---------------------------
/* (例)
	// _fullpath() の応用
	P82(iFget_RdirA(".")); => ".\\"
*/
// v2014-10-25
MBS
*iFget_RdirA(
	MBS *path // ファイルパス
)
{
	MBS *rtn = 0;
	if(PathIsDirectoryA(path))
	{
		rtn = ims_clone(path);
		UINT u1 = iji_searchLenR(rtn, "\\");
		MBS *pEnd = imp_eod(rtn) - u1;
		*(pEnd + 0) = '\\';
		*(pEnd + 1) = 0;
	}
	else
	{
		rtn = ijs_simplify(path, "\\");
	}
	return rtn;
}
//--------------------
// 複階層のDirを作成
//--------------------
/* (例)
	P83(imk_dir("aaa\\bbb"));
*/
// v2014-05-03
BOOL
imk_dir(
	MBS *path // ファイルパス
)
{
	UINT flg = 0;
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
//---------------------------------
// 現在の文字・画面の表示色を得る
//---------------------------------
/* (注)
	画面色を設定する場合、
	バッファ不足になると表示に不具合が発生する（バグではない）。
	都度 cls すれば治る。
*/
/* (例)
	// [文字色]+([背景色]*16)
	//  0 = Black    1 = Navy     2 = Green    3 = Teal
	//  4 = Maroon   5 = Purple   6 = Olive    7 = Silver
	//  8 = Gray     9 = Blue    10 = Lime    11 = Aqua
	// 12 = Red     13 = Fuchsia 14 = Yellow  15 = White
	iConsole_getColor();
		//=> $IWM_ColorDefault // 現在色を保存
		//=> $IWM_StdoutHandle // 画面ハンドルを保存
	iConsole_setTextColor(9 + (15 * 16)); // 変更
	iConsole_setTextColor(-1); // 元の色に戻す
*/
// v2021-03-19
UINT
iConsole_getColor()
{
	$IWM_StdoutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO info;
	GetConsoleScreenBufferInfo($IWM_StdoutHandle, &info);
	$IWM_ColorDefault = info.wAttributes;
	return $IWM_ColorDefault;
}
// v2021-03-19
VOID
iConsole_setTextColor(
	INT rgb // 表示色
)
{
	if(rgb < 0)
	{
		rgb = $IWM_ColorDefault;
	}
	HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(handle, rgb);
}
/////////////////////////////////////////////////////////////////////////////////////////
/*---------------------------------------------------------------------------------------
	Clipboard
---------------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////
//---------------------------
// クリップボードを使用する
//---------------------------
/* (例)
	iClipboard_setText("abcde"); // コピー
	iClipboard_addText("123");   // 追加
	P82(iClipboard_getText());   // 文字列取得
	iclipboard_erase();          // クリア
*/
// v2015-11-26
BOOL
iClipboard_erase()
{
	if(OpenClipboard(0))
	{
		EmptyClipboard();
		CloseClipboard();
		return TRUE;
	}
	return FALSE;
}
// v2015-12-05
BOOL
iClipboard_setText(
	MBS *pM
)
{
	if(!pM)
	{
		return FALSE;
	}
	BOOL rtn = FALSE;
	UINT size = imi_len(pM) + 1;       // +NULL 分
	MBS *p1 = GlobalAlloc(GPTR, size); // 継承しないので固定メモリで十分
	if(p1)
	{
		imp_cpy(p1, pM);
		if(OpenClipboard(0))
		{
			SetClipboardData(CF_TEXT, p1);
			rtn = TRUE;
		}
		CloseClipboard();
	}
	GlobalFree(p1);
	return rtn;
}
// v2015-11-26
MBS
*iClipboard_getText()
{
	MBS *rtn = 0;
	if(OpenClipboard(0))
	{
		HANDLE handle = GetClipboardData(CF_TEXT);
		MBS *p1 = GlobalLock(handle);
		rtn = ims_clone(p1);
		GlobalUnlock(handle);
		CloseClipboard();
	}
	return rtn;
}
// v2020-05-29
BOOL
iClipboard_addText(
	MBS *pM
)
{
	if(!pM)
	{
		return FALSE;
	}
	MBS *p1 = iClipboard_getText();         // Clipboardを読む
	MBS *p2 = ims_ncat_clone(p1, pM, NULL); // pMを追加
	BOOL rtn = iClipboard_setText(p2);      // Clipboardへ書き込む
	ifree(p2);
	ifree(p1);
	return rtn;
}
// v2015-12-05
/*
	エクスプローラにてコピーされたファイルからファイル名を抽出
	引数処理では長さ制限があるので、こちらを使用する
*/
MBS
*iClipboard_getDropFn(
	UINT option // iFget_extPathname()参照
)
{
	MBS *rtn = 0;
	MBS *pBgn = 0, *pEnd = 0;
	UINT u1 = 0;
	OpenClipboard(0);
	HDROP hDrop = (HDROP)GetClipboardData(CF_HDROP);
	if(hDrop)
	{
		MBS *buf = icalloc_MBS(IMAX_PATHA);
		//ファイル数を取得
		CONST UINT cnt = DragQueryFileA(hDrop, 0XFFFFFFFF, NULL, 0);
		// rtn作成
		UINT size = 0;
		u1 = 0;
		while(u1 < cnt)
		{
			size += DragQueryFileA(hDrop, u1, buf, IMAX_PATHA) + 4; // CRLF+NULL+"\" 分
			++u1;
		}
		pEnd = rtn = icalloc_MBS(size);
		// パス名正規化
		size = 0;
		u1 = 0;
		while(u1 < cnt)
		{
			size = DragQueryFileA(hDrop, u1, buf, IMAX_PATHA); //ファイル名を取得
			// 速度を重視しない⇒速度重視のときはインライン化
			pBgn = iFget_extPathname(buf, option);
			if(*pBgn)
			{
				pEnd = imp_cpy(pEnd, pBgn);
				pEnd = imp_cpy(pEnd, "\r\n"); // CRLF
			}
			ifree(pBgn);
			++u1;
		}
		ifree(buf);
	}
	// 一度、iClipboard_setText()⇒ペーストすると、
	// クリップボードに反映されなくなるので EmptyClipboard()する.
	EmptyClipboard();
	CloseClipboard();
	return rtn;
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
// v2013-03-21
BOOL
idate_chk_uruu(
	INT i_y // 年
)
{
	if(i_y > (INT)(NS_AFTER[1] / 10000))
	{
		if((i_y % 400) == 0)
		{
			return TRUE;
		}
		if((i_y % 100) == 0)
		{
			return FALSE;
		}
	}
	return ((i_y % 4) == 0 ? TRUE : FALSE);
}
//-------------
// 月を正規化
//-------------
/* (例)
	INT i1 = 0;
	INT *ai = idate_cnv_month(2011, 14, 1, 12);
	for(i1 = 0; i1 < 2; i1++) P("[%d]", *(ai + i1)); //=> [2012][2]
	ifree(ai);
	NL();
*/
// v2013-03-21
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
	*(rtn + 0) = i_y;
	*(rtn + 1) = i_m;
	return rtn;
}
//---------------
// 月末日を返す
//---------------
/* (例)
	idate_month_end(2012, 2); //=> 29
	idate_month_end(2013, 2); //=> 28
*/
// v2013-03-21
INT
idate_month_end(
	INT i_y, // 年
	INT i_m  // 月
)
{
	INT *ai = idate_cnv_month1(i_y, i_m);
	INT i_d = MDAYS[*(ai + 1)];
	// 閏２月末日
	if(*(ai + 1) == 2 && idate_chk_uruu(*(ai + 0)))
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
	P83(idate_chk_month_end(2012, 2, 28, FALSE)); //=> FALSE
	P83(idate_chk_month_end(2012, 2, 29, FALSE)); //=> TRUE
	P83(idate_chk_month_end(2012, 1, 60, TRUE )); //=> TRUE
	P83(idate_chk_month_end(2012, 1, 60, FALSE)); //=> FALSE
*/
// v2014-11-21
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
			i_y = *(ai1 + 0);
			i_m = *(ai1 + 1);
			i_d = *(ai1 + 2);
		ifree(ai1);
	}
	return (i_d == idate_month_end(i_y, i_m) ? TRUE : FALSE);
}
//-----------------------
// strを強引にCJDに変換
//-----------------------
/* (例)
	P84(idate_MBStoCjd(">[1970-12-10] and <[2015-12-10]")); //=> 2440931.00000000
	P84(idate_MBStoCjd(">[1970-12-10]"));                   //=> 2440931.00000000
	P84(idate_MBStoCjd(">[+1970-12-10]"));                  //=> 2440931.00000000
	P84(idate_MBStoCjd(">[0000-00-00]"));                   //=> 1721026.00000000
	P84(idate_MBStoCjd(">[-1970-12-10]"));                  //=> 1001859.00000000
	P84(idate_MBStoCjd(">[2016-01-02]"));                   //=> 2457390.00000000
	P84(idate_MBStoCjd(">[0D]"));    // "2016-01-02 10:44:08" => 2457390.00000000
	P84(idate_MBStoCjd(">[]"));      // "2016-01-02 10:44:08" => 2457390.44731481
	P84(idate_MBStoCjd(""));                                //=> DBL_MAX
*/
// v2016-01-03
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
		*(ai + 0), *(ai + 1), *(ai + 2), *(ai + 3), *(ai + 4), *(ai + 5)
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
	DOUBLE rtn = idate_ymdhnsToCjd(*(ai + 0), *(ai + 1), *(ai + 2), *(ai + 3), *(ai + 4), *(ai + 5));
	ifree(ai);
	ifree(p1);
	return rtn;
}
//--------------------
// strを年月日に分割
//--------------------
/* (例)
	MBS **ary = idate_MBS_to_mAryYmdhns("-2012-8-12 12:45:00");
		iary_print(ary); //=> "-2012" "8" "12" "12" "45" "00"
	ifree(ary);
*/
// v2016-01-20
MBS
**idate_MBS_to_mAryYmdhns(
	MBS *pM // (例)"2012-03-12 13:40:00"
)
{
	BOOL bMinus = (pM && *pM == '-' ? TRUE : FALSE);
	MBS **rtn = ija_token(pM, "/.-: "); // NULL対応
	if(bMinus)
	{
		MBS *p1 = *(rtn + 0);
		memmove(p1 + 1, p1, imi_len(p1) + 1); // NULLも移動
		*p1 = '-';
	}
	return rtn;
}
//-------------------------
// strを年月日に分割(int)
//-------------------------
/* (例)
	INT i1 = 0;
	INT *ai = idate_MBS_to_iAryYmdhns("-2012-8-12 12:45:00");
	for(i1 = 0; *i1 < 6; i1++)
	{
		P83(*(ai + i1)); //=> -2012 8 12 12 45 00
	}
	ifree(ary);
*/
// v2014-10-19
INT
*idate_MBS_to_iAryYmdhns(
	MBS *pM // (例)"2012-03-12 13:40:00"
)
{
	INT *rtn = icalloc_INT(6);
	MBS **ary = idate_MBS_to_mAryYmdhns(pM);
	INT i1 = 0;
	while(i1 < 6)
	{
		*(rtn + i1) = atoi(*(ary + i1));
		++i1;
	}
	ifree(ary);
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
// v2013-03-21
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
		i_y = *(ai + 0);
		i_m = *(ai + 1);
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
// v2016-01-06
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
	*(rtn + 0) = i_h;
	*(rtn + 1) = i_n;
	*(rtn + 2) = i_s;
	return rtn;
}
//--------------------
// CJDをYMDHNSに変換
//--------------------
// v2014-10-19
INT
*idate_cjd_to_iAryYmdhns(
	DOUBLE cjd // cjd通日
)
{
	INT *ai1 = icalloc_INT(6);
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
		*(ai1 + 0) = i_y;
		*(ai1 + 1) = i_m;
		*(ai1 + 2) = i_d;
		*(ai1 + 3) = *(ai2 + 0);
		*(ai1 + 4) = *(ai2 + 1);
		*(ai1 + 5) = *(ai2 + 2);
	ifree(ai2);
	return ai1;
}
//----------------------
// CJDをFILETIMEに変換
//----------------------
// v2014-11-21
FILETIME
idate_cjdToFtime(
	DOUBLE cjd // cjd通日
)
{
	INT *ai1 = idate_cjd_to_iAryYmdhns(cjd);
	INT i_y = *(ai1 + 0), i_m = *(ai1 + 1), i_d = *(ai1 + 2), i_h = *(ai1 + 3), i_n = *(ai1 + 4), i_s = *(ai1 + 5);
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
// v2013-03-21
MBS
*idate_cjd_mWday(
	DOUBLE cjd // cjd通日
)
{
	return *(WDAYS + idate_cjd_iWday(cjd));
}
//------------------------------
// cjd通日から年内の通日を返す
//------------------------------
// v2013-03-21
INT
idate_cjd_yeardays(
	DOUBLE cjd // cjd通日
)
{
	INT *ai = idate_cjd_to_iAryYmdhns(cjd);
	INT i1 = *(ai + 0);
	ifree(ai);
	return (INT)(cjd-idate_ymdhnsToCjd(i1, 1, 0, 0, 0, 0));
}
//--------------------------------------
// 日付の前後 [6] = {y, m, d, h, n, s}
//--------------------------------------
/* (例)
	INT *ai = idate_add(2012, 1, 31, 0, 0, 0, 0, 1, 0, 0, 0, 0);
	INT i1 = 0;
	for(i1 = 0; i1 < 6; i1++)
	{
		P("[%d]%d\n", i1, *(ai + i1)); //=> 2012, 2, 29, 0, 0, 0
	}
*/
// v2015-12-24
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
		i1 = (INT)idate_month_end(*(ai2 + 0) + add_y, *(ai2 + 1) + add_m);
		if(*(ai2 + 2) > (INT)i1)
		{
			*(ai2 + 2) = (INT)i1;
		}
		ai1 = idate_reYmdhns(*(ai2 + 0) + add_y, *(ai2 + 1) + add_m, *(ai2 + 2), *(ai2 + 3), *(ai2 + 4), *(ai2 + 5));
		i2 = 0;
		while(i2 < 6)
		{
			*(ai2 + i2) = *(ai1 + i2);
			++i2;
		}
		ifree(ai1);
		++flg;
	}
	if(add_d != 0)
	{
		cjd = idate_ymdhnsToCjd(*(ai2 + 0), *(ai2 + 1), *(ai2 + 2), *(ai2 + 3), *(ai2 + 4), *(ai2 + 5));
		ai1 = idate_cjd_to_iAryYmdhns(cjd + add_d);
		i2 = 0;
		while(i2 < 6)
		{
			*(ai2 + i2) = *(ai1 + i2);
			++i2;
		}
		ifree(ai1);
		++flg;
	}
	if(add_h != 0 || add_n != 0 || add_s != 0)
	{
		ai1 = idate_reYmdhns(*(ai2 + 0), *(ai2 + 1), *(ai2 + 2), *(ai2 + 3) + add_h, *(ai2 + 4) + add_n, *(ai2 + 5) + add_s);
		i2 = 0;
		while(i2 < 6)
		{
			*(ai2 + i2) = *(ai1 + i2);
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
			*(ai1 + i2) = *(ai2 + i2);
			++i2;
		}
	}
	else
	{
		ai1 = idate_reYmdhns(*(ai2 + 0), *(ai2 + 1), *(ai2 + 2), *(ai2 + 3), *(ai2 + 4), *(ai2 + 5));
	}
	ifree(ai2);
	return ai1;
}
//-------------------------------------------------------
// 日付の差 [8] = {sign, y, m, d, h, n, s, days}
//   (注)便宜上、(日付1<=日付2)に変換して計算するため、
//       結果は以下のとおりとなるので注意。
//       ・5/31⇒6/30 :  + 1m
//       ・6/30⇒5/31 : -30d
//-------------------------------------------------------
/* (例)
	INT *ai = idate_diff(2012, 1, 31, 0, 0, 0, 2012, 2, 29, 0, 0, 0); //=> sign = 1, y = 0, m = 1, d = 0, h = 0, n = 0, s = 0, days = 29
	INT i1 = 0;
	for(i1 = 0; i1 < 7; i1++) P("[%d]%d\n", i1, *(ai + i1)); //=> 2012, 2, 29, 0, 0, 0, 29
*/
// v2014-11-23
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
	/*
		cjd2>cjd1 に入替
	*/
	if(cjd1 > cjd2)
	{
		*(rtn + 0) = -1; // sign(-)
		cjd  = cjd1;
		cjd1 = cjd2;
		cjd2 = cjd;
	}
	else
	{
		*(rtn + 0) = 1; // sign(+)
	}
	/*
		days
	*/
	*(rtn + 7) = (INT)(cjd2 - cjd1);
	/*
		正規化2
	*/
	INT *ai1 = idate_cjd_to_iAryYmdhns(cjd1);
	INT *ai2 = idate_cjd_to_iAryYmdhns(cjd2);
	/*
		ymdhns
	*/
	*(rtn + 1) = *(ai2 + 0) - *(ai1 + 0);
	*(rtn + 2) = *(ai2 + 1) - *(ai1 + 1);
	*(rtn + 3) = *(ai2 + 2) - *(ai1 + 2);
	*(rtn + 4) = *(ai2 + 3) - *(ai1 + 3);
	*(rtn + 5) = *(ai2 + 4) - *(ai1 + 4);
	*(rtn + 6) = *(ai2 + 5) - *(ai1 + 5);
	/*
		m 調整
	*/
	INT *ai3 = idate_cnv_month2(*(rtn + 1), *(rtn + 2));
		*(rtn + 1) = *(ai3 + 0);
		*(rtn + 2) = *(ai3 + 1);
	ifree(ai3);
	/*
		hns 調整
	*/
	while(*(rtn + 6) < 0)
	{
		*(rtn + 6) += 60;
		*(rtn + 5) -= 1;
	}
	while(*(rtn + 5) < 0)
	{
		*(rtn + 5) += 60;
		*(rtn + 4) -= 1;
	}
	while(*(rtn + 4) < 0)
	{
		*(rtn + 4) += 24;
		*(rtn + 3) -= 1;
	}
	/*
		d 調整
		前処理
	*/
	if(*(rtn + 3) < 0)
	{
		*(rtn + 2) -= 1;
		if(*(rtn + 2) < 0)
		{
			*(rtn + 2) += 12;
			*(rtn + 1) -= 1;
		}
	}
	/*
		本処理
	*/
	if(*(rtn + 0) > 0)
	{
		ai3 = idate_add(*(ai1 + 0), *(ai1 + 1), *(ai1 + 2), *(ai1 + 3), *(ai1 + 4), *(ai1 + 5), *(rtn + 1), *(rtn + 2), 0, 0, 0, 0);
			i1 = (INT)idate_ymdhnsToCjd(*(ai2 + 0), *(ai2 + 1), *(ai2 + 2), 0, 0, 0);
			i2 = (INT)idate_ymdhnsToCjd(*(ai3 + 0), *(ai3 + 1), *(ai3 + 2), 0, 0, 0);
		ifree(ai3);
	}
	else
	{
		ai3 = idate_add(*(ai2 + 0), *(ai2 + 1), *(ai2 + 2), *(ai2 + 3), *(ai2 + 4), *(ai2 + 5), -*(rtn + 1), -*(rtn + 2), 0, 0, 0, 0);
			i1 = (INT)idate_ymdhnsToCjd(*(ai3 + 0), *(ai3 + 1), *(ai3 + 2), 0, 0, 0);
			i2 = (INT)idate_ymdhnsToCjd(*(ai1 + 0), *(ai1 + 1), *(ai1 + 2), 0, 0, 0);
		ifree(ai3);
	}
	i3 = idate_ymd_num(*(ai1 + 3), *(ai1 + 4), *(ai1 + 5));
	i4 = idate_ymd_num(*(ai2 + 3), *(ai2 + 4), *(ai2 + 5));
	/* 変換例
		"05-31" "06-30" のとき m = 1, d = 0
		"06-30" "05-31" のとき m = 0, d = -30
		※分岐条件は非常にシビアなので安易に変更するな!!
	*/
	if(*(rtn + 0) > 0                                                     // +のときのみ
		&& i3 <= i4                                                       // (例) "12:00:00 =< 23:00:00"
		&& idate_chk_month_end(*(ai2 + 0), *(ai2 + 1), *(ai2 + 2), FALSE) // (例) 06-"30" は月末日？
		&& *(ai1 + 2) > *(ai2 + 2)                                        // (例) 05-"31" > 06-"30"
	)
	{
		*(rtn + 2) += 1;
		*(rtn + 3) = 0;
	}
	else
	{
		*(rtn + 3) = i1 - i2 - (INT)(i3 > i4 ? 1 : 0);
	}
	ifree(ai2);
	ifree(ai1);
	return rtn;
}
//--------------------------
// diff()／add()の動作確認
//--------------------------
/* (例)西暦1年から2000年迄のランダムな100例について評価
	idate_diff_checker(1, 2000, 100);
*/
// v2015-12-30
/*
VOID
idate_diff_checker(
	INT from_year, // 何年から
	INT to_year,   // 何年まで
	INT repeat     // 乱数発生回数
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
	INT i = 0;
	P("+----From---+------To-----+---sign, y, m, d--+---DateAdd--+----+\n");
	MT_initByAry(TRUE); // 初期化
	for(i = repeat; i; i--)
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
		ai3 = idate_diff(*(ai1 + 0), *(ai1 + 1), *(ai1 + 2), 0, 0, 0, *(ai2 + 0), *(ai2 + 1), *(ai2 + 2), 0, 0, 0);
		ai4 = (*(ai3 + 0) > 0 ?
				idate_add(*(ai1 + 0), *(ai1 + 1), *(ai1 + 2), 0, 0, 0, *(ai3 + 1), *(ai3 + 2), *(ai3 + 3), 0, 0, 0) :
				idate_add(*(ai1 + 0), *(ai1 + 1), *(ai1 + 2), 0, 0, 0, -(*(ai3 + 1)), -(*(ai3 + 2)), -(*(ai3 + 3)), 0, 0, 0)
			);
		// 計算結果の照合
		sprintf(s1, "%d%02d%02d", *(ai2 + 0), *(ai2 + 1), *(ai2 + 2));
		sprintf(s2, "%d%02d%02d", *(ai4 + 0), *(ai4 + 1), *(ai4 + 2));
		err = (imb_cmpp(s1, s2) ? "" : "NG");
		P(
			"%5d-%02d-%02d : %5d-%02d-%02d  [%2d, %4d, %2d, %2d]  %5d-%02d-%02d  %s\n",
			*(ai1 + 0), *(ai1 + 1), *(ai1 + 2), *(ai2 + 0), *(ai2 + 1), *(ai2 + 2),
			*(ai3 + 0), *(ai3 + 1), *(ai3 + 2), *(ai3 + 3), *(ai4 + 0), *(ai4 + 1), *(ai4 + 2),
			err
		);
		ifree(ai4);
		ifree(ai3);
		ifree(ai2);
		ifree(ai1);
	}
	MT_freeAry(); // 解放
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
/* (例)ymdhns
	INT *ai = idate_reYmdhns(1970, 12, 10, 0, 0, 0);
	MBS *s1 = 0;
		s1 = idate_format_ymdhns("%g%y-%m-%d(%a), %c/%C", *(ai + 0), *(ai + 1), *(ai + 2), *(ai + 3), *(ai + 4), *(ai + 5));
		P82(s1);
	ifree(s1);
	ifree(ai);
*/
/* (例)diff
	INT *ai = idate_diff(1970, 12, 10, 0, 0, 0, 2016, 10, 6, 0, 0, 0);
	MBS *s1 = idate_format_diff("%g%y-%m-%d, %W週+%w日, %S秒", *(ai + 0), *(ai + 1), *(ai + 2), *(ai + 3), *(ai + 4), *(ai + 5), *(ai + 6), *(ai + 7));
		P82(s1);
	ifree(s1);
	ifree(ai);
*/
// v2020-08-08
MBS
*idate_format_diff(
	MBS *format, //
	INT i_sign,  // 符号／-1="-", 0<="+"
	INT i_y,     // 年
	INT i_m,     // 月
	INT i_d,     // 日
	INT i_h,     // 時
	INT i_n,     // 分
	INT i_s,     // 秒
	INT i_days   // 通産日／idate_diff()で使用
)
{
	if(!format)
	{
		return "";
	}
	MBS *rtn = icalloc_MBS(imi_len(format) + (8 * iji_searchCnt(format, "%")));
	MBS *pEnd = rtn;
	MBS s1[32] = "";
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
					pEnd = imp_cpy(pEnd, idate_cjd_mWday(cjd));
					break;

				case 'A': // 曜日数
					sprintf(s1, "%d", idate_cjd_iWday(cjd));
					pEnd = imp_cpy(pEnd, s1);
					break;

				case 'c': // 年内の通算日
					sprintf(s1, "%d", idate_cjd_yeardays(cjd));
					pEnd = imp_cpy(pEnd, s1);
					break;

				case 'C': // CJD通算日
					sprintf(s1, CJD_FORMAT, cjd);
					pEnd = imp_cpy(pEnd, s1);
					break;

				case 'J': // JD通算日
					sprintf(s1, CJD_FORMAT, jd);
					pEnd = imp_cpy(pEnd, s1);
					break;

				case 'e': // 年内の通算週
					sprintf(s1, "%d", idate_cjd_yearweeks(cjd));
					pEnd = imp_cpy(pEnd, s1);
					break;

				// Diff
				case 'Y': // 通算年
					sprintf(s1, "%d", i_y);
					pEnd = imp_cpy(pEnd, s1);
					break;

				case 'M': // 通算月
					sprintf(s1, "%d", (i_y * 12) + i_m);
					pEnd = imp_cpy(pEnd, s1);
					break;

				case 'D': // 通算日
					sprintf(s1, "%d", i_days);
					pEnd = imp_cpy(pEnd, s1);
					break;

				case 'H': // 通算時
					sprintf(s1, "%I64d", ((INT64)i_days * 24) + i_h);
					pEnd = imp_cpy(pEnd, s1);
					break;

				case 'N': // 通算分
					sprintf(s1, "%I64d", ((INT64)i_days * 24 * 60) + (i_h * 60) + i_n);
					pEnd = imp_cpy(pEnd, s1);
					break;

				case 'S': // 通算秒
					sprintf(s1, "%I64d", ((INT64)i_days * 24 * 60 * 60) + (i_h * 60 * 60) + (i_n * 60) + i_s);
					pEnd = imp_cpy(pEnd, s1);
					break;

				case 'W': // 通算週
					sprintf(s1, "%d", (INT)(i_days / 7));
					pEnd = imp_cpy(pEnd, s1);
					break;

				case 'w': // 通算週の余日
					sprintf(s1, "%d", (i_days % 7));
					pEnd = imp_cpy(pEnd, s1);
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
					sprintf(s1, "%04d", i_y);
					pEnd = imp_cpy(pEnd, s1);
					break;

				case 'm': // 月
					sprintf(s1, "%02d", i_m);
					pEnd = imp_cpy(pEnd, s1);
					break;

				case 'd': // 日
					sprintf(s1, "%02d", i_d);
					pEnd = imp_cpy(pEnd, s1);
					break;

				case 'h': // 時
					sprintf(s1, "%02d", i_h);
					pEnd = imp_cpy(pEnd, s1);
					break;

				case 'n': // 分
					sprintf(s1, "%02d", i_n);
					pEnd = imp_cpy(pEnd, s1);
					break;

				case 's': // 秒
					sprintf(s1, "%02d", i_s);
					pEnd = imp_cpy(pEnd, s1);
					break;

				case '%':
					*pEnd = '%';
					++pEnd;
					break;

				default:
					--format; // else に処理を振る
					break;
			}
		}
		else if(*format == '\\')
		{
			switch(*(format + 1))
			{
				case('a'): *pEnd = '\a';          break;
				case('n'): *pEnd = '\n';          break;
				case('t'): *pEnd = '\t';          break;
				default:   *pEnd = *(format + 1); break;
			}
			++format;
			++pEnd;
		}
		else
		{
			*pEnd = *format;
			++pEnd;
		}
		++format;
	}
	return rtn;
}
/* (例)
	MBS *p1 = "1970/12/10 12:25:00";
	INT *ai1 = idate_MBS_to_iAryYmdhns(p1);
	P82(idate_format_iAryToA(IDATE_FORMAT_STD, ai1));
*/
// v2015-12-24
MBS
*idate_format_iAryToA(
	MBS *format, //
	INT *ymdhns  // {y, m, d, h, n, s}
)
{
	INT *ai1 = ymdhns;
	MBS *rtn = idate_format_ymdhns(format, *(ai1 + 0), *(ai1 + 1), *(ai1 + 2), *(ai1 + 3), *(ai1 + 4), *(ai1 + 5));
	return rtn;
}
// v2014-11-23
MBS
*idate_format_cjdToA(
	MBS *format,
	DOUBLE cjd
)
{
	INT *ai1 = idate_cjd_to_iAryYmdhns(cjd);
	MBS *rtn = idate_format_ymdhns(format, *(ai1 + 0), *(ai1 + 1), *(ai1 + 2), *(ai1 + 3), *(ai1 + 4), *(ai1 + 5));
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
	P82(p1);
	ifree(p1);
	ifree(ai1);
*/
// v2021-01-31
MBS
*idate_replace_format_ymdhns(
	MBS *pM,        // 変換対象文字列
	MBS *quoteBgn,  // 囲文字 1文字 (例)"["
	MBS *quoteEnd,  // 囲文字 1文字 (例)"]"
	MBS *add_quote, // 出力文字に追加するquote (例)"'"
	CONST INT i_y,  // ベース年
	CONST INT i_m,  // ベース月
	CONST INT i_d,  // ベース日
	CONST INT i_h,  // ベース時
	CONST INT i_n,  // ベース分
	CONST INT i_s   // ベース秒
)
{
	if(!pM)
	{
		return NULL;
	}
	MBS *p1 = 0, *p2 = 0, *p3 = 0, *p4 = 0, *p5 = 0;
	MBS *pEnd = 0;
	INT i1 = iji_searchCnt(pM, quoteBgn);
	MBS *rtn = icalloc_MBS(imi_len(pM) + (32 * i1));

	// quoteBgn がないとき、pのクローンを返す
	if(!i1)
	{
		imp_cpy(rtn, pM);
		return rtn;
	}

	// add_quoteの除外文字
	MBS *pAddQuote = (
		(*add_quote >= '0' && *add_quote <= '9') || *add_quote == '+' || *add_quote == '-' ?
		"" :
		add_quote
	);
	INT iQuote1 = imi_len(quoteBgn), iQuote2 = imi_len(quoteEnd);
	INT add_y = 0, add_m = 0, add_d = 0, add_h = 0, add_n = 0, add_s = 0;
	INT cntPercent = 0;
	INT *ai = 0;
	BOOL bAdd = FALSE;
	BOOL flg = FALSE;
	BOOL zero = FALSE;
	MBS *ts1 = icalloc_MBS(imi_len(pM));

	// quoteBgn - quoteEnd を日付に変換
	p1 = p2 = pM;
	pEnd = rtn;
	while(*p2)
	{
		// "[" を探す
		// pM = "[[", quoteBgn = "["のときを想定しておく
		if(*quoteBgn && imb_cmpf(p2, quoteBgn) && !imb_cmpf(p2 + iQuote1, quoteBgn))
		{
			bAdd = FALSE;  // 初期化
			p2 += iQuote1; // quoteBgn.len
			p1 = p2;

			// "]" を探す
			if(*quoteEnd)
			{
				p2 = ijp_searchL(p1, quoteEnd);
				imp_pcpy(ts1, p1, p2); // 解析用文字列

				// "[]" の中に数字が含まれているか
				i1 = 0;
				p3 = ts1;
				while(*p3)
				{
					if(*p3 >= '0' && *p3 <= '9')
					{
						i1 = 1;
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
						i1 = 1;
						break;

					case(1): // "[%]"
						if(*ts1 == '%')
						{
							p3 = "y%";
							i1 = 1;
						}
						break;

					default:
						p3 = ts1;
						break;
				}
				if(i1)
				{
					zero = FALSE; // "00:00:00" かどうか
					i1 = inum_atoi(p3); // 引数から数字を抽出
					while(*p3)
					{
						switch(*p3)
						{
							case 'Y': // 年 => "yyyy-mm-dd 00:00:00"
								zero = TRUE;
								/* Falls through. */ // -Wimplicit-fallthrough=3
							case 'y': // 年 => "yyyy-mm-dd hh:nn:ss"
								add_y = i1;
								flg = TRUE;
								bAdd = TRUE;
								break;

							case 'M': // 月 => "yyyy-mm-dd 00:00:00"
								zero = TRUE;
								/* Falls through. */ // -Wimplicit-fallthrough=3
							case 'm': // 月 => "yyyy-mm-dd hh:nn:ss"
								add_m = i1;
								flg = TRUE;
								bAdd = TRUE;
								break;

							case 'W': // 週 => "yyyy-mm-dd 00:00:00"
								zero = TRUE;
								/* Falls through. */ // -Wimplicit-fallthrough=3
							case 'w': // 週 => "yyyy-mm-dd hh:nn:ss"
								add_d = i1 * 7;
								flg = TRUE;
								bAdd = TRUE;
								break;

							case 'D': // 日 => "yyyy-mm-dd 00:00:00"
								zero = TRUE;
								/* Falls through. */ // -Wimplicit-fallthrough=3
							case 'd': // 日 => "yyyy-mm-dd hh:nn:ss"
								add_d = i1;
								flg = TRUE;
								bAdd = TRUE;
								break;

							case 'H': // 時 => "yyyy-mm-dd 00:00:00"
								zero = TRUE;
								/* Falls through. */ // -Wimplicit-fallthrough=3
							case 'h': // 時 => "yyyy-mm-dd hh:nn:ss"
								add_h = i1;
								flg = TRUE;
								bAdd = TRUE;
								break;

							case 'N': // 分 => "yyyy-mm-dd 00:00:00"
								zero = TRUE;
								/* Falls through. */ // -Wimplicit-fallthrough=3
							case 'n': // 分 => "yyyy-mm-dd hh:nn:ss"
								add_n = i1;
								flg = TRUE;
								bAdd = TRUE;
								break;

							case 'S': // 秒 => "yyyy-mm-dd 00:00:00"
								zero = TRUE;
								/* Falls through. */ // -Wimplicit-fallthrough=3
							case 's': // 秒 => "yyyy-mm-dd hh:nn:ss"
								add_s = i1;
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
							*(ai + 3) = *(ai + 4) = *(ai + 5) = 0;
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
						*(ai + 0), *(ai + 1), *(ai + 2), *(ai + 3), *(ai + 4), *(ai + 5)
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
						pEnd = imp_cpy(pEnd, pAddQuote);
						pEnd = imp_cpy(pEnd, p5);
						pEnd = imp_cpy(pEnd, pAddQuote);
					ifree(p5);
					ifree(ai);
				}
				p2 += (iQuote2); // quoteEnd.len
				p1 = p2;
			}
		}
		else
		{
			pEnd = imp_pcpy(pEnd, p2, p2 + 1);
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
// v2013-03-21
INT
*idate_now_to_iAryYmdhns(
	BOOL area // TRUE=LOCAL, FALSE=SYSTEM
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
	INT *ai = icalloc_INT(7);
		*(ai + 0) = (INT)st.wYear;
		*(ai + 1) = (INT)st.wMonth;
		*(ai + 2) = (INT)st.wDay;
		*(ai + 3) = (INT)st.wHour;
		*(ai + 4) = (INT)st.wMinute;
		*(ai + 5) = (INT)st.wSecond;
		*(ai + 6) = (INT)st.wMilliseconds;
	return ai;
}
//------------------
// 今日のcjdを返す
//------------------
/* (例)
	idate_nowToCjd(0); // System(-9h)
	idate_nowToCjd(1); // Local
*/
// v2013-03-21
DOUBLE
idate_nowToCjd(
	BOOL area // TRUE=LOCAL, FALSE=SYSTEM
)
{
	INT *ai = idate_now_to_iAryYmdhns(area);
	INT y = *(ai + 0), m = *(ai + 1), d = *(ai + 2), h = *(ai + 3), n = *(ai + 4), s = *(ai + 5);
	ifree(ai);
	return idate_ymdhnsToCjd(y, m, d, h, n, s);
}
/////////////////////////////////////////////////////////////////////////////////////////
/*---------------------------------------------------------------------------------------
	Geography
---------------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////
//-------------------
// 度分秒 => 十進法
//-------------------
/* (例)
	printf("%f度\n", rtnGeoIBLto10A(24, 26, 58.495200));
*/
// v2019-12-19
DOUBLE
rtnGeoIBLto10A(
	INT deg,   // 度
	INT min,   // 分
	DOUBLE sec // 秒
)
{
	return (DOUBLE)deg + ((DOUBLE)min / 60.0) + (sec / 3600.0);
}
/* (例)
	printf("%f度\n", rtnGeoIBLto10B(242658.495200));
*/
// v2019-12-19
DOUBLE
rtnGeoIBLto10B(
	DOUBLE ddmmss // ddmmss.s...
)
{
	DOUBLE sec = fmod(ddmmss, 100.0);
	INT min = (INT)(ddmmss / 100) % 100;
	INT deg = (INT)(ddmmss / 10000);
	return deg + (min / 60.0) + (sec / 3600.0);
}

//-------------------
// 十進法 => 度分秒
//-------------------
/* (例)
	$Geo geo = rtnGeo10toIBL(24.449582);
	printf("%d度%d分%f秒\n", geo.deg, geo.min, geo.sec);
*/
// v2019-12-18
$Geo
rtnGeo10toIBL(
	DOUBLE angle // 十進法
)
{
	INT deg = (INT)angle;
		angle = (angle - (DOUBLE)deg) * 60.0;
	INT min = (INT)angle;
		angle -= min;
	DOUBLE sec = (DOUBLE)angle * 60.0;

	// 0.999... * 60 => 60.0 対策
	if(sec == 60.0)
	{
		min += 1;
		sec = 0;
	}
	return ($Geo){0, 0, deg, min, sec};
}

//-------------------------------
// Vincenty法による２点間の距離
//-------------------------------
/* (参考)
	http://tancro.e-central.tv/grandmaster/script/vincentyJS.html
*/
/* (例)
	$Geo geo = rtnGeoVincentry(35.685187, 139.752274, 24.449582, 122.934340);
	printf("%fkm %f度\n", geo.dist, geo.angle);
*/
// v2021-03-05
$Geo
rtnGeoVincentry(
	DOUBLE lat1, // 開始～緯度
	DOUBLE lng1, // 開始～経度
	DOUBLE lat2, // 終了～緯度
	DOUBLE lng2  // 終了～経度
)
{
	if(lat1 == lat2 && lng1 == lng2)
	{
		return ($Geo){0, 0, 0, 0, 0};
	}

	/// CONST DOUBLE _A = 6378137.0;
	CONST DOUBLE _B   = 6356752.314140356;    // GRS80
	CONST DOUBLE _F   = 0.003352810681182319; // 1 / 298.257222101
	CONST DOUBLE _RAD = 0.017453292519943295; // π / 180

	CONST DOUBLE latR1 = lat1 * _RAD;
	CONST DOUBLE lngR1 = lng1 * _RAD;
	CONST DOUBLE latR2 = lat2 * _RAD;
	CONST DOUBLE lngR2 = lng2 * _RAD;

	CONST DOUBLE f1 = 1 - _F;

	CONST DOUBLE omega = lngR2 - lngR1;
	CONST DOUBLE tanU1 = f1 * tan(latR1);
	CONST DOUBLE cosU1 = 1 / sqrt(1 + tanU1 * tanU1);
	CONST DOUBLE sinU1 = tanU1 * cosU1;
	CONST DOUBLE tanU2 = f1 * tan(latR2);
	CONST DOUBLE cosU2 = 1 / sqrt(1 + tanU2 * tanU2);
	CONST DOUBLE sinU2 = tanU2 * cosU2;

	DOUBLE lamda  = omega;
	DOUBLE dLamda = 0.0;

	DOUBLE sinLamda  = 0.0;
	DOUBLE cosLamda  = 0.0;
	DOUBLE sin2sigma = 0.0;
	DOUBLE sinSigma  = 0.0;
	DOUBLE cosSigma  = 0.0;
	DOUBLE sigma     = 0.0;
	DOUBLE sinAlpha  = 0.0;
	DOUBLE cos2alpha = 0.0;
	DOUBLE cos2sm    = 0.0;
	DOUBLE c = 0.0;

	while(TRUE)
	{
		sinLamda = sin(lamda);
		cosLamda = cos(lamda);
		sin2sigma = (cosU2 * sinLamda) * (cosU2 * sinLamda) + (cosU1 * sinU2 - sinU1 * cosU2 * cosLamda) * (cosU1 * sinU2 - sinU1 * cosU2 * cosLamda);
		if(sin2sigma < 0.0)
		{
			return ($Geo){0, 0, 0, 0, 0};
		}
		sinSigma = sqrt(sin2sigma);
		cosSigma = sinU1 * sinU2 + cosU1 * cosU2 * cosLamda;
		sigma = atan2(sinSigma, cosSigma);
		sinAlpha = cosU1 * cosU2 * sinLamda / sinSigma;
		cos2alpha = 1 - sinAlpha * sinAlpha;
		cos2sm = cosSigma - 2 * sinU1 * sinU2 / cos2alpha;
		c = _F / 16 * cos2alpha * (4 + _F * (4 - 3 * cos2alpha));
		dLamda = lamda;
		lamda = omega + (1 - c) * _F * sinAlpha * (sigma + c * sinSigma * (cos2sm + c * cosSigma * (-1 + 2 * cos2sm * cos2sm)));
		if(fabs(lamda - dLamda) <= 1e-12)
		{
			break;
		}
	}

	DOUBLE u2 = cos2alpha * (1 - f1 * f1) / (f1 * f1);
	DOUBLE a = 1 + u2 / 16384 * (4096 + u2 * (-768 + u2 * (320 - 175 * u2)));
	DOUBLE b = u2 / 1024 * (256 + u2 * (-128 + u2 * (74 - 47 * u2)));
	DOUBLE dSigma = b * sinSigma * (cos2sm + b / 4 * (cosSigma * (-1 + 2 * cos2sm * cos2sm) - b / 6 * cos2sm * (-3 + 4 * sinSigma * sinSigma) * (-3 + 4 * cos2sm * cos2sm)));
	DOUBLE angle = atan2(cosU2 * sinLamda, cosU1 * sinU2 - sinU1 * cosU2 * cosLamda) * 57.29577951308232;
	DOUBLE dist = _B * a * (sigma - dSigma);

	// 変換
	if(angle < 0)
	{
		angle += 360.0; // 360度表記
	}
	dist /= 1000.0; // m => km

	return ($Geo){dist, angle, 0, 0, 0};
}
