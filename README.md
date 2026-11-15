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

## 使い方(まずここ)

TPCの較正・診断は、次の二段階を基本にします。生データのデコードとイベント同期を一度行い、保存したROOTを後段で繰り返し使います。

```text
生データ → ReadData → RawTreeWriter → raw ROOT
raw ROOT → 飛跡・FADCの積算、1-hitスペクトル、ドリフト速度・ゲインマップ
```

ビルドは `bash scripts/env/build_ericanext.sh` で行います。`mkdir.sh` の実行は不要です。
以下はリポジトリ直下で実行します。入力・出力のパスは自分の環境に書き換えてください。

```bash
# 第1段階: 同じランの2つのstorageを読み、波形とヒットを保存する
mkdir -p /path/to/analysis/raw /path/to/analysis/diagnostics
python3 run_script/run_chain.py smile3_raw \
    --data-dir /path/to/storage_g0/20261113_run13 \
    --extra-data-dir /path/to/storage_g1/20261113_run13 \
    -o /path/to/analysis/raw/run13.root

# 第2段階: 保存済みROOTから作る。生データを読み直さない
python3 scripts/calib/make_tpc_spec1hit.py \
    /path/to/analysis/raw/run13.root -o /path/to/analysis/diagnostics/spec1hit.root
python3 scripts/analysis/make_tpc_fadc_hist.py \
    /path/to/analysis/raw/run13.root -o /path/to/analysis/diagnostics/fadc_accum.root
```

同じrunのファイルが `calltt` と `killua` など複数フォルダに分かれている場合は、
片方を `--data-dir`、残りを `--extra-data-dir` に指定します。
このデータ配置では `calltt` が `storage_g0`、`killua` が `storage_g1` に対応します。
フォルダ名は固定ではないので、実際にファイルがあるrunのディレクトリを指定してください。
事前にファイルをコピーしたり、1つのフォルダに集めたりする必要はありません。

```bash
# 同じrunがcalltt・killuaに分かれている場合（TPC＋PSAの例）
python3 run_script/run_chain.py smile3_etcc_raw \
    --data-dir /path/to/20261102/calltt/20261102_run04 \
    --extra-data-dir /path/to/20261102/killua/20261102_run04 \
    -o /path/to/analysis/raw/run04.root
```

TPCだけなら設定名を `smile3_raw`、PSAだけなら `smile3_psa_raw` に変えます。
3フォルダ以上に分かれている場合は、`--extra-data-dir <フォルダ>` を必要な回数繰り返します。
これらは**同じrunの分割先**を指定するオプションです。異なるrunはrunごとにROOTを作ってください。
フォルダの指定は第1段階だけで、第2段階は作成済みROOTを `--root-input` に指定します。

`smile3_raw` は既定で `save_hits: true`、`save_fadc: true`、`save_threshold: 0.0` です。
飛跡積算にはヒット、ゲイン較正・FADC積算には波形が必要です。保存時に省略・閾値選別した情報は後段で復元できません。
まず `-n 10000` を付けて小さいROOTで確認し、全件処理では外します。

飛跡積算とドリフト速度は [`TPC_DriftVelocity.ipynb`](tutorials/4_MakeCalibration/TPC_DriftVelocity.ipynb)、
ゲインマップは [`TPC_GainMap.ipynb`](tutorials/4_MakeCalibration/TPC_GainMap.ipynb)、
全chの波形積算は [`TPC_FADC_Accumulation.ipynb`](tutorials/2_Diagnostics/TPC_FADC_Accumulation.ipynb) を使います。
各冊の `RAWS` に第1段階のROOT、`OUT` にリポジトリ外の出力先を指定します。
複数runは `RAWS` に列挙します。同じ測定条件のデータをまとめ、fit範囲などを変えるだけなら第1段階を再実行する必要はありません。

PSAだけの測定も `smile3_psa_raw` → `smile3_psa --root-input <ROOT>` の二段階です。

TPC再構成も保存済みROOTを入力にします。

```bash
python3 run_script/run_chain.py smile3_tpc_only \
    --root-input /path/to/analysis/raw/run13.root \
    -o /path/to/analysis/reconstructed/run13.root
```

TPC＋PSAのETCC再構成では、第1段階を `smile3_etcc_raw` にします。
共通の `RawTreeWriter` が同じROOTへ、同じイベント順で生情報を書きます。

```bash
# 第1段階: TPC＋PSAを同期して保存（較正・積分・再構成は行わない）
python3 run_script/run_chain.py smile3_etcc_raw \
    --data-dir /path/to/storage_g0/20261102_run04 \
    --extra-data-dir /path/to/storage_g1/20261102_run04 \
    -o /path/to/analysis/raw/run04.root

# 第2段階: ROOTから積分・較正・飛跡解析・イベント再構成
python3 run_script/run_chain.py smile3_reconstruct \
    --root-input /path/to/analysis/raw/run04.root \
    -o /path/to/analysis/reconstructed/run04.root
```

`--root-input` は1回の実行につき1ファイルです。複数runはrunごとに第1・第2段階を実行します。
積算・較正ノートブックの `RAWS` では複数ROOTをまとめて扱えます。
TPC＋PSAの再構成にはTPC・PSA両方の生情報を含むROOTが必要です。
従来の `PSA_TreeWriter/psa` は処理済みヒットのツリーなので、生波形の入力としては使いません。
既存TPC ROOTも必要なヒットと全波形があれば利用できます（旧名 `RawTreeWriter_sm3` にも対応）。
生データの直接処理は比較・検証用として引き続き利用できます。

第1段階を実行するスクリプトは [`run_script/run_chain.py`](run_script/run_chain.py) です。
最初の引数に [`config/<名前>.json`](config/) の設定名を渡します。

- 設定ファイルは名前(拡張子なし)で書くと `config/` から探します。`.json` のパスを直接書いても構いません。
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
  CFG["config/&lt;名前&gt;.json<br>chain: モジュールの順番<br>parameters: パラメータ(_doc, _units)"]
  EC["config/ericanext_config.py<br>{data_root} などを置き換え<br>_ で始まるキーを捨てる"]
  RUN["run_script/run_chain.py<br>--data-dir, --set で上書き"]
  CH["ANLNext の AnalysisChain<br>SaveFile → chain の順にモジュール"]
  OUT[("出力の ROOT ファイル<br>(成功したときだけ)")]
  CFG --> EC --> RUN --> CH --> OUT
```

モジュールの順番とパラメータはすべて設定ファイルに書き、スクリプトには書きません。
新しい解析の組み方を作るときは、`config/` に設定ファイルを足します(書き方は [`config/README.md`](config/README.md))。

| 設定ファイル | つなぐモジュール |
| --- | --- |
| [`smile3_tpc_only`](config/smile3_tpc_only.json) | `ReadData → CalcFADC → TPC_Calibration → TOT_Skewness`(TPC だけ。ゲインマップ無し、電子の飛跡まで) |
| [`smile3_etcc_raw`](config/smile3_etcc_raw.json) | `ReadData → PSA_ReadData → RawTreeWriter`（TPC＋PSAの第1段階） |
| [`smile3_raw`](config/smile3_raw.json) | `ReadData → RawTreeWriter`(TPCの同期済み生情報。後段の較正・診断・再構成の入力) |
| [`smile3_psa`](config/smile3_psa.json) | `PSA_ReadData → PSA_CalcFADC → PSA_CenterGravity → PSA_Calibration → PSA_TreeWriter`(シンチレータだけ) |
| [`smile3_reconstruct`](config/smile3_reconstruct.json) | `ReadData → PSA_* → CalcFADC → TPC_Calibration → PSA_Event → TOT_Skewness → Reconstruct`(TPC + シンチレータ) |

### モジュールのつながり

TPCの処理とPSAの処理を、それぞれの読み込み・積分・較正モジュールで行い、イベント再構成で組み合わせます。

```mermaid
flowchart TD
  RAW[("生データ<br>tpc_enc_a00〜a11, c00〜c11 *.raw<br>PSA *.dat(MPPC の波形)")]
  subgraph L0["層0 入力"]
    RD["ReadData<br>読み込み・ボード間のイベント同期"]
    PRD["PSA_ReadData<br>PSA の読み込み・トリガーカウンタで同期"]
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
    RAWT["TPC_RawTreeWriter/raw<br>同期済み生データ TTree"]
    PSAT["PSA_TreeWriter/psa<br>シンチレータのヒットの TTree"]
  end
  PRM["パラメータファイル<br>ゲインマップ<br>ドリフト速度"]
  PPRM["calib/psa/<br>温度センサー・位置(64 点)<br>エネルギー・ジオメトリ"]

  RAW --> RD
  RAW --> PRD
  PRM -.-> TC
  PRM -.-> TOT
  PPRM -.-> PCF
  PPRM -.-> PCAL
  PPRM -.-> PEV
  RD -- "FADC 波形(48 ch × 2)" --> CF
  RD -- "TPC ヒット(パネル 0〜3)" --> TC
  RD -- "TPC ヒット・時刻" --> TOT
  RD -- "トリガーカウンタ(sync_tpc)" --> PRD
  PRD -- "MPPC の波形(ボード × 6 ch)" --> PCF
  CF -- "電荷" --> TC
  TC -- "Ke" --> REC
  PEV -- "Eg, Pg(1 ヒットのイベント)" --> REC
  PCAL --> PSAT
  TOT -- "Pe, Ve, Len" --> REC
  REC --> OUT
  TOT --> TOTT
  RD -- "RawTreeWriter<br>(較正・診断用)" --> RAWT

```

- 実線の矢印はデータの受け渡し(ラベルは渡すもの)、点線の矢印はパラメータファイルの読み込みです。
- 実際の実行順は設定ファイルの `chain` の順です。TPC と PSA を一緒に読むときは `ReadData` を `PSA_ReadData` より前に置きます
  (`PSA_ReadData.sync_tpc: true` で `ReadData` のトリガーカウンタに合わせて読む)。
- ゲインマップは `TPC_Calibration.NoCalData: false` のときだけ読みます。
- PSA の部分は `analysis_sm3_FM/PSA_analysis` の `psa_decode.py` → `pixel_id.py` と同じ計算です
  (同じ .dat で、ヒット・peak_id・ピクセルは全部一致、hg などは Python の float32 の丸めの範囲で一致)。
  較正ファイルは [`calib/psa/`](calib/psa/)(作り方は [`config/README.md`](config/README.md#psa-の較正ファイル-calibpsa))。
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
| `ReadData` | 生データを読み、ボード間でイベント ID をそろえる | 波形・ヒットは `RawTreeWriter` で保存。積算・選別・描画は保存後に行う |
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
  --python /path/to/chain/python \
  --analysis-python /path/to/analysis/python
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
| [`scripts/env/`](scripts/env/) | ビルドのスクリプト |
| `mount_data/`, `root_data/` | データと出力の置き場所(git では管理しない) |

保存モジュールはSMILE-3共通の `RawTreeWriter` です。`smile3_raw`、`smile3_psa_raw`、`smile3_etcc_raw` は、読み込む検出器が異なる設定名です。
ROOT内のディレクトリ名 `TPC_RawTreeWriter/`・`PSA_RawTreeWriter/` は既存ファイル・後段解析との互換性のため維持しています。
