//////////////////////////////////////////////////////////////////////////////////////////
#define   LIB_IWMUTIL_COPYLIGHT         "(C)2008-2024 iwm-iwama"
#define   LIB_IWMUTIL_FILENAME          "lib_iwmutil2_20240915"
//////////////////////////////////////////////////////////////////////////////////////////
#include <math.h>
#include <shlwapi.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <wchar.h>
#include <windows.h>

//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	共通定数
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
typedef   CHAR      MS; // imx_xxx() = Muliti Byte String／iux_xxx() = UTF-8N
typedef   WCHAR     WS; // iwx_xxx()／UTF-16／Wide Char String

#define   IMAX_PATH           ((MAX_PATH * 4) + 1) // UTF-8 = (Max)4byte

#define   DATETIME_FORMAT     (WS*)L"%.4d-%02d-%02d %02d:%02d:%02d"
#define   IDATE_FORMAT_STD    (WS*)L"%G%y-%m-%d %h:%n:%s"

//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	大域変数
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
#define   imain_begin()       iExecSec(0);iCLI_begin();iConsole_EscOn()
#define   imain_end()         ifree_all();iCLI_end(EXIT_SUCCESS)
#define   imain_err()         P1(IESC_RESET);ifree_all();iCLI_end(EXIT_FAILURE)

extern    WS        *$CMD;         // コマンド名を格納
extern    WS        *$ARG;         // 引数からコマンド名を消去したもの
extern    UINT      $ARGC;         // 引数配列数
extern    WS        **$ARGV;       // 引数配列／ダブルクォーテーションを消去したもの
extern    HANDLE    $StdoutHandle; // 画面制御用ハンドル
extern    UINT64    $ExecSecBgn;   // 実行開始時間

//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	Command Line
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
VOID      iCLI_begin();
VOID      iCLI_end(INT exitStatus);

WS        *iCLI_getOptValue(UINT argc, CONST WS *opt1, CONST WS *opt2);
BOOL      iCLI_getOptMatch(UINT argc, CONST WS *opt1, CONST WS *opt2);

VOID      iCLI_VarList();

//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	実行開始時間
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
UINT64    iExecSec(CONST UINT64 microSec);
#define   iExecSec_init()     (UINT64)iExecSec(0)
#define   iExecSec_next()     (DOUBLE)(iExecSec($ExecSecBgn)) / 1000

//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	メモリ確保
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
VOID      *icalloc(UINT64 n, UINT64 sizeOf, BOOL aryOn);
VOID      *irealloc(VOID *ptr, UINT64 n, UINT64 sizeOf);

// MS 文字列
#define   icalloc_MS(n)                 (MS*)icalloc(n, sizeof(MS), FALSE)
#define   irealloc_MS(str, n)           (MS*)irealloc(str, n, sizeof(MS))

// MS 配列
#define   icalloc_MS_ary(n)             (MS**)icalloc(n, sizeof(MS*), TRUE)
#define   irealloc_MS_ary(str, n)       (MS**)irealloc(str, n, sizeof(MS*))

// WS 文字列
#define   icalloc_WS(n)                 (WS*)icalloc(n, sizeof(WS), FALSE)
#define   irealloc_WS(str, n)           (WS*)irealloc(str, n, sizeof(WS))

// WS 配列
#define   icalloc_WS_ary(n)             (WS**)icalloc(n, sizeof(WS*), TRUE)
#define   irealloc_WS_ary(str, n)       (WS**)irealloc(str, n, sizeof(WS*))

// INT 配列
#define   icalloc_INT(n)                (INT*)icalloc(n, sizeof(INT), FALSE)
#define   irealloc_INT(ptr, n)          (INT*)irealloc(ptr, n, sizeof(INT))

VOID      icalloc_err(VOID *ptr);

VOID      icalloc_free(VOID *ptr);
VOID      icalloc_freeAll();
VOID      icalloc_mapSweep();

#define   ifree(ptr)          icalloc_free(ptr)
#define   ifree_all()         icalloc_freeAll()

//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	Debug
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
VOID      idebug_printMap();
#define   idebug_map()        PL();NL();idebug_printMap()

VOID      idebug_printPointer0(CONST VOID *ptr, UINT sizeOf);
#define   idebug_pointer(ptr)           PL();idebug_printPointer0(ptr, sizeof(ptr[0]));NL()

//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	Print関係
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
VOID      P(CONST MS *format, ...);

VOID      QP(CONST MS *str, UINT size);
#define   QP1(str)            QP(str, strlen(str))
#define   QP2(str)            QP1(str);NL()

#define   PL()                P("[L%u] ", __LINE__)
#define   NL()                putchar('\n')

#define   P1(str)             fputs(str, stdout)
#define   P2(str)             puts(str)
#define   P3(num)             P("%lld\n", (INT64)num)
#define   P4(num)             P("%.8lf\n", (DOUBLE)num)

#define   PL2(str)            PL();P2(str)
#define   PL3(num)            PL();P3(num)
#define   PL4(num)            PL();P4(num)

VOID      P1W(CONST WS *str);
#define   P2W(str)            P1W(str);putchar('\n')
#define   PL2W(str)           PL();P2W(str)

VOID      PR1(CONST MS *str, UINT uRepeat);
#define   LN(uRepeat)         PR1("-", uRepeat);NL()

WS        *iws_cnv_escape(WS *str);

VOID      imv_systemW(WS *cmd);
MS        *ims_popenW(WS *cmd);

//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	Console
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
VOID      iConsole_EscOn();
#define   IESC()              SetConsoleOutputCP(65001);iConsole_EscOn()
#define   IESC_CLEAR          "\033[2J\033[1;1H"
#define   IESC_RESET          "\033[0m"
#define   IESC_TITLE1         "\033[38;2;250;250;250m\033[104m" // 白／青
#define   IESC_OPT1           "\033[38;2;250;150;150m"          // 赤
#define   IESC_OPT2           "\033[38;2;150;150;250m"          // 青
#define   IESC_OPT21          "\033[38;2;25;225;235m"           // 水
#define   IESC_OPT22          "\033[38;2;250;100;250m"          // 紅紫
#define   IESC_LBL1           "\033[38;2;250;250;100m"          // 黄
#define   IESC_LBL2           "\033[38;2;100;100;250m"          // 青
#define   IESC_STR1           "\033[38;2;225;225;225m"          // 白
#define   IESC_STR2           "\033[38;2;200;200;200m"          // 銀
#define   IESC_TRUE1          "\033[38;2;0;250;250m"            // 水
#define   IESC_FALSE1         "\033[38;2;250;50;50m"            // 紅

WS        *iCLI_GetStdin(BOOL bInputKey);

//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	Clipboard
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
VOID      iClipboard_setText(CONST WS *str);
WS        *iClipboard_getText();

//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	UTF-16／UTF-8変換
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
MS        *icnv_W2M(CONST WS *str, UINT uCP);
#define   W2M(str)            (MS*)icnv_W2M(str, 65001)

WS        *icnv_M2W(CONST MS *str, UINT uCP);
#define   M2W(str)            (WS*)icnv_M2W(str, 65001)

//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	文字列処理
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
UINT64    imn_len(CONST MS *str);
UINT64    iwn_len(CONST WS *str);
UINT64    iun_len(CONST MS *str);

UINT      imn_Codepage(MS *str);

VOID      imv_cpy(MS *to, CONST MS *from);
VOID      iwv_cpy(WS *to, CONST WS *from);

UINT      imn_cpy(MS *to, CONST MS *from);
UINT      iwn_cpy(WS *to, CONST WS *from);

UINT      ivn_pcpy(VOID *to, CONST VOID *from1, CONST VOID *from2, UINT sizeOf);
#define   imn_pcpy(to, from1, from2)    (UINT64)ivn_pcpy(to, from1, from2, sizeof(MS))
#define   iwn_pcpy(to, from1, from2)    (UINT64)ivn_pcpy(to, from1, from2, sizeof(WS))

MS        *ims_clone(CONST MS *from);
WS        *iws_clone(CONST WS *from);

VOID      *ivs_pclone(CONST VOID *from1, CONST VOID *from2, INT sizeOf);
#define   ims_pclone(from1, from2)      (MS*)ivs_pclone(from1, from2, sizeof(MS))
#define   iws_pclone(from1, from2)      (WS*)ivs_pclone(from1, from2, sizeof(WS))

MS        *ims_cats(UINT size, ...);
WS        *iws_cats(UINT size, ...);

MS        *ims_sprintf(CONST MS *format, ...);
WS        *iws_sprintf(CONST WS *format, ...);

MS        *ims_repeat(CONST MS *str, UINT uRepeat);

BOOL      iwb_cmp(CONST WS *str, CONST WS *search, BOOL perfect, BOOL icase);
#define   iwb_cmpf(str, search)         (BOOL)iwb_cmp(str, search, FALSE, FALSE)
#define   iwb_cmpfi(str, search)        (BOOL)iwb_cmp(str, search, FALSE, TRUE)
#define   iwb_cmpp(str, search)         (BOOL)iwb_cmp(str, search, TRUE, FALSE)
#define   iwb_cmppi(str, search)        (BOOL)iwb_cmp(str, search, TRUE, TRUE)
#define   iwb_cmp_leqf(str, search)     (BOOL)iwb_cmp(search, str, FALSE, FALSE)
#define   iwb_cmp_leqfi(str, search)    (BOOL)iwb_cmp(search, str, FALSE, TRUE)

WS        *iwp_searchPos(WS *str, CONST WS *search, BOOL icase);
UINT      iwn_searchCnt(CONST WS *str, CONST WS *search, BOOL icase);

WS        **iwaa_split(WS *str, CONST WS *tokens, BOOL bRmEmpty);

WS        *iws_replace(WS *from, CONST WS *before, CONST WS *after, BOOL icase);

MS        *ims_IntToMs(INT64 num);
MS        *ims_DblToMs(DOUBLE num, INT iDigit);

WS        *iws_strip(WS *str, BOOL bStripLeft, BOOL bStripRight);
#define   iws_trim(str)       (WS*)iws_strip(str, TRUE, TRUE)
#define   iws_trimL(str)      (WS*)iws_strip(str, TRUE, FALSE)
#define   iws_trimR(str)      (WS*)iws_strip(str, FALSE, TRUE)

WS        *iws_cutYenR(WS *path);

WS        *iws_withoutESC(WS *str);

//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	Array
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
UINT      iwan_size(WS **ary);
UINT64    iwan_strlen(WS **ary);

INT       iwan_sort_Asc(CONST VOID *arg1, CONST VOID *arg2);
INT       iwan_sort_iAsc(CONST VOID *arg1, CONST VOID *arg2);
INT       iwan_sort_Desc(CONST VOID *arg1, CONST VOID *arg2);
INT       iwan_sort_iDesc(CONST VOID *arg1, CONST VOID *arg2);
#define   iwav_sort_Asc(ary)            qsort(ary, iwan_size(ary), sizeof(WS*), iwan_sort_Asc)
#define   iwav_sort_iAsc(ary)           qsort(ary, iwan_size(ary), sizeof(WS*), iwan_sort_iAsc)
#define   iwav_sort_Desc(ary)           qsort(ary, iwan_size(ary), sizeof(WS*), iwan_sort_Desc)
#define   iwav_sort_iDesc(ary)          qsort(ary, iwan_size(ary), sizeof(WS*), iwan_sort_iDesc)

WS        *iwas_njoin(WS **ary, CONST WS *token, UINT start, UINT count);
#define   iwas_join(ary, token)         (WS*)iwas_njoin(ary, token, 0, iwan_size(ary))

WS        **iwaa_uniq(WS **ary, BOOL icase);
WS        **iwaa_getDirFile(WS **ary, INT iType);
WS        **iwaa_higherDir(WS **ary);

VOID      imav_print(MS **ary);
VOID      iwav_print(WS **ary);

VOID      iwav_print2(WS **ary, CONST WS *sLeft, CONST WS *sRight);

//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	Variable Buffer
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
typedef struct
{
	UINT sizeOf;   // sizeof(str[0])
	VOID *str;     // String Pointer
	UINT length;   // String Length
	UINT freesize; // Free Buffers Size
}
$struct_iVariableBuffer,
$struct_iVB,
$struct_iVBM,
$struct_iVBW;

$struct_iVB         *iVB_alloc0(UINT sizeOf, UINT strLen);
VOID                iVB_add0($struct_iVB *iVB, CONST VOID *str, UINT strLen);
VOID                iVBM_add_sprintf($struct_iVBM *iVBM, CONST MS *format, ...);
VOID                iVBW_add_sprintf($struct_iVBW *iVBW, CONST WS *format, ...);

// MS
#define   iVBM_alloc()                  ($struct_iVBM*)iVB_alloc0(sizeof(MS), 256)
#define   iVBM_alloc2(strLen)           ($struct_iVBM*)iVB_alloc0(sizeof(MS), strLen)
#define   iVBM_add(iVBM, str)           iVB_add0(iVBM, str, imn_len(str))
#define   iVBM_add2(iVBM, str, strLen)  iVB_add0(iVBM, str, strLen)
#define   iVBM_clear(iVBM)              memset(iVBM->str, 0, (iVBM->length * iVBM->sizeOf));iVBM->freesize += iVBM->length;iVBM->length = 0
#define   iVBM_getSizeof(iVBW)          (UINT)(iVBM->sizeOf)
#define   iVBM_getStr(iVBM)             (MS*)(iVBM->str)
#define   iVBM_getLength(iVBM)          (UINT)(iVBM->length)
#define   iVBM_getFreesize(iVBM)        (UINT)(iVBM->freesize)
#define   iVBM_getSize(iVBM)            (UINT)(iVBM->length + iVBM->freesize)
#define   iVBM_free(iVBM)               ifree(iVBM->str);ifree(iVBM)

// WS
#define   iVBW_alloc()                  ($struct_iVBW*)iVB_alloc0(sizeof(WS), 256)
#define   iVBW_alloc2(strLen)           ($struct_iVBW*)iVB_alloc0(sizeof(WS), strLen)
#define   iVBW_add(iVBW, str)           iVB_add0(iVBW, str, iwn_len(str))
#define   iVBW_add2(iVBW, str, strLen)  iVB_add0(iVBW, str, strLen)
#define   iVBW_clear(iVBW)              memset(iVBW->str, 0, (iVBW->length * iVBW->sizeOf));iVBW->freesize += iVBW->length;iVBW->length = 0
#define   iVBW_getSizeof(iVBW)          (UINT)(iVBW->sizeOf)
#define   iVBW_getStr(iVBW)             (WS*)(iVBW->str)
#define   iVBW_getLength(iVBW)          (UINT)(iVBW->length)
#define   iVBW_getFreesize(iVBW)        (UINT)(iVBW->freesize)
#define   iVBW_getSize(iVBW)            (UINT)(iVBW->length + iVBW->freesize)
#define   iVBW_free(iVBW)               ifree(iVBW->str);ifree(iVBW)

//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	File/Dir処理／WIN32_FIND_DATAW
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
typedef struct
{
	WS     sPath[IMAX_PATH]; // フルパス
	UINT   uFnPos;           // ファイル名開始位置
	UINT   uAttr;            // 属性
	BOOL   bType;            // TRUE=Dir／FALSE=File
	DOUBLE ctime_cjd;        // 作成時間
	DOUBLE mtime_cjd;        // 更新時間
	DOUBLE atime_cjd;        // アクセス時間
	UINT64 uFsize;           // ファイルサイズ
}
$struct_iFinfo;

$struct_iFinfo      *iFinfo_alloc();

BOOL      iFinfo_init($struct_iFinfo *FI, WIN32_FIND_DATAW *F, WS *dir, WS *fname);
#define   iFinfo_free(FI)     ifree(FI)

WS        *iFinfo_attrToWS(UINT uAttr);

DOUBLE    iFinfo_ftimeToCjd(FILETIME ftime);
#define   iFinfo_ftimeToINT64(ftime)    (INT64)(((INT64)ftime.dwHighDateTime << 32) + (INT64)ftime.dwLowDateTime)

//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	File/Dir処理
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
#define   iFchk_existPath(path)         (BOOL)PathFileExistsW(path)
#define   iFchk_DirName(path)           (BOOL)(GetFileAttributesW(path) & FILE_ATTRIBUTE_DIRECTORY ? TRUE : FALSE)

#define   iFchk_attrDirFile(attr)       (INT)(((INT)attr & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 2)

BOOL      iFchk_Binfile(WS *fn);

WS        *iFget_extPathname(WS *path, INT option);

WS        *iFget_APath(WS *path);
WS        *iFget_RPath(WS *path);

UINT      iF_mkdir(WS *path);
WS        **iF_trash(WS *path);

//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	暦
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
/*
	◆はじめに
		ユリウス暦     : "B.C.4713-01-01 12:00" - "A.C.1582-10-04 23:59"
		グレゴリウス暦 : "A.C.1582-10-15 00:00" - 現在

		実暦は上記のとおりだが、ユリウス暦以前も(ユリウス暦に則り)計算可能である。
		『B.C.暦』の取り扱いについては、後述『仮B.C.暦』に則る。
		なお、派生暦等については、後述『各暦の変数』を使用のこと。

	◆仮B.C.暦
		(実暦) "-4713-01-01" - "-0001-12-31"
		(仮)   "-4712-01-01" - "+0000-12-31" // 実歴＋１年

	◆各暦の変数
		※CJDを基準に計算。
		JD   Julian Day                -4712-01-01 12:00:00開始
		CJD  Chronological Julian Day  -4712-01-01 00:00:00開始 = (JD - 0.5)
		MJD  Modified Julian Day        1858-11-17 00:00:00開始 = (JD - 2400000.5)
		LD   Lilian Day                 1582-10-15 00:00:00開始 = (JD - 2299159.5)
*/
#define   CJD_START           L"-4712-01-01 00:00:00"
#define   JD_START            L"-4712-01-01 12:00:00"

#define   CJD_TO_JD           (DOUBLE)(0.5)
#define   CJD_TO_MJD          (DOUBLE)(2400000.5 - CJD_TO_JD)
#define   CJD_TO_LD           (DOUBLE)(2299159.5 - CJD_TO_JD)

/*
	CJD暦(=JD暦-0.5)の最終日 NS_before[]
	NS暦 (=GD暦)    の開始日 NS_after[]

	JD暦は本来、BC.4713-1-1 "12:00" を起点とするが、
	計算上、"00:00" を起点(=CJD暦)として扱う.
	<cf> idate_jdToCjd(JD)

	起点は国によって違う
	<ITALY>
		CJD:2299160／YMD:1582-10-04
		CJD:2299161／YMD:1582-10-15

	<ENGLAND>
		CJD:2361221／YMD:1752-09-02
		CJD:2361222／YMD:1752-09-14
*/

// 本来UINT/UINT64でよいが、保険のためINT/INT64を使用
typedef struct
{
	BOOL sign; //TRUE='+'／FALSE='-'
	INT y;
	INT m;
	INT d;
	INT h;
	INT n;
	INT s;
	DOUBLE days;
}
$struct_idate_value, 
$struct_iDV;

#define   iDV_alloc()         ($struct_iDV*)icalloc(1, sizeof($struct_iDV), FALSE);IDV->sign = TRUE
#define   iDV_set(IDV, i_y, i_m, i_d, i_h, i_n, i_s)        idate_cjdToYmdhns(IDV, idate_ymdhnsToCjd(i_y, i_m, i_d, i_h, i_n, i_s))
#define   iDV_set2(IDV, cjd)  idate_cjdToYmdhns(IDV, cjd)
#define   iDV_getCJD(IDV)     (INT64)idate_ymdhnsToCjd(IDV->y, IDV->m, IDV->d, IDV->h, IDV->n, IDV->s)
#define   iDV_add(IDV, i_y, i_m, i_d, i_h, i_n, i_s)        iDV_set(IDV, (IDV->y + i_y), (IDV->m + i_m), (IDV->d + i_d), (IDV->h + i_h), (IDV->n + i_n), (IDV->s + i_s))
#define   iDV_add2(IDV, cjd)  idate_cjdToYmdhns(IDV, cjd + iDV_getCJD(IDV))
#define   iDV_clear(IDV)      IDV->sign = TRUE;IDV->y = 0;IDV->m = 0;IDV->d = 0;IDV->h = 0;IDV->n = 0;IDV->s = 0;IDV->days = 0.0
#define   iDV_free(IDV)       ifree(IDV)

BOOL      idate_chk_ymdhnsW(WS *str);

BOOL      idate_chk_uruu(INT i_y);

VOID      idate_cnv_month(INT *i_y, INT *i_m, INT from_m, INT to_m);
// 1-12月
#define   idate_cnv_month1(i_y, i_m)   idate_cnv_month(i_y, i_m, 1, 12)
// 0-11月
#define   idate_cnv_month2(i_y, i_m)   idate_cnv_month(i_y, i_m, 0, 11)

INT       idate_month_end(INT i_y, INT i_m);
BOOL      idate_chk_month_end(INT i_y, INT i_m, INT i_d);

INT       *idate_WsToiAryYmdhns(WS *str);

INT       idate_ymdToINT(INT i_y, INT i_m, INT i_d);
DOUBLE    idate_ymdhnsToCjd(INT i_y, INT i_m, INT i_d, INT i_h, INT i_n, INT i_s);

VOID      idate_cjdToYmdhns($struct_iDV *IDV, CONST DOUBLE cjd);

INT       idate_cjd_iWday(DOUBLE cjd);
WS        *idate_cjd_Wday(DOUBLE cjd);

// 年内の通算日
INT       idate_cjd_yeardays(DOUBLE cjd);
// cjd1 - cjd2 の通算日
#define   idate_cjd_days(cjd1, cjd2)    (INT)((INT)cjd2 - (INT)cjd1)

// 年内の通算週
#define   idate_cjd_yearweeks(cjd)      (INT)((6 + idate_cjd_yeardays(cjd)) / 7)
// cjd1 - cjd2 の通算週
#define   idate_cjd_weeks(cjd1, cjd2)   (INT)((idate_cjd_days(cjd1, cjd2) + 6) / 7)

// {TRUE, y, m, d, h, n, s, 0.0}
VOID      idate_add($struct_iDV *IDV, INT i_y, INT i_m, INT i_d, INT i_h, INT i_n, INT i_s, INT add_y, INT add_m, INT add_d, INT add_h, INT add_n, INT add_s);

// {sign, y, m, d, h, n, s, days}
VOID      idate_diff($struct_iDV *IDV, INT i_y1, INT i_m1, INT i_d1, INT i_h1, INT i_n1, INT i_s1, INT i_y2, INT i_m2, INT i_d2, INT i_h2, INT i_n2, INT i_s2);

/// VOID iDV_checker(INT from_year, INT to_year, INT repeat);

/*
// Ymdhns
	%a : 曜日(例:Su)
	%A : 曜日数
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
WS        *idate_format(WS *format, BOOL b_sign, INT i_y, INT i_m, INT i_d, INT i_h, INT i_n, INT i_s, DOUBLE d_days);
#define   idate_format_ymdhns(format, i_y, i_m, i_d, i_h, i_n, i_s)   (WS*)idate_format(format, TRUE, i_y, i_m, i_d, i_h, i_n, i_s, 0.0)

WS        *idate_format_cjdToWS(WS *format, DOUBLE cjd);

WS        *idate_replace_format_ymdhns(WS *str, WS *quote1, WS *quote2, WS *add_quote, INT i_y, INT i_m, INT i_d, INT i_h, INT i_n, INT i_s);
#define   idate_format_nowToYmdhns(i_y, i_m, i_d, i_h, i_n, i_s)      (WS*)idate_replace_format_ymdhns(L"[]", L"[", L"]", "", i_y, i_m, i_d, i_h, i_n, i_s)

INT       *idate_nowToiAryYmdhns(BOOL area);
#define   idate_nowToiAryYmdhns_localtime()       (INT*)idate_nowToiAryYmdhns(TRUE)
#define   idate_nowToiAryYmdhns_systemtime()      (INT*)idate_nowToiAryYmdhns(FALSE)

DOUBLE    idate_nowToCjd(BOOL area);
#define   CJD_NOW_LOCAL()     (DOUBLE)idate_nowToCjd(TRUE)
#define   CJD_NOW_SYSTEM()    (DOUBLE)idate_nowToCjd(FALSE)

#define   CJD_SEC(cjd)        (DOUBLE)(cjd * 86400.0)
