# 設定ファイルの書き方

ここには `run_script/run_chain.py` が読む設定ファイルを置いています。
このページでは設定ファイルの書き方だけを説明します。スクリプトの使い方は [`run_script/README.md`](../run_script/README.md) にあります。

| ファイル | チェーン |
| --- | --- |
| [`smile3_tpc_only.json`](smile3_tpc_only.json) | `ReadData → CalcFADC`(FADC の電荷と 1-hit スペクトル) |
| [`smile3_tot_skewness.json`](smile3_tot_skewness.json) | `ReadData → CalcFADC → TPC_Calibration → TOT_Skewness`(ゲインマップ無し、電子の飛跡まで) |
| [`smile3_raw.json`](smile3_raw.json) | `ReadData → RawTreeWriter_sm3`(同期した生データのツリー) |

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
