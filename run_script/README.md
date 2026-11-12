# run_script: チェーンの実行と図のスクリプト

| スクリプト | 何をするか |
| --- | --- |
| [`run_chain.py`](run_chain.py) | 設定ファイル(`config/*.json`)を指定して EricaNext のチェーンを実行する。**通常はこれを使う** |
| [`run_runs.sh`](run_runs.sh) | 複数のランに `run_chain.py` を順に実行する(1 ランに 1 ファイル) |
| [`plot_track3d.py`](plot_track3d.py) | `TOT_Skewness/tot_skewness` の点群(`flag_save_cloud: true`)から 1 イベントの飛跡を 3 次元で描く |
| [`psa_position_nm.py`](psa_position_nm.py) | `peak_search.py` の `position_haNN.csv` にピクセル番号 `n,m` を足して `calib/psa/` に書く(`PSA_Calibration` 用。numpy の入った `/usr/bin/python3` で実行) |
| [`anl_run.py`](anl_run.py) | `run_chain.py` が使う、失敗を終了コードにするための関数(直接は実行しない) |

`run_chain.py` はこのリポジトリのビルド(`install/lib/python` の `EricaPy`)を自分で読み込むので、
`PYTHONPATH` を設定しなくても動きます(先に `bash scripts/env/build_ericanext.sh` でビルドしておく)。
データの場所は設定ファイルの `{data_root}` で決まります(リポジトリ直下の README の「データの置き場所」)。

---

## run_chain.py

設定ファイルに書かれた順番・パラメータでモジュールをつなぎ、チェーンを実行します。

```bash
cd EricaNext/run_script
python3 run_chain.py <設定名> -o <出力ファイル> [-n <イベント数>] [--data-dir <ラン>] [--set モジュール.パラメータ=値 ...]
```

| 引数 | 意味 |
| --- | --- |
| `<設定名>` | `config/` の中の設定ファイルの名前(拡張子なし、例 `smile3_tpc_only`)。`.json` のパスを直接書いてもよい |
| `-o`, `--output` | 出力する ROOT ファイル(必須) |
| `-n`, `--n-events` | 処理するイベント数。省略すると全イベント(`-1`) |
| `--data-dir` | ランのディレクトリ(`tpc_enc_a*.raw`、`ha*.dat` などがある所)。設定ファイルの `ReadData.data_dir`(と `PSA_ReadData.data_dir`、chain にあれば)を置き換える |
| `--extra-data-dir <ディレクトリ>` | 同じランの別のディレクトリ(FM では 2 つの storage の片方)。`ReadData.extra_data_dirs` と `PSA_ReadData.extra_data_dirs` に足す(何回でも) |
| `--set モジュール.パラメータ=値` | 設定ファイルのパラメータをその場で上書き(何回でも)。値は JSON として読むので、小数のパラメータは `30.0` のように書く(`30` だと整数になり、型が合わずに止まる)。例: `--set CalcFADC.threshold=30.0` |

### 例

```bash
# 設定ファイルに書いたランの全イベント
python3 run_chain.py smile3_tpc_only -o out.root

# 別のランの先頭 1 万イベントだけ試す
python3 run_chain.py smile3_tpc_only -o test.root -n 10000 --data-dir ../mount_data/storage_g0/run_20260618T010203Z

# ボードごとの 1-hit スペクトル: 生データのツリーを書いて、後段の解析で作る
python3 run_chain.py smile3_raw -o raw.root --data-dir ../mount_data/storage_g0/run_20260618T010203Z --compression 505
/usr/bin/python3 ../scripts/calib/make_tpc_spec1hit.py -o spec1hit.root raw.root

# 飛跡の点群も保存して描く
python3 run_chain.py smile3_tpc_only -o tot.root   # 点群(flag_save_cloud)は既定で保存する

# 同期した生データのツリー(較正・診断用)
python3 run_chain.py smile3_raw -o raw.root

# シンチレータ(PSA)だけ。ボードが storage_g0 と g1 に分かれたランは extra_data_dirs にもう片方
python3 run_chain.py smile3_psa -o psa.root --data-dir ../mount_data/storage_g1/run_20261029T071524Z \
    --set 'PSA_ReadData.extra_data_dirs=["../mount_data/storage_g0/run_20261029T071524Z"]'
```

### 終了コード

- 0: データを最後まで読んだとき、`-n` で止めたとき。
- 1: ランのディレクトリが無いとき、モジュールの初期化や処理が失敗したとき。

出力はいったん `<出力ファイル>.part` に書き、成功したときだけ `<出力ファイル>` に名前を変えます。失敗したときは何も残しません。

### パラメータを変えて試すとき

設定ファイルをコピーして値を書き換え、そのファイルを指定します。

```bash
cp ../config/smile3_tpc_only.json ../config/my_test.json
# my_test.json の "parameters" の値を編集
python3 run_chain.py my_test -o my_test.root -n 10000
```

### 飛跡・波形の描画

`ReadData` はイベントの読み込みと同期を行います。飛跡・波形の積算、閾値による選別、PNG保存は、
`smile3_raw` で保存した `RawTreeWriter_sm3/raw` を読み、後段で行います。
旧 `Image_A_C`、`FADC_TH`、`save_track_png`、`save_fadc_png`、`save_fadc_*_filter` と関連する閾値設定は削除しました。
既存の設定ファイルや `--set` に残っている場合は取り除いてください。

飛跡の積算は [TPC_DriftVelocity](../tutorials/4_MakeCalibration/TPC_DriftVelocity.ipynb)、
イベントごとの飛跡・波形は [ReadData](../tutorials/1_DataIO/modules/1_ReadData/ReadData.ipynb) を参照してください。
rawツリーの `hit_a/c_strip`, `hit_a/c_clock`、`fadc_ch`, `fadc_wave`、`anode_time`, `cathode_time` で
選別・描画・時刻に対する集計を行えます（時刻配列とboard IDの対応は `run_info`）。

---

## run_runs.sh

複数のランに `run_chain.py` を順に実行します。1 つのランが失敗しても次のランに進み、最後に失敗したランを表示します(1 つでも失敗すると終了コード 1)。

```bash
bash run_runs.sh <設定名> <出力ディレクトリ> <ラン>... [-- <run_chain.py の引数>...]

# 2026-06-18 のランをすべて
bash run_runs.sh smile3_tpc_only ../root_data/board/output20_0618 ../mount_data/storage_g0/run_20260618*

# run_chain.py の引数を足す(-- の後ろ)
bash run_runs.sh smile3_tpc_only out ../mount_data/storage_g0/run_2026061* -- --set CalcFADC.threshold=30.0 -n 10000
```

出力は `<出力ディレクトリ>/output_<ラン名>.root` です。

---

## plot_track3d.py

```bash
python3 plot_track3d.py [<エントリー番号>]
```

読むファイルは中の `ROOT_FILE`(今は `output_3.root`)です。`uproot`、`awkward`、`matplotlib` が要ります。
