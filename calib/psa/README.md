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

## 設定との対応

第2段階の `config/stage2/smile3_psa.json`・`smile3_reconstruct.json` で、
`PSA_CalcFADC.psa_param_file`、`PSA_Calibration.position_dir`・`temp_gain_dir`・`energy_dir`、
`PSA_Event.geometry_file` に較正ファイルの場所を指定します。

作成手順は [Make_calibration](../../tutorials/4_MakeCalibration/README.md) を参照してください。
生データを `smile3_psa_raw` で一度ROOTへ保存し、較正値を更新した後は `smile3_psa --root-input` で後段処理を繰り返します。
`psa_param.csv` が無いボードは、温度センサー係数の既定値（a = 3.9258、b = 1230.30）を使います。

## geometry.csvの座標と作成方法

1 行に 1 ブロック(HA のボード `board` と ch 0〜5)で、単位は mm です。座標は [`tutorials/0_Introduction`](../../tutorials/0_Introduction/Introduction.ipynb) のもの
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
/usr/bin/python3 scripts/calib/make_psa_geometry.py calib/detector_coordinates/SM3_Inner_3atm.step -o calib/psa/geometry.csv
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
