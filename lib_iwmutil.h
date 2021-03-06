/////////////////////////////////////////////////////////////////////////////////////////
#define  LIB_IWMUTIL_VERSION   "lib_iwmutil_20220526"
#define  LIB_IWMUTIL_COPYLIGHT "Copyright (C)2008-2022 iwm-iwama"
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
	¤Êè
---------------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////
#define  IMAX_PATH                               (((MAX_PATH>>3)<<3)+(1<<3)) // windef.hQÆ

#define  MBS                                     CHAR  // imx_xxx()^MBCS^Muliti Byte String
#define  WCS                                     WCHAR // iwx_xxx()^UTF-16^Wide Char String
#define  U8N                                     CHAR  // iux_xxx()^UTF-8^UTF-8 NoBOM

#define  NULL_DEVICE                             "NUL" // "/dev/null"

#define  ISO_FORMAT_DATE                         "%.4d-%02d-%02d" // "yyyy-mm-dd"
#define  ISO_FORMAT_TIME                         "%02d:%02d:%02d" // "hh:nn:ss"
#define  ISO_FORMAT_DATETIME                     "%.4d-%02d-%02d %02d:%02d:%02d"

#define  IDATE_FORMAT_STD                        "%G%y-%m-%d %h:%n:%s"
#define  IDATE_FORMAT_DIFF                       "%g%y-%m-%d %h:%n:%s"

#define  LINE                                    "--------------------------------------------------------------------------------"

/////////////////////////////////////////////////////////////////////////////////////////
/*---------------------------------------------------------------------------------------
	åæÏ
---------------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////
extern   MBS    *$CMD;         // R}h¼ði[
extern   UINT   $ARGC;         // øzñ
extern   MBS    **$ARGV;       // øzñ
extern   MBS    **$ARGS;       // $ARGV©ç_uNH[e[VðÁµ½àÌ
extern   HANDLE $StdoutHandle; // æÊ§äpnh
extern   UINT   $ExecSecBgn;   // ÀsJnÔ

/////////////////////////////////////////////////////////////////////////////////////////
/*---------------------------------------------------------------------------------------
	ÀsJnÔ
---------------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////
UINT     iExecSec(CONST UINT microSec);
#define  iExecSec_init()                         (UINT)iExecSec(0)
#define  iExecSec_next()                         (DOUBLE)(iExecSec($ExecSecBgn))/1000

/////////////////////////////////////////////////////////////////////////////////////////
/*---------------------------------------------------------------------------------------
	mÛ
---------------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////
VOID     *icalloc(UINT n,UINT size,BOOL aryOn);
VOID     *irealloc(VOID *ptr,UINT n,UINT size);

/*
	MBS^[] = [0...] + NULL
	(á) MBS[]=[0]"ABC",[1]"DEF",[2]NULL
*/
#define  icalloc_MBS(n)                          (MBS*)icalloc(n,sizeof(MBS),FALSE)
#define  irealloc_MBS(pM,n)                      (MBS*)irealloc(pM,n,sizeof(MBS))

#define  icalloc_MBS_ary(n)                      (MBS**)icalloc(n,sizeof(MBS*),TRUE)
#define  irealloc_MBS_ary(pM,n)                  (MBS**)irealloc(pM,n,sizeof(MBS*))

#define  icalloc_WCS(n)                          (WCS*)icalloc(n,sizeof(WCS),FALSE)
#define  irealloc_WCS(pW,n)                      (WCS*)irealloc(pW,n,sizeof(WCS))

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
	PrintÖW
---------------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////
VOID     P(MBS *format,...);
VOID     QP(MBS *pM);

#define  PL()                                    P("L%u\t",__LINE__)
#define  PP(pM)                                  P("[%p] ",pM)
#define  PX(pM)                                  P("|%#hx|",*pM)
#define  NL()                                    fputc('\n', stdout)
#define  LN()                                    P2(LINE)

#define  P0(pM)                                  fputs(pM, stdout)
#define  P2(pM)                                  fputs(pM, stdout);fputc('\n', stdout)
#define  P3(num)                                 P("%lld\n",(INT64)num)
#define  P4(num)                                 P("%.8lf\n",(DOUBLE)num)

#define  P22(pM1,pM2)                            P("%s%s\n", pM1, pM2)
#define  P23(pM,num)                             P("%s%lld\n",pM,(INT64)num)
#define  P24(pM,num)                             P("%s%.8lf\n",pM,(DOUBLE)num)

#define  PL2(pM)                                 PL();P2(pM)
#define  PL3(num)                                PL();P3(num)
#define  PL4(num)                                PL();P4(num)

#define  PL23(pM,num)                            PL();P("%s%lld\n",pM,(INT64)num)
#define  PL24(pM,num)                            PL();P("%s%.8lf\n",pM,(DOUBLE)num)
#define  PL32(num,pM)                            PL();P("[%lld]%s\n",(INT64)num,pM)

MBS      *ims_conv_escape(MBS *pM);
MBS      *ims_sprintf(MBS *format,...);

/////////////////////////////////////////////////////////////////////////////////////////
/*---------------------------------------------------------------------------------------
	MBS^WCS^U8NÏ·
---------------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////
WCS      *icnv_M2W(MBS *pM);
#define  M2W(pM)                                 (WCS*)icnv_M2W(pM)

U8N      *icnv_W2U(WCS *pW);
#define  W2U(pW)                                 (U8N*)icnv_W2U(pW)

U8N      *icnv_M2U(MBS *pM);
#define  M2U(pM)                                 (U8N*)icnv_M2U(pM)

WCS      *icnv_U2W(U8N *pU);
#define  U2W(pU)                                 (WCS*)icnv_U2W(pU)

MBS      *icnv_W2M(WCS *pW);
#define  W2M(pW)                                 (MBS*)icnv_W2M(pW)

MBS      *icnv_U2M(U8N *pU);
#define  U2M(pU)                                 (MBS*)icnv_U2M(pU)

/////////////////////////////////////////////////////////////////////////////////////////
/*---------------------------------------------------------------------------------------
	¶ñ
		"p" : return Pointer
		"s" : return String
		1byte     MBS : imp_, imp_, imn_
		1 & 2byte MBS : ijp_, ijs_, ijn_
		UTF-8     U8N : iup_, ius_, iun_
		UTF-16    WCS : iwp_, iws_, iwn_
---------------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////
UINT     imn_len(MBS *pM);
UINT     ijn_len(MBS *pM);
UINT     iun_len(U8N *pU);
UINT     iwn_len(WCS *pW);

MBS      *ijp_forwardN(MBS *pM,UINT sizeJ);
U8N      *iup_forwardN(U8N *pU,UINT sizeU);

MBS      *ims_UpperLower(MBS *pM,INT option);
#define  ims_upper(pM)                           (MBS*)ims_UpperLower(pM,1)
#define  ims_lower(pM)                           (MBS*)ims_UpperLower(pM,2)

UINT     imn_cpy(MBS *to,MBS *from);
UINT     imn_pcpy(MBS *to,MBS *from1,MBS *from2);

UINT     iwn_cpy(WCS *to,WCS *from);

MBS      *ims_clone(MBS *from);
WCS      *iws_clone(WCS *from);

MBS      *ims_pclone(MBS *from1,MBS *from2);
#define  ims_nclone(from1,size)                  (MBS*)ims_pclone(from1,(from1+size))

MBS      *ims_cats(UINT size,...);

BOOL     imb_cmp(MBS *pM,MBS *search,BOOL perfect,BOOL icase);
#define  imb_cmpf(pM,search)                     (BOOL)imb_cmp(pM,search,FALSE,FALSE)
#define  imb_cmpfi(pM,search)                    (BOOL)imb_cmp(pM,search,FALSE,TRUE)
#define  imb_cmpp(pM,search)                     (BOOL)imb_cmp(pM,search,TRUE,FALSE)
#define  imb_cmppi(pM,search)                    (BOOL)imb_cmp(pM,search,TRUE,TRUE)
#define  imb_cmp_leq(pM,search,icase)            (BOOL)imb_cmp(search,pM,FALSE,icase)
#define  imb_cmp_leqf(pM,search)                 (BOOL)imb_cmp_leq(pM,search,FALSE)
#define  imb_cmp_leqfi(pM,search)                (BOOL)imb_cmp_leq(pM,search,TRUE)

BOOL     iwb_cmp(WCS *pW,WCS *search,BOOL perfect,BOOL icase);
#define  iwb_cmpf(pW,search)                     (BOOL)iwb_cmp(pW,search,FALSE,FALSE)
#define  iwb_cmpfi(pW,search)                    (BOOL)iwb_cmp(pW,search,FALSE,TRUE)
#define  iwb_cmpp(pW,search)                     (BOOL)iwb_cmp(pW,search,TRUE,FALSE)
#define  iwb_cmppi(pW,search)                    (BOOL)iwb_cmp(pW,search,TRUE,TRUE)
#define  iwb_cmp_leq(pW,search,icase)            (BOOL)iwb_cmp(search,pW,FALSE,icase)
#define  iwb_cmp_leqf(pW,search)                 (BOOL)iwb_cmp_leq(pW,search,FALSE)
#define  iwb_cmp_leqfi(pW,search)                (BOOL)iwb_cmp_leq(pW,search,TRUE)

MBS      *ijp_bypass(MBS *pM,MBS *from,MBS *to);

UINT     ijn_searchCntA(MBS *pM,MBS *search,BOOL icase);
#define  ijn_searchCnt(pM,search)                (UINT)ijn_searchCntA(pM,search,FALSE)
#define  ijn_searchCnti(pM,search)               (UINT)ijn_searchCntA(pM,search,TRUE)

UINT     iwn_searchCntW(WCS *pW,WCS *search,BOOL icase);
#define  iwn_searchCnt(pW,search)                (UINT)iwn_searchCntW(pW,search,FALSE)
#define  iwn_searchCnti(pW,search)               (UINT)iwn_searchCntW(pW,search,TRUE)

UINT     ijn_searchCntLA(MBS *pM,MBS *search,BOOL icase,INT option);
#define  ijn_searchCntL(pM,search)               (UINT)ijn_searchCntLA(pM,search,FALSE,0)
#define  ijn_searchCntLi(pM,search)              (UINT)ijn_searchCntLA(pM,search,TRUE,0)
#define  imn_searchLenL(pM,search)               (UINT)ijn_searchCntLA(pM,search,FALSE,1)
#define  imn_searchLenLi(pM,search)              (UINT)ijn_searchCntLA(pM,search,TRUE,1)
#define  ijn_searchLenL(pM,search)               (UINT)ijn_searchCntLA(pM,search,FALSE,2)
#define  ijn_searchLenLi(pM,search)              (UINT)ijn_searchCntLA(pM,search,TRUE,2)

UINT     ijn_searchCntRA(MBS *pM,MBS *search,BOOL icase,INT option);
#define  ijn_searchCntR(pM,search)               (UINT)ijn_searchCntRA(pM,search,FALSE,0)
#define  ijn_searchCntRi(pM,search)              (UINT)ijn_searchCntRA(pM,search,TRUE,0)
#define  imn_searchLenR(pM,search)               (UINT)ijn_searchCntRA(pM,search,FALSE,1)
#define  imn_searchLenRi(pM,search)              (UINT)ijn_searchCntRA(pM,search,TRUE,1)
#define  ijn_searchLenR(pM,search)               (UINT)ijn_searchCntRA(pM,search,FALSE,2)
#define  ijn_searchLenRi(pM,search)              (UINT)ijn_searchCntRA(pM,search,TRUE,2)

MBS      *ijp_searchLA(MBS *pM,MBS *search,BOOL icase);
#define  ijp_searchL(pM,search)                  (MBS*)ijp_searchLA(pM,search,FALSE)
#define  ijp_searchLi(pM,search)                 (MBS*)ijp_searchLA(pM,search,TRUE)

INT      icmpOperator_extractHead(MBS *pM);
MBS      *icmpOperator_toHeadA(INT operator);
BOOL     icmpOperator_chk_INT64(INT64 i1,INT64 i2,INT operator);
BOOL     icmpOperator_chkDBL(DOUBLE d1,DOUBLE d2,INT operator);

MBS      **ija_split(MBS *pM,MBS *tokensM);
MBS      **ija_split_zero(MBS *pM);

MBS      *ijs_rm_quote(MBS *pM,MBS *quoteS,MBS *quoteE,BOOL icase,BOOL one_to_one);

MBS      *ims_addTokenNStr(MBS *pM);

MBS      *ijs_cut(MBS *pM,MBS *rmLs,MBS *rmRs);
#define  ijs_cutL(pM,rmLs)                       (MBS*)ijs_cut(pM,rmLs,NULL)
#define  ijs_cutR(pM,rmRs)                       (MBS*)ijs_cut(pM,NULL,rmRs)

MBS      *ijs_cutAry(MBS *pM,MBS **aryLs,MBS **aryRs);
MBS      *ijs_trim(MBS *pM);
MBS      *ijs_trimL(MBS *pM);
MBS      *ijs_trimR(MBS *pM);

MBS      *ijs_chomp(MBS *pM);

MBS      *ijs_replace(MBS *from,MBS *before,MBS *after,BOOL icase);
WCS      *iws_replace(WCS *from,WCS *before,WCS *after,BOOL icase);

BOOL     imb_shiftL(MBS *pM,UINT byte);
BOOL     imb_shiftR(MBS *pM,UINT byte);

/////////////////////////////////////////////////////////////////////////////////////////
/*---------------------------------------------------------------------------------------
	ÖW
---------------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////
#define  inum_chkM(pM)                           (BOOL)((*pM>='0'&&*pM<='9')||*pM=='+'||*pM=='-'||*pM=='.'?TRUE:FALSE)
#define  inum_chk2M(pM)                          (BOOL)(*pM>='0'&&*pM<='9'?TRUE:FALSE)
INT64    inum_atoi(MBS *pM);
DOUBLE   inum_atof(MBS *pM);

/*yCopyright (C) 1997 - 2002,Makoto Matsumoto and Takuji Nishimura,All rights reserved.z
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
	Command Line
---------------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////
VOID     iCLI_getCommandLine();
MBS      *iCLI_getOptValue(UINT argc,MBS *opt1,MBS *opt2);
BOOL     iCLI_getOptMatch(UINT argc,MBS *opt1,MBS *opt2);

/////////////////////////////////////////////////////////////////////////////////////////
/*---------------------------------------------------------------------------------------
	Array
---------------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////
MBS      **ima_null();

UINT     iary_size(MBS **ary);

UINT     iary_Mlen(MBS **ary);
UINT     iary_Jlen(MBS **ary);

INT      iary_qsort_cmp(CONST VOID *p1,CONST VOID *p2,BOOL asc);
INT      iary_qsort_cmpAsc(CONST VOID *p1,CONST VOID *p2);
INT      iary_qsort_cmpDesc(CONST VOID *p1,CONST VOID *p2);
#define  iary_sortAsc(ary)                       (VOID)qsort((MBS*)ary,iary_size(ary),sizeof(MBS**),iary_qsort_cmpAsc)
#define  iary_sortDesc(ary)                      (VOID)qsort((MBS*)ary,iary_size(ary),sizeof(MBS**),iary_qsort_cmpDesc)

MBS      *iary_njoin(MBS **ary,MBS *token,UINT start,UINT count);
#define  iary_join(ary,token)                    (MBS*)iary_njoin(ary,token,0,iary_size(ary))

MBS      **iary_simplify(MBS **ary,BOOL icase);
MBS      **iary_higherDir(MBS **ary);

VOID     iary_print(MBS **ary);

/////////////////////////////////////////////////////////////////////////////////////////
/*---------------------------------------------------------------------------------------
	File/Dir(WIN32_FIND_DATAA)
---------------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////
typedef struct
{
	MBS    fullnameA[IMAX_PATH]; // tpX
	UINT   uFname;               // t@C¼JnÊu
	UINT   uExt;                 // g£qJnÊu
	UINT   uEnd;                 // I[NULLÊu = tpX·
	UINT   uAttr;                // ®«
	UINT   uFtype;               // s¾ = 0, Dir = 1, File = 2
	DOUBLE cjdCtime;             // ì¬Ô
	DOUBLE cjdMtime;             // XVÔ
	DOUBLE cjdAtime;             // ANZXÔ
	INT64  iFsize;               // t@CTCY
}
$struct_iFinfoA;

typedef struct
{
	WCS    fullnameW[IMAX_PATH]; // tpX
	UINT   uFname;               // t@C¼JnÊu
	UINT   uExt;                 // g£qJnÊu
	UINT   uEnd;                 // I[NULLÊu = tpX·
	UINT   uAttr;                // ®«
	UINT   uFtype;               // s¾ = 0^Dir = 1^File = 2
	DOUBLE cjdCtime;             // ì¬Ô
	DOUBLE cjdMtime;             // XVÔ
	DOUBLE cjdAtime;             // ANZXÔ
	INT64  iFsize;               // t@CTCY
}
$struct_iFinfoW;

$struct_iFinfoA *iFinfo_allocA();
$struct_iFinfoW *iFinfo_allocW();

VOID     iFinfo_clearA($struct_iFinfoA *FI);
VOID     iFinfo_clearW($struct_iFinfoW *FI);

BOOL     iFinfo_initA($struct_iFinfoA *FI,WIN32_FIND_DATAA *F,MBS *dir,UINT dirLenA,MBS *name);
BOOL     iFinfo_initW($struct_iFinfoW *FI,WIN32_FIND_DATAW *F,WCS *dir,UINT dirLenW,WCS *name);
BOOL     iFinfo_init2A($struct_iFinfoA *FI,MBS *path);

VOID     iFinfo_freeA($struct_iFinfoA *FI);
VOID     iFinfo_freeW($struct_iFinfoW *FI);

MBS      *iFinfo_attrToA(UINT uAttr);
INT      iFinfo_attrAtoINT(MBS *sAttr);

MBS      *iFinfo_ftypeToA(INT iFtype);

INT      iFinfo_depthA($struct_iFinfoA *FI);
INT      iFinfo_depthW($struct_iFinfoW *FI);

INT64    iFinfo_fsize(MBS *Fn);

MBS      *iFinfo_ftimeToA(FILETIME ftime);
DOUBLE   iFinfo_ftimeToCjd(FILETIME ftime);

FILETIME iFinfo_ymdhnsToFtime(INT wYear,INT wMonth,INT wDay,INT wHour,INT wMinute,INT wSecond,BOOL reChk);

/////////////////////////////////////////////////////////////////////////////////////////
/*---------------------------------------------------------------------------------------
	File/Dir
---------------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////
BOOL     iFchk_existPathA(MBS *path);

INT      iFchk_typePathA(MBS *path);

BOOL     iFchk_Bfile(MBS *fn);
#define  iFchk_Tfile(fn)                         (BOOL)(iFchk_typePathA(fn) == 2 && !iFchk_Bfile(fn) ? TRUE : FALSE)

#define  ichk_attrDirFile(attr)                  (INT)(((INT)attr & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 2)

MBS      *iFget_extPathname(MBS *path,INT option);

MBS      *iFget_AdirA(MBS *path);
MBS      *iFget_RdirA(MBS *path);

BOOL     imk_dir(MBS *path);

/////////////////////////////////////////////////////////////////////////////////////////
/*---------------------------------------------------------------------------------------
	Console
---------------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////
#define  iConsole_setTitle(pM)                   (VOID)SetConsoleTitleA(pM)

VOID     iConsole_EscOn();

/////////////////////////////////////////////////////////////////////////////////////////
/*---------------------------------------------------------------------------------------
	ï
---------------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////
/*
	Í¶ßÉ
		EXï     : "B.C.4713-01-01 12:00" - "A.C.1582-10-04 23:59"
		OSEXï : "A.C.1582-10-15 00:00" - »Ý

		ÀïÍãLÌÆ¨è¾ªAEXïÈOà(EXïÉ¥è)vZÂ\Å éB
		wB.C.ïxÌæèµ¢ÉÂ¢ÄÍAãqw¼B.C.ïxÉ¥éB
		È¨Ah¶ïÉÂ¢ÄÍAãqweïÌÏxðgpÌ±ÆB

	¼B.C.ï
		(Àï) "-4713-01-01" - "-0001-12-31"
		(¼)   "-4712-01-01" - "+0000-12-31" // Àð+PN

	eïÌÏ
		¦CJDðîÉvZB
		JD  : Julian Day               :-4712-01-01 12:00:00Jn
		CJD : Chronological Julian Day :-4712-01-01 00:00:00Jn :JD-0.5
		MJD : Modified Julian Day      : 1858-11-17 00:00:00Jn :JD-2400000.5
		LD  : Lilian Day               : 1582-10-15 00:00:00Jn :JD-2299159.5
*/
#define  CJD_TO_JD                               (DOUBLE)(0.5)
#define  CJD_TO_MJD                              (DOUBLE)(2400000.5-CJD_TO_JD)
#define  CJD_TO_LD                               (DOUBLE)(2299159.5-CJD_TO_JD)

#define  CJD_FORMAT                              "%.8f"

/*
	CJDï(=JDï-0.5)ÌÅIú NS_before[]
	NSï (=GDï)    ÌJnú NS_after[]

	JDïÍ{ABC.4713-1-1 12:00ðN_Æ·éªA
	vZãA00:00ðN_(=CJDï)ÆµÄµ¤.
	<cf> idate_jdToCjd(JD)

	N_ÍAÉæÁÄá¤
	<ITALY>
		CJD:2299160	YMD:1582-10-04
		CJD:2299161	YMD:1582-10-15

	<ENGLAND>
		CJD:2361221	YMD:1752-09-02
		CJD:2361222	YMD:1752-09-14
*/

// CJDÊú
#define  idate_cjd_print(CJD)                    P(CJD_FORMAT,CJD)

// CJDÊú©çAJDÊúðÔ·
#define  idate_cjdToJd(CJD)                      (DOUBLE)CJD-CJD_TO_JD
#define  idate_cjdToJd_print(CJD)                P(CJD_FORMAT,idate_cjdToJd(CJD))

// JDÊú©çACJDÊúðÔ·
#define  idate_jdToCjd(JD)                       (DOUBLE)JD+CJD_TO_JD
#define  idate_jdToCjd_print(JD)                 P(CJD_FORMAT,idate_jdToCjd(JD))

// CJDÊú©çAMJDÊúðÔ·
#define  idate_cjdToMjd(CJD)                     (DOUBLE)CJD-CJD_TO_MJD
#define  idate_cjdToMjd_print(CJD)               P(CJD_FORMAT,idate_cjdToMjd(CJD))

// MJDÊú©çACJDÊúðÔ·
#define  idate_mjdToCjd(MJD)                     (DOUBLE)MJD+CJD_TO_MJD
#define  idate_mjdToCjd_print(MJD)               P(CJD_FORMAT,idate_mjdToCjd(MJD))

// CJDÊú©çALDÊúðÔ·
#define  idate_cjdToLd(CJD)                      (DOUBLE)CJD-CJD_TO_LD
#define  idate_cjdToLd_print(CJD)                P(CJD_FORMAT,idate_cjdToLd(CJD))

// LDÊú©çACJDÊúðÔ·
#define  idate_ldToCjd(LD)                       (DOUBLE)LD+CJD_TO_LD
#define  idate_ldToCjd_print(LD)                 P(CJD_FORMAT,idate_ldToCjd(LD))

BOOL     idate_chk_uruu(INT i_y);

INT      *idate_cnv_month(INT i_y,INT i_m,INT from_m,INT to_m);
// 1-12
#define  idate_cnv_month1(i_y,i_m)               (INT*)idate_cnv_month(i_y,i_m,1,12)
// 0-11
#define  idate_cnv_month2(i_y,i_m)               (INT*)idate_cnv_month(i_y,i_m,0,11)

INT      idate_month_end(INT i_y,INT i_m);
BOOL     idate_chk_month_end(INT i_y,INT i_m,INT i_d,BOOL reChk);

DOUBLE   idate_MBStoCjd(MBS *pM);

MBS      **idate_MBS_to_mAryYmdhns(MBS *pM);
INT      *idate_MBS_to_iAryYmdhns(MBS *pM);

INT      idate_ymd_num(INT i_y,INT i_m,INT i_d);
DOUBLE   idate_ymdhnsToCjd(INT i_y,INT i_m,INT i_d,INT i_h,INT i_n,INT i_s);

INT      *idate_cjd_to_iAryHns(DOUBLE cjd);
INT      *idate_cjd_to_iAryYmdhns(DOUBLE cjd);

INT      *idate_reYmdhns(INT i_y,INT i_m,INT i_d,INT i_h,INT i_n,INT i_s);

INT      idate_cjd_iWday(DOUBLE cjd);
MBS      *idate_cjd_mWday(DOUBLE cjd);
FILETIME idate_cjdToFtime(DOUBLE cjd);

// NàÌÊZú
INT      idate_cjd_yeardays(DOUBLE cjd);
// cjd1 - cjd2 ÌÊZú
#define  idate_cjd_days(cjd1,cjd2)               (INT)((INT)cjd2-(INT)cjd1)

// NàÌÊZT
#define  idate_cjd_yearweeks(cjd)                (INT)((6+idate_cjd_yeardays(cjd))/7)
// cjd1 - cjd2 ÌÊZT
#define  idate_cjd_weeks(cjd1,cjd2)              (INT)((idate_cjd_days(cjd1,cjd2)+6)/7)

// [6]={y,m,d,h,n,s}
INT      *idate_add(INT i_y,INT i_m,INT i_d,INT i_h,INT i_n,INT i_s,INT add_y,INT add_m,INT add_d,INT add_h,INT add_n,INT add_s);
// [8]={sign,y,m,d,h,n,s,days}
INT      *idate_diff(INT i_y1,INT i_m1,INT i_d1,INT i_h1,INT i_n1,INT i_s1,INT i_y2,INT i_m2,INT i_d2,INT i_h2,INT i_n2,INT i_s2);

/// VOID idate_diff_checker(INT from_year,INT to_year,INT repeat);

/*
// Ymdhns
	%a : jú(á:Su)
	%A : jú
	%c : NàÌÊZú
	%C : CJDÊZú
	%J : JDÊZú
	%e : NàÌÊZT

// Diff
	%Y : ÊZN
	%M : ÊZ
	%D : ÊZú
	%H : ÊZ
	%N : ÊZª
	%S : ÊZb
	%W : ÊZT
	%w : ÊZTÌ]ú

// ¤Ê
	%g : Sign "-" "+"
	%G : Sign "-" ÌÝ
	%y : N(0000)
	%m : (00)
	%d : ú(00)
	%h : (00)
	%n : ª(00)
	%s : b(00)
	%% : "%"
	\a
	\n
	\t
*/
MBS      *idate_format_diff(MBS *format,INT i_sign,INT i_y,INT i_m,INT i_d,INT i_h,INT i_n,INT i_s,INT64 i_days);
#define  idate_format_ymdhns(format,i_y,i_m,i_d,i_h,i_n,i_s)         (MBS*)idate_format_diff(format,0,i_y,i_m,i_d,i_h,i_n,i_s,0)

MBS      *idate_format_iAryToA(MBS *format,INT *ymdhns);
MBS      *idate_format_cjdToA(MBS *format,DOUBLE cjd);

MBS      *idate_replace_format_ymdhns(MBS *pM,MBS *quote1,MBS *quote2,MBS *add_quote,INT i_y,INT i_m,INT i_d,INT i_h,INT i_n,INT i_s);
#define  idate_format_nowToYmdhns(i_y,i_m,i_d,i_h,i_n,i_s)           (MBS*)idate_replace_format_ymdhns("[]","[","]","",i_y,i_m,i_d,i_h,i_n,i_s)

INT      *idate_now_to_iAryYmdhns(BOOL area);
#define  idate_now_to_iAryYmdhns_localtime()     (INT*)idate_now_to_iAryYmdhns(TRUE)
#define  idate_now_to_iAryYmdhns_systemtime()    (INT*)idate_now_to_iAryYmdhns(FALSE)

DOUBLE   idate_nowToCjd(BOOL area);
#define  idate_nowToCjd_localtime()              (DOUBLE)idate_nowToCjd(TRUE)
#define  idate_nowToCjd_systemtime()             (DOUBLE)idate_nowToCjd(FALSE)

#define  idate_cjd_sec(cjd)                      (DOUBLE)(cjd)*86400.0
