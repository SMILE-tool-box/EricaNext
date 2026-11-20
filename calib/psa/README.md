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

今ある位置・エネルギーの表は 20261115 の HA_cal(位置: Co の run07、エネルギー: Cs の run04〜06 と Co の run07)から作った 12 ボード(ha01〜07、09、10、12、13、15)のものです(2026-11-18 に `smile3_reconstruct.json` が使っていた NAS のファイルをコピー)。
エネルギーは温度ゲインの表を使わずに作った(`hg_corr = hg`)ので、温度ゲインの表 `temp_gain_haNN.csv` は置いていません(設定ファイルの `temp_gain_dir` も空)。ha00 と ha16 の表はありません。
前の UVSOR(20261103)の表と温度ゲインの表は、git の履歴(コミット `91afdbd` など)にあります。
作ったときの状態と注意点は AGENTS.md の「較正ファイル（`calib/psa/`）」にまとめています。

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

### 作り方(`scripts/calib/make_psa_geometry.py`)

```bash
/usr/bin/python3 scripts/calib/make_psa_geometry.py calib/detector_coordinates/SM3_Inner_3atm.step -o calib/psa/geometry.csv
# 既にあるファイルを置き換えるときは --overwrite。数秒で終わる
```

入力は 2 つです。

- **CAD の STEP ファイル**(`calib/detector_coordinates/SM3_Inner_3atm.step`): ブロックの**位置と大きさ**、ピクセルの間隔を取る。
- **スクリプトの中の表 `HA_LAYOUT`**(HA のモジュールの配置図から手で書いたもの): **どのブロックがどの HA・ch か**と、**ピクセルの向き**を決める。
  CAD の部品には HA の番号も ch も付いていないので、この対応は CAD からは出せません。

スクリプトがしていることを順に書きます。

#### 1. CAD から結晶のブロックを取り出す(`read_blocks`)

- STEP は CAD のライブラリを使わず、`scripts/calib/step_reader.py` でテキストのまま読みます。組み立ての木(部品の入れ子と、それぞれの置き場所と向き)をたどって、部品ごとに**世界座標での頂点の点**の集まりを出します。
- 名前が `GSO` で始まる部品だけを使います。SMILE-3 の CAD では次の 108 個です。

  | 部品 | 置き場所 | 数 | 大きさ(EricaNext の X × Y × Z) |
  | --- | --- | --- | --- |
  | `GSO_26mm` | 底面 | 36 | 47.74 × 47.74 × 26.06 mm(厚さ 26 mm が Z の向き) |
  | `GSO_13mm` | 北と南の壁 | 36 | 47.74 × 13.06 × 47.74 mm(厚さが Y の向き) |
  | `GSO_13mm` | 東と西の壁 | 36 | 13.06 × 47.74 × 47.74 mm(厚さが X の向き) |

- 頂点を EricaNext の座標に直し(2 を参照)、頂点を囲む直方体を作ります。**その直方体の中心がブロックの中心 `(x0, y0, z0)`**、辺の長さがブロックの大きさです。
  直方体の中心なので、深さの向きは結晶の厚さのちょうど真ん中です(底面なら上の面から 13 mm、側面なら内側の面から 6.5 mm)。
- **ピクセルの間隔 `pitch`**: CAD の結晶には、8 × 8 のピクセルを分ける溝が彫ってあります。40 mm より長い辺の向きで頂点の座標を並べると、
  「外側の端、溝 7 本、外側の端」の 9 か所が、0.06 mm 離れた 2 つずつの組(溝の両側の面)で現れ、合わせて 18 個になります。
  組の始まりどうしの間隔を全ブロック・全向きで平均したものを `pitch` にします(5.960 mm)。

#### 2. CAD の座標から EricaNext の座標へ(`to_det`)

| | CAD | EricaNext |
| --- | --- | --- |
| 原点 | 底板 | μ-PIC の中央(μ-PIC の基板の下の面 = CAD の z = 427.04 mm) |
| 変換 | (x_CAD, y_CAD, z_CAD) | X = y_CAD、Y = x_CAD、Z = 427.04 − z_CAD |

- CAD の z は上向き、EricaNext の Z は下向き(ドリフトの向き)なので、427.04 から引きます。
- CAD は東西・南北に対称で、どちらが東かは CAD からは分かりません。CAD の +y を東(+X)としました。X と Y を入れ替えて Z を反転すると右手系のままになります。
- この変換で、ドリフトケージの天板の内側の面は Z = 317.0 mm になります。GEM は CAD では Z = 10.0 mm ですが、これは 2 枚目の GEM を入れた構成の位置で、今の構成(GEM 1 枚)では 5.0 mm です(`calib/detector_coordinates/README.md`)。
  ブロックの中心は、底面が Z = 346.06 mm、側面が 3 段で Z = 183.04、237.04、291.04 mm、側面の壁の位置は X または Y = ±183.47 mm です。

#### 3. HA のモジュールを見つける

`HA_LAYOUT` には、HA ごとに次の 3 つを書いています(配置図から)。

- **モジュールのおおよその中心 (X, Y)**: ブロックを探すための目印なので、おおよそでよい(例: HA0 は (81, −108))。
- **c**: 配置図の番号で ch0 → ch1 の向き。
- **r**: 配置図の番号で ch0 → ch2 → ch4 の向き。

(データの ch とピクセルの向きは、4 の `FIG_CH` で決まります。)

| 場所 | HA(並び) | c | r |
| --- | --- | --- | --- |
| 底面の東半分 | 北から HA0、1、2 | 北(−Y) | 東(+X) |
| 底面の西半分 | 北から HA3、4、5 | 北(−Y) | 東(+X) |
| 北の壁 | 西から HA8、7、6 | 東(+X) | 下(+Z) |
| 西の壁 | 北から HA9、10、11 | 北(−Y) | 下(+Z) |
| 南の壁 | 西から HA12、13、14 | 西(−X) | 下(+Z) |
| 東の壁 | 北から HA17、16、15 | 南(+Y) | 下(+Z) |

側面の c は「検出器の内側から壁を見て右」、r は「下」です。配置図で側面の壁は底辺を軸に外側へ開いて描かれていて、
どの HA も図の向き(コネクタとヘッドアンプの棒の位置)で同じ形をしているので、こう読めます。

モジュールの中心は、CAD のブロックから次のように決めます。

- (X, Y) の平面で目印に近い順にブロックを並べ、近い 6 個の中心の平均をモジュールの中心にします。
- 底面のモジュールは Z ≥ 330 mm のブロック(底面の結晶)だけ、側面のモジュールは Z < 330 mm のブロック(壁の結晶)だけを候補にします。
  側面では 1 つのモジュールの 6 個が 2 列 × 3 段(深さ違い)なので、平面での距離だけで 6 個がそろいます。

#### 4. ch ごとのブロックを決める

配置図の番号(図の ch)では、モジュールの中のブロックは c の向きに 2 列、r の向きに 3 段で、

```
ch0 ch1      → c
ch2 ch3
ch4 ch5      ↓ r
```

と並びます。ブロックどうしの間隔は 54 mm(`BLOCK_STEP`)です。

データの ch は ADC 番号で、配置図の番号と同じです(README の「PSA の raw ファイルの読み方とチャンネル番号」)。
`FIG_CH`(データの ch c が配置図の ch `FIG_CH[c]` のブロックにある)は、2026-11-20 から `[0, 1, 2, 3, 4, 5]` です。ch ごとに、f = `FIG_CH[c]` として

```
ブロックがあるはずの位置 = モジュールの中心 + (f % 2 − 0.5) × 54 mm × c + (f // 2 − 1) × 54 mm × r
```

を計算し、そこに一番近い CAD のブロックをその ch にします。例えば HA0 では、中心 (81, −108, 346.06)、c = −Y、r = +X なので、
ch0 は (81 − 54, −108 + 27) = (27, −81)、ch1 はその北の (27, −135) です。

結果の並び(`calib/detector_coordinates/ha_views.png`):

| | ch の並び | ピクセル (1, 1) | n が増える向き(u) | m が増える向き(v) |
| --- | --- | --- | --- | --- |
| 底面(上から見る) | ch0 が南西、ch1 がその北、ch2 がその東 | 各ブロックの南西の角 | 北(−Y) | 東(+X) |
| 側面(外から見る) | ch0 が右上、ch1 がその左、ch2 がその下 | 各ブロックの右上の角 | 左 | 下(+Z) |

ピクセルの向きはデータの ch の並びに合わせます(配置図の拡大図と同じ約束): ピクセル (1, 1) は ch0 の側の角、
n は ch0 → ch1 の向き、m は ch0 → ch2 の向きに増えます。

ミューオンの飛跡(20261115 run03 の 20 万行)で、飛跡の通ったブロックとヒットのあったブロックが、底面 96 %・側面 91 % 一致します。
ピクセルの向きは、ミューオンのヒットがほとんど飽和する(位置がブロックの中央に寄る)ため、データではまだ確かめられていません。

次のどれかのときは、止まってエラーを出します(配置図の読み違いや CAD の変更に気づくため)。

- 計算した位置から 1 mm 以内にブロックが無い。
- 同じブロックが 2 つの (HA, ch) に使われた。
- c と r がブロックの面(47.74 mm 角の面)の向きになっていない。例えば側面の c を壁の厚さの向きにしてしまったとき。

CAD のブロックのうち、どの HA にも割り当てられなかったものがあると警告を出します(今は 108 個すべてが割り当てられる)。

#### 5. 書き出す

1 ブロック 1 行で、`board, ch, x0, y0, z0, ux, uy, uz, vx, vy, vz, pitch` を書きます。

- `x0, y0, z0` は 1 で求めたブロックの中心、u、v は 4 のピクセルの向き(単位ベクトル)、`pitch` は 5.960 mm です。
- `PSA_Event` は、ピクセル (n, m)(1〜8)の吸収点を `(x0, y0, z0) + pitch × ((n − 4.5) u + (m − 4.5) v)` とします。
  面の向きはピクセルの中心、深さはブロックの厚さの真ん中です。
- ファイルの先頭の `#` の行に、作った日時、入力の STEP のパス、座標の決め方を書きます。

#### CAD から決まることと、決まっていないこと

| 項目 | 出どころ | 確かさ |
| --- | --- | --- |
| ブロックの位置・大きさ、ピクセルの間隔 | CAD | CAD どおり |
| CAD の +y を東とすること | 仮定(CAD は対称) | 東西・南北の取り違えはデータで確かめる |
| どのブロックがどの HA か | 配置図(`HA_LAYOUT`) | 図の読み取り |
| どのブロックがどの ch か | 配置図(ch = ADC 番号) | ミューオンの飛跡で確認(2026-11-20、底面 96 %・側面 91 %) |
| ピクセルの向き u, v | ch の並びから(4、配置図の拡大図の通り) | **データでの確認がまだ** |
| 吸収の深さ | 厚さの真ん中と仮定 | 底面は Z に ±13 mm、側面は横に ±6.5 mm ずれうる |

- ピクセルの向きは、配置図だけでなく、MPPC の 4 隅の信号の配線(重心 `x_hg`, `y_hg` の式の符号)でも変わります。
  n や m の向きが逆だと、点の位置そのものは CAD どおりでも、ピクセル (n, m) とその点の対応が鏡写しになります。
  線源の位置を変えたランなどで、ピクセルの番号と位置の対応を確かめる必要があります。
- 配置図を読み直して `HA_LAYOUT` や `FIG_CH` を直したら、このスクリプトを流し直し、`scripts/calib/plot_detector_coordinates.py` で
  `calib/detector_coordinates/` の図(`coordinates.png`、`ha_layout.png`、`ha_views.png`)も作り直してください。
  `ha_views.png` は、底面を上から、側面を外から見た図に、HA・ch・ピクセル (1, 1)・n と m の向きを書いたもので、確認に使います。

TPCの再構成位置とPSAジオメトリが同じ座標系で対応するかは、実データでの確認がまだです。
