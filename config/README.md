# 設定ファイルの書き方

ここには `run_script/run_chain.py` が読む設定ファイルを置いています。
このページでは設定ファイルの書き方だけを説明します。スクリプトの使い方は [`run_script/README.md`](../run_script/README.md) にあります。

| ファイル | チェーン |
| --- | --- |
| [`smile3_tpc_only.json`](smile3_tpc_only.json) | `ReadData → CalcFADC`(FADC の電荷と 1-hit スペクトル) |
| [`smile3_tot_skewness.json`](smile3_tot_skewness.json) | `ReadData → CalcFADC → TPC_Calibration → TOT_Skewness`(ゲインマップ無し、電子の飛跡まで) |
| [`smile3_raw.json`](smile3_raw.json) | `ReadData → RawTreeWriter_sm3`(同期した生データのツリー) |
| [`smile3_reconstruct_psa.json`](smile3_reconstruct_psa.json) | `ReadData → PSA_* → CalcFADC → TPC_Calibration → PSA_Event → Reconstruct`(`energy_haNN.csv`、`geometry.csv` が要る) |
| [`smile3_psa.json`](smile3_psa.json) | `PSA_ReadData → PSA_CalcFADC → PSA_CenterGravity → PSA_Calibration → PSA_TreeWriter`(シンチレータだけ。較正ファイルは下の [PSA の較正ファイル](#psa-の較正ファイル-calibpsa)) |

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
| `position_ha<NN>.csv` | ch ごとにフラッドマップの 64 点(`x,y`)とピクセル番号(`n,m`)。(x_hg, y_hg) → (n, m) の薄板スプラインはこれで決まる | `scripts/calib/make_psa_position.py` | ピクセルを求めない(`pixel = -1`) |
| `temp_gain_ha<NN>.csv` | ch ごとの温度と相対ゲインの表(`ch,temp,gain`)。`hg_corr = hg / gain(T)` | `scripts/calib/make_psa_temp_gain.py` | `PSA_Calibration.temp_coef` の式(既定 0 = 直さない) |
| `energy_ha<NN>.csv` | ピクセルごとの `p0,p1`(`ch,n,m,p0,p1`)。`energy = p0 + p1 × hg_corr` [keV] | `scripts/calib/make_psa_energy.py`(線源のラン) | `energy = -1` |
| `geometry.csv` | `PSA_Event` が使う、ブロック(board, ch)ごとの位置と向き。下の説明 | 検出器の配置から手で書く(ひな形 `geometry_template.csv`) | ヒットを使わない(`Reconstruct/single_hit` が空) |

### 作る順番

温度ゲインはピクセルによらないので位置と並べて作れますが、エネルギーはピクセルと `hg_corr` を使うので最後です。

```bash
cd run_script
# 0. 較正のランをチェーンに通す(位置・温度ゲインは HA_cal のランを何本でも。ヒットが多いほどよい)
python3 run_chain.py smile3_psa -o cal_run01.root --data-dir <ラン> --set 'PSA_ReadData.extra_data_dirs=["<もう片方の storage>"]'
# 1. 位置(フラッドマップの 64 点)。ch あたり --min-hits(既定 1 万)ヒット未満の ch は作らない
/usr/bin/python3 ../scripts/calib/make_psa_position.py cal_run*.root -o ../calib/psa
# 2. 温度ゲイン(温度の違うランを合わせて入れる)
/usr/bin/python3 ../scripts/calib/make_psa_temp_gain.py cal_run*.root -o ../calib/psa
# 3. 線源のランを、1 と 2 を置いた状態でチェーンに通してから、エネルギー
python3 run_chain.py smile3_psa -o src_run01.root --data-dir <線源のラン> ...
/usr/bin/python3 ../scripts/calib/make_psa_energy.py src_run*.root -o ../calib/psa --line 662
```

- スクリプトは `/usr/bin/python3`(numpy、scipy、PyROOT のある Python)で動かします。
- どのスクリプトも、既にあるファイルは `--overwrite` を付けないと上書きしません。`<out>/qa/` に確認の図を書くので、使う前に見てください。
- 位置のピークの探し方は、今は `peak_search.py` と同じです(同じヒストグラムで同じ 64 点になることを確かめた)。格子の制約が無いので、統計が少ないと外側の列を取り逃したり、1 つの山で 2 点取ったりします。その ch は図で見て、ファイルから外してください(外した ch はピクセルを求めない)。
- **今の `position_ha<NN>.csv` は、2026-11-03 の UVSOR の HA_cal(run01〜11)を `peak_search.py` で探したものに、`run_script/psa_position_nm.py` で `n,m` を足したものです。統計が少なく 64 点がきれいな格子として取れていないので、補正後の位置はどの ch でも格子にならず、`pixel` はまだ正しくありません(仕組みを動かすための仮のもの)。** 本番の較正データで作り直す予定です。
- ボードが 2 つの storage に分かれたラン(`storage_g0` と `storage_g1`)は、`--data-dir` に片方、`PSA_ReadData.extra_data_dirs` にもう片方を書きます。

  ```bash
  python3 run_chain.py smile3_psa -o psa.root --data-dir ../mount_data/storage_g1/<ラン> \
      --set 'PSA_ReadData.extra_data_dirs=["../mount_data/storage_g0/<ラン>"]'
  ```
- TPC と一緒に読むときは、`chain` の `PSA_ReadData` より前に `ReadData` を置きます(`PSA_ReadData.sync_tpc: true`)。
  `ReadData` は `data_dir` を 1 つしか読まないので、アノードとカソードが別の storage にあるランは、両方のファイルを 1 つのディレクトリに(シンボリックリンクで)集めてから読みます。

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

