# 設定ファイルの書き方

ここには `run_script/run_chain.py` が読む設定ファイルを置いています。第1段階は `stage1/`、第2段階は `stage2/` に分けています。設定名はフォルダを省略して指定できます。
このページでは設定ファイルの書き方だけを説明します。スクリプトの使い方は [`run_script/README.md`](../run_script/README.md) にあります。

## 第1段階：生データの保存（stage1/）

| ファイル | チェーン |
| --- | --- |
| [`smile3_etcc_raw.json`](stage1/smile3_etcc_raw.json) | `ReadData → PSA_ReadData → RawTreeWriter` |
| [`smile3_tpc_raw.json`](stage1/smile3_tpc_raw.json) | `ReadData → RawTreeWriter` |
| [`smile3_psa_raw.json`](stage1/smile3_psa_raw.json) | `PSA_ReadData → RawTreeWriter` |

## 第2段階：ROOTから解析（stage2/）

`--root-input` で第1段階のROOTを指定します。

| ファイル | チェーン |
| --- | --- |
| [`smile3_reconstruct.json`](stage2/smile3_reconstruct.json) | `ReadData → PSA_ReadData → PSA_CalcFADC → PSA_CenterGravity → PSA_Calibration → PSA_TreeWriter → CalcFADC → TPC_Calibration → PSA_Event → TOT_Skewness → Reconstruct` |
| [`smile3_tpc_energy.json`](stage2/smile3_tpc_energy.json) | `ReadData → CalcFADC → TPC_Calibration`（全イベントのエネルギースペクトル。ゲインマップを指定） |
| [`smile3_tpc_only.json`](stage2/smile3_tpc_only.json) | `ReadData → CalcFADC → TPC_Calibration → TOT_Skewness` |
| [`smile3_psa.json`](stage2/smile3_psa.json) | `PSA_ReadData → PSA_CalcFADC → PSA_CenterGravity → PSA_Calibration → PSA_TreeWriter` |

スクリプトには、名前(拡張子なし)かファイルのパスを渡します。

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

## 第1段階の設定例

```json
{
  "_comment": [
    "第1段階: TPC生データを全情報保存する設定例"
  ],
  "chain": [
    "ReadData",
    "RawTreeWriter"
  ],
  "parameters": {
    "ReadData": {
      "data_dir": "",
      "extra_data_dirs": [],
      "TPC_Only": true,
      "event_id": true,
      "_doc": {
        "data_dir": "同じrunの入力rawフォルダ。空にしておき、run_chain.py の --data-dir で指定する",
        "extra_data_dirs": "同じrunの追加rawフォルダ(run_chain.py の --extra-data-dir でも足せる)",
        "TPC_Only": "true: TPCの生データを読む",
        "event_id": "true: トリガーカウンタでボード間のイベントをそろえる"
      }
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

較正ファイルの内容・作成方法は [PSA較正](../calib/psa/README.md) と [TPC較正](../calib/tpc/README.md) を参照してください。

## Raw ROOTを経由する標準手順

| 用途 | 第1段階（生データ → ROOT） | 第2段階（ROOT → 解析） |
| --- | --- | --- |
| TPC | `smile3_tpc_raw` | `smile3_tpc_only --root-input <ROOT>`、較正・診断スクリプト |
| PSA | `smile3_psa_raw` | `smile3_psa --root-input <ROOT>` |
| ETCC | `smile3_etcc_raw` | `smile3_reconstruct --root-input <ROOT>` |

`ReadData.root_file` と `PSA_ReadData.root_file` は入力ROOTのパスです。
空文字なら従来の生データ読み込み、指定時は `data_dir`・`extra_data_dirs` を使いません。
`--root-input` はチェーン内の両readerに同じファイルを指定します。1ファイル/runです。
生データ入力の `--data-dir`・`--extra-data-dir`・`--raw` と同時には指定しません。
`reverse_TPC`・`event_id` は第1段階にのみ適用します。後段では保存済みのストリップとIDを使います。

第1段階のRawTreeWriterは全ヒット・レコード・全チャンネルの波形を必ず保存します。保存有無や閾値の設定はありません。
PSAは `PSA_RawTreeWriter/raw` の生情報を読み戻します。従来の `PSA_TreeWriter/psa` は後段の処理結果です。

PSA rawは1イベント1行で、`board` にpresentボードのID（ゼロヒットのフレームも含む）、
`board_epoch`、`clk` [1 MHz clock]、`time` [s]、`hit_mask`（bit ch）、`temp_raw` [raw ADC]、
`adc` [raw ADC, 0–16383] を保存します。
`temp_raw` はボードごとに6値、`adc` はボード→ch（6）→sample（50）→信号（HG0–3、LG0–3）の順に平坦化した2400値です。
`run_info` には `board_ids`、`schema_version=1`、`n_sample=50`、`n_sig=8` を記録します。
TPCとPSAは同一run・同一イベント順で保存し、後段でイベントIDと連番の一致を確認します。

検証は `python3 scripts/check/check_raw_replay.py --data-dir <run> --extra-data-dir <other storage/run>`。
直接処理と二段階処理の全ツリー・ヒストグラム、raw ROOTの往復一致、不完全な入力の拒否を確認します。
