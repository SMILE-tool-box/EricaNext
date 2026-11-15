# EricaNext

**SMILE-3 プロジェクトのデータパイプライン**

EricaNext は、SMILE-3 プロジェクトの検出器生データ（detector raw data）から、
天体解析が可能なイベント再構成（event reconstruction）までを一貫して処理する
パイプラインソフトウェアです。

ANLnext フレームワークをベースに構築されており、解析モジュールを組み合わせて
データ処理フローを構成します。

SMILE-3 の検出器の構成(μ-PIC の 4 分割、ストリップ 0.4 mm、MPPC の PSA)は
[`tutorials/0_Introduction`](tutorials/0_Introduction/Introduction.ipynb) にまとめています。

開発の引き継ぎ(環境の落とし穴、検証のやり方、残りの仕事)は [`AGENTS.md`](AGENTS.md) にあります。

## 目次

- [使い方(まずここ)](#使い方まずここ)
- [解析チェーン](#解析チェーン)
  - [設定ファイルからチェーンを組む](#設定ファイルからチェーンを組む)
  - [モジュールのつながり](#モジュールのつながり)
- [依存関係](#依存関係)
- [インストール](#インストール)
  - [0. EricaNext を取ってくる](#0-ericanext-を取ってくる)
  - [1. ANLnext を入れる](#1-anlnext-を入れる)
  - [環境を確認する](#環境を確認する)
  - [2. EricaNext をビルドする](#2-ericanext-をビルドする)
- [データの置き場所](#データの置き場所)
- [リポジトリの構成](#リポジトリの構成)

## 使い方(まずここ)

ETCC（TPC＋PSA）の解析は、次の二段階を基本にします。
生データのデコードとイベント同期を一度行い、保存したROOTを積分・較正・飛跡解析・イベント再構成で繰り返し使います。

```text
TPC・PSAの生データ → ReadData・PSA_ReadData → RawTreeWriter → raw ROOT
raw ROOT → 波形の積分・検出器の較正 → TPCの飛跡解析・PSAの吸収点計算 → ETCCイベント再構成
```

ビルドは `bash scripts/env/build_ericanext.sh` で行います。
以下はリポジトリ直下で実行します。`<…>` は役割を示す仮のパスです。自分の環境のパスに書き換えてください。第2段階の入力には第1段階で出力したraw ROOTを指定します。

```bash
# 第1段階: 同じrunのTPC・PSAを読み、同期したヒット・波形・時刻を保存する
python3 run_script/run_chain.py smile3_etcc_raw \
    --data-dir "<入力rawのフォルダ（calltt）>" \
    --extra-data-dir "<入力rawの追加フォルダ（killua）>" \
    -o "<出力raw_ROOTのpath>"

# 第2段階: 保存済みROOTから積分・較正・飛跡解析・ETCCイベント再構成を行う
python3 run_script/run_chain.py smile3_reconstruct \
    --root-input "<入力raw_ROOTのpath>" \
    -o "<出力再構成ROOTのpath>"
```

出力先の親ディレクトリは自動作成します。
第1段階では共通の `RawTreeWriter` がTPC・PSAの生情報を同じROOTへ、同じイベント順で書きます。
第2段階は設定ファイルに指定した較正を適用します。測定に対応するTPC・PSAの較正ファイルを用意してください。
較正値や解析条件を変える場合は、第2段階を再実行します。

同じrunのファイルが `calltt` と `killua` など複数フォルダに分かれている場合は、
片方を `--data-dir`、残りを `--extra-data-dir` に指定します。
このデータ配置では `calltt` が `storage_g0`、`killua` が `storage_g1` に対応します。
フォルダ名は固定ではないので、実際にファイルがあるrunのディレクトリを指定してください。
事前にファイルをコピーしたり、1つのフォルダに集めたりする必要はありません。

3フォルダ以上に分かれている場合は、`--extra-data-dir <フォルダ>` を必要な回数繰り返します。
これらは**同じrunの分割先**を指定するオプションです。異なるrunはrunごとにROOTを作ってください。
フォルダの指定は第1段階だけで、第2段階は作成済みROOTを `--root-input` に指定します。

TPCだけ・PSAだけの測定も同じ二段階で処理できます。

| 対象 | 第1段階：生データ → ROOT | 第2段階：ROOT → 解析 |
| --- | --- | --- |
| ETCC（TPC＋PSA） | `smile3_etcc_raw` | `smile3_reconstruct --root-input <ROOT>` |
| TPCのみ | `smile3_tpc_raw` | `smile3_tpc_only --root-input <ROOT>` |
| PSAのみ | `smile3_psa_raw` | `smile3_psa --root-input <ROOT>` |

`--root-input` は1回の実行につき1ファイルです。複数runはrunごとに第1・第2段階を実行します。
ETCC再構成にはTPC・PSA両方の生情報を含むROOTが必要です。

較正・診断にも第1段階のROOTを使います。例えばTPCの1-hitスペクトルとFADC積算は次で作れます。

```bash
python3 scripts/calib/make_tpc_spec1hit.py \
    "<入力raw_ROOTのpath>" -o "<出力スペクトルROOTのpath>"
python3 scripts/analysis/make_tpc_fadc_hist.py \
    "<入力raw_ROOTのpath>" -o "<出力FADC積算ROOTのpath>"
```

TPC・PSAの較正と飛跡積算のノートブックは [Make_calibration](tutorials/4_MakeCalibration/README.md)、
FADC波形積算などの診断は [Diagnostics](tutorials/2_Diagnostics/) を参照してください。
入力には第1段階のROOTを指定します。同じ測定条件の複数runをまとめる場合は、各ノートブックの入力一覧に列挙します。

第1段階は全ヒット・レコード・全チャンネルの波形・時刻を保存します。閾値やイベント選別は第2段階以降で適用します。

第1・第2段階を実行するスクリプトは [`run_script/run_chain.py`](run_script/run_chain.py) です。
最初の引数に [`config/stage1/`](config/stage1/) または [`config/stage2/`](config/stage2/) に置いた設定の名前を渡します。

- 設定ファイルは名前(拡張子なし)で書くと `config/` 配下の段階別フォルダも含めて探します。`.json` のパスを直接書いても構いません。
- 値を変えるときは、設定ファイルをコピーして書き換えるか、`--set モジュール.パラメータ=値` でその場だけ上書きします。
- 設定ファイルの各モジュールには、全パラメータの説明(`_doc`)と単位(`_units`)を書いています。書き方は [`config/README.md`](config/README.md)。
- 失敗すると終了コード 1 で終わり、出力ファイルは残しません。
- スクリプトの詳しい使い方は [`run_script/README.md`](run_script/README.md)。

| `run_chain.py` の引数 | 意味 |
| --- | --- |
| `<設定>` | 設定ファイル(必須) |
| `-o` | 出力の ROOT ファイル(必須) |
| `-n` | 処理するイベント数。省略すると全イベント |
| `--data-dir` | ランのディレクトリ(設定ファイルの `ReadData.data_dir`、`PSA_ReadData.data_dir` を置き換える) |
| `--extra-data-dir <フォルダ>` | 同じrunのファイルがある追加フォルダ。`calltt`・`killua`などに分かれているときに指定。何回でも指定可能 |
| `--root-input <ROOT>` | 第1段階で作成したROOTを後段解析へ入力。`--data-dir`・`--extra-data-dir`とは併用しない |
| `--set モジュール.パラメータ=値` | パラメータの上書き(何回でも)。小数は `10.0` のように書く |

## 解析チェーン

### 設定ファイルからチェーンを組む

```mermaid
flowchart LR
  CFG["config/stage1/・stage2/<br>&lt;名前&gt;.json<br>chain: モジュールの順番<br>parameters: パラメータ(_doc, _units)"]
  EC["config/ericanext_config.py<br>{data_root} などを置き換え<br>_ で始まるキーを捨てる"]
  RUN["run_script/run_chain.py<br>--data-dir, --set で上書き"]
  CH["ANLNext の AnalysisChain<br>SaveFile → chain の順にモジュール"]
  OUT[("出力の ROOT ファイル<br>(成功したときだけ)")]
  CFG --> EC --> RUN --> CH --> OUT
```

モジュールの順番とパラメータはすべて設定ファイルに書き、スクリプトには書きません。
新しい解析の組み方を作るときは、`config/` に設定ファイルを足します(書き方は [`config/README.md`](config/README.md))。

#### 第1段階：生データ → raw ROOT（config/stage1）

| 設定ファイル | つなぐモジュール |
| --- | --- |
| [`smile3_etcc_raw`](config/stage1/smile3_etcc_raw.json) | `ReadData → PSA_ReadData → RawTreeWriter`（TPC＋PSA） |
| [`smile3_tpc_raw`](config/stage1/smile3_tpc_raw.json) | `ReadData → RawTreeWriter`（TPCのみ） |
| [`smile3_psa_raw`](config/stage1/smile3_psa_raw.json) | `PSA_ReadData → RawTreeWriter`（PSAのみ） |

#### 第2段階：raw ROOT → 積分・較正・再構成（config/stage2）

入力ROOTを `--root-input` で指定します。先頭のreaderは保存済みの生情報を読み戻します。

| 設定ファイル | つなぐモジュール |
| --- | --- |
| [`smile3_reconstruct`](config/stage2/smile3_reconstruct.json) | `ReadData → PSA_ReadData → PSA_CalcFADC → PSA_CenterGravity → PSA_Calibration → PSA_TreeWriter → CalcFADC → TPC_Calibration → PSA_Event → TOT_Skewness → Reconstruct`（TPC＋PSA） |
| [`smile3_tpc_only`](config/stage2/smile3_tpc_only.json) | `ReadData → CalcFADC → TPC_Calibration → TOT_Skewness`（TPCのみ） |
| [`smile3_psa`](config/stage2/smile3_psa.json) | `PSA_ReadData → PSA_CalcFADC → PSA_CenterGravity → PSA_Calibration → PSA_TreeWriter`（PSAのみ） |

### モジュールのつながり

第1段階はデコードとイベント同期、ROOTへの保存を行います。

```mermaid
flowchart TD
  RAW[("生データ<br>tpc_enc_a00〜a11, c00〜c11 *.raw<br>PSA *.dat（MPPCの波形）")]
  subgraph L0["読み込み・同期"]
    RD["ReadData<br>TPC読み込み・ボード間同期"]
    PRD["PSA_ReadData<br>PSA読み込み・TPCと同期"]
  end
  subgraph L1["保存"]
    WR[RawTreeWriter]
  end
  ROOT[("raw ROOT<br>TPC・PSAの全生情報")]
  RAW --> RD
  RAW --> PRD
  RD -- "トリガーカウンタ(sync_tpc)" --> PRD
  RD -- "TPCのヒット・FADC波形" --> WR
  PRD -- "MPPCの波形" --> WR
  WR --> ROOT
```

第2段階はraw ROOTを読み、TPC・PSAそれぞれの積分・較正の結果をイベント再構成へ渡します。

```mermaid
flowchart TD
  ROOT[(raw ROOT)]
  subgraph L0["層0 読み戻し"]
    RD["ReadData<br>TPCを読み戻す"]
    PRD["PSA_ReadData<br>PSAを読み戻す"]
  end
  subgraph L1["層1 検出器ごとの処理"]
    CF[CalcFADC] --> TC[TPC_Calibration]
    TOT[TOT_Skewness]
    PCF[PSA_CalcFADC] --> PCG[PSA_CenterGravity] --> PCAL[PSA_Calibration] --> PEV[PSA_Event]
  end
  subgraph L2["層2 イベント再構成"]
    REC[Reconstruct]
  end
  subgraph L3["層3 出力"]
    OUT["Reconstruct/single_hit<br>再構成 TTree"]
    TOTT["TOT_Skewness/tot_skewness<br>電子飛跡の TTree"]
    PSAT["PSA_TreeWriter/psa<br>シンチレータのヒットの TTree"]
  end
  PRM["TPC較正ファイル<br>ゲイン・ドリフト速度"]
  PPRM["PSA較正ファイル<br>位置・温度・エネルギー・ジオメトリ"]

  ROOT --> RD
  ROOT --> PRD
  PRM -.-> TC
  PRM -.-> TOT
  PPRM -.-> PCF
  PPRM -.-> PCAL
  PPRM -.-> PEV
  RD -- "FADC 波形(48 ch × 2)" --> CF
  RD -- "TPC ヒット(パネル 0〜3)" --> TC
  RD -- "TPC ヒット・時刻" --> TOT
  PRD -- "MPPC の波形(ボード × 6 ch)" --> PCF
  CF -- "電荷" --> TC
  TC -- "Ke" --> REC
  TOT -- "Pe, Ve, Len" --> REC
  PEV -- "Eg, Pg(1 ヒットのイベント)" --> REC
  PCAL --> PSAT
  REC --> OUT
  TOT --> TOTT
```

- 実線の矢印はデータの受け渡し(ラベルは渡すもの)、点線の矢印はパラメータファイルの読み込みです。
- 実際の実行順は設定ファイルの `chain` の順です。TPC と PSA を一緒に読むときは `ReadData` を `PSA_ReadData` より前に置きます
  (`PSA_ReadData.sync_tpc: true` で `ReadData` のトリガーカウンタに合わせて読む)。
- ゲインマップは `TPC_Calibration.NoCalData: false` のときだけ読みます。
- PSA の部分は `analysis_sm3_FM/PSA_analysis` の `psa_decode.py` → `pixel_id.py` と同じ計算です
  (同じ .dat で、ヒット・peak_id・ピクセルは全部一致、hg などは Python の float32 の丸めの範囲で一致)。
  PSAの較正ファイルの内容・作り方は [`calib/psa/README.md`](calib/psa/README.md) を参照してください。
- `Reconstruct` は `PSA_Event`(`PSA_SingleHit` のイベント)を使います。
- ジオメトリ `calib/psa/geometry.csv` は、CAD の STEP ファイルと HA の配置図から `scripts/calib/make_psa_geometry.py` で作りました(ピクセルの向きはデータでの確認がまだ)。
  エネルギー(`energy_haNN.csv`)とジオメトリが無いブロックのヒットは、`PSA_Event` が使いません。
  `CalcFADC` と `TPC_Calibration` はイベントを飛ばしません。`TPC_Calibration` は TPC のエネルギーがあるイベントに `TPC_Ee_Valid` の印を付けます。
  `Reconstruct` は、シンチレータの 1 ヒット(`PSA_SingleHit`)と TPC のエネルギー(`TPC_Ee_Valid`)の両方があるイベントだけを書きます。
  飛跡(`TOT_Ve_TRA`)があれば、幾何の α(`cos_alpha_geo`)と入射ガンマ線の方向(`Vi`)も書きます。
  `source_position` を書けば ARM と SPD も出します。TPC の位置とシンチレータのジオメトリが同じ座標系かは、まだ確かめていません。

| モジュール | すること | 主な出力(`SaveFile` の ROOT ファイルの中) |
| --- | --- | --- |
| `SaveFile` | 出力の ROOT ファイルを開く(`run_chain.py` が先頭に必ずつなぐ) | |
| `ReadData` | 第1段階では生データのボード間同期、第2段階ではROOTからTPC情報を読み戻す | 波形・ヒットは `RawTreeWriter` で保存。積算・選別・描画は保存後に行う |
| `CalcFADC` | FADC 波形からベースラインを引いて電荷を積分し、`TPC_Calibration` に渡す | なし(ベースライン・電荷の分布と 1-hit スペクトルは、生データのツリーから `scripts/calib/make_tpc_spec1hit.py` で後段で作る) |
| `TPC_Calibration` | 電荷とゲインマップから反跳電子のエネルギー | `TPC_Calibration/`: `Spectrum` など |
| `TOT_Skewness` | 飛跡から散乱点 `Pe`、電子の方向 `Ve`、飛程 `Len` | `TOT_Skewness/tot_skewness`(TTree) |
| `PSA_ReadData` | PSA の生データ `ha<NN>_<番号>.dat` を全ボード読み、トリガーカウンタでイベントをそろえる(`ReadData` があればそれに合わせる)。別の storage は `extra_data_dirs` | `PSA_ReadData/HitPattern` |
| `PSA_CalcFADC` | 波形のペデスタルを引いてピークの周りを積分(`hg`, `lg`)、温度センサーの較正 | `PSA_CalcFADC/HG_haNN`, `LG_haNN` |
| `PSA_CenterGravity` | 4 隅の信号の重心 `x_hg`, `y_hg`(`x_lg`, `y_lg`) | `PSA_CenterGravity/Map_haNN_chC` |
| `PSA_Calibration` | 位置の補正(64 点の薄板スプライン)、ピクセル (n, m)、エネルギー | `PSA_Calibration/Corrected_haNN_chC`, `PixelHG_haNN`, `Energy_haNN` |
| `PSA_TreeWriter` | PSA のヒットを 1 ヒット 1 行で書く | `PSA_TreeWriter/psa`(TTree) |
| `PSA_Event` | ピクセルとジオメトリから吸収点 `Pg` [mm]、使えるヒットの数(`PSA_SingleHit` / `MultiHit` / `NoHits`)。`Reconstruct` に渡す | `PSA_Event/NHits`, `SingleHitEnergy` |
| `Reconstruct` | TPC と PSA(`PSA_Event`)を合わせたイベント再構成。運動学の角度、飛跡があれば幾何の α と入射ガンマ線の方向 | `Reconstruct/single_hit`(TTree、`evt_id` = トリガーカウンタ) |
| `RawTreeWriter` | チェーンにあるTPC・PSAの生情報を保存する。`ReadData`・`PSA_ReadData`の後につなぐ | `TPC_RawTreeWriter/raw`, `TPC_RawTreeWriter/run_info`(TTree) |

---

## 依存関係

| ソフト | 版 | |
| --- | --- | --- |
| [ANLnext](https://github.com/odakahirokazu/ANLNext) | c22eb3e で確認 | インストール先を `ANLNEXT_INSTALL` に指定 |
| [ROOT](https://root.cern/) | 6.36 で確認 | |
| [Boost](https://www.boost.org/) | 1.80.0 以降 | |
| CMake | 3.8 以降 | |
| C++ コンパイラ | C++17 | |
| [SWIG](https://www.swig.org/) | | Python 拡張の生成に必須 |
| Python 3 | | |

---

## インストール

### 0. EricaNext を取ってくる

```bash
git clone https://github.com/SMILE-tool-box/EricaNext.git      # SSH の鍵を登録していれば git@github.com:SMILE-tool-box/EricaNext.git
cd EricaNext
```

以下のコマンドは、このリポジトリの一番上のディレクトリで実行します。

### 1. ANLnext を入れる

先に ANLnext をインストールしておきます(手順は ANLnext のドキュメントを参照)。
既にインストール済みのANLNextがあれば、その環境を有効にして使用できます。
Geant4に依存する構成の場合は、Geant4のライブラリの環境も有効にしてください。

### 環境を確認する

使用するPythonとROOT・ANLNextの環境を有効にした後、まず確認します。
`~/opt/setup_ericanext.sh` は開発者環境の例で、このリポジトリには含まれません。
各自のROOT・ANLNextの設定スクリプトを使ってください。

```bash
# ビルド前：ツール、Python開発ヘッダー、ANLNextのimportを確認
python3 scripts/env/check_environment.py --mode build

# ビルド後：EricaPyと、較正・ノートブック用の依存関係も確認
python3 scripts/env/check_environment.py --mode all
```

`OK` / `NG` と対処の案内を表示し、NGがあれば終了コード1になります。
データを読まず、パッケージのインストールやビルドは行いません。
Boostの検出・リンクや依存関係全体の整合は、実際のCMakeビルドで確認します。

Pythonが複数ある場合は、用途ごとに明示できます（パスは各自のものに置き換えます）。

```bash
python3 scripts/env/check_environment.py --mode all \
  --python "<チェーン実行用Pythonのpath>" \
  --analysis-python "<解析用Pythonのpath>"
```

`--python` はANLNext・EricaPyのビルドとチェーン実行に使うPython、
`--analysis-python` はPyROOT・numpy・scipy・matplotlib・Jupyterを使うPythonです。
後者を省略すると前者と同じPythonを確認します。
チェーンだけなら `--mode run`、解析用Pythonだけなら `--mode notebook` で確認できます。
カーネル表示名「EricaNext」は登録済みであることを前提にしません。

### 2. EricaNext をビルドする

```bash
export ANLNEXT_INSTALL=<ANLnext のインストール先>      # setup_ericanext.sh を読んだなら不要
bash scripts/env/build_ericanext.sh
```

`build/` でビルドし、リポジトリの `install/` に入れます(`/usr/local` には入れないので `sudo` は要りません)。
`run_script/run_chain.py` は `install/lib/python` の `EricaPy` を自分で読み込むので、`PYTHONPATH` を設定しなくても動きます。
コードを直したあとも同じコマンドでビルドし直します。

手で実行するときは次と同じです。

```bash
mkdir build install && cd build
cmake ../source -DCMAKE_INSTALL_PREFIX=../install -DANLNEXT_INSTALL=$ANLNEXT_INSTALL -DPython3_EXECUTABLE=$(which python3)
make -j8 && make install
```

| オプション | 説明 |
| --- | --- |
| `-DCMAKE_INSTALL_PREFIX=../install` | インストール先。`run_script/run_chain.py` はリポジトリの `install/lib/python` を読むので、この場所のままにする(未指定だと `$HOME`) |
| `-DANLNEXT_INSTALL=...` | ANLnext の場所(未指定なら環境変数 `ANLNEXT_INSTALL`、それも無ければ `$HOME`) |
| `-DPython3_EXECUTABLE=...` | Python が何種類も入っている環境(conda / pyenv / system など)で、使う Python を固定する(`CreateSwigPython.cmake` は `find_package(Python3 ...)` を使う。ANLnext を入れたときと同じ Python にする) |
| `make -j8` | ビルドの並列数。使うコア数に合わせて変える |

`USE_PYTHON` は `source/CMakeLists.txt` で `ON` になっているので指定は要りません。

---

## データの置き場所

設定ファイルの `ReadData.data_dir` は `{data_root}/storage_g0/run_<日時>` のように書き、`{data_root}` は読み込むときに次に置き換わります
(`config/ericanext_config.py`)。

| 優先 | 場所 |
| --- | --- |
| 1 | 環境変数 `SMILE3_DATA` |
| 2 | このリポジトリの `mount_data/`(`ericanext_config.py` の `DATA_DIR`) |

中の並び:

```
<data_root>/
  storage_g0/
    run_20260628T232149Z/        1 ラン: tpc_enc_a<ボード>_<番号>.raw, tpc_enc_c<ボード>_<番号>.raw
    ...
```

ほかの場所を探すことはしません。`<data_root>` に `storage_g*/` が無いときは、どのディレクトリを見たかを警告します。
1 つのランだけなら、`--data-dir` にランのディレクトリを直接渡しても構いません。

## リポジトリの構成

| 場所 | 中身 |
| --- | --- |
| [`source/`](source/) | モジュール(C++)と Python 拡張 `EricaPy` |
| [`config/`](config/README.md) | 設定ファイル(チェーンとパラメータ)と読み込み(`ericanext_config.py`) |
| [`tutorials/`](tutorials/README.md) | チュートリアル(まず `0_Introduction` で SMILE-3 の構成と座標系) |
| [`run_script/`](run_script/README.md) | 実行スクリプト(`run_chain.py`, `run_runs.sh`)と図を描くスクリプト |
| [`calib/`](calib/) | 較正ファイル: [`psa/`](calib/psa/README.md)(PSA)、[`tpc/`](calib/tpc/README.md)(TPC)、[`detector_coordinates/`](calib/detector_coordinates/README.md)(CAD と検出器の座標系) |
| [`scripts/calib/`](scripts/calib/) | 較正ファイルを作るスクリプト |
| [`scripts/env/`](scripts/env/) | ビルドのスクリプト |
| `mount_data/`, `root_data/` | データと出力の置き場所(git では管理しない) |

保存モジュールはSMILE-3共通の `RawTreeWriter` です。`smile3_tpc_raw`、`smile3_psa_raw`、`smile3_etcc_raw` は、読み込む検出器が異なる設定名です。
ROOT内のディレクトリ名 `TPC_RawTreeWriter/`・`PSA_RawTreeWriter/` は既存ファイル・後段解析との互換性のため維持しています。
