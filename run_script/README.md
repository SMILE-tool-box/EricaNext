# run_script: チェーンの実行と図のスクリプト

| スクリプト | 何をするか |
| --- | --- |
| [`run_chain.py`](run_chain.py) | 設定ファイル(`config/*.json`)を指定して EricaNext のチェーンを実行する。**通常はこれを使う** |
| [`run_runs.sh`](run_runs.sh) | 複数のランに `run_chain.py` を順に実行する(1 ランに 1 ファイル) |
| [`plot_track3d.py`](plot_track3d.py) | `TOT_Skewness/tot_skewness` の点群(`flag_save_cloud: true`)から 1 イベントの飛跡を 3 次元で描く |
| [`anl_run.py`](anl_run.py) | `run_chain.py` が使う、失敗を終了コードにするための関数(直接は実行しない) |
| `test2.py`, `test3.py` | 前の実行スクリプト(チェーンとパラメータを中に書いたもの)。`config/smile3_tot_skewness.json`、`config/smile3_tpc_only.json` が同じ内容 |

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
| `--data-dir` | ランのディレクトリ(`tpc_enc_a*.raw` などがある所)。設定ファイルの `ReadData.data_dir` を置き換える |
| `--set モジュール.パラメータ=値` | 設定ファイルのパラメータをその場で上書き(何回でも)。値は JSON として読むので、小数のパラメータは `30.0` のように書く(`30` だと整数になり、型が合わずに止まる)。例: `--set CalcFADC.threshold=30.0`、`--set CalcFADC.save_board_plots=true` |

### 例

```bash
# 設定ファイルに書いたランの全イベント
python3 run_chain.py smile3_tpc_only -o out.root

# 別のランの先頭 1 万イベントだけ試す
python3 run_chain.py smile3_tpc_only -o test.root -n 10000 --data-dir ../mount_data/storage_g0/run_20260618T010203Z

# ボードごとの 1-hit スペクトルも保存する
python3 run_chain.py smile3_tpc_only -o board.root --set CalcFADC.save_board_plots=true

# 電子の飛跡まで。点群を描く
python3 run_chain.py smile3_tot_skewness -o tot.root

# 同期した生データのツリー(較正・診断用)
python3 run_chain.py smile3_raw -o raw.root
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

### PNG の出力

`ReadData` の `save_track_png`、`save_fadc_png`、`save_fadc_a_filter`、`save_fadc_c_filter` は、
**実行したディレクトリ** の `png_A/`、`png_C/`、`png_FADC_A/` などにイベントごとの PNG を書きます。
`save_fadc_png` と `save_fadc_*_filter` には `FADC_TH: true` も必要です。

---

## run_runs.sh

複数のランに `run_chain.py` を順に実行します。1 つのランが失敗しても次のランに進み、最後に失敗したランを表示します(1 つでも失敗すると終了コード 1)。

```bash
bash run_runs.sh <設定名> <出力ディレクトリ> <ラン>... [-- <run_chain.py の引数>...]

# 2026-06-18 のランをすべて
bash run_runs.sh smile3_tpc_only ../root_data/board/output20_0618 ../mount_data/storage_g0/run_20260618*

# run_chain.py の引数を足す(-- の後ろ)
bash run_runs.sh smile3_tpc_only out ../mount_data/storage_g0/run_2026061* -- --set CalcFADC.save_board_plots=true -n 10000
```

出力は `<出力ディレクトリ>/output_<ラン名>.root` です。

---

## plot_track3d.py

```bash
python3 plot_track3d.py [<エントリー番号>]
```

読むファイルは中の `ROOT_FILE`(今は `output_3.root`)です。`uproot`、`awkward`、`matplotlib` が要ります。
