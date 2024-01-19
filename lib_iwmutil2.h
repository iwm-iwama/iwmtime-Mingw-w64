//////////////////////////////////////////////////////////////////////////////////////////
#define   LIB_IWMUTIL_COPYLIGHT                   "(C)2008-2024 iwm-iwama"
#define   LIB_IWMUTIL_VERSION                     "lib_iwmutil2_20240118"
//////////////////////////////////////////////////////////////////////////////////////////
#include <conio.h>
#include <ctype.h>
#include <float.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <shlwapi.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <windows.h>

//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	共通定数
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
typedef   CHAR      MS; // imx_xxx() = Muliti Byte String／iux_xxx() = UTF-8N
typedef   WCHAR     WS; // iwx_xxx()／UTF-16／Wide Char String

#define   IMAX_PATH                               ((MAX_PATH*4)+1) // UTF-8 = (Max)4byte

#define   DATETIME_FORMAT                         L"%.4d-%02d-%02d %02d:%02d:%02d"
#define   IDATE_FORMAT_STD                        L"%G%y-%m-%d %h:%n:%s"

//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	大域変数
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
#define   imain_begin()                           iExecSec(0);iCLI_begin();iConsole_EscOn()
#define   imain_end()                             ifree_all();iCLI_end(EXIT_SUCCESS)
#define   imain_err()                             P1(IESC_RESET);ifree_all();iCLI_end(EXIT_FAILURE)

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

WS        *iCLI_getOptValue(UINT argc,WS *opt1,WS *opt2);
BOOL      iCLI_getOptMatch(UINT argc,WS *opt1,WS *opt2);

VOID      iCLI_VarList();

//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	実行開始時間
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
UINT64    iExecSec(CONST UINT64 microSec);
#define   iExecSec_init()                         (UINT64)iExecSec(0)
#define   iExecSec_next()                         (DOUBLE)(iExecSec($ExecSecBgn))/1000

//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	メモリ確保
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
VOID      *icalloc(UINT64 n,UINT64 size,BOOL aryOn);
VOID      *irealloc(VOID *ptr,UINT64 n,UINT64 size);

// MS 文字列
#define   icalloc_MS(n)                           (MS*)icalloc(n,sizeof(MS),FALSE)
#define   irealloc_MS(str,n)                      (MS*)irealloc(str,n,sizeof(MS))

// MS 配列
#define   icalloc_MS_ary(n)                       (MS**)icalloc(n,sizeof(MS*),TRUE)
#define   irealloc_MS_ary(str,n)                  (MS**)irealloc(str,n,sizeof(MS*))

// WS 文字列
#define   icalloc_WS(n)                           (WS*)icalloc(n,sizeof(WS),FALSE)
#define   irealloc_WS(str,n)                      (WS*)irealloc(str,n,sizeof(WS))

// WS 配列
#define   icalloc_WS_ary(n)                       (WS**)icalloc(n,sizeof(WS*),TRUE)
#define   irealloc_WS_ary(str,n)                  (WS**)irealloc(str,n,sizeof(WS*))

// INT 配列
#define   icalloc_INT(n)                          (INT*)icalloc(n,sizeof(INT),FALSE)
#define   irealloc_INT(ptr,n)                     (INT*)irealloc(ptr,n,sizeof(INT))

VOID      icalloc_err(VOID *ptr);

VOID      icalloc_free(VOID *ptr);
VOID      icalloc_freeAll();
VOID      icalloc_mapSweep();

#define   ifree(ptr)                              icalloc_free(ptr);icalloc_mapSweep();
#define   ifree_all()                             icalloc_freeAll()

VOID      icalloc_mapPrint1();
#define   icalloc_mapPrint()                      PL();NL();icalloc_mapPrint1()

//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	Print関係
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
VOID      P(MS *format,...);

VOID      QP(MS *str, UINT size);
#define   QP1(str)                                QP(str,strlen(str))
#define   QP2(str)                                QP1(str);NL()

#define   PL()                                    P("[L%u] ",__LINE__)
#define   NL()                                    putchar('\n')

#define   P1(str)                                 fputs(str, stdout)
#define   P2(str)                                 puts(str)
#define   P3(num)                                 P("%lld\n",(INT64)num)
#define   P4(num)                                 P("%.8lf\n",(DOUBLE)num)

#define   PL2(str)                                PL();P2(str)
#define   PL3(num)                                PL();P3(num)
#define   PL4(num)                                PL();P4(num)

VOID      P1W(WS *str);
#define   P2W(str)                                P1W(str);putchar('\n')
#define   PL2W(str)                               PL();P2W(str)

VOID      PR1(MS *str, UINT iRepeat);
#define   LN(iRepeat)                             PR1("-",iRepeat);NL();

WS        *iws_cnv_escape(WS *str);

VOID      imv_system(WS *wCmd, BOOL bOutput);
WS        *iws_popen(WS *cmd);

//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	UTF-16／UTF-8変換
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
MS        *icnv_W2M(WS *str,UINT uCP);
#define   W2M(str)                                (MS*)icnv_W2M(str,65001)

WS        *icnv_M2W(MS *str,UINT uCP);
#define   M2W(str)                                (WS*)icnv_M2W(str,65001)

//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	文字列処理
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
UINT64    imn_len(MS *str);
UINT64    iwn_len(WS *str);
UINT64    iun_len(MS *str);

UINT      imn_Codepage(MS *str);

UINT64    imn_cpy(MS *to,MS *from);
UINT64    iwn_cpy(WS *to,WS *from);

UINT64    imn_pcpy(MS *to,MS *from1,MS *from2);
UINT64    iwn_pcpy(WS *to,WS *from1,WS *from2);

MS        *ims_clone(MS *from);
WS        *iws_clone(WS *from);

WS        *iws_pclone(WS *from1,WS *from2);

MS        *ims_cats(UINT size,...);
WS        *iws_cats(UINT size,...);

MS        *ims_sprintf(MS *format,...);
WS        *iws_sprintf(WS *format,...);

BOOL      iwb_cmp(WS *str,WS *search,BOOL perfect,BOOL icase);
#define   iwb_cmpf(str,search)                    (BOOL)iwb_cmp(str,search,FALSE,FALSE)
#define   iwb_cmpfi(str,search)                   (BOOL)iwb_cmp(str,search,FALSE,TRUE)
#define   iwb_cmpp(str,search)                    (BOOL)iwb_cmp(str,search,TRUE,FALSE)
#define   iwb_cmppi(str,search)                   (BOOL)iwb_cmp(str,search,TRUE,TRUE)
#define   iwb_cmp_leq(str,search,icase)           (BOOL)iwb_cmp(search,str,FALSE,icase)
#define   iwb_cmp_leqf(str,search)                (BOOL)iwb_cmp_leq(str,search,FALSE)
#define   iwb_cmp_leqfi(str,search)               (BOOL)iwb_cmp_leq(str,search,TRUE)

UINT64    iwn_searchCnt(WS *str,WS *search,BOOL icase);
#define   iwn_search(str,search)                  (UINT)iwn_searchCnt(str,search,FALSE)
#define   iwn_searchi(str,search)                 (UINT)iwn_searchCnt(str,search,TRUE)

WS        **iwaa_split(WS *str,WS *tokens, BOOL bRmEmpty);

WS        *iws_replace(WS *from,WS *before,WS *after,BOOL icase);

MS        *ims_IntToMs(INT64 num);
MS        *ims_DblToMs(DOUBLE num,INT iDigit);

WS        *iws_strip(WS *str,BOOL bStripLeft,BOOL bStripRight);
#define   iws_trim(str)                           (WS*)iws_strip(str,TRUE,TRUE)
#define   iws_trimL(str)                          (WS*)iws_strip(str,TRUE,FALSE)
#define   iws_trimR(str)                          (WS*)iws_strip(str,FALSE,TRUE)

WS        *iws_cutYenR(WS *path);

//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	Array
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
UINT64    iwan_size(WS **ary);
UINT64    iwan_strlen(WS **ary);

INT       iwan_sort_Asc(CONST VOID *arg1,CONST VOID *arg2);
INT       iwan_sort_iAsc(CONST VOID *arg1,CONST VOID *arg2);
INT       iwan_sort_Desc(CONST VOID *arg1,CONST VOID *arg2);
INT       iwan_sort_iDesc(CONST VOID *arg1,CONST VOID *arg2);
#define   iwav_sort_Asc(ary)                      qsort(ary,iwan_size(ary),sizeof(WS*),iwan_sort_Asc)
#define   iwav_sort_iAsc(ary)                     qsort(ary,iwan_size(ary),sizeof(WS*),iwan_sort_iAsc)
#define   iwav_sort_Desc(ary)                     qsort(ary,iwan_size(ary),sizeof(WS*),iwan_sort_Desc)
#define   iwav_sort_iDesc(ary)                    qsort(ary,iwan_size(ary),sizeof(WS*),iwan_sort_iDesc)

WS        *iwas_njoin(WS **ary,WS *token,UINT start,UINT count);
#define   iwas_join(ary,token)                    (WS*)iwas_njoin(ary,token,0,iwan_size(ary))

WS        **iwaa_uniq(WS **ary,BOOL icase);
WS        **iwaa_getDirFile(WS **ary,INT iType);
WS        **iwaa_higherDir(WS **ary);

VOID      iwav_print(WS **ary);
VOID      iwav_print2(WS **ary,WS *sLeft,WS *sRight);

//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	Variable Buffer
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
typedef struct
{
	UINT64 size;   // Buffers Size
	VOID   *str;   // String Pointer
	UINT64 length; // String Length
}
$struct_iVBStr,
$struct_iVBM,
$struct_iVBW;

$struct_iVBStr      *iVBStr_alloc(UINT64 startSize,INT sizeOfChar);
VOID      *iVBStr_add($struct_iVBStr *IBS,VOID *str,UINT64 strLen,INT sizeOfChar);

// MS
#define   iVBM_alloc()                            iVBStr_alloc(256,sizeof(MS))
#define   iVBM_alloc2(startSize)                  iVBStr_alloc(startSize,sizeof(MS))
#define   iVBM_add(IVBM,str)                      (MS*)iVBStr_add(IVBM,str,strlen(str),sizeof(MS))
#define   iVBM_add2(IVBM,str,strLen)              (MS*)iVBStr_add(IVBM,str,strLen,sizeof(MS))
#define   iVBM_clear(IVBM)                        memset(IVBM->str,0,sizeof(MS));IVBM->length=0
#define   iVBM_getStr(IVBM)                       (MS*)(IVBM->str)
#define   iVBM_getLength(IVBM)                    (UINT64)(IVBM->length)
#define   iVBM_getSize(IVBM)                      (UINT64)(IVBM->size)
#define   iVBM_free(IVBM)                         IVBM->size=0;IVBM->length=0;ifree(IVBM->str);ifree(IVBM)

// WS
#define   iVBW_alloc()                            iVBStr_alloc(256,sizeof(WS))
#define   iVBW_alloc2(startSize)                  iVBStr_alloc(startSize,sizeof(WS))
#define   iVBW_add(IVBW,str)                      (WS*)iVBStr_add(IVBW,str,wcslen(str),sizeof(WS))
#define   iVBW_add2(IVBW,str,strLen)              (WS*)iVBStr_add(IVBW,str,strLen,sizeof(WS))
#define   iVBW_clear(IVBW)                        memset(IVBW->str,0,sizeof(WS));IVBW->length=0
#define   iVBW_getStr(IVBW)                       (WS*)(IVBW->str)
#define   iVBW_getLength(IVBW)                    (UINT64)(IVBW->length)
#define   iVBW_getSize(IVBW)                      (UINT64)(IVBW->size)
#define   iVBW_free(IVBW)                         IVBW->size=0;IVBW->length=0;ifree(IVBW->str);ifree(IVBW)

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
	DOUBLE cjdCtime;         // 作成時間
	DOUBLE cjdMtime;         // 更新時間
	DOUBLE cjdAtime;         // アクセス時間
	UINT64 uFsize;           // ファイルサイズ
}
$struct_iFinfo;

$struct_iFinfo      *iFinfo_alloc();

BOOL      iFinfo_init($struct_iFinfo *FI,WIN32_FIND_DATAW *F,WS *dir,WS *fname);
VOID      iFinfo_free($struct_iFinfo *FI);

WS        *iFinfo_attrToWS(UINT uAttr);
UINT      iFinfo_attrToINT(WS *sAttr);

WS        *iFinfo_ftimeToWS(FILETIME ftime);
INT64     iFinfo_ftimeToINT64(FILETIME ftime);
DOUBLE    iFinfo_ftimeToCjd(FILETIME ftime);

FILETIME  iFinfo_ymdhnsToFtime(INT wYear,INT wMonth,INT wDay,INT wHour,INT wMinute,INT wSecond,BOOL reChk);

//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	File/Dir処理
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
#define   iFchk_existPath(path)                   (BOOL)PathFileExistsW(path)
#define   iFchk_DirName(path)                     (BOOL)(GetFileAttributesW(path) & FILE_ATTRIBUTE_DIRECTORY ? TRUE : FALSE)

#define   iFchk_attrDirFile(attr)                 (INT)(((INT)attr & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 2)

BOOL      iFchk_Binfile(WS *fn);

WS        *iFget_extPathname(WS *path,INT option);

WS        *iFget_APath(WS *path);
WS        *iFget_RPath(WS *path);

UINT      iF_mkdir(WS *path);
WS        **iF_trash(WS *path);

//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	Console
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
#define   IESC_CLEAR                              "\033[H;\033[2J"
#define   IESC_RESET                              "\033[0m"
#define   IESC_TITLE1                             "\033[38;2;250;250;250m\033[104m" // 白／青
#define   IESC_OPT1                               "\033[38;2;250;150;150m"          // 赤
#define   IESC_OPT2                               "\033[38;2;150;150;250m"          // 青
#define   IESC_OPT21                              "\033[38;2;25;225;235m"           // 水
#define   IESC_OPT22                              "\033[38;2;250;100;250m"          // 紅紫
#define   IESC_LBL1                               "\033[38;2;250;250;100m"          // 黄
#define   IESC_LBL2                               "\033[38;2;100;100;250m"          // 青
#define   IESC_STR1                               "\033[38;2;225;225;225m"          // 白
#define   IESC_STR2                               "\033[38;2;175;175;175m"          // 銀
#define   IESC_TRUE1                              "\033[38;2;0;250;250m"            // 水
#define   IESC_FALSE1                             "\033[38;2;250;50;50m"            // 紅

#define   iConsole_setTitle(str)                  SetConsoleTitleW(str)

VOID      iConsole_EscOn();

WS        *iCLI_GetStdin();

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
		JD  : Julian Day               :-4712-01-01 12:00:00開始
		CJD : Chronological Julian Day :-4712-01-01 00:00:00開始 :JD-0.5
		MJD : Modified Julian Day      : 1858-11-17 00:00:00開始 :JD-2400000.5
		LD  : Lilian Day               : 1582-10-15 00:00:00開始 :JD-2299159.5
*/
#define   CJD_START                               L"-4712-01-01 00:00:00"
#define   JD_START                                L"-4712-01-01 12:00:00"

#define   CJD_TO_JD                               (DOUBLE)(0.5)
#define   CJD_TO_MJD                              (DOUBLE)(2400000.5-CJD_TO_JD)
#define   CJD_TO_LD                               (DOUBLE)(2299159.5-CJD_TO_JD)

/*
	CJD暦(=JD暦-0.5)の最終日 NS_before[]
	NS暦 (=GD暦)    の開始日 NS_after[]

	JD暦は本来、BC.4713-1-1 12:00を起点とするが、
	計算上、00:00を起点(=CJD暦)として扱う.
	<cf> idate_jdToCjd(JD)

	起点は、国によって違う
	<ITALY>
		CJD:2299160	YMD:1582-10-04
		CJD:2299161	YMD:1582-10-15

	<ENGLAND>
		CJD:2361221	YMD:1752-09-02
		CJD:2361222	YMD:1752-09-14
*/

BOOL      idate_chk_ymdhnsW(WS *str);

BOOL      idate_chk_uruu(INT i_y);

INT       *idate_cnv_month(INT i_y,INT i_m,INT from_m,INT to_m);
// 1-12月
#define   idate_cnv_month1(i_y,i_m)               (INT*)idate_cnv_month(i_y,i_m,1,12)
// 0-11月
#define   idate_cnv_month2(i_y,i_m)               (INT*)idate_cnv_month(i_y,i_m,0,11)

INT       idate_month_end(INT i_y,INT i_m);
BOOL      idate_chk_month_end(INT i_y,INT i_m,INT i_d,BOOL reChk);

INT       *idate_WsToiAryYmdhns(WS *str);

INT       idate_ymd_num(INT i_y,INT i_m,INT i_d);
DOUBLE    idate_ymdhnsToCjd(INT i_y,INT i_m,INT i_d,INT i_h,INT i_n,INT i_s);

INT       *idate_cjdToiAryHns(DOUBLE cjd);
INT       *idate_cjdToiAryYmdhns(DOUBLE cjd);

INT       *idate_reYmdhns(INT i_y,INT i_m,INT i_d,INT i_h,INT i_n,INT i_s);

INT       idate_cjd_iWday(DOUBLE cjd);
WS        *idate_cjd_Wday(DOUBLE cjd);

// 年内の通算日
INT       idate_cjd_yeardays(DOUBLE cjd);
// cjd1 - cjd2 の通算日
#define   idate_cjd_days(cjd1,cjd2)               (INT)((INT)cjd2-(INT)cjd1)

// 年内の通算週
#define   idate_cjd_yearweeks(cjd)                (INT)((6+idate_cjd_yeardays(cjd))/7)
// cjd1 - cjd2 の通算週
#define   idate_cjd_weeks(cjd1,cjd2)              (INT)((idate_cjd_days(cjd1,cjd2)+6)/7)

// [6]={y,m,d,h,n,s}
INT       *idate_add(INT i_y,INT i_m,INT i_d,INT i_h,INT i_n,INT i_s,INT add_y,INT add_m,INT add_d,INT add_h,INT add_n,INT add_s);
// [8]={sign,y,m,d,h,n,s,days}
INT       *idate_diff(INT i_y1,INT i_m1,INT i_d1,INT i_h1,INT i_n1,INT i_s1,INT i_y2,INT i_m2,INT i_d2,INT i_h2,INT i_n2,INT i_s2);

/// VOID idate_diff_checker(INT from_year,INT to_year,INT repeat);

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
WS        *idate_format_diff(WS *format,INT i_sign,INT i_y,INT i_m,INT i_d,INT i_h,INT i_n,INT i_s,INT64 i_days);
#define   idate_format_ymdhns(format,i_y,i_m,i_d,i_h,i_n,i_s)         (WS*)idate_format_diff(format,0,i_y,i_m,i_d,i_h,i_n,i_s,0)

WS        *idate_format_cjdToWS(WS *format,DOUBLE cjd);

WS        *idate_replace_format_ymdhns(WS *str,WS *quote1,WS *quote2,WS *add_quote,INT i_y,INT i_m,INT i_d,INT i_h,INT i_n,INT i_s);
#define   idate_format_nowToYmdhns(i_y,i_m,i_d,i_h,i_n,i_s)           (WS*)idate_replace_format_ymdhns(L"[]",L"[",L"]","",i_y,i_m,i_d,i_h,i_n,i_s)

INT       *idate_nowToiAryYmdhns(BOOL area);
#define   idate_nowToiAryYmdhns_localtime()       (INT*)idate_nowToiAryYmdhns(TRUE)
#define   idate_nowToiAryYmdhns_systemtime()      (INT*)idate_nowToiAryYmdhns(FALSE)

DOUBLE    idate_nowToCjd(BOOL area);
#define   CJD_NOW_LOCAL()                         (DOUBLE)idate_nowToCjd(TRUE)
#define   CJD_NOW_SYSTEM()                        (DOUBLE)idate_nowToCjd(FALSE)

#define   CJD_SEC(cjd)                            (DOUBLE)(cjd*86400.0)
