/////////////////////////////////////////////////////////////////////////////////////////
#define  LIB_IWMUTIL_VERSION                     "lib_iwmutil2_20220905"
#define  LIB_IWMUTIL_COPYLIGHT                   "Copyright (C)2008-2022 iwm-iwama"
/////////////////////////////////////////////////////////////////////////////////////////
#include <conio.h>
#include <ctype.h>
#include <float.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <shlwapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <windows.h>

/////////////////////////////////////////////////////////////////////////////////////////
/*---------------------------------------------------------------------------------------
	共通定数
---------------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////
#define  IMAX_PATH                               (((MAX_PATH>>3)<<3)+(1<<3)) // windef.h参照

#define  MBS                                     CHAR  // imx_xxx()／MBCS／Muliti Byte String
#define  U8N                                     CHAR  // iux_xxx()／UTF-8／UTF-8 NoBOM
#define  WCS                                     WCHAR // iwx_xxx()／UTF-16／Wide Char String

#define  ISO_FORMAT_DATETIME                     L"%.4d-%02d-%02d %02d:%02d:%02d"

#define  IDATE_FORMAT_STD                        L"%G%y-%m-%d %h:%n:%s"

#define  LINE                                    "--------------------------------------------------------------------------------"

/////////////////////////////////////////////////////////////////////////////////////////
/*---------------------------------------------------------------------------------------
	大域変数
---------------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////
extern   WCS    *$CMD;         // コマンド名を格納
extern   UINT   $ARGC;         // 引数配列数
extern   WCS    **$ARGV;       // 引数配列
extern   WCS    **$ARGS;       // $ARGVからダブルクォーテーションを消去したもの
extern   UINT   $CP;           // 出力コードページ
extern   HANDLE $StdoutHandle; // 画面制御用ハンドル
extern   UINT   $ExecSecBgn;   // 実行開始時間

/////////////////////////////////////////////////////////////////////////////////////////
/*---------------------------------------------------------------------------------------
	Command Line
---------------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////
VOID     iCLI_getCommandLine(UINT codepage);
WCS      *iCLI_getOptValue(UINT argc,WCS *opt1,WCS *opt2);
BOOL     iCLI_getOptMatch(UINT argc,WCS *opt1,WCS *opt2);

VOID     iCLI_VarList();

/////////////////////////////////////////////////////////////////////////////////////////
/*---------------------------------------------------------------------------------------
	実行開始時間
---------------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////
UINT     iExecSec(CONST UINT microSec);
#define  iExecSec_init()                         (UINT)iExecSec(0)
#define  iExecSec_next()                         (DOUBLE)(iExecSec($ExecSecBgn))/1000

/////////////////////////////////////////////////////////////////////////////////////////
/*---------------------------------------------------------------------------------------
	メモリ確保
---------------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////
VOID     *icalloc(UINT n,UINT size,BOOL aryOn);
VOID     *irealloc(VOID *ptr,UINT n,UINT size);

#define  icalloc_MBS(n)                          (MBS*)icalloc(n,sizeof(MBS),FALSE)
#define  irealloc_MBS(str,n)                     (MBS*)irealloc(str,n,sizeof(MBS))

#define  icalloc_MBS_ary(n)                      (MBS**)icalloc(n,sizeof(MBS*),TRUE)
#define  irealloc_MBS_ary(str,n)                 (MBS**)irealloc(str,n,sizeof(MBS*))

#define  icalloc_WCS(n)                          (WCS*)icalloc(n,sizeof(WCS),FALSE)
#define  irealloc_WCS(str,n)                     (WCS*)irealloc(str,n,sizeof(WCS))

#define  icalloc_WCS_ary(n)                      (WCS**)icalloc(n,sizeof(WCS*),TRUE)
#define  irealloc_WCS_ary(str,n)                 (WCS**)irealloc(str,n,sizeof(WCS*))

#define  icalloc_INT(n)                          (INT*)icalloc(n,sizeof(INT),FALSE)
#define  irealloc_INT(ptr,n)                     (INT*)irealloc(ptr,n,sizeof(INT))

#define  icalloc_INT64(n)                        (INT64*)icalloc(n,sizeof(INT64),FALSE)
#define  irealloc_INT64(ptr,n)                   (INT64*)irealloc(ptr,n,sizeof(INT64))

#define  icalloc_DBL(n)                          (DOUBLE*)icalloc(n,sizeof(DOUBLE),FALSE)
#define  irealloc_DBL(ptr,n)                     (DOUBLE*)irealloc(ptr,n,sizeof(DOUBLE))

VOID     icalloc_err(VOID *ptr);

VOID     icalloc_free(VOID *ptr);
VOID     icalloc_freeAll();
VOID     icalloc_mapSweep();

#define  ifree(ptr)                              (VOID)icalloc_free(ptr);(VOID)icalloc_mapSweep();
#define  ifree_all()                             (VOID)icalloc_freeAll()
#define  imain_end()                             ifree_all();exit(EXIT_SUCCESS)

VOID     icalloc_mapPrint1();
VOID     icalloc_mapPrint2();
#define  icalloc_mapPrint()                      PL();NL();icalloc_mapPrint1();icalloc_mapPrint2()

#define  ierr_end(msg)                           P("Err: %s\n",msg);imain_end()

/////////////////////////////////////////////////////////////////////////////////////////
/*---------------------------------------------------------------------------------------
	Print関係
---------------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////
VOID     P(MBS *format,...);
VOID     QP(MBS *str);

#define  PL()                                    P("L%u\t",__LINE__)
#define  NL()                                    fputc('\n', stdout)
#define  LN()                                    P2(LINE)

#define  P0(str)                                 fputs(str, stdout)
#define  P2(str)                                 fputs(str, stdout);fputc('\n', stdout)
#define  P3(num)                                 P("%lld\n",(INT64)num)
#define  P4(num)                                 P("%.8lf\n",(DOUBLE)num)

#define  PL2(str)                                PL();P2(str)
#define  PL3(num)                                PL();P3(num)
#define  PL4(num)                                PL();P4(num)

WCS      *iws_conv_escape(WCS *str);

/////////////////////////////////////////////////////////////////////////////////////////
/*---------------------------------------------------------------------------------------
	MBS／WCS／U8N変換
---------------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////
WCS      *icnv_M2W(MBS *str);
#define  M2W(str)                                (WCS*)icnv_M2W(str)

U8N      *icnv_W2U(WCS *str);
#define  W2U(str)                                (U8N*)icnv_W2U(str)

WCS      *icnv_U2W(U8N *str);
#define  U2W(str)                                (WCS*)icnv_U2W(str)

MBS      *icnv_W2M(WCS *str);
#define  W2M(str)                                (MBS*)icnv_W2M(str)

/////////////////////////////////////////////////////////////////////////////////////////
/*---------------------------------------------------------------------------------------
	文字列処理
		"p" : return Pointer
		"s" : return String
		1byte     MBS : imp_, ims_, imn_
		1 & 2byte MBS : ijp_, ijs_, ijn_
		UTF-8     U8N : iup_, ius_, iun_
		UTF-16    WCS : iwp_, iws_, iwn_
---------------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////
UINT     imn_len(MBS *str);
UINT     iun_len(U8N *str);
UINT     iwn_len(WCS *str);

UINT     imn_cpy(MBS *to,MBS *from);
UINT     iwn_cpy(WCS *to,WCS *from);

UINT     iwn_pcpy(WCS *to,WCS *from1,WCS *from2);

WCS      *iws_clone(WCS *from);

WCS      *iws_pclone(WCS *from1,WCS *from2);

MBS      *ims_cats(UINT size,...);
WCS      *iws_cats(UINT size,...);

MBS      *ims_sprintf(MBS *format,...);
WCS      *iws_sprintf(WCS *format,...);

BOOL     iwb_cmp(WCS *str,WCS *search,BOOL perfect,BOOL icase);
#define  iwb_cmpf(str,search)                    (BOOL)iwb_cmp(str,search,FALSE,FALSE)
#define  iwb_cmpfi(str,search)                   (BOOL)iwb_cmp(str,search,FALSE,TRUE)
#define  iwb_cmpp(str,search)                    (BOOL)iwb_cmp(str,search,TRUE,FALSE)
#define  iwb_cmppi(str,search)                   (BOOL)iwb_cmp(str,search,TRUE,TRUE)
#define  iwb_cmp_leq(str,search,icase)           (BOOL)iwb_cmp(search,str,FALSE,icase)
#define  iwb_cmp_leqf(str,search)                (BOOL)iwb_cmp_leq(str,search,FALSE)
#define  iwb_cmp_leqfi(str,search)               (BOOL)iwb_cmp_leq(str,search,TRUE)

UINT     iwn_searchCntW(WCS *str,WCS *search,BOOL icase);
#define  iwn_searchCnt(str,search)               (UINT)iwn_searchCntW(str,search,FALSE)
#define  iwn_searchCnti(str,search)              (UINT)iwn_searchCntW(str,search,TRUE)

WCS      *iwp_searchLM(WCS *str,WCS *search,BOOL icase);
#define  iwp_searchL(str,search)                 (WCS*)iwp_searchLM(str,search,FALSE)
#define  iwp_searchLi(str,search)                (WCS*)iwp_searchLM(str,search,TRUE)

INT      icmpOperator_extractHead(MBS *str);
BOOL     icmpOperator_chk_INT64(INT64 i1,INT64 i2,INT operator);
BOOL     icmpOperator_chkDBL(DOUBLE d1,DOUBLE d2,INT operator);

WCS      **iwa_split(WCS *str,WCS *tokens, BOOL bRmEmpty);

WCS      *iws_addTokenNStr(WCS *str);

WCS      *iws_cutYenR(WCS *path);

WCS      *iws_replace(WCS *from,WCS *before,WCS *after,BOOL icase);

/////////////////////////////////////////////////////////////////////////////////////////
/*---------------------------------------------------------------------------------------
	数字関係
---------------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////
#define  inum_chkM(str)                          (BOOL)((*str>='0'&&*str<='9')||*str=='+'||*str=='-'||*str=='.'?TRUE:FALSE)
#define  inum_chk2M(str)                         (BOOL)(*str>='0'&&*str<='9'?TRUE:FALSE)
#define  inum_chkW(str)                          (BOOL)((*str>='0'&&*str<='9')||*str=='+'||*str=='-'||*str=='.'?TRUE:FALSE)
#define  inum_chk2W(str)                         (BOOL)(*str>='0'&&*str<='9'?TRUE:FALSE)
INT64    inum_wtoi(WCS *str);
DOUBLE   inum_wtof(WCS *str);

/* Copyright (C) 1997 - 2002,Makoto Matsumoto and Takuji Nishimura,All rights reserved.
	http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/MT2002/CODES/mt19937ar.c
	http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/mt.html
*/
VOID     MT_init(BOOL fixOn);
UINT     MT_genrand_UINT();
VOID     MT_free();

INT64    MT_irand_INT64(INT posMin,INT posMax);
DOUBLE   MT_irand_DBL(INT posMin,INT posMax,INT decRound);

/////////////////////////////////////////////////////////////////////////////////////////
/*---------------------------------------------------------------------------------------
	Array
---------------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////
UINT     iwa_size(WCS **ary);
UINT     iwa_len(WCS **ary);

INT      iwa_qsort_Asc(CONST VOID *arg1,CONST VOID *arg2);
INT      iwa_qsort_iAsc(CONST VOID *arg1,CONST VOID *arg2);
INT      iwa_qsort_Desc(CONST VOID *arg1,CONST VOID *arg2);
INT      iwa_qsort_iDesc(CONST VOID *arg1,CONST VOID *arg2);
#define  iwa_sort_Asc(ary)                       (VOID)qsort(ary,iwa_size(ary),sizeof(WCS*),iwa_qsort_Asc)
#define  iwa_sort_iAsc(ary)                      (VOID)qsort(ary,iwa_size(ary),sizeof(WCS*),iwa_qsort_iAsc)
#define  iwa_sort_Desc(ary)                      (VOID)qsort(ary,iwa_size(ary),sizeof(WCS*),iwa_qsort_Desc)
#define  iwa_sort_iDesc(ary)                     (VOID)qsort(ary,iwa_size(ary),sizeof(WCS*),iwa_qsort_iDesc)

WCS      *iwa_njoin(WCS **ary,WCS *token,UINT start,UINT count);
#define  iwa_join(ary,token)                     (WCS*)iwa_njoin(ary,token,0,iwa_size(ary))

WCS      **iwa_simplify(WCS **ary,BOOL icase);
WCS      **iwa_getDir(WCS **ary);
WCS      **iwa_higherDir(WCS **ary);

VOID     iwa_print(WCS **ary);

/////////////////////////////////////////////////////////////////////////////////////////
/*---------------------------------------------------------------------------------------
	File/Dir処理(WIN32_FIND_DATAA)
---------------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////
typedef struct
{
	WCS    fullnameW[IMAX_PATH]; // フルパス
	UINT   uFname;               // ファイル名開始位置
	UINT   uExt;                 // 拡張子開始位置
	UINT   uEnd;                 // 終端NULL位置 = フルパス長
	UINT   uAttr;                // 属性
	UINT   uFtype;               // 不明 = 0／Dir = 1／File = 2
	DOUBLE cjdCtime;             // 作成時間
	DOUBLE cjdMtime;             // 更新時間
	DOUBLE cjdAtime;             // アクセス時間
	INT64  iFsize;               // ファイルサイズ
}
$struct_iFinfoW;

$struct_iFinfoW *iFinfo_allocW();

VOID     iFinfo_clearW($struct_iFinfoW *FI);

BOOL     iFinfo_initW($struct_iFinfoW *FI,WIN32_FIND_DATAW *F,WCS *dir,UINT dirLenW,WCS *name);
BOOL     iFinfo_init2W($struct_iFinfoW *FI,WCS *path);

VOID     iFinfo_freeW($struct_iFinfoW *FI);

WCS      *iFinfo_attrToW(UINT uAttr);
INT      iFinfo_attrWtoINT(WCS *sAttr);

WCS      *iFinfo_ftypeToW(INT iFtype);

INT      iFinfo_depthW($struct_iFinfoW *FI);

INT64    iFinfo_fsizeW(WCS *Fn);

WCS      *iFinfo_ftimeToW(FILETIME ftime);
DOUBLE   iFinfo_ftimeToCjd(FILETIME ftime);

FILETIME iFinfo_ymdhnsToFtime(INT wYear,INT wMonth,INT wDay,INT wHour,INT wMinute,INT wSecond,BOOL reChk);

/////////////////////////////////////////////////////////////////////////////////////////
/*---------------------------------------------------------------------------------------
	File/Dir処理
---------------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////
BOOL     iFchk_existPathW(WCS *path);

INT      iFchk_typePathW(WCS *path);

BOOL     iFchk_BfileW(WCS *fn);
#define  iFchk_TfileW(fn)                        (BOOL)(iFchk_typePathW(fn) == 2 && !iFchk_BfileW(fn) ? TRUE : FALSE)

#define  ichk_attrDirFile(attr)                  (INT)(((INT)attr & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 2)

WCS      *iFget_extPathnameW(WCS *path,INT option);

WCS      *iFget_AdirW(WCS *path);
WCS      *iFget_RdirW(WCS *path);

BOOL     imk_dirW(WCS *path);
BOOL     imv_trashW(WCS *path,BOOL bFileOnly);

/////////////////////////////////////////////////////////////////////////////////////////
/*---------------------------------------------------------------------------------------
	Console
---------------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////
#define  iConsole_setTitleW(str)                 (VOID)SetConsoleTitleW(str)

VOID     iConsole_EscOn();

/////////////////////////////////////////////////////////////////////////////////////////
/*---------------------------------------------------------------------------------------
	暦
---------------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////
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
#define  CJD_TO_JD                               (DOUBLE)(0.5)
#define  CJD_TO_MJD                              (DOUBLE)(2400000.5-CJD_TO_JD)
#define  CJD_TO_LD                               (DOUBLE)(2299159.5-CJD_TO_JD)

#define  CJD_FORMAT                              L"%.8f"

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
#define  idate_cjd_print(CJD)                    P(CJD_FORMAT,CJD)

// CJD通日から、JD通日を返す
#define  idate_cjdToJd(CJD)                      (DOUBLE)CJD-CJD_TO_JD
#define  idate_cjdToJd_print(CJD)                P(CJD_FORMAT,idate_cjdToJd(CJD))

// JD通日から、CJD通日を返す
#define  idate_jdToCjd(JD)                       (DOUBLE)JD+CJD_TO_JD
#define  idate_jdToCjd_print(JD)                 P(CJD_FORMAT,idate_jdToCjd(JD))

// CJD通日から、MJD通日を返す
#define  idate_cjdToMjd(CJD)                     (DOUBLE)CJD-CJD_TO_MJD
#define  idate_cjdToMjd_print(CJD)               P(CJD_FORMAT,idate_cjdToMjd(CJD))

// MJD通日から、CJD通日を返す
#define  idate_mjdToCjd(MJD)                     (DOUBLE)MJD+CJD_TO_MJD
#define  idate_mjdToCjd_print(MJD)               P(CJD_FORMAT,idate_mjdToCjd(MJD))

// CJD通日から、LD通日を返す
#define  idate_cjdToLd(CJD)                      (DOUBLE)CJD-CJD_TO_LD
#define  idate_cjdToLd_print(CJD)                P(CJD_FORMAT,idate_cjdToLd(CJD))

// LD通日から、CJD通日を返す
#define  idate_ldToCjd(LD)                       (DOUBLE)LD+CJD_TO_LD
#define  idate_ldToCjd_print(LD)                 P(CJD_FORMAT,idate_ldToCjd(LD))

BOOL     idate_chk_uruu(INT i_y);

INT      *idate_cnv_month(INT i_y,INT i_m,INT from_m,INT to_m);
// 1-12月
#define  idate_cnv_month1(i_y,i_m)               (INT*)idate_cnv_month(i_y,i_m,1,12)
// 0-11月
#define  idate_cnv_month2(i_y,i_m)               (INT*)idate_cnv_month(i_y,i_m,0,11)

INT      idate_month_end(INT i_y,INT i_m);
BOOL     idate_chk_month_end(INT i_y,INT i_m,INT i_d,BOOL reChk);

INT      *idate_WCS_to_iAryYmdhns(WCS *str);

INT      idate_ymd_num(INT i_y,INT i_m,INT i_d);
DOUBLE   idate_ymdhnsToCjd(INT i_y,INT i_m,INT i_d,INT i_h,INT i_n,INT i_s);

INT      *idate_cjd_to_iAryHns(DOUBLE cjd);
INT      *idate_cjd_to_iAryYmdhns(DOUBLE cjd);

INT      *idate_reYmdhns(INT i_y,INT i_m,INT i_d,INT i_h,INT i_n,INT i_s);

INT      idate_cjd_iWday(DOUBLE cjd);
WCS      *idate_cjd_Wday(DOUBLE cjd);

FILETIME idate_cjdToFtime(DOUBLE cjd);

// 年内の通算日
INT      idate_cjd_yeardays(DOUBLE cjd);
// cjd1 - cjd2 の通算日
#define  idate_cjd_days(cjd1,cjd2)               (INT)((INT)cjd2-(INT)cjd1)

// 年内の通算週
#define  idate_cjd_yearweeks(cjd)                (INT)((6+idate_cjd_yeardays(cjd))/7)
// cjd1 - cjd2 の通算週
#define  idate_cjd_weeks(cjd1,cjd2)              (INT)((idate_cjd_days(cjd1,cjd2)+6)/7)

// [6]={y,m,d,h,n,s}
INT      *idate_add(INT i_y,INT i_m,INT i_d,INT i_h,INT i_n,INT i_s,INT add_y,INT add_m,INT add_d,INT add_h,INT add_n,INT add_s);
// [8]={sign,y,m,d,h,n,s,days}
INT      *idate_diff(INT i_y1,INT i_m1,INT i_d1,INT i_h1,INT i_n1,INT i_s1,INT i_y2,INT i_m2,INT i_d2,INT i_h2,INT i_n2,INT i_s2);

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
WCS      *idate_format_diff(WCS *format,INT i_sign,INT i_y,INT i_m,INT i_d,INT i_h,INT i_n,INT i_s,INT64 i_days);
#define  idate_format_ymdhns(format,i_y,i_m,i_d,i_h,i_n,i_s)         (WCS*)idate_format_diff(format,0,i_y,i_m,i_d,i_h,i_n,i_s,0)

WCS      *idate_format_cjdToW(WCS *format,DOUBLE cjd);

WCS      *idate_replace_format_ymdhns(WCS *str,WCS *quote1,WCS *quote2,WCS *add_quote,INT i_y,INT i_m,INT i_d,INT i_h,INT i_n,INT i_s);
#define  idate_format_nowToYmdhns(i_y,i_m,i_d,i_h,i_n,i_s)           (WCS*)idate_replace_format_ymdhns(L"[]",L"[",L"]","",i_y,i_m,i_d,i_h,i_n,i_s)

INT      *idate_now_to_iAryYmdhns(BOOL area);
#define  idate_now_to_iAryYmdhns_localtime()     (INT*)idate_now_to_iAryYmdhns(TRUE)
#define  idate_now_to_iAryYmdhns_systemtime()    (INT*)idate_now_to_iAryYmdhns(FALSE)

DOUBLE   idate_nowToCjd(BOOL area);
#define  idate_nowToCjd_localtime()              (DOUBLE)idate_nowToCjd(TRUE)
#define  idate_nowToCjd_systemtime()             (DOUBLE)idate_nowToCjd(FALSE)

#define  idate_cjd_sec(cjd)                      (DOUBLE)(cjd)*86400.0
