//////////////////////////////////////////////////////////////////////////////////////////
#define   LIB_IWMUTIL_VERSION                     "lib_iwmutil2_20230815"
#define   LIB_IWMUTIL_COPYLIGHT                   "Copyright (C)2008-2023 iwm-iwama"
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

#define   IMAX_PATH                               ((MAX_PATH>>3)<<4) // windef.h参照
#define   ISO_FORMAT_DATETIME                     L"%.4d-%02d-%02d %02d:%02d:%02d"
#define   IDATE_FORMAT_STD                        L"%G%y-%m-%d %h:%n:%s"

//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	大域変数
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
#define   imain_begin()                           iExecSec(0);iCLI_getCommandLine();iConsole_EscOn();
#define   imain_end()                             P1(ICLR_RESET);ifree_all();exit(EXIT_SUCCESS)
#define   imain_err()                             P1(ICLR_RESET);ifree_all();exit(EXIT_FAILURE)

extern    WS        *$CMD;         // コマンド名を格納
extern    UINT      $ARGC;         // 引数配列数
extern    WS        **$ARGV;       // 引数配列／ダブルクォーテーションを消去したもの
extern    UINT      $CP_STDIN;     // 入力コードページ／不定
extern    UINT      $CP_STDOUT;    // 出力コードページ
extern    HANDLE    $StdoutHandle; // 画面制御用ハンドル
extern    UINT64    $ExecSecBgn;   // 実行開始時間

//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	Command Line
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
VOID      iCLI_getCommandLine();
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

#define   icalloc_MS(n)                           (MS*)icalloc(n,sizeof(MS),FALSE)
#define   irealloc_MS(str,n)                      (MS*)irealloc(str,n,sizeof(MS))

#define   icalloc_MS_ary(n)                       (MS**)icalloc(n,sizeof(MS*),TRUE)
#define   irealloc_MS_ary(str,n)                  (MS**)irealloc(str,n,sizeof(MS*))

#define   icalloc_WS(n)                           (WS*)icalloc(n,sizeof(WS),FALSE)
#define   irealloc_WS(str,n)                      (WS*)irealloc(str,n,sizeof(WS))

#define   icalloc_WS_ary(n)                       (WS**)icalloc(n,sizeof(WS*),TRUE)
#define   irealloc_WS_ary(str,n)                  (WS**)irealloc(str,n,sizeof(WS*))

#define   icalloc_INT(n)                          (INT*)icalloc(n,sizeof(INT),FALSE)
#define   irealloc_INT(ptr,n)                     (INT*)irealloc(ptr,n,sizeof(INT))

#define   icalloc_INT64(n)                        (INT64*)icalloc(n,sizeof(INT64),FALSE)
#define   irealloc_INT64(ptr,n)                   (INT64*)irealloc(ptr,n,sizeof(INT64))

#define   icalloc_DBL(n)                          (DOUBLE*)icalloc(n,sizeof(DOUBLE),FALSE)
#define   irealloc_DBL(ptr,n)                     (DOUBLE*)irealloc(ptr,n,sizeof(DOUBLE))

VOID      icalloc_err(VOID *ptr);

VOID      icalloc_free(VOID *ptr);
VOID      icalloc_freeAll();
VOID      icalloc_mapSweep();

#define   ifree(ptr)                              (VOID)icalloc_free(ptr);(VOID)icalloc_mapSweep();
#define   ifree_all()                             (VOID)icalloc_freeAll()

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

WS        *iws_conv_escape(WS *str);

//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	UTF-16／UTF-8変換
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
MS        *icnv_W2M(WS *str);
#define   W2M(str)                                (MS*)icnv_W2M(str)

WS        *icnv_M2W(MS *str);
#define   M2W(str)                                (WS*)icnv_M2W(str)

//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	文字列処理
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
UINT64    imn_len(MS *str);
UINT64    iwn_len(WS *str);
UINT64    iun_len(MS *str);

UINT64    imn_cpy(MS *to,MS *from);
UINT64    iwn_cpy(WS *to,WS *from);

UINT64    imn_pcpy(MS *to,MS *from1,MS *from2);
UINT64    iwn_pcpy(WS *to,WS *from1,WS *from2);

MS        *ims_clone(MS *from);
WS        *iws_clone(WS *from);

MS        *ims_pclone(MS *from1,MS *from2);
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

UINT64    iwn_searchCntW(WS *str,WS *search,BOOL icase);
#define   iwn_searchCnt(str,search)               (UINT)iwn_searchCntW(str,search,FALSE)
#define   iwn_searchCnti(str,search)              (UINT)iwn_searchCntW(str,search,TRUE)

WS        *iwp_searchLM(WS *str,WS *search,BOOL icase);
#define   iwp_searchL(str,search)                 (WS*)iwp_searchLM(str,search,FALSE)
#define   iwp_searchLi(str,search)                (WS*)iwp_searchLM(str,search,TRUE)

WS        **iwaa_split(WS *str,WS *tokens, BOOL bRmEmpty);

WS        *iws_replace(WS *from,WS *before,WS *after,BOOL icase);

MS        *ims_IntToMs(INT64 num);
MS        *ims_DblToMs(DOUBLE num,INT iDigit);

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
#define   iwav_sort_Asc(ary)                      (VOID)qsort(ary,iwan_size(ary),sizeof(WS*),iwan_sort_Asc)
#define   iwav_sort_iAsc(ary)                     (VOID)qsort(ary,iwan_size(ary),sizeof(WS*),iwan_sort_iAsc)
#define   iwav_sort_Desc(ary)                     (VOID)qsort(ary,iwan_size(ary),sizeof(WS*),iwan_sort_Desc)
#define   iwav_sort_iDesc(ary)                    (VOID)qsort(ary,iwan_size(ary),sizeof(WS*),iwan_sort_iDesc)

WS        *iwas_njoin(WS **ary,WS *token,UINT start,UINT count);
#define   iwas_join(ary,token)                    (WS*)iwas_njoin(ary,token,0,iwan_size(ary))

WS        **iwaa_simplify(WS **ary,BOOL icase);
WS        **iwaa_getDir(WS **ary);
WS        **iwaa_higherDir(WS **ary);

VOID      iwav_print(WS **ary);

//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	File/Dir処理／WIN32_FIND_DATAW
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
typedef struct
{
	WS     fullnameW[IMAX_PATH]; // フルパス
	UINT   uFname;               // ファイル名開始位置
	UINT   uAttr;                // 属性
	BOOL   bType;                // TRUE=Dir／FALSE=File
	DOUBLE cjdCtime;             // 作成時間
	DOUBLE cjdMtime;             // 更新時間
	DOUBLE cjdAtime;             // アクセス時間
	UINT64 uFsize;               // ファイルサイズ
}
$struct_iFinfoW;

$struct_iFinfoW *iFinfo_allocW();

BOOL      iFinfo_initW($struct_iFinfoW *FI,WIN32_FIND_DATAW *F,WS *dir,WS *name);
VOID      iFinfo_freeW($struct_iFinfoW *FI);

WS        *iFinfo_attrToW(UINT uAttr);
INT       iFinfo_attrWtoINT(WS *sAttr);

WS        *iFinfo_ftypeToW(INT iFtype);

INT       iFinfo_depthW($struct_iFinfoW *FI);

WS        *iFinfo_ftimeToW(FILETIME ftime);
DOUBLE    iFinfo_ftimeToCjd(FILETIME ftime);

FILETIME  iFinfo_ymdhnsToFtime(INT wYear,INT wMonth,INT wDay,INT wHour,INT wMinute,INT wSecond,BOOL reChk);

//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	File/Dir処理
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
BOOL      iFchk_existPathW(WS *path);

INT       iFchk_typePathW(WS *path);

BOOL      iFchk_BfileW(WS *fn);
#define   iFchk_TfileW(fn)                        (BOOL)(iFchk_typePathW(fn) == 2 && !iFchk_BfileW(fn) ? TRUE : FALSE)

#define   ichk_attrDirFile(attr)                  (INT)(((INT)attr & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 2)

WS        *iFget_extPathnameW(WS *path,INT option);

WS        *iFget_ApathW(WS *path);
WS        *iFget_RpathW(WS *path);

BOOL      imk_dirW(WS *path);
BOOL      imv_trashW(WS *path);

//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	Console
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
#define   ICLR_RESET                              "\033[0m"
#define   ICLR_TITLE1                             "\033[38;2;250;250;250m\033[104m" // 白／青
#define   ICLR_OPT1                               "\033[38;2;250;150;150m"          // 赤
#define   ICLR_OPT2                               "\033[38;2;150;150;250m"          // 青
#define   ICLR_OPT21                              "\033[38;2;80;250;250m"           // 水
#define   ICLR_OPT22                              "\033[38;2;250;100;250m"          // 紅紫
#define   ICLR_LBL1                               "\033[38;2;250;250;100m"          // 黄
#define   ICLR_LBL2                               "\033[38;2;100;100;250m"          // 青
#define   ICLR_STR1                               "\033[38;2;225;225;225m"          // 白
#define   ICLR_STR2                               "\033[38;2;175;175;175m"          // 銀
#define   ICLR_ERR1                               "\033[38;2;200;0;0m"              // 紅

#define   iConsole_setTitleW(str)                 (VOID)SetConsoleTitleW(str)

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

#define   CJD_FORMAT                              L"%.8f"

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

// CJD通日
#define   idate_cjd_print(CJD)                    P(CJD_FORMAT,CJD)

// CJD通日から、JD通日を返す
#define   idate_cjdToJd(CJD)                      (DOUBLE)CJD-CJD_TO_JD
#define   idate_cjdToJd_print(CJD)                P(CJD_FORMAT,idate_cjdToJd(CJD))

// JD通日から、CJD通日を返す
#define   idate_jdToCjd(JD)                       (DOUBLE)JD+CJD_TO_JD
#define   idate_jdToCjd_print(JD)                 P(CJD_FORMAT,idate_jdToCjd(JD))

// CJD通日から、MJD通日を返す
#define   idate_cjdToMjd(CJD)                     (DOUBLE)CJD-CJD_TO_MJD
#define   idate_cjdToMjd_print(CJD)               P(CJD_FORMAT,idate_cjdToMjd(CJD))

// MJD通日から、CJD通日を返す
#define   idate_mjdToCjd(MJD)                     (DOUBLE)MJD+CJD_TO_MJD
#define   idate_mjdToCjd_print(MJD)               P(CJD_FORMAT,idate_mjdToCjd(MJD))

// CJD通日から、LD通日を返す
#define   idate_cjdToLd(CJD)                      (DOUBLE)CJD-CJD_TO_LD
#define   idate_cjdToLd_print(CJD)                P(CJD_FORMAT,idate_cjdToLd(CJD))

// LD通日から、CJD通日を返す
#define   idate_ldToCjd(LD)                       (DOUBLE)LD+CJD_TO_LD
#define   idate_ldToCjd_print(LD)                 P(CJD_FORMAT,idate_ldToCjd(LD))

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

WS        *idate_format_cjdToW(WS *format,DOUBLE cjd);

WS        *idate_replace_format_ymdhns(WS *str,WS *quote1,WS *quote2,WS *add_quote,INT i_y,INT i_m,INT i_d,INT i_h,INT i_n,INT i_s);
#define   idate_format_nowToYmdhns(i_y,i_m,i_d,i_h,i_n,i_s)           (WS*)idate_replace_format_ymdhns(L"[]",L"[",L"]","",i_y,i_m,i_d,i_h,i_n,i_s)

INT       *idate_nowToiAryYmdhns(BOOL area);
#define   idate_nowToiAryYmdhns_localtime()       (INT*)idate_nowToiAryYmdhns(TRUE)
#define   idate_nowToiAryYmdhns_systemtime()      (INT*)idate_nowToiAryYmdhns(FALSE)

DOUBLE    idate_nowToCjd(BOOL area);
#define   CJD_NOW_LOCAL()                         (DOUBLE)idate_nowToCjd(TRUE)
#define   CJD_NOW_SYSTEM()                        (DOUBLE)idate_nowToCjd(FALSE)

#define   CJD_SEC(cjd)                            (DOUBLE)(cjd*86400.0)
