# チュートリアル

EricaNext_try の `tutorials_caldata` と同じ形です。モジュールの解説の番号は、設定ファイル `smile3_reconstruct` のチェーンの順です。

| フォルダ | 中身 |
| --- | --- |
| [`0_Introduction`](0_Introduction/Introduction.ipynb) | SMILE-3 の検出器の構成(μ-PIC の 4 分割、ストリップ 0.4 mm、MPPC の PSA)と SMILE-2+ からの変更点、座標系 |
| `1_DataIO/modules/` | 各モジュールの解説 |
| ├ [`1_ReadData/`](1_DataIO/modules/1_ReadData/ReadData.ipynb) | TPC の生データの読み込みとボード間のイベント同期 |
| ├ [`2_PSA_ReadData/`](1_DataIO/modules/2_PSA_ReadData/PSA_ReadData.ipynb) | シンチレータ(PSA)の生データの読み込みとイベントの同期 |
| ├ [`3_PSA_CalcFADC/`](1_DataIO/modules/3_PSA_CalcFADC/PSA_CalcFADC.ipynb) | 波形の積分(hg, lg)、ピークの時刻、飽和、温度 |
| ├ [`4_PSA_CenterGravity/`](1_DataIO/modules/4_PSA_CenterGravity/PSA_CenterGravity.ipynb) | 4 隅の信号の重心(バタフライマップ) |
| ├ [`5_PSA_Calibration/`](1_DataIO/modules/5_PSA_Calibration/PSA_Calibration.ipynb) | 位置の補正とピクセル、温度補正、エネルギー |
| ├ [`6_PSA_TreeWriter/`](1_DataIO/modules/6_PSA_TreeWriter/PSA_TreeWriter.ipynb) | PSA のヒットの TTree の列と読み方 |
| ├ [`7_CalcFADC/`](1_DataIO/modules/7_CalcFADC/CalcFADC.ipynb) | TPC の FADC 波形 → 電荷、1-hit スペクトル |
| ├ [`8_TPC_Calibration/`](1_DataIO/modules/8_TPC_Calibration/TPC_Calibration.ipynb) | アノード × カソードの同時計数 → 反跳電子のエネルギー |
| ├ [`9_TOT_Skewness/`](1_DataIO/modules/9_TOT_Skewness/TOT_Skewness.ipynb) | 電子の飛跡(散乱点・方向・飛程) |
| ├ [`10_PSA_Event/`](1_DataIO/modules/10_PSA_Event/PSA_Event.ipynb) | 吸収点と 1 ヒットの判定(Reconstruct へ渡す) |
| ├ [`11_Reconstruct/`](1_DataIO/modules/11_Reconstruct/Reconstruct.ipynb) | 1 イベント 1 行の再構成ツリー(今は例の値で仕組みだけ) |
| └ [`12_RawTreeWriter_sm3/`](1_DataIO/modules/12_RawTreeWriter_sm3/RawTreeWriter_sm3.ipynb) | 同期した TPC の生データの TTree |
| `4_MakeCalibration/` | 較正ファイルを作る(EricaNext_try のゲインマップ、ドリフト速度と同じ扱い。チェーンはファイルを読むだけ) |
| ├ [`TPC_DriftVelocity`](4_MakeCalibration/TPC_DriftVelocity.ipynb) | RawTreeWriterのROOT → 飛跡積算図・クロック分布 → ドリフト速度 |
| ├ [`TPC_GainMap`](4_MakeCalibration/TPC_GainMap.ipynb) | RawTreeWriterのROOT → 1-hitスペクトル・2本の線のfit → TPCゲインマップ |
| ├ [`PSA_Position`](4_MakeCalibration/PSA_Position.ipynb) | バタフライマップの 64 点 → `calib/psa/position_haNN.csv` |
| ├ [`PSA_TempGain`](4_MakeCalibration/PSA_TempGain.ipynb) | 温度ごとの相対ゲイン → `calib/psa/temp_gain_haNN.csv` |
| └ [`PSA_Energy`](4_MakeCalibration/PSA_Energy.ipynb) | ピクセルごとの hg_corr → keV(Cs-137 の 662 keV)→ `calib/psa/energy_haNN.csv` |

## 準備

1. EricaNext をビルドしておく(`bash scripts/env/build_ericanext.sh`)。
2. Jupyter のカーネルは「EricaNext」(EricaNext_try の `make_jupyter_kernel.sh` で登録したもの、`/usr/bin/python3`)を選びます。
   ノートブックはリポジトリの中のどこから開いても、リポジトリの場所を自分で探します。
3. 入力と出力: 各ノートブックの冒頭のセル「入力と出力」に絶対パスで書きます。データ量が大きいので、どちらもリポジトリの外(NAS など)に置きます。
   `<書き換え>` が残っているとエラーで止まるので、自分の環境のパスに書き換えてから実行してください。
4. データ(入力の既定値):

   | 使うノートブック | データ | 場所(冒頭のセルで書き換える) |
   | --- | --- | --- |
   | PSA のモジュール、4_MakeCalibration | 2026-11-03 の UVSOR の HA_cal(Cs-137 の `20261103_run01`〜`11`、Co の `run12`) | `DATA = /mnt/d/UVSOR2026_late`。解析用の計算機の D ドライブ |
   | TPC のモジュール(1, 7, 8, 9, 12) | 2026-06-23 の TPC_cal(`run_20260623T072649Z`、23 ボード、約 4 GB) | `RUN_DIR`(`<書き換え>`。ランのディレクトリ `run_20260623T072649Z`) |
   | 10_PSA_Event、11_Reconstruct | 2026-11-02 の ETCC(`20261102_run04`、TPC + PSA) | `DATA = /mnt/nas_eureka/SMILE-3FM_data`。研究室の NAS |

チェーンの出力は冒頭のセルの `CACHE`(既定は `/mnt/nas_eureka/onoda/<書き換え>/EricaNext_tutorial`)に置き、ノートブックの間で共有します。
共有するので、全ノートブックで同じ `CACHE` を指定してください。
最初に実行したノートブックがチェーンを流し、以降は再利用します。

チュートリアル専用の関数は使わず、実際に使うときと同じ手順で書いています。

- チェーンの実行: `run_script/run_chain.py`(設定ファイル `config/*.json` を指定)をノートブックから `subprocess` で実行
- 較正ファイルを作る: `scripts/calib/*.py` を `subprocess` で実行
- 結果の読み込み・計算: ROOT の `RDataFrame` と numpy で各ノートブックに直接記述

TPC較正の2冊は既存の `RawTreeWriter_sm3/raw` を入力にします。入力ROOTのパスは冒頭の `RAWS`、出力先は `OUT` で指定します。生データからROOTを用意するコマンドも各冊に記載しています。ゲイン較正の線源・線のエネルギー・電荷のfit範囲は測定に合わせて指定してください。
