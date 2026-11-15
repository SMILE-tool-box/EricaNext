# PSA の較正ファイル

PSA（シンチレータの HA）の較正ファイルの置き場です。較正のランから `scripts/calib/` のスクリプトで作ったファイルをここに置き、解析チェーンはそれを読むだけにします。

`NN` は HA の番号（00〜16）です。どのファイルも `#` で始まる行はコメントで、作ったスクリプトが入力ファイルと条件を書いています。

## ファイル

| ファイル | 中身 | 読むモジュール（設定） | 作り方 |
| --- | --- | --- | --- |
| `psa_param.csv` | HA ごとのボードのシリアル番号、IP、HV、DAC と、温度センサー 6 個（TL, TR, ML, MR, BL, BR）の較正係数（`_A`, `_B`）。`CAL_NOTE` 列は仮の値の印 | `PSA_CalcFADC`（`psa_param_file`） | `PSA_analysis/param/psa_param_20261029.csv` のコピー |
| `position_haNN.csv` | ピクセルの位置。列は `ch,No,x,y,z,n,m`。ch（0〜5）ごとに 8 × 8 = 64 点で、バタフライマップ上の (x, y) とピクセルの番号 (n, m) の対応 | `PSA_Calibration`（`position_dir`） | `make_psa_position.py`、[PSA_Position.ipynb](../../tutorials/4_MakeCalibration/PSA_Position.ipynb) |
| `temp_gain_haNN.csv` | 温度によるゲインの変化。列は `ch,temp,gain,photopeak,hits`。1 °C ごとの相対ゲイン（基準の温度で 1） | `PSA_Calibration`（`temp_gain_dir`） | `make_psa_temp_gain.py`、[PSA_TempGain.ipynb](../../tutorials/4_MakeCalibration/PSA_TempGain.ipynb) |
| `energy_haNN.csv` | ピクセルごとのエネルギーの係数。列は `ch,n,m,p0,p1,src,…`。エネルギー = `p0` + `p1` × 温度補正後の波高。`src` は係数の求め方 | `PSA_Calibration`（`energy_dir`） | `make_psa_energy.py`、[PSA_Energy.ipynb](../../tutorials/4_MakeCalibration/PSA_Energy.ipynb) |
| `geometry.csv` | シンチレータのブロックの位置と向き。列は `board,ch,x0,y0,z0,ux,uy,uz,vx,vy,vz,pitch`。座標は EricaNext の座標（[detector_coordinates](../detector_coordinates/README.md)） | `PSA_Event`（`geometry_file`） | `make_psa_geometry.py`（CAD の STEP から） |
| `geometry_template.csv` | `geometry.csv` の書式の見本（列の意味の説明だけ） | 使わない | — |
| `qa/` | 作るときに出る確認の図（`position_haNN.png`, `temp_gain_haNN.png`, `energy_haNN.png`）。使う前に必ず見る | 使わない | 各スクリプトが書く |

## 作る順番

1. 位置（`position_haNN.csv`）と温度ゲイン（`temp_gain_haNN.csv`）を作る
2. その2つを置いた状態で線源のランを流し直す
3. エネルギー（`energy_haNN.csv`）を作る（温度補正後の波高とピクセルを使うため）

`geometry.csv` は CAD から作るので、ほかのファイルとは関係なく作れます。

## ファイルが無いボード

- `position_haNN.csv` が無いボードは、ピクセルを求めません。
- `temp_gain_haNN.csv` が無いボードは、温度の補正をしません。
- `energy_haNN.csv` が無いボードは、エネルギーが -1 になり、`PSA_Event` はそのヒットを使いません。

どのボード・ch のファイルがあるか、作ったときの状態と注意点は AGENTS.md の「較正ファイル（`calib/psa/`）」にまとめています。
