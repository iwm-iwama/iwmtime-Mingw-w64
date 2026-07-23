/*
## 1. 基本構文
```
#include "lib_iwmutil2.h"

INT main()
{
  // 1. 開始処理
  imain_begin(); // imain_end() とセット

  // 2. 大域変数リスト／デバッグ用
  iCLI_VarList();

  //--------------------------------------------------------------
  // 3. 主処理をここに書く
  //--------------------------------------------------------------
  // 3.1. 個別にヒープ管理するとき
  $struct_HeapManager *pMgr = iHM_begin(); // iHM_end() とセット
	// ヒープ管理情報／デバッグ用
	idebug_map(pMgr);
  iHM_end(pMgr); // 必ず!!
  //--------------------------------------------------------------

  // 4. 処理時間／デバッグ用
  P("-- %.3f sec\n\n", iExecSec_next());

  // 5. ヒープ管理情報／デバッグ用
  idebug_map(NULL);

  // 6. 終了処理
  imain_end(); // 必ず!!
}
```

### 1.1. コンパイル
- ソースファイル: aaa.c
- 実行ファイル  : aaa.exe

```
コンパイル例:
$ gcc.exe -std=c23 aaa.c lib_iwmutil2.a -Os -Wall -Wextra -lgdi32 -luser32 -lshlwapi -o aaa.exe
```

```
ファイルサイズ削減:
$ strip aaa.exe
```

### 1.2. 参考ファイル
- lib_iwmutil2.c_make.bat
- iwmhello.c, iwmhello.c_make.bat

---

## 2. スレッドモデル
```
メインスレッド：
┌───────────┐
│imain_begin()         │
│icalloc_begin()       │
│ ↓                   │
│$icallocManager       │← メインになるヒープ管理構造体
│ ↓                   │
│icalloc / ifree       │← 原則、手動解放
│ ↓                   │
│icalloc_end()         │← 自動解放
│imain_end()           │
└───────────┘

サブスレッド：
┌───────────┐
│pMgr = iHM_begin()    │← 任意のヒープ管理構造体
│ ↓                   │
│iHM_calloc / iHM_free │← 原則、手動解放
│ ↓                   │
│iHM_end(pMgr)         │← 自動解放
└───────────┘
```

### 2.1. iHM_calloc の動作フロー
```
iHM_calloc()
  │
  ├─ calloc 窓口（ppTarget）
  │
  ├─ calloc 実体（pRealData）
  │
  ├─ *ppTarget = pRealData
  │
  ├─ 管理情報（pMap[uEOD]）に登録
  │
  └─ return pRealData
```

### 2.2. iHM_free の動作フロー
```
iHM_free(ptr)
  │
  ├─ ヒープ管理（pMgr->pMap）を後方から検索
  │
  ├─ ppTarget = pMap[i].ptr
  │
  ├─ pRealData = *ppTarget
  │
  ├─ memZero(pRealData)
  ├─ free(pRealData)
  │
  ├─ memZero(ppTarget)
  ├─ free(ppTarget)
  │
  ├─ memZero(pMap[i])
  │
  └─ return TRUE
```

✔ free の順序が常に一定
  実体（pRealData）→窓口（ppTarget）→管理情報（pMap[i]）

### 2.3. iHM_defrag の動作
ヒープ管理（pMgr->pMap）
```
Before Defrag:
  [0] ptr=0x1000  [1] ptr=NULL    [2] ptr=0x2000  [3] ptr=NULL    [4] ptr=0x3000

After Defrag:
  [0] ptr=0x1000  [1] ptr=0x2000  [2] ptr=0x3000  [3] ptr=NULL    [4] ptr=NULL
```
✔ 空き行を前方に詰める
✔ uEOD を更新
✔ ヒープ管理（pMgr->pMap）の拡張を最小限に抑える

---

## 3. ヒープ管理思想
```
Fail Fast? Fail Safe?
Not "And". Not "Or".
I embrace both.
Making debugging effortless.
```
間違ったらすぐ止める。しかし、できる限りフォローする。
適材適所。デバッグを圧倒的に楽にする。
```
┌─────────────────────┐
│元データは壊さない                        │
│必要に応じて複製して加工                  │
│ポインタは窓口（ppTarget）経由で安全管理  │
│free の順序は常に一定                     │
│自動解放（icalloc_end, iHM_end）          │
└─────────────────────┘
				   ↓
┌─────────────────────┐
│・C より安全                              │
│・C++ より簡単                            │
│・realloc安全                             │
│・自動解放                                │
│・スレッド安全（iHM）                     │
└─────────────────────┘
```

### 3.1. ヒープ管理の全体構造
```
┌─────────────────────┐
│ヒープ管理（pMgr）                        │
│─────────────────────│
│pMap  ─────┐ 管理情報（pMap[i]）    │
│uMax            │                        │
│uEOD            │                        │
│uMaxId          │                        │
└─────────────────────┘
				  ↓
┌─────────────────────┐
│管理情報（pMap[i]）                       │
│─────────────────────│
│ptr     →  ppTarget                      │
│uAry    →  配列長（配列のとき）          │
│uSizeOf →  型サイズ（1,2,4…）           │
│uAlloc  →  実体（pRealData）の総バイト数 │
│uId     →  ユニークID                    │
└─────────────────────┘
				  ↓
┌─────────────────────┐
│窓口（ppTarget）                          │
│─────────────────────│
│*ppTarget → pRealData                    │
└─────────────────────┘
				  ↓
┌─────────────────────┐
│実体（pRealData）                         │
│─────────────────────│
│calloc で確保された領域                   │
│reallocで移動する可能性あり               │
└─────────────────────┘
```

### 3.2. なぜ二重ポインタ（ppTarget）が必要なのか
```
┌─────────────────────┐
│ユーザ変数 → 実体（pRealData）           │
│                ↑                        │
│              窓口（ppTarget）            │
│                ↑                        │
│              管理情報（pMap[i]）         │
└─────────────────────┘
```
✔ realloc（実体が移動）しても窓口は動かない

✔ ユーザのポインタは常に最新
iHM_realloc() は内部で、

1. 新しい実体を確保
2. 古い実体をコピー
3. 古い実体を SecureZero + free
4. 窓口を新しい実体に更新

ユーザの変数は常に正しい実体を指す。

---

## 4. このライブラリについて
2009年、コマンドラインプログラム開発の簡素化のために書き始める。
オブジェクト指向やガベージコレクションを参考にしつつ、古典的手続き型プログラミングの拡張によるメモリ安全かつ高速な実装が目標。

### 4.1. ヒープ管理の基本設計
- 管理情報（pMap[i]）に登録された窓口（ppTarget）をたどって実体（pRealData）を取得
- 動的ヒープ管理（iHM系をベースにicalloc系を実装）
  - 表による管理
  - 管理している実体等は終了時に自動解放、基本は手動解放
  - デバッグ用のメモリマップ関数 idebug_map() を実装
- [v2026-06-14] iHM_xxx() へ移行
  - スレッド毎に動的ヒープ管理するケースに対応（同時アクセス問題回避）
  - AIによるコード生成を試行、従来コードの潜在バグを回避するためフルスクラッチ

### 4.2. 日付計算（iwmdateadd.exe, iwmdatediff.exe）
- ユリウス暦 -4712/01/01..1582/10/04 とグレゴリオ暦 1582/10/15..9999/12/31 を使用
- 空白暦 1582/10/05..1582/10/14 を考慮
- 数ヶ月前後の計算
  - 05/31 の１ヶ月後は 06/30
  - 06/30 の１ヶ月前は 05/30

### 4.3. Ruby, Unixなどから得たアイデア
- 引数の切り分け
\$CMD, \$ARG, \$ARGC, \$ARGV[]
- パイプ経由標準入力の改装
  - iwmesc.exe
  - iwmclipboard.exe
- エスケープシーケンス文字の表示
  - iwmesc.exe

### 4.4. UTF-8
2022年、**lib_iwmutil**（Shift_JIS／CP932）から、**lib_iwmutil2**（UTF-8／CP65001）に**移行**
- 2018年頃? からMicrosoft社はUTF-8による開発を推奨しているが、DOSプロンプトにおける日本語処理は難解。
- imn_CodePage(), SetConsoleOutputCP(65001) 周辺の実装は試行解
  - 標準出力は、UTF-8／BOMなし／改行コードLF
  - 標準入力は、改行コードCRLF

---

## 5. lib_iwmutil2 が使用する表記

### 5.1. 主要なデータ型
- BOOL   = TRUE, FALSE
- INT    = 32bit integer（INT8, INT16, UINT8, UINT16の代替）
- INT64  = 64bit integer（日計算）
- UINT   = 32bit unsigned integer（NTFSファイル個数）
- UINT64 = 64bit unsigned integer（size_t／秒計算／ファイルサイズ）
- DOUBLE = double（日計算）
- MS     = char
- WS     = wchar
- VOID   = void
- DWORD  = typedef unsigned long DWORD（Win32API）
- size_t = 64bit環境ではUINT64（UINT64_MAX）と同値だが、UINT（UINT_MAX）として実装した箇所がある。

### 5.2. 変数など

#### 5.2.1. lib_iwmutil2
- 大域変数の "１文字目は $"
  - \$ARGV, \$CMD など
- \#define定数（関数は含まない）は "すべて大文字"
  - IMAX_PATHW, WS など

#### 5.2.2. 開発者に期待する表記
- 大域変数, \#define定数（関数は含まない）の "１文字目は大文字"
  - Buf, BufSIze など
- オプション変数の "１文字目は _" かつ "２文字目は大文字"
  - _NL, _Format など
- 局所変数の "１文字目は _" かつ "２文字目は小文字"
  - _i1, _u1, _mp1, _wp1 など

### 5.3. 関数名
- i = iwmutil2 ライブラリ
- m = MS: Multi Byte String: UTF-8, Shift_JIS
- w = WS: Wide String: UTF-16
- u = MS: UTF-8 限定
- v = VOID: 型不明
- b = BOOL: TRUE, FALSE
- n = 数値
- s = 文字列
- a = 配列

```
i = iwmutil2 ライブラリ
```
```
m = MS: Multi Byte String: UTF-8, Shift_JIS
u = MS: UTF-8 限定
w = WS: Wide String: UTF-16
```
```
a = Array
b = BOOL: TRUE, FALSE
n = Number
s = String
v = VOID
```

### 5.4. ポインタ *(ptr + n) と、配列 ary[n] どちらが速い?
MinGW-w64では最適化すると同じ。

---
*/

#include "lib_iwmutil2.h"

// スコープ
static VOID iHM_defrag($struct_HeapManager *pMgr);
static VOID iHM_add($struct_HeapManager *pMgr, VOID *ptr, UINT uAry, UINT uSizeOf, UINT uAlloc);
static inline VOID iSecure_memZero(volatile VOID *v, UINT n);

//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	大域変数
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
WS *$CMD = NULL;		  // コマンド名を格納
WS *$ARG = NULL;		  // 引数からコマンド名を消去したもの
UINT $ARGC = 0;			  // 引数配列数
WS **$ARGV = {NULL};	  // 引数配列／ダブルクォーテーションを消去したもの
HANDLE $StdinHandle = 0;  // STDIN ハンドル
HANDLE $StdoutHandle = 0; // STDOUT ハンドル
HANDLE $StderrHandle = 0; // STDERR ハンドル
UINT64 $ExecSecBgn = 0;	  // 実行開始時間

#define IcallocDiv 32
#define MemX(len, sizeOf) (UINT)((len + 2) * sizeOf)

static $struct_HeapManager *$icallocManager = 0;

//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	Command Line
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
//-------------------------
// コマンド名／引数を取得
//-------------------------
// v2023-08-25
VOID iCLI_signal()
{
	imain_err();
}
// v2025-03-31
VOID iCLI_begin()
{
	// [Ctrl]+[C]
	signal(SIGINT, (__p_sig_fn_t)iCLI_signal);
	// CodePage
	SetConsoleCP(65001);
	SetConsoleOutputCP(65001);
	// $icallocManager 開始
	icalloc_begin();
	// $CMD
	// $ARG
	// $ARGC
	// $ARGV
	WS *pBgn = GetCommandLineW();
	INT64 iPos = iwn_searchPos(pBgn, (WS *)L" ", 1);
	if (iPos < 0)
	{
		iPos = wcslen(pBgn);
	}
	$CMD = iws_nclone(pBgn, iPos);
	pBgn += iPos;
	for (; *pBgn == L' '; pBgn++)
		;
	if (!*pBgn)
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
	while (u1 < $ARGC)
	{
		$ARGV[u1] = iws_clone(lpCmdLine[u1]);
		++u1;
	}
	LocalFree(lpCmdLine);
}
// v2026-07-17
VOID iCLI_end(
	INT exitStatus)
{
	// $icallocManager 終了
	icalloc_end();
	// ここで STDOUT は禁止
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
WS *iCLI_getOptValue(
	UINT argc,		// $ARGV[argc]
	CONST WS *opt1, // (例) "-w=", NULL
	CONST WS *opt2	// (例) "-where=", NULL
)
{
	if (argc >= $ARGC)
	{
		return 0;
	}
	WS *wp1 = $ARGV[argc];
	UINT u1 = iwn_len(opt1);
	if (u1 && !wcsncmp(wp1, opt1, u1))
	{
		return (wp1 + u1);
	}
	UINT u2 = iwn_len(opt2);
	if (u2 && !wcsncmp(wp1, opt2, u2))
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
BOOL iCLI_getOptMatch(
	UINT argc,		// $ARGV[argc]
	CONST WS *opt1, // (例) "-r", NULL
	CONST WS *opt2	// (例) "-repeat", NULL
)
{
	if (argc >= $ARGC)
	{
		return FALSE;
	}
	WS *wp1 = $ARGV[argc];
	if (opt1 && !wcscmp(wp1, opt1))
	{
		return TRUE;
	}
	if (opt2 && !wcscmp(wp1, opt2))
	{
		return TRUE;
	}
	return FALSE;
}
// v2025-03-26
VOID iCLI_VarList()
{
	P1(
		"\033[1G"
		"\033[44;97m $CMD \033[0m"
		"\n"
		"\033[3G");
	P2W($CMD);
	P1(
		"\033[1G"
		"\033[44;97m $ARG \033[0m"
		"\n"
		"\033[3G");
	P2W($ARG);
	P1(
		"\033[1G"
		"\033[44;97m $ARGC \033[0m"
		"\n"
		"\033[3G");
	P(
		"\033[92m[%d]\n", $ARGC);
	P2(
		"\033[1G"
		"\033[44;97m $ARGV \033[0m");
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
	P("-- %.3f sec\n\n", iExecSec_next());
	Sleep(1500);
	P("-- %.3f sec\n\n", iExecSec_next());
*/
// v2025-02-14
DOUBLE iExecSec(
	UINT64 microSec // 0=$ExecSecBgnに保存
)
{
	UINT64 u1 = GetTickCount64();
	if (!microSec)
	{
		$ExecSecBgn = u1;
		return 0.0;
	}
	return (u1 < microSec ? 0.0 : (u1 - microSec) / 1000.0);
}
//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	ヒープ管理
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
/* (例)
// v2026-06-21
#include "lib_iwmutil2.h"

#define NUM_THREADS 3

DWORD WINAPI Task1(LPVOID lpParam)
{
	(VOID) lpParam; // 警告消去のための記述（何もしない）
	for (UINT i = 1; i <= 10; i++)
	{
		P("\033[33m[Task1]\033[0m %u sec\n", i);
		Sleep(1000);
	}
	P2("\033[33m[Task1]\033[0m 完了しました。\n");
	return 0;
}

DWORD WINAPI Task2(LPVOID lpParam)
{
	(VOID) lpParam; // 警告消去のための記述（何もしない）
	$struct_HeapManager *pMgr2 = iHM_begin();
	for (INT i = 0; i < 100; i++)
	{
		WS *wp1 = M2W("あいうえお"); // ワイド文字に変換
		UINT len = wcslen(wp1);
		VOID *p1 = iHM_calloc(pMgr2, len, sizeof(WS), FALSE);
		memcpy(p1, wp1, len * sizeof(WS));
		/// ifree(wp1); // 忘れずにfree
		if (i % 4 != 0)
		{
			ifree(wp1); // テストのためここでfree
			iHM_free(pMgr2, p1);
		}
	}
	NL();
	idebug_map(pMgr2);
	iHM_end(pMgr2);
	P2("\033[33m[Task2]\033[0m 完了しました。\n");
	return 0;
}

DWORD WINAPI Task3(LPVOID lpParam)
{
	(VOID) lpParam; // 警告消去のための記述（何もしない）
	for (INT i = 0; i < 100; i++)
	{
		Sleep(50);
		MS *mp1 = "かきくけこ";
		UINT len = strlen(mp1);
		VOID *p1 = icalloc(len, sizeof(MS), FALSE);
		memcpy(p1, mp1, len * sizeof(MS));
		if (i % 2 == 0)
		{
			ifree(p1);
		}
		else if (i % 3 == 0)
		{
			irealloc(p1, len * 2);
		}
	}
	NL();
	idebug_map(NULL);
	P2("\033[33m[Task3]\033[0m 完了しました。\n");
	return 0;
}

INT main()
{
	imain_begin();

	HANDLE threads[NUM_THREADS] = {};

	P2("1. 非同期タスクを開始します。");

	// 1回だけ実行するブロック
	// 失敗した時点で break
	while (TRUE)
	{
		threads[0] = CreateThread(NULL, 0, Task1, NULL, 0, NULL);
		/// threads[0] = NULL; // Debug
		if (threads[0] == NULL)
		{
			break;
		}

		threads[1] = CreateThread(NULL, 0, Task2, NULL, 0, NULL);
		/// threads[1] = NULL; // Debug
		if (threads[1] == NULL)
		{
			break;
		}

		threads[2] = CreateThread(NULL, 0, Task3, NULL, 0, NULL);
		/// threads[2] = NULL; // Debug
		if (threads[2] == NULL)
		{
			break;
		}

		// すべて成功した時だけ待機
		WaitForMultipleObjects(NUM_THREADS, threads, TRUE, INFINITE);
		P2("2. すべての非同期タスクが終了しました。");

		// ループしない
		break;
	}

	BOOL bErr = FALSE;

	// 一括クローズ
	for (UINT i = 0; i < NUM_THREADS; i++)
	{
		if (threads[i] != NULL)
		{
			CloseHandle(threads[i]);
			threads[i] = NULL; // 二重解放防止
		}
		else
		{
			bErr = TRUE;
		}
	}

	// 失敗
	if (bErr == TRUE)
	{
		P2("\033[91m"
		   "3. 失敗しました。"
		   "\033[0m");
	}

	NL();
	idebug_map(NULL);
	P("%d個解放しました。\n", ifree_all());

	imain_end();
}
*/
//-------------------
// CRITICAL_SECTION
//-------------------
static CRITICAL_SECTION $iHM_CS;
// 開始（１度だけ）
// v2026-06-21
#define iHM_CS_begin() InitializeCriticalSection(&$iHM_CS)
// ロック
// v2026-06-21
#define iHM_CS_lock() EnterCriticalSection(&$iHM_CS)
// アンロック
// v2026-06-21
#define iHM_CS_unlock() LeaveCriticalSection(&$iHM_CS);
// 終了（１度だけ）
// v2026-06-21
#define iHM_CS_end() DeleteCriticalSection(&$iHM_CS)
//-----------------
// ヒープ管理開始
//-----------------
// v2026-06-22
$struct_HeapManager *iHM_begin()
{
	// 1. マネージャ自体を確保（この時点では空）
	$struct_HeapManager *pMgr = ($struct_HeapManager *)calloc(1, sizeof($struct_HeapManager));
	iHM_err(pMgr, "$struct_HeapManager *pMgr == NULL", __LINE__, __FILE_NAME__);

	//  2. 配列用メモリを一時ポインタへ仮確保
	$struct_HeapMap *pTemporaryMap = ($struct_HeapMap *)calloc(IcallocDiv, sizeof($struct_HeapMap));
	iHM_err(pTemporaryMap, "$struct_HeapMap *pTemporaryMap == NULL", __LINE__, __FILE_NAME__);

	// 3. すべて成功して初めて構造体に値をセットする（利用者にズレを意識させない隠蔽）
	pMgr->pMap = pTemporaryMap;
	pMgr->uMax = IcallocDiv;
	pMgr->uEOD = 0;
	pMgr->uMaxId = 0;

	return pMgr;
}
//-----------
// デフラグ
//-----------
// v2026-06-21
VOID iHM_defrag(
	$struct_HeapManager *pMgr)
{
	// CRITICAL_SECTION ロック
	iHM_CS_lock();

	UINT uWrite = 0;

	// 1. 前方から走査、空きがあれば前に詰めていく
	for (UINT uRead = 0; uRead < pMgr->uEOD; uRead++)
	{
		if (pMgr->pMap[uRead].ptr != NULL)
		{
			if (uWrite != uRead)
			{
				// 空き枠へ構造体を丸ごとコピー
				pMgr->pMap[uWrite] = pMgr->pMap[uRead];
				// コピー元（古い位置）に同じ値が残らないよう、その場で安全にゼロクリア
				iSecure_memZero(&pMgr->pMap[uRead], sizeof($struct_HeapMap));
			}
			++uWrite;
		}
	}

	// 2. 次の書き込み位置を記憶
	pMgr->uEOD = uWrite;

	// CRITICAL_SECTION アンロック
	iHM_CS_unlock();
}
//-----------
// 情報追加
//-----------
// v2026-06-22
static VOID iHM_add(
	$struct_HeapManager *pMgr,
	VOID *ptr,
	UINT uAry,
	UINT uSizeOf,
	UINT uAlloc)
{
	// コンパイル時に判明
	//	if (pMgr == NULL || ptr == NULL)
	//	{
	//		return FALSE;
	//	}

	// CRITICAL_SECTION ロック
	iHM_CS_lock();

	// 1. 最後尾[uMax - 1]に達しているかチェック（例: 31 >= 32 - 1）
	if (pMgr->uEOD >= (pMgr->uMax - 1))
	{
		// 2. デフラグする
		iHM_defrag(pMgr);

		// 3. デフラグしても空きがなければ拡張
		if (pMgr->uEOD >= (pMgr->uMax - 1))
		{
			UINT newMax = pMgr->uMax + IcallocDiv;

			// callocで完全に真っ白なメモリ空間を確保（ゼロクリアの徹底）
			$struct_HeapMap *pNewMap = ($struct_HeapMap *)calloc(newMax, sizeof($struct_HeapMap));
			iHM_err(pNewMap, "$struct_HeapMap *pNewMap == NULL", __LINE__, __FILE_NAME__);

			// 整理済みの古い配列の内容を、新しい配列の先頭へmemcpyで明瞭に一括上書きコピー
			memcpy(pNewMap, pMgr->pMap, pMgr->uMax * sizeof($struct_HeapMap));

			// コピー後、古い配列の内容を完全に消去して解放する
			iSecure_memZero(pMgr->pMap, pMgr->uMax * sizeof($struct_HeapMap));
			free(pMgr->pMap);

			// マネージャが指す先を、新しく作った綺麗な配列に切り替える
			pMgr->pMap = pNewMap;
			pMgr->uMax = newMax;
		}
	}

	// 4. ユニークIDを作成（使い回ししない）
	++pMgr->uMaxId;

	// 5. データを書き込み、ユニークIDを付与
	pMgr->pMap[pMgr->uEOD] = ($struct_HeapMap){ptr, uAry, uSizeOf, uAlloc, pMgr->uMaxId};

	// 6. 次の書き込み位置を記憶
	++pMgr->uEOD;

	// CRITICAL_SECTION アンロック
	iHM_CS_unlock();
}
//---------
// calloc
//---------
// v2026-06-22
VOID *iHM_calloc(
	$struct_HeapManager *pMgr,
	UINT len,
	UINT sizeOf,
	BOOL aryOn)
{
	// コンパイル時に判明
	//	if (pMgr == NULL)
	//	{
	//		return NULL;
	//	}

	VOID **ppTargetAlloc = (VOID **)calloc(1, sizeof(VOID *));
	iHM_err(ppTargetAlloc, "VOID **ppTargetAlloc == NULL", __LINE__, __FILE_NAME__);

	// len = 0 の可能性あり／newAllocを使用
	UINT newAlloc = MemX(len, sizeOf);
	VOID *pNewRealData = calloc(newAlloc, 1);
	if (pNewRealData == NULL)
	{
		free(ppTargetAlloc);
		ppTargetAlloc = NULL;
		return NULL;
	}

	*ppTargetAlloc = pNewRealData;
	iHM_add(pMgr, (VOID *)ppTargetAlloc, (aryOn ? len : 0), sizeOf, newAlloc);

	return pNewRealData;
}
//------------
// reallocEx
//------------
// v2026-06-28
VOID *iHM_reallocEx(
	$struct_HeapManager *pMgr,
	VOID *oldPtr, // 既存ポインタ
	VOID *newPtr, // 既存データを置き換えるとき指定／それ以外は NULL
	UINT newLen	  // 純粋に長さのみ／sizeof は既存ポインタの値を使用
)
{
	// コンパイル時に判明
	//	if (pMgr == NULL || oldPtr == NULL)
	//	{
	//		return oldPtr;
	//	}

	// CRITICAL_SECTION ロック
	iHM_CS_lock();

	// 1. 後方から前方へ走査し、対象の管理行を特定
	for (INT i = (INT)(pMgr->uEOD - 1); i >= 0; i--)
	{
		if (pMgr->pMap[i].ptr == NULL)
		{
			continue;
		}
		VOID **ppTarget = (VOID **)pMgr->pMap[i].ptr;
		if (*ppTarget == oldPtr)
		{
			// 2. 新しいアロケート長を算出
			UINT newAlloc = MemX(newLen, pMgr->pMap[i].uSizeOf);
			// 新サイズ <= 古いサイズ のとき
			if (newAlloc <= pMgr->pMap[i].uAlloc)
			{
				newAlloc = pMgr->pMap[i].uAlloc;
			}

			// 3. callocで完全に真っ白な新しい空間を確保
			VOID *pNewRealData = calloc(newAlloc, 1);
			iHM_err(pNewRealData, "VOID *pNewRealData == NULL", __LINE__, __FILE_NAME__);

			// 4. 古い実体から新しい実体へ、既存のデータをmemcpyで引き継ぐ
			UINT oldAlloc = pMgr->pMap[i].uAlloc;
			//  4.1. 既存データを引き継ぐ
			if (newPtr == NULL)
			{
				memcpy(pNewRealData, oldPtr, oldAlloc);
			}
			// 4.2. データを置き換える
			else
			{
				memcpy(pNewRealData, newPtr, newAlloc);
			}

			// 5. 古い実体データを安全に完全消去してfree
			iSecure_memZero(oldPtr, oldAlloc);
			free(oldPtr);
			oldPtr = NULL;

			// 6. 窓口ポインタの指す先を、新しく作り直した綺麗な実体アドレスにパッと切り替える
			*ppTarget = pNewRealData;
			// 7. 管理行 (pMap[i]) の情報を更新
			// 配列のとき
			if (pMgr->pMap[i].uAry > 0)
			{
				pMgr->pMap[i].uAry = newLen;
			}
			pMgr->pMap[i].uAlloc = newAlloc;

			// CRITICAL_SECTION アンロック
			iHM_CS_unlock();
			return pNewRealData;
		}
	}

	// CRITICAL_SECTION アンロック
	iHM_CS_unlock();
	return oldPtr;
}
//------------------------
// free（個別／速度優先）
//------------------------
// v2026-07-15
UINT iHM_free(
	$struct_HeapManager *pMgr,
	VOID *ptr)
{
	UINT rtn = 0;

	if (pMgr == NULL || ptr == NULL)
	{
		return rtn;
	}

	// CRITICAL_SECTION ロック
	iHM_CS_lock();

	// 1. 後方から前方へ逆引き走査して対象の管理行を特定する
	for (INT i = (INT)(pMgr->uEOD - 1); i >= 0; i--)
	{
		// すでに消去されている行（NULL行）は安全のためにスキップ
		if (pMgr->pMap[i].ptr == NULL)
		{
			continue;
		}
		// 管理簿から窓口ポインタ（ppTarget）を取り出す
		VOID **ppTarget = (VOID **)pMgr->pMap[i].ptr;
		// 窓口の中身（*ppTarget = 実体アドレス）と、引数の ptr（実体アドレス）を比較する
		if (*ppTarget == ptr)
		{
			VOID *pRealData = *ppTarget;
			// A. 先にある「ヒープ上の本当の実体データ」を安全に消去してfree
			if (pRealData != NULL)
			{
				UINT totalBytes = pMgr->pMap[i].uAlloc;
				iSecure_memZero(pRealData, totalBytes);
				free(pRealData);
				rtn = 1;
			}
			// B. 次に、iHM_calloc内で確保された「窓口領域（多重ポインタ領域）」自体を消去してfree
			iSecure_memZero(ppTarget, sizeof(VOID *));
			free(ppTarget);
			// C. 管理情報を行ごと完全にゼロクリアして「NULL行」にする
			iSecure_memZero(&pMgr->pMap[i], sizeof($struct_HeapMap));
			break;
		}
	}

	// CRITICAL_SECTION アンロック
	iHM_CS_unlock();
	return rtn;
}
//--------------------------
// free（個別／確実性優先）
//--------------------------
//
// iHM_free() との大きな違いは『配列に収納されたポインタ実体をfreeするタイミング』
//   iHM_free() は速度と効率 O(1..n) を追求したため、終了時にまとめてfree。
//   iHM_free2() は遅い O(n) が、確実にfree。
// 以上、
// 配列関連の不要データが問題になる環境ならば、配列freeの際、iHM_free2() の使用を検討してください。
//
// v2026-07-14
UINT iHM_free2(
	$struct_HeapManager *pMgr,
	VOID *ptr)
{
	if (pMgr == NULL || ptr == NULL)
	{
		return FALSE;
	}

	UINT rtn = 0;

	// CRITICAL_SECTION ロック
	iHM_CS_lock();

	$struct_HeapMap info = iHM_info(pMgr, ptr);

	VOID **ppTarget = (VOID **)ptr;
	for (UINT _u2 = 0; _u2 < info.uAry; _u2++)
	{
		if (ppTarget[_u2] == NULL)
		{
			continue;
		}
		// 各要素（ポインタのポインタ）が出す実体アドレスを出力
		/// P("[%u] ppTarget: %p, *ppTarget(RealData): %p\n", (_u2 + 1), ppTarget[_u2], *(VOID **)ppTarget[_u2]);
		rtn += iHM_free(pMgr, ppTarget[_u2]);
	}
	// 実体（本体）をfree
	rtn += iHM_free(pMgr, ptr);

	// CRITICAL_SECTION アンロック
	iHM_CS_unlock();
	return rtn;
}
//--------------
// free（一括）
//--------------
// v2026-07-14
UINT iHM_freeAll(
	$struct_HeapManager *pMgr)
{
	if (pMgr == NULL)
	{
		return 0;
	}

	// CRITICAL_SECTION ロック
	iHM_CS_lock();

	UINT rtn = 0;

	// 1. 内部の構造体配列が確保されている場合、iHM_free を再利用して残った実体・窓口をすべて消去
	if (pMgr->pMap != NULL)
	{
		for (UINT i = 0; i < pMgr->uEOD; i++)
		{
			// すでに iHM_free で消去（NULL行化）されている場合は、安全にスキップ
			if (pMgr->pMap[i].ptr == NULL)
			{
				continue;
			}
			// 管理簿の窓口（ppTarget）から「実体アドレス（*ppTarget）」を取得
			VOID **ppTarget = (VOID **)pMgr->pMap[i].ptr;
			VOID *pRealData = *ppTarget;
			// 既存の iHM_free を再利用して、該当要素を根こそぎ安全に完全消滅させる（重複の完全排除）
			rtn += iHM_free(pMgr, pRealData);
		}
		pMgr->uEOD = 0;
	}

	// CRITICAL_SECTION アンロック
	iHM_CS_unlock();

	return rtn;
}
//-----------------
// ヒープ管理終了
//-----------------
// v2026-06-22
UINT iHM_end(
	$struct_HeapManager *pMgr)
{
	// 1. 実体を消去
	UINT rtn = iHM_freeAll(pMgr);
	if (pMgr->pMap != NULL)
	{
		// 2. 実体を管理していたpMapを消去
		iSecure_memZero(pMgr->pMap, sizeof($struct_HeapMap) * pMgr->uMax);
		free(pMgr->pMap);
		pMgr->pMap = NULL;
	}
	if (pMgr != NULL)
	{
		// 3. ヒープを管理していたpMgr自身を消去
		iSecure_memZero(pMgr, sizeof($struct_HeapManager));
		free(pMgr);
		pMgr = NULL;
	}
	return rtn;
}
//-----------------------
// ヒープ管理エラー終了
//-----------------------
/* (例)
	iHM_err(
		pMgr,
		"$struct_HeapManager *pMgr == NULL",
		__LINE__, __FILE_NAME__);
*/
// v2026-06-28
VOID iHM_err(
	VOID *ptr,	   // ポインタ
	CONST MS *rem, // エラーに関するコメント
	UINT line,	   // __LINE__
	CONST MS *fn   // __FILE_NAME__
)
{
	if (ptr != NULL)
	{
		return;
	}
	P(
		"\033[37;41m L%d (%s) \033[0m\033[37m Abort: \033[91m%s\033[0m\n",
		line, fn, rem);
	// 強制終了: メモリにゴミが残った状態
	P1(IESC_RESET);
	SetConsoleOutputCP(GetACP());
	exit(EXIT_FAILURE);
}
//-----------------
// ヒープ情報取得
//-----------------
// v2026-07-14
$struct_HeapMap iHM_info(
	$struct_HeapManager *pMgr,
	VOID *ptr)
{
	$struct_HeapMap rtn = {NULL, 0, 0, 0, 0};

	if (ptr == NULL)
	{
		return rtn;
	}

	if (pMgr == NULL)
	{
		pMgr = $icallocManager;
	}

	// CRITICAL_SECTION ロック
	iHM_CS_lock();

	// 重要：先頭から走査。配列容器が見つかった時点でbreak
	for (UINT _u1 = 0; _u1 < pMgr->uEOD; _u1++)
	{
		if (pMgr->pMap[_u1].ptr == NULL)
		{
			continue;
		}
		// 管理簿から窓口ポインタ（ppTarget）を取り出す
		VOID **ppTarget = (VOID **)pMgr->pMap[_u1].ptr;
		if (*ppTarget == ptr)
		{
			memcpy(&rtn, &pMgr->pMap[_u1], sizeof($struct_HeapMap));
			break;
		}
	}

	// CRITICAL_SECTION アンロック
	iHM_CS_unlock();
	return rtn;
}
//-----------------------
// $icallocManager 開始
//-----------------------
// 【重要】
//   $icallocManager はシングルスレッド専用（ただし、実装上はマルチスレッド可）
//   マルチスレッドを期待するなら個別に iHM_begin() / iHM_end() を割り当てること
//
// v2026-06-22
VOID icalloc_begin()
{
	// CRITICAL_SECTION 開始（１度だけ）
	iHM_CS_begin();

	$icallocManager = iHM_begin();
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
// v2026-06-15
VOID *icalloc(
	UINT len,	 // 個数
	UINT sizeOf, // sizeof
	BOOL aryOn	 // TRUE=配列
)
{
	return iHM_calloc($icallocManager, len, sizeOf, aryOn);
}
//----------
// realloc
//----------
// v2026-06-15
VOID *irealloc(
	VOID *ptr, // icalloc()ポインタ
	UINT len   // 個数
)
{
	return iHM_realloc($icallocManager, ptr, len);
}
//----------
// replace
//----------
// v2026-06-28
VOID *irepalloc(
	VOID *oldPtr, // icalloc()ポインタ
	VOID *newPtr, // 置き換えるデータ
	UINT len	  // 個数
)
{
	return iHM_replace($icallocManager, oldPtr, newPtr, len);
}
//------------------------
// free（個別／速度優先）
//------------------------
// v2026-07-14
UINT icalloc_free(
	VOID *ptr)
{
	return iHM_free($icallocManager, ptr);
}
//--------------------------
// free（個別／確実性優先）
//--------------------------
// v2026-07-14
UINT icalloc_free2(
	VOID *ptr)
{
	return iHM_free2($icallocManager, ptr);
}
//--------------
// free（一括）
//--------------
// v2026-06-15
UINT icalloc_freeAll()
{
	return iHM_freeAll($icallocManager);
}
//-----------------------
// $icallocManager 終了
//-----------------------
// v2026-06-22
VOID icalloc_end()
{
	// 終了（１度だけ）
	iHM_end($icallocManager);

	// CRITICAL_SECTION 終了（１度だけ）
	iHM_CS_end();
}
//-----------------------
// $icallocManager 情報
//-----------------------
// v2026-07-14
$struct_HeapMap icalloc_info(
	VOID *ptr)
{
	return iHM_info($icallocManager, ptr);
}
//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	Security
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------
// 最適化(-Osなど)によって削除されない安全なメモリゼロクリア関数
//-----------------------------------------------------------------
// v2026-07-17
static inline VOID iSecure_memZero(
	volatile VOID *v,
	UINT n)
{
	if (!v || n == 0)
	{
		return;
	}
	// ゼロクリア
	volatile MS *p = (volatile MS *)v;
	UINT counter = n;
	while (counter--)
	{
		*p++ = 0;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	Debug
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
//--------------------------------
// $icallocManager等の内容を出力
//--------------------------------
/* (例)
	idebug_map(NULL);
*/
// v2026-07-16
VOID idebug_printMap(
	$struct_HeapManager *pMgr,
	UINT line,		  // 行番号
	CONST MS *fn,	  // ソースファイル名
	DOUBLE elapsedSec // 経過秒
)
{
	// pMgr 選択
	if (pMgr == NULL)
	{
		pMgr = $icallocManager;
	}

	// CRITICAL_SECTION ロック
	iHM_CS_lock();

	iConsole_EscOn();

	// 表示がずれるのを防ぐため PL() を内装
	P(
		"\033[37;44m"
		" L%u (%s) "
		"\033[0m"
		"\033[37m"
		" %s\n"
		"\033[96m"
		"uMax: "
		"\033[37m"
		"%u  "
		"\033[96m"
		"uMaxId: "
		"\033[37m"
		"%u  "
		"\033[96m"
		"uEOD: "
		"\033[37m"
		"%u  "
		"\033[36m"
		"Time: "
		"\033[94m"
		"%.3f sec\n"
		"\033[34m"
		"- count - id ------- pointer -------- array - sizeof ---------- alloc -------------\n",
		line, fn, (pMgr == $icallocManager ? "$icallocManager" : ""),
		pMgr->uMax, pMgr->uMaxId, pMgr->uEOD, elapsedSec);

	UINT64 uAllocUsed = 0;
	UINT uCnt = 0;
	for (UINT i = 0; i < pMgr->uMax; i++)
	{
		$struct_HeapMap *map = &pMgr->pMap[i];
		uAllocUsed += map->uAlloc;
		if (map->uAry > 0)
		{
			P1("\033[37;44m");
		}
		else if (map->ptr)
		{
			P1("\033[37m");
		}
		else
		{
			P1("\033[31m");
		}
		++uCnt;
		P(
			"  %-7u %-10u %p %5u %8u %16u ",
			uCnt, map->uId, map->ptr, map->uAry, map->uSizeOf, map->uAlloc);
		if (map->ptr && map->uAry == 0)
		{
			VOID **pp1 = (VOID **)map->ptr;

			P1("\033[32m");
			if (map->uSizeOf == sizeof(WS))
			{
				P1W((WS *)*pp1);
			}
			else if (map->uSizeOf == sizeof(MS))
			{
				P1((MS *)*pp1);
			}
		}
		P2("\033[0m");
	}

	P(
		"\033[34m"
		"---------------------------------------------------- %16llu byte --------"
		"\033[0m\n\n",
		uAllocUsed);

	// CRITICAL_SECTION アンロック
	iHM_CS_unlock();
}
//---------------------------------------
// メモリの内容を16進数形式でダンプ出力
//---------------------------------------
/* (例)
	UINT u1 = 0;

	MS *mp1 = ims_clone("あいうえお12345");
	u1 = imn_len(mp1);
	P2(mp1);
	idebug_dumpMem(NULL, mp1, TRUE);
	memset(mp1, 0, u1 * sizeof(MS));
	idebug_dumpMem(NULL, mp1, FALSE);
	ifree(mp1);
	idebug_dumpMem(NULL, mp1, FALSE);

	WS *wp1 = iws_clone(L"あいうえお12345");
	u1 = iwn_len(wp1);
	P2W(wp1);
	idebug_dumpMem(NULL, wp1, TRUE);
	memset(wp1, 0, u1 * sizeof(WS));
	idebug_dumpMem(NULL, wp1, FALSE);
	ifree(wp1);
	idebug_dumpMem(NULL, wp1, FALSE);
*/
// v2026-07-17
VOID idebug_dumpMem(
	$struct_HeapManager *pMgr,
	volatile CONST VOID *ptr,
	BOOL bBefore // TRUE=変更前, FALSE=変更後
)
{
	$struct_HeapMap info = iHM_info(pMgr, (VOID *)ptr);
	UINT uLen = info.uAlloc;

	volatile CONST UCHAR *p = (volatile CONST UCHAR *)ptr;

	if (bBefore == TRUE)
	{
		P(
			"\033[37m"
			"[%p] "
			"\033[94m"
			"%u bytes\n",
			(VOID *)ptr, uLen);
		P1("\033[92m"
		   "Before:");
	}
	else
	{
		P1("\033[31m"
		   "After:");
	}

	UINT uTrue = 0;

	for (UINT _u1 = 0; _u1 < uLen; _u1++)
	{
		if (p[_u1] != 0)
		{
			++uTrue;
		}
		// 16バイトごとに改行
		if (_u1 % 16 == 0)
		{
			P1("\n ");
		}
		P(" %02X", p[_u1]);
	}

	P(
		"\n"
		"( %u / %u )"
		"\033[0m"
		"\n",
		uTrue, uLen);
}
//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	Print関係
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
//--------
// Print
//--------
// v2026-06-28
VOID P1W(
	CONST WS *str)
{
	MS *mp1 = W2M(str);
	P1(mp1);
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
VOID QP(
	CONST MS *str,
	UINT strLen)
{
	if (!str)
	{
		return;
	}
	fflush(STDOUT);
	WriteFile($StdoutHandle, str, strLen, NULL, NULL);
	FlushFileBuffers($StdoutHandle);
}
// v2025-04-10
VOID QP1W(
	CONST WS *str)
{
	if (!str)
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
// v2026-06-19
VOID PR(
	CONST MS *str,
	UINT strRepeat)
{
	MS *mp1 = ims_repeat(str, strRepeat);
	P1(mp1);
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
WS *iws_cnv_escape(
	WS *str)
{
	if (!str)
	{
		return icalloc_WS(0);
	}
	WS *rtn = icalloc_WS(wcslen(str));
	WS *pEnd = rtn;
	while (*str)
	{
		if (*str == '\\')
		{
			++str;
			switch (*str)
			{
			case ('a'):
				*pEnd = '\a';
				break;
			case ('b'):
				*pEnd = '\b';
				break;
			case ('e'):
				*pEnd = '\e';
				break;
			case ('t'):
				*pEnd = '\t';
				break;
			case ('n'):
				*pEnd = '\n';
				break;
			case ('v'):
				*pEnd = '\v';
				break;
			case ('f'):
				*pEnd = '\f';
				break;
			case ('r'):
				*pEnd = '\r';
				break;
			case ('\"'):
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
	rtn = iws_replace(wp1, (WS *)L"\\033[", (WS *)L"\033[", FALSE);
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
	//   文字色 9n
	//   背景色 10n
	//     0=黒    1=赤    2=黄緑  3=黄
	//     4=青    5=紅紫  6=水    7=白
	P2("\033[91m 文字[赤] \033[0m");
	P2("\033[101m 背景[赤] \033[0m");
	P2("\033[91;107m 文字[赤]／背景[白] \033[0m");

	// RGBによる指定例
	//  文字色   \033[38;2;R;G;Bm
	//  背景色   \033[48;2;R;G;Bm
	P2("\033[38;2;255;255;255m\033[48;2;0;0;255m 文字[白]／背景[青] \033[0m");
*/
// v2025-02-15
VOID iConsole_EscOn()
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
WS *iCLI_getKeyInput(
	BOOL bKeyInputMultiLine)
{
	UINT rtnMax = 256;
	WS *rtn = icalloc_WS(rtnMax);
	UINT uEnd = 0;
	WS RCW_buf[2];
	DWORD RCW_len;
	// 複数行入力
	//   4  = [Ctrl]+[D]
	//   26 = [Ctrl]+[Z]
	if (bKeyInputMultiLine)
	{
		while (ReadConsoleW($StdinHandle, RCW_buf, 1, &RCW_len, NULL))
		{
			if (RCW_buf[0] == 4 || RCW_buf[0] == 26)
			{
				break;
			}
			rtn[uEnd] = RCW_buf[0];
			++uEnd;
			if (uEnd >= rtnMax)
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
		while (ReadConsoleW($StdinHandle, RCW_buf, 1, &RCW_len, NULL))
		{
			if (RCW_buf[0] == 13)
			{
				break;
			}
			else if (RCW_buf[0] <= 26)
			{
				RCW_buf[0] = ' ';
			}
			rtn[uEnd] = RCW_buf[0];
			++uEnd;
			if (uEnd >= rtnMax)
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
WS *iCLI_getStdin(
	BOOL bKeyInput // STDINが空のとき TRUE=手入力モード／FALSE=空文字を返す
)
{
	WS *rtn = NULL;
	INT iStdin = fseeko64(STDIN, 0, SEEK_END);
	if (iStdin)
	{
		rtn = (bKeyInput ? iCLI_getKeyInput(TRUE) : icalloc_WS(0));
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
VOID iCLI_systemW(
	WS *cmd)
{
	STARTUPINFOW si;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	PROCESS_INFORMATION pi;
	ZeroMemory(&pi, sizeof(pi));
	if (CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
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
VOID iClipboard_setText(
	CONST WS *str)
{
	if (!str)
	{
		return;
	}
	UINT uLen = wcslen(str);
	HGLOBAL hg = GlobalAlloc(GMEM_DDESHARE | GMEM_MOVEABLE, ((uLen + 1) * sizeof(WS)));
	if (hg)
	{
		wcscpy((WS *)GlobalLock(hg), str);
		GlobalUnlock(hg);
		if (OpenClipboard(NULL))
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
WS *iClipboard_getText()
{
	if (!OpenClipboard(NULL))
	{
		return icalloc_WS(0);
	}
	HANDLE hg = GetClipboardData(CF_UNICODETEXT);
	if (!hg)
	{
		return icalloc_WS(0);
	}
	WS *rtn = iws_clone((WS *)GlobalLock(hg));
	GlobalUnlock(hg);
	CloseClipboard();
	return rtn;
}
//////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------------------------
	UTF-16／UTF-8変換
----------------------------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////////////////////////
// v2026-07-07
MS *icnv_W2M(
	CONST WS *str,
	UINT uCP)
{
	// L"" のとき 1 が返るので -1 で調整
	INT i1 = WideCharToMultiByte(uCP, 0, str, -1, NULL, 0, NULL, NULL) - 1;
	if (i1 > 0)
	{
		MS *rtn = icalloc_MS(i1); // 末尾にダブルヌル自動付与
		WideCharToMultiByte(uCP, 0, str, -1, rtn, i1, NULL, NULL);
		return rtn;
	}
	else
	{
		return icalloc_MS(0);
	}
}
// v2026-07-07
WS *icnv_M2W(
	CONST MS *str,
	UINT uCP)
{
	// "" のとき 1 が返るので -1 で調整
	INT i1 = MultiByteToWideChar(uCP, 0, str, -1, NULL, 0) - 1;
	if (i1 > 0)
	{
		WS *rtn = icalloc_WS(i1); // 末尾にダブルヌル自動付与
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
UINT imn_len(
	CONST MS *str)
{
	if (!str || !*str)
	{
		return 0;
	}
	return strlen(str);
}
// v2023-07-29
UINT iwn_len(
	CONST WS *str)
{
	if (!str || !*str)
	{
		return 0;
	}
	return wcslen(str);
}
// v2025-04-12
UINT iun_bomLen(
	CONST MS *str)
{
	if (imn_len(str) >= 3 && str[0] == (MS)0xEF && str[1] == (MS)0xBB && str[2] == (MS)0xBF)
	{
		return 3;
	}
	return 0;
}
// v2025-04-12
UINT iun_len(
	CONST MS *str)
{
	if (!str || !*str)
	{
		return 0;
	}
	// BOM
	str += iun_bomLen(str);
	UINT rtn = 0;
	while (*str)
	{
		// 1byte
		if ((*str & 0x80) == 0x00)
		{
			str += 1;
			++rtn;
		}
		// 2byte
		else if ((*str & 0xE0) == 0xC0)
		{
			str += 2;
			++rtn;
		}
		// 3byte
		else if ((*str & 0xF0) == 0xE0)
		{
			str += 3;
			++rtn;
		}
		// 4byte
		else if ((*str & 0xF8) == 0xF0)
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
//
// UTF-8 BOM   => 65001
// UTF-8 NoBOM => 65001
//   1byte: [0]0x00..0x7F
//   2byte: [0]0xC2..0xDF [1]0x80..0xBF
//   3byte: [0]0xE0..0xEF [1]0x80..0xBF [2]0x80..0xBF
//   4byte: [0]0xF0..0xF7 [1]0x80..0xBF [2]0x80..0xBF [3]0x80..0xBF
// Shift_JIS   => 932
//   2byte: [0]0x81..0x9F or [0]0xE0..0xEC
// Default     => 65001
//
// v2026-07-12
UINT imn_CodePage(
	MS *str)
{
	// UTF-8 ?
	if (!str || !*str)
	{
		return 65001;
	}
	// UTF-8 BOM
	if (strlen(str) >= 3 && str[0] == (MS)0xEF && str[1] == (MS)0xBB && str[2] == (MS)0xBF)
	{
		return 65001;
	}
	// UTF-8 NoBOM
	while (*str)
	{
		// 1byte文字
		if ((*str & 0x80) == 0x00)
		{
			// UTF-8 ? or Shift_JIS ?
		}
		// 2..4byte文字
		// 使用頻度順: 3byte文字 => 2byte文字 => 4byte文字
		else if ((*str & 0xF0) == 0xE0 || (*str & 0xE0) == 0xC0 || (*str & 0xF8) == 0xF0)
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
VOID imv_cpy(
	MS *to,
	CONST MS *from)
{
	if (!from)
	{
		return;
	}
	while ((*to++ = *from++))
		;
	*to = '\0';
}
// v2025-03-08
VOID iwv_cpy(
	WS *to,
	CONST WS *from)
{
	if (!from)
	{
		return;
	}
	while ((*to++ = *from++))
		;
	*to = L'\0';
}
//-------------------------
// コピーした文字長を返す
//-------------------------
/* (例)
	WS *to = iws_clone(L"ABCDEFGHIJKLMNOPQRSTUVWXYZ");
		UINT uEnd = iwn_cpy(to, L"c:\\windows");
			P3(uEnd); //=> 10
			P2W(to);  //=> "c:\windows"
		uEnd += iwn_ncpy((to + uEnd), L"\\123", 1);
			P3(uEnd); //=> 11
			P2W(to);  //=> "c:\windows\"
	ifree(to);
*/
// v2026-07-18
UINT imn_cpy(
	MS *to,
	CONST MS *from)
{
	if (!from)
	{
		return 0;
	}
	UINT rtn = imn_len(from);
	memcpy(to, from, (rtn + 1) * sizeof(MS));
	return rtn;
}
// v2026-07-18
UINT iwn_cpy(
	WS *to,
	CONST WS *from)
{
	if (!from)
	{
		return 0;
	}
	UINT rtn = iwn_len(from);
	memcpy(to, from, (rtn + 1) * sizeof(WS));
	return rtn;
}
// v2026-07-18
UINT imn_ncpy(
	MS *to,
	CONST MS *from,
	UINT fromLen)
{
	if (!from || !fromLen)
	{
		return 0;
	}
	UINT rtn = imn_len(from);
	if (rtn > fromLen)
	{
		rtn = fromLen;
	}
	memcpy(to, from, rtn * sizeof(MS));
	*(to + rtn) = '\0';
	return rtn;
}
// v2026-07-18
UINT iwn_ncpy(
	WS *to,
	CONST WS *from,
	UINT fromLen)
{
	if (!from || !fromLen)
	{
		return 0;
	}
	UINT rtn = iwn_len(from);
	if (rtn > fromLen)
	{
		rtn = fromLen;
	}
	memcpy(to, from, rtn * sizeof(WS));
	*(to + rtn) = L'\0';
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
// v2026-07-18
MS *ims_nclone(
	CONST MS *from,
	UINT fromLen)
{
	MS *rtn = icalloc_MS(fromLen);
	if (from)
	{
		memcpy(rtn, from, fromLen * sizeof(MS));
	}
	return rtn;
}
// v2026-07-18
WS *iws_nclone(
	CONST WS *from,
	UINT fromLen)
{
	WS *rtn = icalloc_WS(fromLen);
	if (from)
	{
		memcpy(rtn, from, fromLen * sizeof(WS));
	}
	return rtn;
}
/* (例)
	WS *wp1 = iws_cats(5, L"123", NULL, L"abcde", L"", L"あいうえお");
		P2W(wp1); //=> "123abcdeあいうえお"
	ifree(wp1);
*/
// v2025-03-06
MS *ims_cats(
	UINT size, // 要素数(n+1)
	...		   // ary[0..n]
)
{
	MS **rtnAry = icalloc_MS_ary(size);
	UINT rtnLen = 0;
	va_list va;
	va_start(va, size);
	for (UINT _u1 = 0; _u1 < size; _u1++)
	{
		rtnAry[_u1] = va_arg(va, MS *);
		rtnLen += imn_len(rtnAry[_u1]);
	}
	va_end(va);
	MS *rtn = icalloc_MS(rtnLen);
	UINT uEnd = 0;
	for (UINT _u1 = 0; _u1 < size; _u1++)
	{
		uEnd += imn_cpy((rtn + uEnd), rtnAry[_u1]);
	}
	ifree(rtnAry);
	return rtn;
}
// v2025-03-06
WS *iws_cats(
	UINT size, // 要素数(n+1)
	...		   // ary[0..n]
)
{
	WS **rtnAry = icalloc_WS_ary(size);
	UINT rtnLen = 0;
	va_list va;
	va_start(va, size);
	for (UINT _u1 = 0; _u1 < size; _u1++)
	{
		rtnAry[_u1] = va_arg(va, WS *);
		rtnLen += iwn_len(rtnAry[_u1]);
	}
	va_end(va);
	WS *rtn = icalloc_WS(rtnLen);
	UINT uEnd = 0;
	for (UINT _u1 = 0; _u1 < size; _u1++)
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
		P2(mp1); //=> "ABC-12300456\n"
	ifree(mp1);
*/
// v2026-06-29
MS *ims_sprintf(
	CONST MS *format,
	...)
{
	va_list va1, va2;
	va_start(va1, format);
	va_copy(va2, va1);
	INT len = vsnprintf(NULL, 0, format, va1) + 1; // '\0' 追加
	va_end(va1);
	if (len < 0)
	{
		va_end(va2);
		return icalloc_MS(0);
	}
	MS *rtn = icalloc_MS(len);
	vsnprintf(rtn, len, format, va2);
	va_end(va2);
	return rtn;
}
/* (例)
	WS *wp1 = iws_sprintf(L"ワイド文字：%S\n", L"あいうえお");
		P2W(wp1); //=> "ワイド文字：あいうえお\n"
	ifree(wp1);
*/
// v2026-06-29
WS *iws_sprintf(
	CONST WS *format,
	...)
{
	va_list va1, va2;
	va_start(va1, format);
	va_copy(va2, va1);
	INT len = vsnwprintf(NULL, 0, format, va1) + 1; // '\0' 追加
	va_end(va1);
	if (len < 0)
	{
		va_end(va2);
		return icalloc_WS(0);
	}
	WS *rtn = icalloc_WS(len);
	vsnwprintf(rtn, len, format, va2);
	va_end(va2);
	return rtn;
}
/* (例)
	// PR("あいうえお", 3); NL();
	MS *mp1 = ims_repeat("あいうえお", 3);
		P2(mp1); //=> "あいうえおあいうえおあいうえお"
	ifree(mp1);
*/
// v2026-07-20
MS *ims_repeat(
	CONST MS *str,
	UINT strRepeat)
{
	UINT uStr = imn_len(str);
	UINT uSize = uStr * strRepeat;
	MS *rtn = icalloc_MS(uSize);
	UINT u1 = 0;
	while (u1 < uSize)
	{
		memcpy((rtn + u1), str, uStr * sizeof(MS));
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
INT64 iwn_searchPos(
	WS *str,	   // 文字列
	WS *search,	   // 検索文字列
	UINT searchLen // wcslen(search)
)
{
	INT64 rtn = 0;
	while (*(str + rtn))
	{
		if (!wcsncmp((str + rtn), search, searchLen))
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
UINT iwn_searchCnt(
	WS *str,   // 文字列
	WS *search // 検索文字列
)
{
	UINT rtn = 0;
	UINT uStr = iwn_len(str);
	UINT uSearch = iwn_len(search);
	UINT uEnd = 0;
	while (uEnd < uStr)
	{
		if (!wcsncmp((str + uEnd), search, uSearch))
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
UINT64 *iwsa_searchPos(
	WS *str,	// 文字列
	WS *search, // 検索文字列
	BOOL icase	// TRUE=大文字小文字区別しない
)
{
	CONST UINT strLen = iwn_len(str);
	CONST UINT searchLen = iwn_len(search);
	if (!strLen || !searchLen)
	{
		UINT64 *rtn = icalloc_UINT64(2);
		rtn[0] = 0;
		rtn[1] = strLen;
		return rtn;
	}
	WS *str2 = NULL;
	WS *search2 = NULL;
	if (icase)
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
	while (uPos < strLen)
	{
		if (!wcsncmp((wp1 + uPos), search2, searchLen))
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
	if (icase)
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
	WS *str = L" 2025////3/26  11:19:50   ";

	P1("'");
	P1W(str);
	P2("'");
	NL();

	P2("// 分割文字消去");
	WS **wa1 = iwsa_nsplit(str, wcslen(str), TRUE, 3, L"/", L":", L" ");
		iwav_print(wa1);
	ifree2(wa1);
	NL();

	P2("// 空データを残す");
	WS **wa2 = iwsa_nsplit(str, wcslen(str), FALSE, 3, L"/", L":", L" ");
		iwav_print(wa2);
	ifree2(wa2);
	NL();
*/
// v2026-07-15
WS **iwsa_nsplit(
	WS *str,	 // 文字列
	UINT strLen, // 文字列サイズ／部分抽出可能
	BOOL rmSep,	 // TRUE=分割文字消去
	UINT size,	 // 要素数(n+1)
	...			 // ary[0..n]
)
{
	if (!str || !*str)
	{
		return icalloc_WS_ary(0);
	}

	UINT uAryRtn = 1; // 最低1個
	WS **arySep = icalloc_WS_ary(size);

	va_list va1;
	va_start(va1, size);
	for (UINT _u1 = 0; _u1 < size; _u1++)
	{
		arySep[_u1] = va_arg(va1, WS *);
		uAryRtn += iwn_searchCnt(str, arySep[_u1]);
	}
	va_end(va1);

	WS **rtn = icalloc_WS_ary(uAryRtn);

	// 分割したいワイド文字列（wcstokは元の文字列を破壊するため新規作成）
	WS *src = iws_nclone(str, strLen);

	// 分割文字列 '\a' に置き換え
	for (UINT _u1 = 0; _u1 < size; _u1++)
	{
		WS *wp1 = iws_replace(src, arySep[_u1], (WS *)L"\a", FALSE);
		ifree(src);
		src = wp1;
	}
	/// P2W(src);

	// 分割文字消去
	if (rmSep == TRUE)
	{
		UINT uBgn = 0;
		UINT uEnd = 0;
		for (UINT _u1 = 0, _u2 = 0; _u1 < wcslen(src); _u1++)
		{
			while (src[_u1] != L'\0' && src[_u1] == L'\a')
			{
				++_u1;
			}
			uBgn = _u1;
			while (src[_u1] != L'\0' && src[_u1] != L'\a')
			{
				++_u1;
			}
			uEnd = _u1;
			if (uBgn < uEnd)
			{
				/// P("%u, %u\n", uBgn, uEnd - uBgn);
				rtn[_u2] = iws_nclone((src + uBgn), (uEnd - uBgn));
				++_u2;
			}
		}
	}
	// 分割文字を空データとして残す
	else
	{
		for (UINT _u1 = 0, _u2 = 0; _u1 < wcslen(src) && _u2 < uAryRtn; _u1++, _u2++)
		{
			if (src[_u1] == L'\a')
			{
				rtn[_u2] = icalloc_WS(0);
			}
			else
			{
				UINT uBgn = _u1;
				while (src[_u1] != L'\0' && src[_u1] != L'\a')
				{
					++_u1;
				}
				/// P("%u, %u\n", uBgn, _u1 - uBgn);
				rtn[_u2] = iws_nclone((src + uBgn), (_u1 - uBgn));
			}
		}
	}

	ifree(src);
	ifree2(arySep);

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
// v2026-07-18
WS *iws_replace(
	WS *from,	// 文字列
	WS *before, // 変換前の文字列
	WS *after,	// 変換後の文字列／NULL（0, L'\0'）への変換も可
	BOOL icase	// TRUE=大文字小文字区別しない
)
{
	UINT uBefore = iwn_len(before);
	UINT uAfter = iwn_len(after);
	if (!after)
	{
		uAfter = 1;
	}
	UINT64 *au1 = iwsa_searchPos(from, before, icase);
	WS *rtn = icalloc_WS(wcslen(from) + ((uAfter - uBefore) * au1[0]));
	UINT fromEnd = 0;
	UINT rtnEnd = 0;
	// au1[1..n]
	UINT u1 = 1;
	while (u1 <= au1[0])
	{
		if (au1[u1] == fromEnd)
		{
			if (!after)
			{
				*(rtn + rtnEnd) = L'\0';
			}
			else
			{
				memcpy((rtn + rtnEnd), after, uAfter * sizeof(WS));
			}
			rtnEnd += uAfter;
			fromEnd += uBefore;
			++u1;
		}
		else
		{
			UINT u2 = au1[u1] - fromEnd;
			memcpy((rtn + rtnEnd), (from + fromEnd), u2 * sizeof(WS));
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
MS *ims_IntToMs(
	INT64 num // 整数
)
{
	INT iSign = 1;
	if (num < 0)
	{
		iSign = -1;
		num = -num;
	}
	MS *pNum = ims_sprintf("%lld", num);
	UINT64 uNum = strlen(pNum);
	MS *rtn = icalloc_MS(uNum << 1);
	MS *pEnd = rtn;
	if (iSign < 0)
	{
		*pEnd = '-';
		++pEnd;
	}
	UINT64 u1 = uNum;
	UINT64 u2 = u1 % 3;
	MS *p1 = pNum;
	if (u2 > 0)
	{
		pEnd += imn_ncpy(pEnd, p1, u2);
	}
	p1 += u2;
	u1 -= u2;
	while (u1 > 0)
	{
		if (u2)
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
MS *ims_DblToMs(
	DOUBLE num, // 小数
	UINT uDigit // 小数点桁数
)
{
	if (!uDigit)
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
MS *ims_strip(
	MS *str,
	BOOL bStripLeft,
	BOOL bStripRight)
{
	if (!str)
	{
		return icalloc_MS(0);
	}
	INT iAll = strlen(str);
	// 末尾 -> 先頭
	INT iIndexR = iAll - 1;
	if (bStripRight)
	{
		while (str[iIndexR] == '\n' || str[iIndexR] == '\r' || str[iIndexR] == ' ' || str[iIndexR] == '\t' || str[iIndexR] == '\f' || str[iIndexR] == '\v')
		{
			if (!iIndexR)
			{
				return icalloc_MS(0);
			}
			--iIndexR;
		}
	}
	// 先頭 -> 末尾
	INT iIndexL = 0;
	if (bStripLeft)
	{
		while (str[iIndexL] == ' ' || str[iIndexL] == '\t' || str[iIndexL] == '\n' || str[iIndexL] == '\r' || str[iIndexL] == '\f' || str[iIndexL] == '\v')
		{
			++iIndexL;
			if (!str[iIndexL])
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
WS *iws_cutYenR(
	WS *path)
{
	WS *rtn = iws_clone(path);
	if (!*rtn)
	{
		return rtn;
	}
	WS *pEnd = rtn + wcslen(rtn) - 1;
	while (*pEnd == L'\\' || *pEnd == L'/')
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
WS *iws_withoutESC(
	WS *str)
{
	WS *rtn = icalloc_WS(iwn_len(str));
	WS *uEnd = rtn;
	while (*str)
	{
		// \033[(NUM;NUM)(BYTE)
		while (*str == '\033' && *(str + 1) == '[')
		{
			// '\033' + '['
			str += 2;
			// (NUM;NUM)
			while ((*str >= '0' && *str <= '9') || *str == ';')
			{
				++str;
			}
			// (BYTE)
			if ((*str >= 'A' && *str <= 'Z') || (*str >= 'a' && *str <= 'z'))
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
UINT iwan_size(
	WS **ary)
{
	UINT rtn = 0;
	while (*ary)
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
UINT iwan_strlen(
	WS **ary)
{
	UINT rtn = 0;
	while (*ary)
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
INT iwan_sort_Asc(
	CONST VOID *arg1,
	CONST VOID *arg2)
{
	return wcscmp(*(WS **)arg1, *(WS **)arg2);
}
/* (例)
	// 順ソート／大小文字区別しない
	qsort($ARGV, $ARGC, sizeof(WS*), iwan_sort_iAsc);
	iwav_print($ARGV);
*/
// v2023-07-19
INT iwan_sort_iAsc(
	CONST VOID *arg1,
	CONST VOID *arg2)
{
	return wcsicmp(*(WS **)arg1, *(WS **)arg2);
}
/* (例)
	// 逆順ソート／大小文字区別する
	qsort($ARGV, $ARGC, sizeof(WS*), iwan_sort_Desc);
	iwav_print($ARGV);
*/
// v2023-07-19
INT iwan_sort_Desc(
	CONST VOID *arg1,
	CONST VOID *arg2)
{
	return -(wcscmp(*(WS **)arg1, *(WS **)arg2));
}
/* (例)
	// 逆順ソート／大小文字区別しない
	qsort($ARGV, $ARGC, sizeof(WS*), iwan_sort_iDesc);
	iwav_print($ARGV);
*/
// v2023-07-19
INT iwan_sort_iDesc(
	CONST VOID *arg1,
	CONST VOID *arg2)
{
	return -(wcsicmp(*(WS **)arg1, *(WS **)arg2));
}
//---------------------
// 配列を文字列に変換
//---------------------
/* (例)
	// 引数を", "で結合
	P2W(iwas_njoin($ARGV, L", ", 0, $ARGC));
*/
// v2024-03-22
WS *iwas_njoin(
	WS **ary,		 // 配列
	CONST WS *token, // 区切文字
	UINT start,		 // 取得位置
	UINT count		 // 個数
)
{
	UINT uAry = iwan_size(ary);
	if (!uAry || start >= uAry || !count)
	{
		return icalloc_WS(0);
	}
	if (!token)
	{
		token = L"";
	}
	UINT uToken = wcslen(token);
	WS *rtn = icalloc_WS(iwan_strlen(ary) + (uAry * uToken));
	WS *pEnd = rtn;
	UINT u1 = 0;
	while (u1 < uAry && count > 0)
	{
		if (u1 >= start)
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
	ifree2(wa1);
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
	ifree2(wa1);
*/
// v2025-03-22
WS **iwaa_uniq(
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
	while (u1 < uArySize)
	{
		if (*ary[u1] && iAryFlg[u1] > -1)
		{
			iAryFlg[u1] = 1; // 仮
			u2 = u1 + 1;
			while (u2 < uArySize)
			{
				if (icase)
				{
					if (!_wcsicmp(ary[u1], ary[u2]))
					{
						iAryFlg[u2] = -1; // NG
					}
				}
				else
				{
					if (!wcscmp(ary[u1], ary[u2]))
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
	while (u1 < uArySize)
	{
		if (iAryFlg[u1] == 1)
		{
			++uAryUsed;
		}
		++u1;
	}
	WS **rtn = icalloc_WS_ary(uAryUsed);
	u1 = 0;
	u2 = 0;
	while (u1 < uArySize)
	{
		if (iAryFlg[u1] == 1)
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
	ifree2(wa1);
*/
// v2026-07-15
WS **iwaa_getDirFile(
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
	while (u1 < uArySize)
	{
		if (iType == 0 || iType == 1)
		{
			if (iF_chkDirName(wa1[u1]) && PathFileExistsW(wa1[u1]))
			{
				rtn[u2] = iF_getAPath(wa1[u1]);
				++u2;
			}
		}
		if (iType == 0 || iType == 2)
		{
			if (!iF_chkDirName(wa1[u1]) && PathFileExistsW(wa1[u1]))
			{
				WS *wp1 = icalloc_WS(IMAX_PATHW);
				_wfullpath(wp1, wa1[u1], IMAX_PATHW);
				rtn[u2] = wp1;
				++u2;
			}
		}
		++u1;
	}
	ifree2(wa1);
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
	ifree2(wa1);
*/
// v2025-03-22
WS **iwaa_higherDir(
	WS **ary)
{
	WS **rtn = iwaa_getDirFile(ary, 1);
	UINT uArySize = iwan_size(rtn);
	UINT u1 = 0, u2 = 0;
	// 上位Dirを取得
	while (u1 < uArySize)
	{
		u2 = u1 + 1;
		while (u2 < uArySize)
		{
			// 前方一致／大小文字区別しない
			if (!_wcsnicmp(rtn[u2], rtn[u1], wcslen(rtn[u1])))
			{
				ifree(rtn[u2]);
				rtn[u2] = (WS *)L"";
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
	while (u1 < uArySize)
	{
		if (!*rtn[u1])
		{
			u2 = u1 + 1;
			while (u2 < uArySize)
			{
				if (*rtn[u2])
				{
					rtn[u1] = rtn[u2];
					rtn[u2] = (WS *)L"";
					break;
				}
				++u2;
			}
		}
		++u1;
	}
	// 末尾の空白をNULL詰め
	u1 = uArySize;
	while (u1 > 0)
	{
		--u1;
		if (!*rtn[u1])
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
VOID imav_print(
	MS **ary)
{
	if (!ary)
	{
		return;
	}
	UINT u1 = 0;
	while (ary[u1])
	{
		P("\033[3G\033[92m[%u]\033[0m '%s'\n", u1, ary[u1]);
		++u1;
	}
}
// v2025-01-31
VOID iwav_print(
	WS **ary)
{
	if (!ary)
	{
		return;
	}
	UINT u1 = 0;
	while (ary[u1])
	{
		P("\033[3G\033[92m[%u]\033[0m '", u1);
		P1W(ary[u1]);
		P2("'");
		++u1;
	}
}
/* (例)
	iwav_print2($ARGV, L"-- ", L"\n");
*/
// v2026-06-19
VOID iwav_print2(
	WS **ary,
	CONST WS *sLeft,
	CONST WS *sRight)
{
	if (!ary)
	{
		return;
	}
	MS *pL = W2M(sLeft);
	MS *pR = W2M(sRight);
	UINT u1 = 0;
	while (ary[u1])
	{
		P1(pL);
		P1W(ary[u1]);
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
	$struct_iVBM *iVBM = iVBM_alloc();
		P2(iVBM_getStr(iVBM)); //=> ""
	iVBM_push2(iVBM, "1234567890");
		P2(iVBM_getStr(iVBM)); //=> "1234567890"
	iVBM_clear(iVBM);
		P2(iVBM_getStr(iVBM)); //=> ""
	iVBM_push2(iVBM, "あいうえお");
		P2(iVBM_getStr(iVBM)); //=> "あいうえお"
	iVBM_push_sprintf(iVBM, "%.2f", 123456.7890);
	iVBM_push_sprintf(iVBM, "%lc", 97);
	iVBM_push_sprintf(iVBM, "%d", 2025);
		P2(iVBM_getStr(iVBM)); //=> "あいうえお123456.79a2025"
	iVBM_pop(iVBM, 5);
		P2(iVBM_getStr(iVBM)); //=> "あいうえお123456.79"
	MS *rtn = iVBM_free(iVBM);
		P2(rtn);               //=> "あいうえお123456.79"
	ifree(rtn);
*/
// v2026-07-20
$struct_iVB *iVB_alloc(
	UINT sizeOf // sizeof(MS), sizeof(WS)
)
{
	CONST UINT strLen = 512;
	$struct_iVB *iVB = ($struct_iVB *)icalloc(1, sizeof($struct_iVB), FALSE);
	iVB->sizeOf = sizeOf;
	iVB->length = 0;
	iVB->freeSize = strLen;
	iVB->str = icalloc(iVB->freeSize, iVB->sizeOf, FALSE);
	return iVB;
}
// v2026-07-19
VOID iVBM_push(
	$struct_iVBM *iVBM, // 格納場所
	CONST MS *str,		// 追記する文字列
	UINT strLen			// 追記する文字列長／strlen(str), wcslen(str)
)
{
	// CRITICAL_SECTION ロック
	iHM_CS_lock();

	if (strLen >= iVBM->freeSize)
	{
		UINT u1 = (iVBM->length + iVBM->freeSize + strLen) * 2;
		iVBM->str = irealloc_MS(iVBM->str, u1);
		iVBM->freeSize = u1 - iVBM->length - strLen;
	}
	else
	{
		iVBM->freeSize -= strLen;
	}
	memcpy(((MS *)iVBM->str + iVBM->length), str, (strLen + 1) * iVBM->sizeOf);
	iVBM->length += strLen;

	// CRITICAL_SECTION アンロック
	iHM_CS_unlock();
}
// v2026-07-19
VOID iVBW_push(
	$struct_iVBW *iVBW, // 格納場所
	CONST WS *str,		// 追記する文字列
	UINT strLen			// 追記する文字列長／strlen(str), wcslen(str)
)
{
	// CRITICAL_SECTION ロック
	iHM_CS_lock();

	if (strLen >= iVBW->freeSize)
	{
		UINT u1 = (iVBW->length + iVBW->freeSize + strLen) * 2;
		iVBW->str = irealloc_WS(iVBW->str, u1);
		iVBW->freeSize = u1 - iVBW->length - strLen;
	}
	else
	{
		iVBW->freeSize -= strLen;
	}
	memcpy(((WS *)iVBW->str + iVBW->length), str, (strLen + 1) * iVBW->sizeOf);
	iVBW->length += strLen;

	// CRITICAL_SECTION アンロック
	iHM_CS_unlock();
}
// v2026-07-18
VOID iVBM_push_sprintf(
	$struct_iVBM *iVBM,
	CONST MS *format,
	...)
{
	va_list va1, va2;
	va_start(va1, format);
	va_copy(va2, va1);
	INT len = vsnprintf(NULL, 0, format, va1);
	va_end(va1);
	if (len < 0)
	{
		va_end(va2);
		return;
	}
	MS *mp1 = icalloc_MS(len);
	vsnprintf(mp1, len + 1, format, va2);
	iVBM_push(iVBM, mp1, len);
	ifree(mp1);
	va_end(va2);
}
// v2026-07-18
VOID iVBW_push_sprintf(
	$struct_iVBW *iVBW,
	CONST WS *format,
	...)
{
	va_list va1, va2;
	va_start(va1, format);
	va_copy(va2, va1);
	INT len = vsnwprintf(NULL, 0, format, va1);
	va_end(va1);
	if (len < 0)
	{
		va_end(va2);
		return;
	}
	WS *wp1 = icalloc_WS(len);
	vsnwprintf(wp1, len + 1, format, va2);
	iVBW_push(iVBW, wp1, len);
	ifree(wp1);
	va_end(va2);
}
// v2026-07-19
VOID iVBM_pop(
	$struct_iVBM *iVBM,
	UINT strLen)
{
	// CRITICAL_SECTION ロック
	iHM_CS_lock();

	if (!strLen)
	{
		return;
	}
	if (strLen > iVBM->length)
	{
		strLen = iVBM->length;
	}
	iVBM->length -= strLen;
	iVBM->freeSize += strLen;
	iSecure_memZero(((MS *)iVBM->str + iVBM->length), (strLen * iVBM->sizeOf));

	// CRITICAL_SECTION アンロック
	iHM_CS_unlock();
}
// v2026-07-19
VOID iVBW_pop(
	$struct_iVBW *iVBW,
	UINT strLen)
{
	// CRITICAL_SECTION ロック
	iHM_CS_lock();

	if (!strLen)
	{
		return;
	}
	if (strLen > iVBW->length)
	{
		strLen = iVBW->length;
	}
	iVBW->length -= strLen;
	iVBW->freeSize += strLen;
	iSecure_memZero(((WS *)iVBW->str + iVBW->length), (strLen * iVBW->sizeOf));

	// CRITICAL_SECTION アンロック
	iHM_CS_unlock();
}
// v2026-07-19
VOID iVBM_clear(
	$struct_iVBM *iVBM)
{
	// CRITICAL_SECTION ロック
	iHM_CS_lock();

	iSecure_memZero((MS *)iVBM->str, (iVBM->length * iVBM->sizeOf));
	iVBM->freeSize += iVBM->length;
	iVBM->length = 0;

	// CRITICAL_SECTION アンロック
	iHM_CS_unlock();
}
// v2026-07-19
VOID iVBW_clear(
	$struct_iVBW *iVBW)
{
	// CRITICAL_SECTION ロック
	iHM_CS_lock();

	iSecure_memZero((WS *)iVBW->str, (iVBW->length * iVBW->sizeOf));
	iVBW->freeSize += iVBW->length;
	iVBW->length = 0;

	// CRITICAL_SECTION アンロック
	iHM_CS_unlock();
}
// v2026-07-17
VOID *iVB_free(
	$struct_iVB *iVB)
{
	VOID *rtn = iVB->str;
	ifree(iVB);
	return rtn;
}
// v2026-07-17
UINT iVB_free2(
	$struct_iVB *iVB)
{
	ifree(iVB->str);
	return ifree(iVB);
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
// (例１) 複数取得する場合
VOID ifind1(
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
VOID ifind2(
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
$struct_iFinfo *iFinfo_alloc()
{
	return ($struct_iFinfo *)icalloc(1, sizeof($struct_iFinfo), FALSE);
}
//---------------------------
// ファイル情報取得の前処理
//---------------------------
// v2025-04-02
BOOL iFinfo_init(
	$struct_iFinfo *FI,
	WIN32_FIND_DATAW *F,
	WS *dir, // "\"を付与して呼ぶ
	WS *fname)
{
	// 初期化
	*FI->sPath = 0;
	FI->uFnPos = 0;
	FI->uAttr = 0;
	FI->bType = FALSE;
	FI->ctime_cjd = 0.0;
	FI->mtime_cjd = 0.0;
	FI->atime_cjd = 0.0;
	FI->uFsize = 0;
	if (!fname)
	{
		// 後述 FI->uFname で対応
	}
	// Dir "." ".." は除外
	//   else if(fname[0] == L'.' && (! fname[1] || (fname[1] == L'.' && ! fname[2])))
	//   else if((! fname[2] && fname[1] == L'.' && fname[0] == L'.') || (! fname[1] && fname[0] == L'.'))
	else if (!wcscmp(fname, L"..") || !wcscmp(fname, L"."))
	{
		return FALSE;
	}
	// FI->uAttr
	FI->uAttr = (UINT)F->dwFileAttributes;
	// < 32768
	if (FI->uAttr >> 15)
	{
		return FALSE;
	}
	// FI->sPath
	// FI->uFname
	UINT dirLen = iwn_cpy(FI->sPath, dir);
	UINT nameLen = iwn_cpy((FI->sPath + dirLen), fname);
	// FI->bType
	// FI->uFsize
	if (FI->uAttr & FILE_ATTRIBUTE_DIRECTORY)
	{
		// Dir直下は nameLen = 0
		if (nameLen)
		{
			dirLen += nameLen + 1;
			*(FI->sPath + dirLen - 1) = L'\\'; // '\\' 付与
			*(FI->sPath + dirLen) = L'\0';	   // '\0' 付与
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
//
// 1: READONLY
//   FILE_ATTRIBUTE_READONLY
// 2: HIDDEN
//   FILE_ATTRIBUTE_HIDDEN
// 4: SYSTEM
//   FILE_ATTRIBUTE_SYSTEM
// 16: DIRECTORY
//   FILE_ATTRIBUTE_DIRECTORY
// 32: ARCHIVE
//   FILE_ATTRIBUTE_ARCHIVE
// 64: DEVICE
//   FILE_ATTRIBUTE_DEVICE
// 128: NORMAL
//   FILE_ATTRIBUTE_NORMAL
// 256: TEMPORARY
//   FILE_ATTRIBUTE_TEMPORARY
// 512: SPARSE FILE
//   FILE_ATTRIBUTE_SPARSE_FILE
// 1024: REPARSE_POINT
//   FILE_ATTRIBUTE_REPARSE_POINT
// 2048: COMPRESSED
//   FILE_ATTRIBUTE_COMPRESSED
// 8192: NOT CONTENT INDEXED
//   FILE_ATTRIBUTE_NOT_CONTENT_INDEXED
// 16384: ENCRYPTED
//   FILE_ATTRIBUTE_ENCRYPTED
//
// v2023-07-24
WS *iFinfo_attrToWS(
	UINT uAttr)
{
	WS *rtn = icalloc_WS(5);
	rtn[0] = (uAttr & FILE_ATTRIBUTE_DIRECTORY ? 'd' : '-'); // 16=Dir
	rtn[1] = (uAttr & FILE_ATTRIBUTE_READONLY ? 'r' : '-');	 //  1=ReadOnly
	rtn[2] = (uAttr & FILE_ATTRIBUTE_HIDDEN ? 'h' : '-');	 //  2=Hidden
	rtn[3] = (uAttr & FILE_ATTRIBUTE_SYSTEM ? 's' : '-');	 //  4=System
	rtn[4] = (uAttr & FILE_ATTRIBUTE_ARCHIVE ? 'a' : '-');	 // 32=Archive
	return rtn;
}
// v2024-01-06
DOUBLE iFinfo_ftimeToCjd(
	FILETIME ftime)
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
//
// [2024-01-13]
//   フォルダ名／ファイル名を引数とする処理は WS(=WideChar)版 を使用する。
//   理由は以下のとおり。
//   ・[MS]漢字等を含むとき fopen() がエラーを返す／[WS]_wfopen() はOK
//   ・[MS]文字列走査が複雑になる／[WS]１文字毎に走査可能
//
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
BOOL iF_chkBinfile(
	WS *Fn)
{
	BOOL rtn = FALSE;
	FILE *iFp = _wfopen(Fn, L"rb");
	if (iFp)
	{
		INT c = 0;
		while ((c = getc(iFp)) != EOF)
		{
			if (!c)
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
// v2026-07-18
WS *iF_getExtPathname(
	WS *path,
	INT option // 1=拡張子付きファイル名／2=拡張子なしファイル名
)
{
	UINT uPath = iwn_len(path);
	if (!uPath)
	{
		return icalloc_WS(0);
	}
	WS *rtn = icalloc_WS(uPath);
	// Dir or File ?
	if (iF_chkDirName(path))
	{
		if (option == 0)
		{
			memcpy(rtn, path, uPath * sizeof(WS));
		}
	}
	else
	{
		switch (option)
		{
		// path
		case (0):
			memcpy(rtn, path, uPath * sizeof(WS));
			break;
		// name + ext
		case (1):
			iwv_cpy(rtn, PathFindFileNameW(path));
			break;
		// name
		case (2):
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
WS *iF_getAPath(
	WS *path)
{
	WS *p1 = iws_cutYenR(path);
	if (!*p1)
	{
		return p1;
	}
	WS *rtn = NULL;
	// "c:" のような表記は特別処理
	if (p1[1] == ':' && wcslen(p1) == 2 && iF_chkDirName(p1))
	{
		rtn = wcscat(p1, L"\\");
	}
	else
	{
		rtn = icalloc_WS(IMAX_PATHW);
		if (iF_chkDirName(p1) && _wfullpath(rtn, p1, IMAX_PATHW))
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
WS *iF_getRPath(
	WS *path)
{
	WS *rtn = iws_cutYenR(path);
	if (!*rtn)
	{
		return rtn;
	}
	if (iF_chkDirName(path))
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
UINT iF_mkdir(
	WS *path)
{
	UINT rtn = 0;
	WS *pBgn = iws_cats(2, path, L"\\");
	UINT uEnd = 0;
	while (*(pBgn + uEnd))
	{
		if (*(pBgn + uEnd) == '\\')
		{
			WS *wp1 = iws_nclone(pBgn, uEnd);
			if (CreateDirectoryW(wp1, NULL))
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
// v2026-07-15
WS **iF_trash(
	WS *path)
{
	WS **rtn = icalloc_WS_ary(0);
	if (!path)
	{
		return rtn;
	}
	// '\t' '\r' '\n' で分割
	WS **wa1 = iwsa_split(path, TRUE, 3, L"\t", L"\r", L"\n");
	// Uniq
	WS **wa2 = iwaa_uniq(wa1, TRUE);
	CONST UINT uAwp2Size = iwan_size(wa2);
	if (uAwp2Size)
	{
		SHFILEOPSTRUCTW sfos;
		ZeroMemory(&sfos, sizeof(SHFILEOPSTRUCTW));
		sfos.hwnd = 0;
		sfos.wFunc = FO_DELETE;
		sfos.fFlags = FOF_ALLOWUNDO | FOF_NO_UI;
		rtn = irealloc_WS_ary(rtn, uAwp2Size);
		for (UINT _u1 = 0, _u2 = 0; _u1 < uAwp2Size; _u1++)
		{
			// Dir/File Exist?
			// フォルダごと移動されたファイルは、ここでは存在しない。
			if (iF_chkExistPath(wa2[_u1]))
			{
				sfos.pFrom = wa2[_u1];
				if (!SHFileOperationW(&sfos))
				{
					rtn[_u2++] = iws_clone(wa2[_u1]);
				}
			}
		}
	}
	ifree2(wa2);
	ifree2(wa1);
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
WS *iF_read(
	FILE *iFp)
{
	CONST UINT BufLen = 64;
	UINT BufSize = BufLen;
	MS *Buf = icalloc_MS(BufSize);
	UINT uEnd = 0;
	UINT uCnt = 0;
	// Win32API ReadFile() は日本語表示しないので使用しない
	while ((uCnt = fread((Buf + uEnd), sizeof(MS), BufLen, iFp)))
	{
		uEnd += uCnt;
		if (uEnd >= BufSize)
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
INT NS_AFTER[2] = {2299161, 15821015};
//---------------------------
// 曜日表示設定 [7] = Err値
//---------------------------
WS *WDAYS[8] = {(WS *)L"Su", (WS *)L"Mo", (WS *)L"Tu", (WS *)L"We", (WS *)L"Th", (WS *)L"Fr", (WS *)L"Sa", (WS *)L"**"};
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
BOOL idate_chk_ymdhnsW(
	WS *str)
{
	if (!str || !*str)
	{
		return FALSE;
	}
	UINT u1 = 0;
	while (*str)
	{
		if ((*str >= '0' && *str <= '9') || *str == ':' || *str == ' ' || *str == '+')
		{
		}
		else if (*str == '/' || *str == '-')
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
	P3(idate_chk_uruu(2012)); //=> TRUE
	P3(idate_chk_uruu(2013)); //=> FALSE
*/
// v2026-06-26
BOOL idate_chk_uruu(
	INT i_y // 年
)
{
	// 400で割り切れる年は閏年
	if (i_y % 400 == 0)
	{
		return TRUE;
	}
	// 100で割り切れる年は平年
	if (i_y % 100 == 0)
	{
		return FALSE;
	}
	// 4で割り切れる年は閏年
	return (i_y % 4 == 0) ? TRUE : FALSE;
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
// v2026-06-26
VOID idate_cnv_month(
	INT *i_y,		  // 年
	INT *i_m,		  // 月
	CONST INT from_m, // 開始月
	CONST INT to_m	  // 終了月
)
{
	while (*i_m < from_m)
	{
		*i_m += 12;
		*i_y -= 1;
	}
	while (*i_m > to_m)
	{
		*i_m -= 12;
		*i_y += 1;
	}
}
//---------------
// 月末日を返す
//---------------
/* (例)
	P3(idate_month_end(2012, 2)); //=> 閏29
	P3(idate_month_end(2013, 2)); //=> 28
*/
// v2024-01-21
INT idate_month_end(
	INT i_y, // 年
	INT i_m	 // 月
)
{
	idate_cnv_month1(&i_y, &i_m);
	INT i_d = MDAYS[i_m];
	// 閏２月末日
	if (i_m == 2 && idate_chk_uruu(i_y))
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
BOOL idate_chk_month_end(
	INT i_y, // 年
	INT i_m, // 月
	INT i_d	 // 日
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
// v2026-07-16
INT *idate_WsToiAryYmdhns(
	WS *str // (例) "2012-03-12 13:40:00"
)
{
	CONST UINT uArySize = 6;
	INT *rtn = icalloc_INT(uArySize);
	if (!str)
	{
		return rtn;
	}
	INT i1 = 1;
	if (*str == '-')
	{
		i1 = -1;
		++str;
	}
	WS **wa1 = iwsa_split(str, TRUE, 5, L"/", L".", L"-", L":", L" ");
	UINT u1 = 0;
	while (u1 < uArySize && wa1[u1])
	{
		rtn[u1] = _wtoi(wa1[u1]);
		++u1;
	}
	ifree2(wa1);
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
INT idate_ymdToINT(
	INT i_y, // 年
	INT i_m, // 月
	INT i_d	 // 日
)
{
	INT i1 = 1;
	if (i_y < 0)
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
DOUBLE idate_ymdhnsToCjd(
	INT i_y, // 年
	INT i_m, // 月
	INT i_d, // 日
	INT i_h, // 時
	INT i_n, // 分
	INT i_s	 // 秒
)
{
	DOUBLE cjd = 0.0;
	INT i1 = 0, i2 = 0;
	idate_cnv_month1(&i_y, &i_m);
	// 1m=>13m, 2m=>14m
	if (i_m <= 2)
	{
		i_y -= 1;
		i_m += 12;
	}
	// 空白日
	i1 = idate_ymdToINT(i_y, i_m, i_d);
	if (i1 > NS_BEFORE[1] && i1 < NS_AFTER[1])
	{
		return NS_BEFORE[0];
	}
	// ユリウス通日
	cjd = floor((DOUBLE)(365.25 * (i_y + 4716)) + floor(30.6001 * (i_m + 1)) + i_d - 1524.0);
	if ((INT)cjd >= NS_AFTER[0])
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
VOID idate_cjdToYmdhns(
	$struct_iDV *IDV,
	DOUBLE cjd)
{
	INT i_h = 0, i_n = 0, i_s = 0;
	// 小数点部分を抽出
	//   [sec]   [補正前]  [cjd]
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
	if ((INT64)cjd >= NS_AFTER[0])
	{
		i1 = (INT64)floor((cjd - 1867216.25) / 36524.25);
		iCJD += (i1 - (INT64)floor(i1 / 4.0) + 1);
	}
	i1 = iCJD + 1524;
	i2 = (INT64)floor((i1 - 122.1) / 365.25);
	i3 = (INT64)floor(365.25 * i2);
	i4 = (INT64)((i1 - i3) / 30.6001);
	i_d = (i1 - i3 - (INT64)floor(30.6001 * i4));
	if (i4 <= 13)
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
INT idate_cjd_iWday(
	DOUBLE cjd)
{
	return (INT)((INT)cjd + 1) % 7;
}
//------------------------------------------
// cjd通日から 日="Su", 月="Mo", ...を返す
//------------------------------------------
// v2022-08-20
WS *idate_cjd_Wday(
	DOUBLE cjd)
{
	return WDAYS[idate_cjd_iWday(cjd)];
}
//------------------------------
// cjd通日から年内の通日を返す
//------------------------------
// v2024-01-21
INT idate_cjd_yeardays(
	DOUBLE cjd)
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
VOID idate_add(
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
	if (add_y != 0 || add_m != 0)
	{
		i1 = (INT)idate_month_end((IDV->y + add_y), (IDV->m + add_m));
		if (IDV->d > (INT)i1)
		{
			IDV->d = (INT)i1;
		}
		iDV_set(IDV, (IDV->y + add_y), (IDV->m + add_m), IDV->d, IDV->h, IDV->n, IDV->s);
	}
	if (add_d)
	{
		// 【重要】CJDに変換してから加算
		iDV_set2(IDV, add_d + idate_ymdhnsToCjd(IDV->y, IDV->m, IDV->d, IDV->h, IDV->n, IDV->s));
	}
	if (add_h != 0 || add_n != 0 || add_s != 0)
	{
		iDV_set(IDV, IDV->y, IDV->m, IDV->d, (IDV->h + add_h), (IDV->n + add_n), (IDV->s + add_s));
	}
	// 使用しないものは初期化しておく
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
VOID idate_diff(
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

	if (cjd1 > cjd2)
	{
		IDV->sign = FALSE;
		DOUBLE cjd = cjd1;
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
	while (IDV->s < 0)
	{
		IDV->s += 60;
		IDV->n -= 1;
	}
	while (IDV->n < 0)
	{
		IDV->n += 60;
		IDV->h -= 1;
	}
	while (IDV->h < 0)
	{
		IDV->h += 24;
		IDV->d -= 1;
	}

	// d 調整
	// 前処理
	if (IDV->d < 0)
	{
		IDV->m -= 1;
		if (IDV->m < 0)
		{
			IDV->m += 12;
			IDV->y -= 1;
		}
	}

	// 本処理
	$struct_iDV *IDV3 = iDV_alloc();
	if (IDV->sign)
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
	if (IDV->sign										  // ＋のとき
		&& i3 <= i4										  // (例) "12:00:00 =< 23:00:00"
		&& idate_chk_month_end(IDV2->y, IDV2->m, IDV2->d) // (例) "06-30" は月末日？
		&& IDV1->d > IDV2->d							  // (例) "05-31" > "06-30"
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
#include "lib_iwmutil2.h"

INT irand_INT(
	INT min,
	INT max
)
{
	return (min + (INT)(rand() * (max - min + 1.0) / (1.0 + RAND_MAX)));
}
// v2026-05-31
VOID iDV_checker(
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

INT main()
{
	imain_begin();
	iDV_checker(1, 2000, 100);
	imain_end();
}
*/
//
// Ymdhns
//   %a : 曜日(例:Su)
//   %A : 曜日数(例:0)
//   %c : 年内の通算日
//   %C : CJD通算日
//   %J : JD通算日
//   %e : 年内の通算週
//
// Diff
//   %Y : 通算年
//   %M : 通算月
//   %D : 通算日
//   %H : 通算時
//   %N : 通算分
//   %S : 通算秒
//   %W : 通算週
//   %w : 通算週の余日
//
// 共通
//   %g : Sign "-" "+"
//   %G : Sign "-" のみ
//   %y : 年(0000)
//   %m : 月(00)
//   %d : 日(00)
//   %h : 時(00)
//   %n : 分(00)
//   %s : 秒(00)
//   %% : "%"
//   \a
//   \n
//   \t
//
/* (例) ymdhns
	$struct_iDV *IDV = iDV_alloc();
		iDV_set(IDV, 2024, 0, 50, 60, 0, 0);
		WS *wp1 = idate_format(L"%g%y-%m-%d(%a) %C\n", IDV->sign, IDV->y, IDV->m, IDV->d, IDV->h, IDV->n, IDV->s, IDV->days);
			P2W(wp1); //=> "+2024-01-21(Su) 2460331.500000"
		ifree(wp1);
	iDV_free(IDV);
*/
// v2026-07-17
WS *idate_format(
	CONST WS *format,
	BOOL bSign,	 // TRUE='+'／FALSE='-'
	INT iY,		 // 年
	INT iM,		 // 月
	INT iD,		 // 日
	INT iH,		 // 時
	INT iN,		 // 分
	INT iS,		 // 秒
	DOUBLE dDays // 通算日／idate_diff()で使用
)
{
	if (!format)
	{
		return icalloc_WS(0);
	}
	$struct_iVBW *iVBW = iVBW_alloc();
	DOUBLE cjd = (dDays ? dDays : idate_ymdhnsToCjd(iY, iM, iD, iH, iN, iS));
	INT64 iDays = (INT64)cjd;
	// 符号チェック
	if (iY < 0)
	{
		bSign = FALSE;
		iY = -(iY);
	}
	while (*format)
	{
		if (*format == '%')
		{
			++format;
			switch (*format)
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
				iVBW_push_sprintf(iVBW, L"%d", iDays);
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
				iVBW_push_sprintf(iVBW, L"%d", (iDays / 7));
				break;
			case 'w': // 通算週の余日
				iVBW_push_sprintf(iVBW, L"%d", (iDays % 7));
				break;
			// 共通
			case 'g': // Sign "-" "+"
				iVBW_push2(iVBW, (bSign ? L"+" : L"-"));
				break;
			case 'G': // Sign "-" のみ
				if (!bSign)
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
		else if (*format == '\\')
		{
			++format;
			switch (*format)
			{
			case ('a'):
				iVBW_push2(iVBW, L"\a");
				break;
			case ('n'):
				iVBW_push2(iVBW, L"\n");
				break;
			case ('t'):
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
WS *idate_format_cjdToWS(
	WS *format, // NULL=IDATE_FORMAT_STD
	DOUBLE cjd)
{
	if (!format)
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
//
// 大文字 => "yyyy-mm-dd 00:00:00"
// 小文字 => "yyyy-mm-dd hh:nn:ss"
//   Y, y : 年
//   M, m : 月
//   W, w : 週
//   D, d : 日
//   H, h : 時
//   N, n : 分
//   S, s : 秒
// SQL, ワイルドカード
//   %, *
//
/* (例)
//	Now: 2026-06-29 14:59:14
//
//	小文字指示子: 時間を扱う
//	#{-10d}
//	'2026-06-19 14:59:14'
//
//	大文字指示子: 日を扱う
//	#{-10D}
//	'2026-06-19 00:00:00'
//
//	'%' の扱い（'*'も同じ）
//	#{-5%}
//	#{-5y%}
//	#{-5m%}
//	#{-5d%}
//	#{-5w%}
//	#{-5h%}
//	#{-5n%}
//	#{-5s%}
//	'2026-06-29 14:59:14'
//	'2021-%'
//	'2026-01-%'
//	'2026-06-24 %'
//	'2026-05-25 %'
//	'2026-06-29 09:%'
//	'2026-06-29 14:54:%'
//	'2026-06-29 14:59:09'

#include "lib_iwmutil2.h"

VOID idate_replace_format_ymdhns_test()
{
	WS *cases[] = {
		L"小文字指示子: 時間を扱う",
		L"#{-10d}",
		L"大文字指示子: 日を扱う",
		L"#{-10D}",
		L"'%' の扱い（'*'も同じ）",
		L"#{-5%}\n#{-5y%}\n#{-5m%}\n#{-5d%}\n#{-5w%}\n#{-5h%}\n#{-5n%}\n#{-5s%}"
	};

	INT num_cases = sizeof(cases) / sizeof(cases[0]);

	// 基本日時
	INT *ai1 = idate_NOW();

	WS *wpDate1 = idate_format_ymdhns(IDATE_FORMAT_STD, ai1[0], ai1[1], ai1[2], ai1[3], ai1[4], ai1[5]);
	P1("\033[95m"
	   "Now: ");
	P2W(wpDate1);
	P2("\033[0m");
	ifree(wpDate1);

	for (int i = 0; i < num_cases; i += 2)
	{
		WS *result = idate_replace_format_ymdhns(
			cases[i + 1],
			L"#{", L"}",
			L"'",
			ai1[0], ai1[1], ai1[2], ai1[3], ai1[4], ai1[5]);

		MS *mp1 = W2M(cases[i]);
		MS *mp2 = W2M(cases[i + 1]);
		MS *mp3 = W2M(result);

		P(
			"\033[92m%s\033[0m\n"
			"\033[96m%s\033[0m\n"
			"%s\n\n",
			mp1,
			mp2,
			mp3);

		ifree(mp3);
		ifree(mp2);
		ifree(mp1);
		ifree(result);
	}

	ifree(ai1);
}

INT main()
{
	imain_begin();
	idate_replace_format_ymdhns_test();
	imain_end();
}
*/
// v2026-07-15
WS *idate_replace_format_ymdhns(
	WS *str,	   // 変換対象文字列
	WS *quoteBgn,  // 囲文字列 (例) "\{"／当初 "[...]" を想定していたが "[A-Z]" "[0-9]" で不具合なのでRubyの "#{...}" に変更
	WS *quoteEnd,  // 囲文字列 (例) "}"
	WS *add_quote, // 出力文字に追加する囲文字列 (例) "'"
	INT i_y,	   // ベース年
	INT i_m,	   // ベース月
	INT i_d,	   // ベース日
	INT i_h,	   // ベース時
	INT i_n,	   // ベース分
	INT i_s		   // ベース秒
)
{
	if (iwn_len(str) == 0 || iwn_len(quoteBgn) == 0 || iwn_len(quoteEnd) == 0)
	{
		return iws_clone(str);
	}

	if (iwn_len(add_quote) == 0)
	{
		add_quote = (WS *)L"";
	}

	// 1. 元の文字列中の "#{...}" を "\a\t...\a" に置き換える
	//    先頭の "\t" がフラグになる
	WS *wp10 = str;
	WS *wp11 = iws_replace(wp10, quoteBgn, (WS *)L"\a\t", FALSE);
	WS *wp12 = iws_replace(wp11, quoteEnd, (WS *)L"\a", FALSE);

	// 2. 配列に変換
	WS **wa1 = iwsa_split(wp12, FALSE, 1, L"\a");

	ifree(wp12);
	ifree(wp11);

	// 3. 2から空白を消去
	CONST UINT uAw1 = iwan_size(wa1);
	for (UINT _u1 = 0; _u1 < uAw1; _u1++)
	{
		// 1のフラグ "\t" がここで役立つ
		if (wa1[_u1][0] == '\t')
		{
			// 一見無謀だが、空白消去後は間違いなく短くなるので問題ない
			WS *wp1 = iws_replace(wa1[_u1], (WS *)L" ", (WS *)L"", FALSE);
			wcscpy(wa1[_u1], wp1);
			ifree(wp1);
		}
	}

	// 4. 二度手間だが3と分けて処理
	for (UINT _u1 = 0; _u1 < uAw1; _u1++)
	{
		// 1のフラグ "\t" がここで本領発揮
		if (wa1[_u1][0] == '\t')
		{
			WS *wp1 = wa1[_u1] + 1;
			INT iDt = _wtoi(wp1);

			INT add_y = 0, add_m = 0, add_d = 0, add_h = 0, add_n = 0, add_s = 0;
			BOOL bHnsZero = FALSE; // TRUE = "00:00:00"
			BOOL bAddEnd = FALSE;  // TRUE = bAdd が現れたとき

			INT iPosMove = 0;
			WS *wp2 = wp1;
			while (*wp2)
			{
				switch (*wp2)
				{
				// 年
				case 'Y': // "yyyy-mm-dd 00:00:00"
					bHnsZero = TRUE;
					[[fallthrough]];
				case 'y': // "yyyy-mm-dd hh:nn:ss"
					add_y = iDt;
					bAddEnd = TRUE;
					iPosMove = -14;
					break;
				// 月
				case 'M': // "yyyy-mm-dd 00:00:00"
					bHnsZero = TRUE;
					[[fallthrough]];
				case 'm': // "yyyy-mm-dd hh:nn:ss"
					add_m = iDt;
					bAddEnd = TRUE;
					iPosMove = -11;
					break;
				// 週
				case 'W': // "yyyy-mm-dd 00:00:00"
					bHnsZero = TRUE;
					[[fallthrough]];
				case 'w': // "yyyy-mm-dd hh:nn:ss"
					add_d = iDt * 7;
					bAddEnd = TRUE;
					iPosMove = -8;
					break;
				// 日
				case 'D': // "yyyy-mm-dd 00:00:00"
					bHnsZero = TRUE;
					[[fallthrough]];
				case 'd': // "yyyy-mm-dd hh:nn:ss"
					add_d = iDt;
					bAddEnd = TRUE;
					iPosMove = -8;
					break;
				// 時
				case 'H': // "yyyy-mm-dd 00:00:00"
					bHnsZero = TRUE;
					[[fallthrough]];
				case 'h': // "yyyy-mm-dd hh:nn:ss"
					add_h = iDt;
					bAddEnd = TRUE;
					iPosMove = -5;
					break;
				// 分
				case 'N': // "yyyy-mm-dd 00:00:00"
					bHnsZero = TRUE;
					[[fallthrough]];
				case 'n': // "yyyy-mm-dd hh:nn:ss"
					add_n = iDt;
					bAddEnd = TRUE;
					iPosMove = -2;
					break;
				// 秒
				case 'S': // "yyyy-mm-dd 00:00:00"
					bHnsZero = TRUE;
					[[fallthrough]];
				case 's': // "yyyy-mm-dd hh:nn:ss"
					add_s = iDt;
					bAddEnd = TRUE;
					break;
				}
				// [1y6m] のようなとき [1y] で解釈する
				if (bAddEnd)
				{
					break;
				}
				++wp2;
			}

			INT *ai1 = icalloc_INT(6);

			// ±日時
			$struct_iDV *IDV = iDV_alloc();
			idate_add(IDV, i_y, i_m, i_d, i_h, i_n, i_s, add_y, add_m, add_d, add_h, add_n, add_s);
			ai1[0] = IDV->y;
			ai1[1] = IDV->m;
			ai1[2] = IDV->d;
			// "00:00:00"
			if (bHnsZero)
			{
				ai1[3] = 0;
				ai1[4] = 0;
				ai1[5] = 0;
			}
			// "hh:nn:ss"
			else
			{
				ai1[3] = IDV->h;
				ai1[4] = IDV->n;
				ai1[5] = IDV->s;
			}
			iDV_free(IDV);

			WS *wpDate1 = idate_format_ymdhns(IDATE_FORMAT_STD, ai1[0], ai1[1], ai1[2], ai1[3], ai1[4], ai1[5]);

			// "%", "*" を検索
			UINT uAddStr = 0;
			for (UINT _u1 = 0; _u1 < wcslen(wp1); _u1++)
			{
				switch (wp1[_u1])
				{
				case '%':
					uAddStr = 1;
					break;
				case '*':
					uAddStr = 2;
					break;
				}
			}

			// "%", "*" を付与
			if (uAddStr > 0 && iPosMove != 0)
			{
				UINT uPos = wcslen(wpDate1);
				uPos += iPosMove;
				switch (uAddStr)
				{
				case 1: // '%'
					wpDate1[uPos] = '%';
					break;
				case 2: // '*'
					wpDate1[uPos] = '*';
					break;
				}
				++uPos;
				wpDate1[uPos] = 0;
			}

			// 区切り文字を付与して、元の文字列を上書き
			WS *wpDate2 = iws_cats(3, add_quote, wpDate1, add_quote);
			wa1[_u1] = irepalloc_WS(wa1[_u1], wpDate2, wcslen(wpDate2));

			ifree(wpDate2);
			ifree(wpDate1);
			ifree(ai1);
		}
	}

	// 5. 最終整形
	WS *rtn = icalloc_WS(iwan_strlen(wa1));
	WS *wp1 = rtn;
	for (UINT _u1 = 0; _u1 < uAw1; _u1++)
	{
		UINT uLen = wcslen(wa1[_u1]);
		memcpy(wp1, wa1[_u1], uLen * sizeof(WS));
		ifree(wa1[_u1]); // wa1の要素を解放
		wp1 += uLen;
	}
	ifree2(wa1);

	return rtn;
}
//---------------------
// 今日のymdhnsを返す
//---------------------
/* (例)
	// 今日 = 2012-06-19 00:00:00 のとき、
	INT *ai1 = idate_nowToiAryYmdhns(TRUE);  // Local       => 2012, 06, 19, 00, 00, 00, 000
		P("%04d, %02d, %02d, %02d, %02d, %02d, %03d\n", ai1[0], ai1[1], ai1[2], ai1[3], ai1[4], ai1[5], ai1[6]);
	ifree(ai1);
	INT *ai2 = idate_nowToiAryYmdhns(FALSE); // System(-9h) => 2012, 06, 18, 15, 00, 00, 000
		P("%04d, %02d, %02d, %02d, %02d, %02d, %03d\n", ai2[0], ai2[1], ai2[2], ai2[3], ai2[4], ai2[5], ai2[6]);
	ifree(ai2);
*/
// v2023-07-13
INT *idate_nowToiAryYmdhns(
	BOOL area // TRUE=LOCAL／FALSE=SYSTEM
)
{
	SYSTEMTIME st;
	if (area)
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
	idate_nowToCjd(TRUE); // Local
	idate_nowToCjd(FALSE); // System(-9h)
*/
// v2023-07-13
DOUBLE idate_nowToCjd(
	BOOL area // TRUE=LOCAL／FALSE=SYSTEM
)
{
	INT *ai = idate_nowToiAryYmdhns(area);
	INT y = ai[0], m = ai[1], d = ai[2], h = ai[3], n = ai[4], s = ai[5];
	ifree(ai);
	return idate_ymdhnsToCjd(y, m, d, h, n, s);
}
