# チュートリアル

EricaNext_try の `tutorials_caldata` と同じ形です。モジュールの解説の番号は、設定ファイル `smile3_reconstruct_psa` のチェーンの順です。

| フォルダ | 中身 |
| --- | --- |
| [`0_Introduction`](0_Introduction/Introduction.ipynb) | SMILE-3 の検出器の構成(μ-PIC の 4 分割、ストリップ 0.4 mm、MPPC の PSA)と SMILE-2+ からの変更点、座標系 |
| `1_DataIO/modules/` | 各モジュールの解説 |
| ├ `1_ReadData/` | (まだ)TPC の生データの読み込みとボード間のイベント同期 |
| ├ [`2_PSA_ReadData/`](1_DataIO/modules/2_PSA_ReadData/PSA_ReadData.ipynb) | シンチレータ(PSA)の生データの読み込みとイベントの同期 |
| ├ [`3_PSA_CalcFADC/`](1_DataIO/modules/3_PSA_CalcFADC/PSA_CalcFADC.ipynb) | 波形の積分(hg, lg)、ピークの時刻、飽和、温度 |
| ├ [`4_PSA_CenterGravity/`](1_DataIO/modules/4_PSA_CenterGravity/PSA_CenterGravity.ipynb) | 4 隅の信号の重心(フラッドマップ) |
| ├ [`5_PSA_Calibration/`](1_DataIO/modules/5_PSA_Calibration/PSA_Calibration.ipynb) | 位置の補正とピクセル、温度補正、エネルギー |
| ├ [`6_PSA_TreeWriter/`](1_DataIO/modules/6_PSA_TreeWriter/PSA_TreeWriter.ipynb) | PSA のヒットの TTree の列と読み方 |
| ├ `7_CalcFADC/` | (まだ)TPC の FADC 波形 → 電荷 |
| ├ `8_TPC_Calibration/` | (まだ)アノード × カソードの同時計数 → 反跳電子のエネルギー |
| ├ `9_TOT_Skewness/` | (まだ)電子の飛跡 |
| ├ [`10_PSA_Event/`](1_DataIO/modules/10_PSA_Event/PSA_Event.ipynb) | 吸収点と 1 ヒットの判定(Reconstruct へ渡す) |
| ├ `11_Reconstruct/` | (まだ)1 イベント 1 行の再構成ツリー |
| └ `12_RawTreeWriter_sm3/` | (まだ)同期した TPC の生データの TTree |
| `4_MakeCalibration/` | 較正ファイルを作る(EricaNext_try のゲインマップ、ドリフト速度と同じ扱い。チェーンはファイルを読むだけ) |
| ├ [`PSA_Position`](4_MakeCalibration/PSA_Position.ipynb) | フラッドマップの 64 点 → `calib/psa/position_haNN.csv` |
| ├ [`PSA_TempGain`](4_MakeCalibration/PSA_TempGain.ipynb) | 温度ごとの相対ゲイン → `calib/psa/temp_gain_haNN.csv` |
| └ [`PSA_Energy`](4_MakeCalibration/PSA_Energy.ipynb) | ピクセルごとの hg_corr → keV → `calib/psa/energy_haNN.csv`(線源のランが要る。まだ無い) |

## 準備

1. EricaNext をビルドしておく(`bash scripts/env/build_ericanext.sh`)。
2. Jupyter のカーネルは「EricaNext」(EricaNext_try の `make_jupyter_kernel.sh` で登録したもの、`/usr/bin/python3`)を選びます。
   ノートブックはリポジトリの中のどこから開いても、リポジトリの場所を自分で探します。
3. データ:

   | 使うノートブック | データ | 場所(環境変数で変えられる) |
   | --- | --- | --- |
   | PSA のモジュール、4_MakeCalibration | 2026-11-03 の UVSOR の HA_cal(`20261103_run01`〜) | `/mnt/d/UVSOR2026_late`(`SMILE3_PSA_CAL_DATA`)。解析用の計算機の D ドライブ |
   | 10_PSA_Event | 2026-11-02 の ETCC(`20261102_run04`、TPC + PSA) | `/mnt/nas_eureka/SMILE-3FM_data`(`SMILE3_FM_DATA`)。研究室の NAS |

チェーンの出力は `tutorials/cache/` に置き(git には入れない)、ノートブックの間で共有します。
最初に実行したノートブックがチェーンを流し、以降は再利用します。

チュートリアル専用の関数は使わず、実際に使うときと同じ手順で書いています。

- チェーンの実行: `run_script/run_chain.py`(設定ファイル `config/*.json` を指定)をノートブックから `subprocess` で実行
- 較正ファイルを作る: `scripts/calib/*.py` を `subprocess` で実行
- 結果の読み込み・計算: ROOT の `RDataFrame` と numpy で各ノートブックに直接記述
