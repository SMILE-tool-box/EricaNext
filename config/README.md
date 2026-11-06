# 設定ファイルの書き方

ここには `run_script/run_chain.py` が読む設定ファイルを置いています。
このページでは設定ファイルの書き方だけを説明します。スクリプトの使い方は [`run_script/README.md`](../run_script/README.md) にあります。

| ファイル | チェーン |
| --- | --- |
| [`smile3_tpc_only.json`](smile3_tpc_only.json) | `ReadData → CalcFADC → TPC_Calibration → TOT_Skewness`(TPC だけ。ゲインマップ無し、電子の飛跡まで) |
| [`smile3_raw.json`](smile3_raw.json) | `ReadData → RawTreeWriter_sm3`(同期した生データのツリー) |
| [`smile3_reconstruct.json`](smile3_reconstruct.json) | `ReadData → PSA_* → CalcFADC → TPC_Calibration → PSA_Event → Reconstruct`(`energy_haNN.csv`、`geometry.csv` が要る) |
| [`smile3_psa.json`](smile3_psa.json) | `PSA_ReadData → PSA_CalcFADC → PSA_CenterGravity → PSA_Calibration → PSA_TreeWriter`(シンチレータだけ。較正ファイルは下の [PSA の較正ファイル](#psa-の較正ファイル-calibpsa)) |
| [`smile2p_20190701_Per02.json`](smile2p_20190701_Per02.json), [`smile2p_20190701_Per03.json`](smile2p_20190701_Per03.json) | SMILE-2+ の地上較正のラン: `ReadData_2p → CalcFADC_2p → TPC_Calibration_2p → HA_CenterGravity_2p → HA_Calc_2p → TOT_Skewness_2p → Reconstruct_2p`(EricaNext_try と同じ) |
| [`smile2p_sim_d10_recon.json`](smile2p_sim_d10_recon.json) | SMILE-2+ のシミュレーションのデータの再構成(同じチェーン) |
| [`tpc_caldata_2017_Ba133.json`](tpc_caldata_2017_Ba133.json) | SMILE-2+ の TPC だけの Ba-133 の較正のラン: `ReadData_2p → CalcFADC_2p`(ゲインマップを作る) |

スクリプトには、名前(拡張子なし)かファイルのパスを渡します。
新しい設定は、近いファイルをコピーして書き換えるのが簡単です(このディレクトリの外に置いても構いません)。

## 共通の決まり

- 書式は JSON です。数は単位を付けずに書くので、単位は下の `_units` で確かめてください。
- **`_` で始まるキーはコメント** で、読み込むときに捨てます(モジュールには渡りません)。`_comment`、`_note` など自由に使えます。
- 各モジュールのパラメータには、次の 2 つを付けます。**パラメータを足したときは、この 2 つにも書いてください。**

  | キー | 中身 |
  | --- | --- |
  | `_doc` | `{パラメータ名: 説明}`。全パラメータに書く |
  | `_units` | `{パラメータ名: 単位}`。単位のあるパラメータに書く(例 `"threshold": "ADU (above the baseline)"`) |

- 文字列の中の次の置き換え文字は、読み込むときに実際のディレクトリに置き換わります(`ericanext_config.py`)。
  置き換え文字を使わずに絶対パスを書いても構いません。

  | 置き換え文字 | 置き換わるもの |
  | --- | --- |
  | `{data_root}` | 生データの一番上(`storage_g0/` などがある所): 環境変数 `SMILE3_DATA`、無ければリポジトリの `mount_data/` |
  | `{repo}` | このリポジトリの一番上 |
  | `{data_top}` | SMILE-2+: `<SMILE2P_DATA>/calibration_data`(`sm2+_cal_rawdata/`、`param/` がある所)。`SMILE2P_DATA` は環境変数、無ければ研究室の NAS(`ericanext_config.py` の `DATA_DIR_2P`) |
  | `{tpc_caldata}` | SMILE-2+: `<SMILE2P_DATA>/TPC_caldata` |
  | `{sim_data}` | SMILE-2+: `<SMILE2P_DATA>/simulation_data/parameters/smile2p_sim_data` |

- 実際に使われる値(置き換え後、コメントを除いたもの)は、次で確かめられます。

  ```bash
  cd config
  python3 -c "import ericanext_config as ec, json; print(json.dumps(ec.load('smile3_tpc_only'), indent=1))"
  ```

## 形

```json
{
  "_comment": ["SMILE-3 TPC only: ReadData -> CalcFADC ...", "..."],
  "chain": ["ReadData", "CalcFADC"],
  "parameters": {
    "ReadData": {
      "data_dir": "{data_root}/storage_g0/run_20260628T232149Z",
      "TPC_Only": true,
      "_doc": {"data_dir": "1 ラン分の生データのディレクトリ", "TPC_Only": "true: TPC だけを読む"}
    },
    "CalcFADC": {
      "base_window": 30.0,
      "threshold": 20.0,
      "_units": {"base_window": "samples (20 ns each)", "threshold": "ADU (above the baseline)"},
      "_doc": {"base_window": "ベースライン(波形の最初のこのサンプル数の平均)", "threshold": "電荷として積分するサンプルの閾値"}
    }
  }
}
```

| キー | 中身 |
| --- | --- |
| `chain` | つなぐモジュールの順番。`SaveFile` は `run_chain.py` が先頭に必ずつなぐので書かない |
| `parameters` | `{モジュール名: {パラメータ名: 値}}`。書かなかったパラメータはモジュールの既定値になる |

- 値の型はモジュールのパラメータの型に合わせます。小数のパラメータは `30.0` のように書きます(`30` だと整数になり、型が合わずに止まる)。
- パラメータの名前は、各モジュールの `mod_define()` の `define_parameter("名前", ...)` です。
- 実行すると、ANLNext が全モジュールのパラメータの値(既定値を含む)を表示するので、何が使われたかはそこでも確かめられます。

## PSA の較正ファイル (`calib/psa/`)

`smile3_psa.json` の `PSA_CalcFADC.psa_param_file`、`PSA_Calibration.position_dir`、`temp_gain_dir`、`energy_dir` は `{repo}/calib/psa/` を指しています。
ボード `ha<NN>` ごとに次のファイルを置きます。無いボードは、そのファイルを使う処理だけを飛ばします(実行は止まりません)。
EricaNext_try のゲインマップやドリフト速度と同じく、**較正のランから `scripts/calib/` のスクリプトで作り、チェーンはファイルを読むだけ**です。
どのファイルも `#` で始まる行はコメントで、作ったスクリプトが入力のファイルと条件を書き込みます。

| ファイル | 中身 | 作り方 | 無いとき |
| --- | --- | --- | --- |
| `psa_param.csv` | 全ボードの表。`HA` 列が `ha<NN>` の行の `TL_A`, `TL_B` 〜 `BR_A`, `BR_B` が温度センサーの較正 | `analysis_sm3_FM/PSA_analysis/param/psa_param_YYYYMMDD.csv` をコピー(`HA` 列のある版) | psa_decode.py の既定値(a = 3.9258, b = 1230.30) |
| `position_ha<NN>.csv` | ch ごとにバタフライマップの 64 点(`x,y`)とピクセル番号(`n,m`)。(x_hg, y_hg) → (n, m) の薄板スプラインはこれで決まる | `scripts/calib/make_psa_position.py` | ピクセルを求めない(`pixel = -1`) |
| `temp_gain_ha<NN>.csv` | ch ごとの温度と相対ゲインの表(`ch,temp,gain`)。`hg_corr = hg / gain(T)` | `scripts/calib/make_psa_temp_gain.py` | 直さない(`hg_corr = hg`、警告を出す) |
| `energy_ha<NN>.csv` | ピクセルごとの `p0,p1`(`ch,n,m,p0,p1`)。`energy = p0 + p1 × hg_corr` [keV] | `scripts/calib/make_psa_energy.py`(線源のラン) | `energy = -1` |
| `geometry.csv` | `PSA_Event` が使う、ブロック(board, ch)ごとの位置と向き。下の説明 | `scripts/calib/make_psa_geometry.py`(CAD の STEP ファイルと HA の配置図から) | ヒットを使わない(`Reconstruct/single_hit` が空) |

### 作る順番

温度ゲインはピクセルによらないので位置と並べて作れますが、エネルギーはピクセルと `hg_corr` を使うので最後です。

```bash
cd run_script
# 0. 較正のランをチェーンに通す(位置・温度ゲインは HA_cal のランを何本でも。ヒットが多いほどよい)
python3 run_chain.py smile3_psa -o cal_run01.root --data-dir <ラン> --set 'PSA_ReadData.extra_data_dirs=["<もう片方の storage>"]'
# 1. 位置(バタフライマップの 64 点)。ch あたり --min-hits(既定 1 万)ヒット未満の ch は作らない
/usr/bin/python3 ../scripts/calib/make_psa_position.py cal_run*.root -o ../calib/psa
# 2. 温度ゲイン(温度の違うランを合わせて入れる)
/usr/bin/python3 ../scripts/calib/make_psa_temp_gain.py cal_run*.root -o ../calib/psa
# 3. 線源のランを、1 と 2 を置いた状態でチェーンに通してから、エネルギー
python3 run_chain.py smile3_psa -o src_run01.root --data-dir <線源のラン> ...
/usr/bin/python3 ../scripts/calib/make_psa_energy.py src_run*.root -o ../calib/psa --line 662
```

TPC の 1-hit スペクトル(ゲインマップの材料。ゲインマップを作るスクリプトはまだ無い。置き場は [`calib/tpc/`](../calib/tpc/README.md)):

```bash
cd run_script
python3 run_chain.py smile3_raw -o raw.root --data-dir <TPC_cal のラン> --compression 505
/usr/bin/python3 ../scripts/calib/make_tpc_spec1hit.py -o spec1hit.root raw.root   # qa/spec1hit_board.png も書く
# ドリフト速度(ドリフト領域に一様に電子ができるランで。箱の形でないと書かない)
/usr/bin/python3 ../scripts/calib/make_tpc_drift_velocity.py -o ../calib/tpc/drift_velocity.dat raw.root
# 使うとき: --set TOT_Skewness.drift_velocity_file=../calib/tpc/drift_velocity.dat(または設定ファイルに書く)
```

- スクリプトは `/usr/bin/python3`(numpy、scipy、PyROOT のある Python)で動かします。
- どのスクリプトも、既にあるファイルは `--overwrite` を付けないと上書きしません。`<out>/qa/` に確認の図を書くので、使う前に見てください。
- 位置のピークの探し方は 2 つあります。既定の `--method grid` は、x の射影で 8 列、列ごとに y の射影(の 2 階微分)で 8 行を探します。
  `--method localmax` は `peak_search.py` と同じ(同じヒストグラムで同じ 64 点になることを確かめた)ですが、縦に長い中央のスポットから何個もピークを拾い、端の列を取り逃します。
  補正後に 8 × 8 に入るヒットが `--min-frac`(既定 0.9)未満の ch は書きません(その ch はピクセルを求めない)。
- **今のファイル**: `position_haNN.csv` は 2026-11-03 の UVSOR の Co のラン run12 から `grid` で作ったもの(ha04 だけは run12 に無いので、前の peak_search の仮のもの)。
  `temp_gain_haNN.csv` と `energy_haNN.csv`(662 keV の 1 本、`p0 = 0`)は同じ日の Cs-137 のラン run01〜11 から作ったもの(ha04 は無い)。ランの記録は Notion(「SMILE-3 FM UVSOR 試験(2026-11-03)データ確認と HA_cal の解析」)。
- 長いランで `.dat` が `part0`, `part1`, … のサブディレクトリに分かれているときは、`--set PSA_ReadData.recursive=true` を付けます。
- ボードが 2 つの storage に分かれたラン(`storage_g0` と `storage_g1`)は、`--data-dir` に片方、`PSA_ReadData.extra_data_dirs` にもう片方を書きます。

  ```bash
  python3 run_chain.py smile3_psa -o psa.root --data-dir ../mount_data/storage_g1/<ラン> \
      --set 'PSA_ReadData.extra_data_dirs=["../mount_data/storage_g0/<ラン>"]'
  ```
- TPC と一緒に読むときは、`chain` の `PSA_ReadData` より前に `ReadData` を置きます(`PSA_ReadData.sync_tpc: true`)。
  アノードとカソードが別の storage にあるランは、片方を `data_dir`、もう片方を `ReadData.extra_data_dirs` に書きます(`run_chain.py --extra-data-dir` でも足せる。`PSA_ReadData.extra_data_dirs` にも入る)。
  ボードごとに、`data_dir`、`extra_data_dirs` の順で最初にファイルのあるディレクトリから読みます。

### `geometry.csv`(PSA_Event)

1 行に 1 ブロック(HA のボード `board` と ch 0〜5)で、単位は mm です。座標は [`tutorials/0_Introduction`](../tutorials/0_Introduction/Introduction.ipynb) のもの
(原点は μ-PIC の中央、X は W → E、Y は N → S、Z は下向き)。

```
board,ch,x0,y0,z0,ux,uy,uz,vx,vy,vz,pitch
```

| 列 | 中身 |
| --- | --- |
| `x0, y0, z0` | 8 × 8 のブロックの中心(吸収点の深さで決める) |
| `ux, uy, uz` | ピクセル番号 n(1〜8)が増える向きの単位ベクトル |
| `vx, vy, vz` | m が増える向きの単位ベクトル |
| `pitch` | ピクセルの間隔 |

ピクセル (n, m) の吸収点は `(x0, y0, z0) + pitch × ((n − 4.5) u + (m − 4.5) v)` です。`#` で始まる行はコメントです。
n, m の向きは `x_hg`, `y_hg`(4 隅の信号の重心)で決まるので、ブロックの中の MPPC の 4 隅(I0〜I3)の並びと、ch(TL〜BR)の位置に合わせて決めます。

作り方(`scripts/calib/make_psa_geometry.py`):

```bash
/usr/bin/python3 scripts/calib/make_psa_geometry.py calib/cad/SM3_Inner_3atm.step -o calib/psa/geometry.csv
```

- 位置は CAD(`SM3_Inner_3atm.step` の `GSO_26mm`(底面)と `GSO_13mm`(側面)の結晶のブロック)から取ります。CAD のライブラリは使わず、STEP の組み立ての木を `scripts/calib/step_reader.py` で直接たどります。
- CAD の座標(z は上向き、原点は底板)からの変換は X = y_CAD、Y = x_CAD、Z = 427.04 − z_CAD(427.04 mm は μ-PIC の基板の下の面)。CAD は東西・南北に対称なので、CAD の +y を東としました。
  CAD での GEM は Z = 10.0 mm、天板の内側の面は Z = 317.0 mm です。
- どのブロックがどの HA・ch か、ピクセルの向きは、HA のモジュールの配置図から(スクリプトの `HA_LAYOUT` の表):
  - 底面 HA0〜5: 西半分が北から HA3、4、5、東半分が北から HA0、1、2。側面: 北面は西から HA8、7、6、西面は北から HA9、10、11、南面は西から HA12、13、14、東面は北から HA17、16、15。
  - モジュールの中のブロックは ch `0 1 / 2 3 / 4 5`。ch0 → ch1 とピクセル n が増える向きを u、ch0 → ch2 → ch4 と m が増える向きを v とします。
  - 底面は上から見て u = 北、v = 東。側面は検出器の内側から壁を見て u = 右、v = 下(Z の正の向き)。
- 吸収点はブロックの深さの中心です。ピクセルの間隔は CAD のピクセルの溝から 5.96 mm。
- **ピクセルの向き(u, v)は、MPPC の 4 隅の信号の配線(重心の式の符号)でも変わるので、データで確かめる必要があります**(まだ確かめていない)。

