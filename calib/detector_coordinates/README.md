# 検出器の座標系

SMILE-3 の内部（3 気圧の容器の中）の CAD と、EricaNext で使う座標系をまとめたフォルダです。

## ファイル

| ファイル | 中身 |
| --- | --- |
| `SM3_Inner_3atm.step` | SMILE-3 の内部の組み立て（TPC、μ-PIC、GEM、ドリフトケージ、底面と側面の HA のモジュール）。2026-11-06 に Autodesk の CAD から書き出した STEP（AP214） |
| `coordinates.png` | 再構成で使う位置の図: μ-PIC・GEM・天板の Z と、HA の全ピクセルの吸収点（`calib/psa/geometry.csv` から）を EricaNext の座標で描いたもの |

図は次のコマンドで作り直せます（`geometry.csv` を作り直したら、図も作り直す）。

```bash
/usr/bin/python3 scripts/calib/plot_detector_coordinates.py calib/psa/geometry.csv -o calib/detector_coordinates
```

## EricaNext の座標系

![EricaNext の座標系](coordinates.png)

再構成で使う位置は、μ-PIC・GEM・天板の Z と、HA のシンチレータの各ピクセルの位置だけです。図はこの位置関係だけを描いています。
左上が 3D、右上が上から見た図（北が上）、下の2つが横から見た図です。
赤・橙・青の線が μ-PIC・GEM・天板（μ-PIC の範囲 ±153.6 mm で描いた）、緑の点が底面の HA、黄緑の点が側面の HA のピクセルの吸収点です。

### HA のピクセルの点はどこか

各点は、シンチレータのピクセル1つ1つ（直方体）の中心です。`PSA_Event` がガンマ線の吸収点として使う位置そのものです。
`scripts/calib/make_psa_geometry.py` が次のように決めています。

1. ブロックの中心 `(x0, y0, z0)`: CAD の結晶ブロック（底面 `GSO_26mm`、側面 `GSO_13mm`）の頂点を囲む直方体の中心。8 × 8 ピクセルのブロック全体の中心で、深さ方向は結晶の厚さの真ん中
2. ピクセルの間隔 `pitch`: CAD のピクセルの溝の位置から 5.96 mm
3. ピクセル (n, m) の点: `(x0, y0, z0) + pitch × ((n − 4.5) u + (m − 4.5) v)`（n, m = 1〜8）。面の方向は各ピクセルの中心、深さ方向はブロックの厚さの真ん中

注意:

- 実際に結晶のどの深さで吸収されたかは分からないので、厚さの真ん中を代表点にしています。
  底面（厚さ 26 mm）は Z に ±13 mm、側面（厚さ 13 mm）は横方向に ±6.5 mm のずれがありえます。
- ピクセルの向き（u, v。n と m が増える向き）は配線（4 隅の信号の重心の式の符号）でも変わるので、データでの確認がまだです。
  点の位置そのものは CAD どおりですが、どの点がどの (n, m) かはまだ確定していません。

| | EricaNext | CAD との関係 |
| --- | --- | --- |
| 原点 | μ-PIC の中央（μ-PIC の基板の下の面） | CAD の (x, y, z) = (0, 0, 427.04 mm) |
| X | 西 → 東 | X = y_CAD |
| Y | 北 → 南 | Y = x_CAD |
| Z | 下向き（ドリフト領域の方向） | Z = 427.04 − z_CAD |

CAD で確かめた寸法（EricaNext の Z）:

| 位置 | Z |
| --- | --- |
| μ-PIC | 0 mm |
| GEM | 10.0 mm |
| ドリフトケージの天板の内側の面 | 317.0 mm |

座標系の説明は [tutorials/0_Introduction](../../tutorials/0_Introduction/Introduction.ipynb) にもあります。

## 使い道

- `scripts/calib/make_psa_geometry.py` が、このファイルから `calib/psa/geometry.csv`（PSA_Event が使う HA のブロックの位置と向き）を作ります。
- STEP の組み立ての木は `scripts/calib/step_reader.py` で読みます。部品と大きさの一覧は次のコマンドで見られます。

  ```bash
  /usr/bin/python3 scripts/calib/step_reader.py calib/detector_coordinates/SM3_Inner_3atm.step '<部品の名前の正規表現>'
  ```
- シミュレーター（Geant4）の配置も、このファイルからそろえる予定です（AGENTS.md の残りの仕事）。
