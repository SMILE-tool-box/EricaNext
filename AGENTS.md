# AGENTS.md — EricaNext(SMILE-3)の引き継ぎ

このリポジトリで作業する人と AI エージェント(Codex、Claude Code など)向けの引き継ぎ書です。
使い方の説明は [`README.md`](README.md)、設定ファイルの書き方は [`config/README.md`](config/README.md)、スクリプトは [`run_script/README.md`](run_script/README.md) にあります。
ここには、**それらに書いていない作業の約束・環境の落とし穴・これまでの経緯・残りの仕事**をまとめます。

最終更新: 2026-11-17(ブランチ `feature-restructure`)

---

## 1. 何のリポジトリか

- SMILE-3(ETCC: ガス TPC + シンチレータ)の生データから、イベント再構成までを行う ANLNext のモジュール群です。C++ で書き、SWIG で Python(`EricaPy`)から使います。
- チェーンは `config/<名前>.json` に書き、`run_script/run_chain.py` が組み立てて実行します。モジュールの順番やパラメータをスクリプトに直接書いてはいけません。
- SMILE-2+ 版は [EricaNext_try](https://github.com/SMILE-tool-box/EricaNext_try)(`~/ANLNext/EricaNext_try`)です。構成はそれに合わせています。

| ディレクトリ | 中身 |
| --- | --- |
| `source/modules/<モジュール>/` | ANLNext のモジュール。足したら `source/modules/sources.cmake` と `source/pyext/EricaPy.i` の両方に書く |
| `config/` | チェーンの設定ファイル(`*.json`)と読み込み(`ericanext_config.py`)。SMILE-3 は `smile3_tpc_only`(TPC だけ、TOT_Skewness まで)、`smile3_psa`、`smile3_reconstruct`(TPC + PSA)、`smile3_raw` |
| `calib/psa/` | PSA(シンチレータ)の較正ファイル |
| `calib/tpc/` | TPC の較正ファイル(ゲインマップ、ドリフト速度)の置き場。まだ空(README だけ) |
| `calib/detector_coordinates/` | SMILE-3 の内部の CAD(STEP)と座標系の説明。`coordinates.png` は再構成で使う位置(μ-PIC・GEM・天板の Z、HA の全ピクセル)の図で、`scripts/calib/plot_detector_coordinates.py` が `calib/psa/geometry.csv` から作る。2026-11-15 に `calib/cad/` から名前を変えた |
| `run_script/` | `run_chain.py`、`run_runs.sh`、補助スクリプト |
| `scripts/env/` | ビルド(`build_ericanext.sh`) |
| `scripts/check/` | 検証用のスクリプト(Python の解析との突き合わせ) |
| `tutorials/` | ノートブック(検出器と座標系: `0_Introduction`) |

## 2. 環境(このマシン: WSL2、ユーザー onoda)

```bash
source ~/opt/setup_ericanext.sh          # ANLNext(~/opt/anlnext-g4)、Geant4、ライブラリのパス
bash scripts/env/build_ericanext.sh      # build/ と install/ を消して作り直す(数分)
# 少しの変更なら: cd build && make -j$(nproc) && make install
cd run_script && python3 run_chain.py smile3_psa -o /tmp/psa.root --data-dir <ラン>
```

落とし穴:

- **Python が 2 つある**。
  - `python3`(miniforge、3.13): `EricaPy` はこれ向けにビルドされている。`run_chain.py` はこちらで動かす。numpy は入っていない。
  - `/usr/bin/python3`: numpy、scipy、h5py、numba、natsort、PyROOT が入っている。`psa_position_nm.py` や `scripts/check/*.py`、`analysis_sm3_FM/PSA_analysis` のスクリプトはこちらで動かす。
- **push は SSH の URL を直接指定する**。`origin` は https で、このマシンには認証情報が無い。
  `git push git@github.com:SMILE-tool-box/EricaNext.git <ブランチ>`
- **チェーンの中で `AS_SKIP` を返すと、そのイベントの後ろのモジュールは全部動かない**(ANLNext の仕様)。TPC の `CalcFADC` は 2026-11-06 まで「アノード 1 ch・カソードあり」以外のイベントを `FADC_NoHit` で `AS_SKIP` していた(6 月の TPC_cal で 62%、うち 29% はアノード 2 ch 以上の普通のイベント)。SMILE_tool_box(`~/SMILE_tool_box_v2.20/Erica/CalcFADC`)と EricaNext_try に合わせて、今は飛ばさない。2026-11-06 からは、CalcFADC は電荷を計算するだけで、図も印も作らない(ベースライン・電荷の分布と 1-hit スペクトルは、生データのツリーから `scripts/calib/make_tpc_spec1hit.py` で後段で作る。前の CalcFADC の図 808 個とビンまで一致を確認)。`TPC_Calibration` は `TPC_Ee_Valid`(エネルギー ≠ 0)を立て、`Reconstruct` はそれを見る。SMILE_tool_box の TPC_Calibration にある「同時計数が 100 個を超えるパネルだけ足す」条件は、SMILE-3 版にはまだ無い。
- `--data-dir` を指定しても `ericanext_config: WARNING: no storage_g*/ in .../mount_data` が出る。`mount_data/` が空なだけなので、気にしなくてよい(以前からの動き)。
- `build/`、`install/`、`mount_data/`、`root_data/` は `.gitignore` に入っている。

環境の予備確認は `scripts/env/check_environment.py`。ビルド前は `--mode build`、ビルド後は `--mode all`。チェーン用と解析用のPythonを別々に指定できる。データ処理・依存のインストールは行わない。

## 3. データの場所

| データ | 場所 |
| --- | --- |
| SMILE-3 FM の生データ(NAS) | `/mnt/nas_eureka/SMILE-3FM_data/<日付>/<storage>/<日付>_runNN/`。`calltt` = storage_g0(アノード `tpc_enc_a*`、ha06〜ha16)、`killua` = storage_g1(カソード `tpc_enc_c*`、ha00〜ha05)。モードは `run_meta.json` の `mode`(TPC_cal、HA_cal、ETCC) |
| PSA の較正データ(UVSOR、2026-11-03) | `/mnt/d/UVSOR2026_late/{calltt,killua}/20261103_runNN`(このマシンの D ドライブだけ。環境変数 `SMILE3_PSA_CAL_DATA`)。run01〜11 は Cs-137 の HA_cal(39〜49 °C、1 ランは約 2 分、多くは storage の OOM で途中で止まり .dat.tmp が残る)、run12 は Co の HA_cal(4 時間、47〜49 °C、ha04 なし)。run12 は `part0`〜`part4` のサブディレクトリに分かれ、killua の part0 と part2 には同じファイルのコピーが重なっている(13172 組、大きさは全部同じ) → `PSA_ReadData.recursive: true` で読む(重なりは警告して 1 つだけ使う)。ランの記録は Notion の「SMILE-3 FM UVSOR 試験(2026-11-03)データ確認と HA_cal の解析」と「UVSOR 試験 run12(Co)の HA_cal の解析」 |
| PSA の小さいテストラン | `~/analysis_sm3_FM/PSA_analysis/data/cal_psa/run_kyoto/20261029_run06`(ha03 だけ、4 ファイル、約 1.5 万ヒット) |
| PSA の Python 解析(参照の実装) | `~/analysis_sm3_FM/PSA_analysis/src_onoda`(`psa_decode.py`、`peak_search.py`、`grid_spect.py`、`pixel_id.py`)。リポジトリは haru2510/QL |

- 1 つのランのボードは 2 つの storage に分かれる。PSA だけなら、`--data-dir` に片方、`--set 'PSA_ReadData.extra_data_dirs=["<もう片方>"]'` にもう片方を渡す。
- TPC も読むときは、片方を `--data-dir`、もう片方を `--extra-data-dir` で渡す(`ReadData` と `PSA_ReadData` の `extra_data_dirs` に入る。2026-11-06 から。前はシンボリックリンクで 1 つのディレクトリに集めていた)。
  例: `python3 run_chain.py smile3_reconstruct -o run04.root --data-dir <NAS>/20261102/calltt/20261102_run04 --extra-data-dir <NAS>/20261102/killua/20261102_run04`
  run04 で、シンボリックリンクで集めたディレクトリから読んだ出力と全部一致することを確かめた。

## 4. 作業の約束

- ドキュメント、設定ファイルの `_doc`、README は**日本語**で書く。コード中のコメント、コミットメッセージは英語で書く(これまでのコミットと同じ形)。
- 設定ファイルにパラメータを足したら、`_doc`(全パラメータ)と `_units`(単位のあるもの)にも書く(`config/README.md`)。
- 小数のパラメータは JSON で `30.0` と書く(`30` だと型が合わずに止まる)。
- PSA の計算を変えたら、`scripts/check/compare_psa_python.py` で Python の解析と一致するか確かめる(5 節)。わざと変える場合は、何が違うかを README に書く。
- 較正の値やジオメトリの値は**推測で作らない**。決まっていない値は、ファイルを置かずに警告で知らせる(今のモジュールはどれもそう動く)。
- 作業はブランチ `feature-restructure` でしている。`main` に直接コミットしない。

### ReadDataの図出力（2026-11-13）

SMILE-3の `ReadData` から積算ヒストグラム・波形ヒストグラム・PNG出力とその選別パラメータを削除した。
波形・ヒット・時刻は `TPC_RawTreeWriter` で保存し、積算・選別・描画は後段で行う。
`Track_A/C` は再構成モジュールが `Get_Track_A/C` を初めて呼んだときだけ生成する。ROOTには書かない。raw保存だけのチェーンでは生成・Reset・Fillを行わない。
SMILE-2+の `ReadData_2p` は今回の変更対象ではない。

## 5. PSA(シンチレータ)のモジュール — 2026-11-05 時点

`analysis_sm3_FM/PSA_analysis` の `psa_decode.py` → `pixel_id.py` を、ANLNext のモジュールに移しました。

| モジュール | すること |
| --- | --- |
| `PSA_ReadData` | `ha<NN>_<番号>.dat` を全ボード読む(複数のディレクトリに散らばったボードのファイルは番号でまとめる。`recursive` でサブディレクトリも)。`ReadData` があればそのトリガーカウンタに合わせて読み(`sync_tpc`)、無ければボード同士をトリガーカウンタでそろえる。中身の読み取りは `PSA_Board` |
| `PSA_CalcFADC` | ペデスタル(最初の 23 サンプル)、ピーク(LG の和の最大)、台形積分(前 4〜後 9)、`hg`・`lg`、温度 |
| `PSA_CenterGravity` | 4 隅の重心 `x_hg`, `y_hg`, `x_lg`, `y_lg` |
| `PSA_Calibration` | 64 点の薄板スプラインで位置を直してピクセル (n, m)、温度ゲインの表で `hg_corr = hg / gain(T)`、`energy = p0 + p1 × hg_corr` |
| `PSA_TreeWriter` | 1 ヒット 1 行の TTree `PSA_TreeWriter/psa` |
| `PSA_Event` | ピクセルとジオメトリから吸収点 `Pg` を出し、`PSA_SingleHit` / `PSA_MultiHit` / `PSA_NoHits` を立てる。`Reconstruct` に渡す |

`Reconstruct` は `PSA_Event` を使います(SMILE-2+ の HA は `Reconstruct_2p` + `HA_Calc_2p`)。
`single_hit` には `evt_id`(トリガーカウンタ)のブランチを足しました。

設定ファイル:

- `smile3_psa.json`: PSA だけ。
- `smile3_reconstruct.json`: TPC + PSA → `Reconstruct`。

### 確かめたこと

| データ | 結果 |
| --- | --- |
| 20261029_run06(ha03)、窓はピークの周りと固定の両方 | psa_decode + pixel_id と 15432 ヒットすべて一致。hg などは相対 1e-6 以内(Python 側の float32 の丸め) |
| 20261102_run02 HA_cal(14 ボード、2 つの storage) | ha00、ha06 で一致。x, y の差は \|hg\| < 60 のノイズのようなヒットだけ(割り算の桁落ち)。ピクセルは全部一致 |
| 20261102_run03 / run04 ETCC、TPC と一緒に読む | ヒットのある PSA のフレームは、全ボードで取りこぼし無くツリーに入る |
| `smile3_reconstruct` + ダミーの較正値(テスト専用、リポジトリには入れていない) | single_hit の 20 行すべてで、Eg、Pg が式どおり |

検証のやり方:

```bash
cd run_script && python3 run_chain.py smile3_psa -o /tmp/run06.root \
    --data-dir ~/analysis_sm3_FM/PSA_analysis/data/cal_psa/run_kyoto/20261029_run06
/usr/bin/python3 ../scripts/check/compare_psa_python.py /tmp/run06.root \
    ~/analysis_sm3_FM/PSA_analysis/data/cal_psa/run_kyoto/20261029_run06 3 \
    ../calib/psa/psa_param.csv ../calib/psa/position_ha03.csv
# 期待: trig, ch identical / mismatches 0 / hg などの max rel が 1e-6 前後
```

### 較正ファイル(`calib/psa/`)

EricaNext_try のゲインマップ・ドリフト速度と同じ扱い: **較正のランから `scripts/calib/*.py` で作ってファイルに置き、チェーンは読むだけ**。
作り方は `config/README.md` の「PSA の較正ファイル」と `tutorials/4_MakeCalibration/PSA_*.ipynb`。どのファイルも `#` の行はコメント(作ったスクリプトが入力と条件を書く)。

| ファイル | 作るスクリプト | 状態 |
| --- | --- | --- |
| `psa_param.csv` | (`PSA_analysis/param/psa_param_20261029.csv` のコピー) | あり。ha09、ha10、ha12、ha13、ha15 の温度センサーの較正は仮の値(`CAL_NOTE` 列) |
| `position_haNN.csv` | `make_psa_position.py --method grid`(既定。x の射影で 8 列、列ごとに y の射影の 2 階微分で 8 行。`--method localmax` は `peak_search.py` と同じで、同じヒストグラムで 64 点が一致することを確かめた) | 2026-11-18 から 20261115 の HA_cal の Co run07(トリガー時刻 peak_id 26〜29、3 万 ≤ hg ≤ 50 万、飽和なし、`grid`)から作った 12 ボード(ha01〜07、09、10、12、13、15)。全 ch あり、補正後に 8 × 8 に入る割合は 94.5〜97.6%。ha00、ha16 は無い。前の UVSOR(20261103 run12)の表は git の履歴にある |
| `temp_gain_haNN.csv` | `make_psa_temp_gain.py`(光電ピークの探し方とトリガー時刻の範囲は `temp_spectra.py` と同じ) | 2026-11-18 から置いていない(今のエネルギーの表は温度ゲインなしで作った。設定ファイルの `temp_gain_dir` は空)。UVSOR の Cs run01〜11 から作った 13 ボードの表は git の履歴(`91afdbd`)にある |
| `energy_haNN.csv` | `make_psa_energy.py`(`--cs` と `--co`。fit は `analysis_sm3_FM` の `energy_calib.py` と同じで、ピクセルごと) | 2026-11-18 から 20261115 の Cs run04〜06 と Co run07 の 3 点で作った 12 ボード(温度ゲインなし、`hg_corr = hg`、全温度のヒット)。ch の和の Co が確認に通らない 4 ch(ha01 ch3/5、ha09 ch1、ha13 ch0)は Cs だけ(`p0 = 0`)。ピクセルの `src` は pixel 237〜383、残りは cs+ch。ha00、ha16 は無い。前の UVSOR の表(2026-11-07)は git の履歴にある |
| `geometry.csv` | `make_psa_geometry.py`(CAD の STEP `calib/detector_coordinates/SM3_Inner_3atm.step`。HA の番号・ch・ピクセルの向きはスクリプトの `HA_LAYOUT`、HA のモジュールの配置図から) | 18 HA × 6 ch = 108 ブロック(2026-11-06)。ピクセルの向き(u, v)は配線でも変わるので、データでの確認がまだ |

- 作る順番: 位置、温度ゲイン → (それを置いて線源のランを流す) → エネルギー。エネルギーは `hg_corr` とピクセルを使う。
- エネルギーの確認(2026-11-07、温度ゲインの表の範囲の中のヒットを ch ごとに足してエネルギーに直し、線を fit): Cs は全 ch で 657〜663 keV。3 点で作った ch は Co が 1165〜1174 / 1331〜1341 keV。
  Cs だけ(`p0 = 0`)の ch は 1 MeV 付近で 2〜10 % 低い(ha00 ch3 で 1068 / 1213 keV)。ha01 の Cs だけの ch は Co が 740〜1070 keV で、Co のスペクトル自体が Cs のすぐ上に寄っている(温度ゲインの表の高温の端)。
- `<out>/qa/` に確認の図が出る(git には入れない)。使う前に必ず見る。
- `--method localmax`(peak_search.py と同じ)は、縦に長い中央のスポットから何個もピークを拾って端の列を押し出す(統計を増やしても直らない)。`grid` はそのために作った。中央の列の上下の行は山ではなく列の端の斜面の肩なので、極大ではなく 2 階微分で探している。
- ch ごとに図で確かめ、悪い ch はファイルから行を消す(その ch はピクセルを求めない)。
- 温度ゲインは、39.5〜41.5 °C ではほぼ一定で、42.5 °C で約 15% 下がる(ha02)。HV(設定値もフレームの DAC 値も 15853)と Vth は run01〜11 で同じ。温度依存性は過去にも報告されている。

## 6. 残りの仕事(上ほど優先)

1. **エネルギーの較正の残り**: Cs + Co の 3 点で作った(2026-11-07)。ch の和の Co が使えない 11 ch は Cs だけ(`p0 = 0`)。ha01 は Co のランで 49〜50 °C と温度ゲインの表の上の端にいて、Co のスペクトルが期待した形にならない(温度ゲインの表の高温側の統計が少ない)。温度ゲインの表を Co のラン(または高温の Cs のラン)まで広げられれば直る。ha16 ch1 は Cs のピークも広い。ha04 の較正(位置・温度ゲイン・エネルギー)は、ha04 のデータのある較正ランが要る。
   - **本当は温度ごとのエネルギーの較正が要る**。今は「温度は ch ごとのゲイン gain(T) を変えるだけで、直線 `p0 + p1 × hg_corr` の形(とピクセルごとの違い)は温度によらない」と仮定している。
     gain(T) は Cs の 662 keV の 1 点(39〜49 °C)から、p0 と p1 は Co のある 47〜48 °C 付近(温度ゲインの表と重なるところ)だけから作っている。
     温度で直線性(`p0`)やピクセルごとのゲインの比が変わるなら、この仮定は外れる。確かめて直すには、2 本以上の線源(Cs と Co など)を広い温度で取ったランが要る。
     そのランが取れたら、`energy_haNN.csv` に温度の列を足して温度ごとに `p0`, `p1` を持たせ、`PSA_Calibration` で温度の間を線形に補間する形にするのが素直(温度ゲインの表と同じ扱い。`hg_corr` は使わず `hg` から直接でもよい)。
2. **ジオメトリ**(`geometry.csv`): CAD から作った(5 節の表)。残り:
   - ピクセルの向き(u, v)をデータで確かめる(例えば、線源の位置を変えたランで、ピクセルの番号と位置の対応を見る)。
   - シミュレーター(Geant4)の配置と同じ元(この STEP と `HA_LAYOUT`)からそろえる。SMILE-2+ では `HAs.pos` が 2 つあって、底面の z がシミュレーションと実データで 11 mm 違っていた(EricaNext_try の `docs/simulation_port_log.md`)。シミュレーターの製作ログの TPC の Z(10.5〜308 mm)は、この CAD(GEM 10 mm、天板 317 mm)と違うので、古い形状のはず。
3. **イベントの選び方の確認**
   - `PSA_Event` の「1 ヒット」は「ピクセルとエネルギー > `energy_min` があり、geometry にあるヒットが 1 つ」。ノイズのヒットで MultiHit になりすぎないかを、実データで見る。
4. **チュートリアル**: モジュールの解説(`tutorials/1_DataIO/modules/1〜12`)と `4_MakeCalibration/PSA_*` はある。まだ無いものは 1_DataIO の全体の流れの本(EricaNext_try の `DataIO_example`)。TPC のゲインマップとドリフト速度の較正の本は2026-11-13に追加済み（`tutorials/4_MakeCalibration/TPC_GainMap.ipynb`, `TPC_DriftVelocity.ipynb`）。TPC_RawTreeWriterのROOTを入力とし、前者は1-hitスペクトルと指定した2線のfit、後者は飛跡積算図と既存の速度算出スクリプトを使う。実測較正値はまだ確定していない。生成スクリプトはリポジトリに無い(ノートブックを直接直す)。
5. **TPC 側で分かっていて、まだ直していないこと**
   - `TPC_Calibration` には、SMILE_tool_box の 2 つの条件(同時計数が 100 を超える組 (i, j) だけを使う、各チャンネルの 32 ストリップの両端 4 本ずつを数えない)が無い。**今は入れない**と決めた(2026-11-06)。SMILE-3 のゲインマップを作るときに、要るかどうかをもう一度見る。
   - `Reconstruct` の幾何の α・入射方向(2026-11-08): SMILE_tool_box の `Erica/Reconstruct`(TRA、SingleHit)と SMLib `Compton_common`(`recon_vector`、`ARM`、`SPD`)と同じ式で入れた(EricaNext_try の `Reconstruct_2p` には無い)。
     `smile3_reconstruct` に `TOT_Skewness` を足した(AS_SKIP するので `Reconstruct` の直前)。`TOT_Skewness` の `Ve` は長さに意味の無い方向なので、`Reconstruct` で単位ベクトルにする。
     **残り**: TPC の位置(`TOT_Skewness` の `Get_TPC_XYZ`、SMILE-2+ のパネルの並びのまま μ-PIC の中心が原点)と、PSA の `geometry.csv`(X: 東、Y: 南、Z: 下)が同じ座標系かを確かめる。
     TPC のゲインマップが無いので `Ke` は ADU で、角度の値にはまだ意味が無い。
   - 2026-06-23 の TPC_cal には、鳴りっぱなしのストリップを拾って `Len` が数百 mm になるイベントが多い。
   - TOT 補正の係数、skewness のオフセットは SMILE-2+ の値のまま。
   - ドリフト速度: `clock_offset` = 85、`pitch_z_mm_per_clock` = 0.383 は SMILE-2+ の典型値(Per02/Per03)。SMILE-3 の測定はまだで、`drift_velocity_file` が空だと TOT_Skewness が警告を出す。GEM 面は `z_gem_mm` = 10 mm(SMILE-3)、ドリフト長(μ-PIC〜天板)317 mm。6/23 の TPC_cal はクロックの分布が箱の形にならず、ドリフト速度は出せない。ファイルを作るスクリプト `scripts/calib/make_tpc_drift_velocity.py` はある(SMILE-2+ の Per02 で EricaNext_try と同じ値を確認)。測定ができたら流して `calib/tpc/drift_velocity.dat` を置き、`smile3_tpc_only.json` の `drift_velocity_file` に書く。
   ノートブックの生成と実行: `jupyter nbconvert --to notebook --execute --inplace --ExecutePreprocessor.kernel_name=ericanext <nb>`(`/usr/bin/python3 -m jupyter`)。
6. **SMILE-2+ のデータも同じ EricaNext で扱う**: 2026-11-06 に取り込んだ(README の「SMILE-2+ のデータ」)。
   - EricaNext_try の `source/modules/` の 8 つ(ReadData、CalcFADC、TPC_Calibration、TOT_Skewness、HA_CenterGravity、HA_Calc、Reconstruct、RawTreeWriter)を `<名前>_2p/` にコピーし、クラス名・ファイル名・`get_module` の名前・補助クラス(`CP80256`、`TPC_Encoder`、`MDF`)・インクルードガードに `_2p` を付けた。印の名前も `TOT_Skewness_2p` など変わったものがある。ANLNext は名前でモジュールを探して `static_cast` するので、中身の違う同名クラスは置けない。
   - SMILE-3 にあった古い `HA_Calc`、`HA_CenterGravity`(前の版の SMILE-2+ 用のコピー。どの SMILE-3 の設定ファイルも使っていなかった)は消した。SMILE-3 の `Reconstruct` は `PSA_Event` だけを使う。
   - `SaveFile` は 1 つ(EricaNext_try の版。`compression` のパラメータが増えた)。`run_chain.py` に `--raw`、`--compression` を足した。
   - 設定ファイル `smile2p_20190701_Per02/Per03`、`smile2p_sim_d10_recon`、`tpc_caldata_2017_Ba133` は EricaNext_try のものの名前を直しただけ。置き換え文字 `{data_top}`、`{tpc_caldata}`、`{sim_data}` は `$SMILE2P_DATA`(無ければ NAS)。
   - 確かめたこと: Per02 の 3000 イベントの `tot_skewness`、`single_hit`、`--raw` の 2000 イベントの `raw`、`run_info` が EricaNext_try と全部一致。SMILE-3 の `smile3_reconstruct`(run04 の例)はツリー 2 つ、ヒストグラム 213 個が前と一致。
   - まだ: EricaNext_try のチュートリアル(`tutorials_caldata`、`tutorials_simulation`)とシミュレーションの設定(`config/simulation/`、`simulation_source/`)は持ってきていない。EricaNext_try の側を直したら、`_2p` のほうにも同じ直しが要る(二重の管理になる)。
7. `psa_param.csv` には IP アドレスと基板のシリアル番号が入っている。リポジトリに入れたままでよいか、人に確かめる(今は入れて push 済み)。

## 7. 経緯(コミット)

| コミット | 中身 |
| --- | --- |
| `0ca45c8` | PSA_ReadData〜PSA_TreeWriter、`smile3_psa.json`、`calib/psa/`(psa_param、position) |
| `c85494a` | PSA_Event、Reconstruct が PSA を使う、`smile3_reconstruct.json`、`geometry_template.csv`、`scripts/check/compare_psa_python.py`、この AGENTS.md |
| `daede13` | `smile3_reconstruct` で PSA_* を CalcFADC より前に(CalcFADC の AS_SKIP で PSA が止まらないように) |
| `c132646` | 温度ゲインの表と hg_corr、`scripts/calib/`(位置・温度ゲイン・エネルギー)、PSA のモジュールのチュートリアル |
| `7818fdd` | TPC の CalcFADC がイベントを飛ばさない(SMILE_tool_box と同じ。前はアノード 2 ch 以上の 29% を捨てていた)、TPC_Ee_Valid、NoCalData のエネルギーを和に |
| `fa7e472` | TPC_Calibration の FADC の ch を strip / 32 に(ボードが欠けると範囲外で落ちていた) |
| `c9a8343` | TPC のモジュールのチュートリアル(1_ReadData、7_CalcFADC、8_TPC_Calibration、9_TOT_Skewness、11_Reconstruct、12_TPC_RawTreeWriter) |
| `91afdbd` | 位置の格子の方法、PSA_ReadData がボードのファイルを複数のディレクトリから集める(`recursive`)、UVSOR のデータから作った位置・温度ゲイン・エネルギーの較正ファイル、`4_MakeCalibration/PSA_*` |
| `e1c2070` | EricaNext_try の SMILE-2+ のモジュールを `*_2p` で取り込み、`smile2p_*.json`、古い HA_Calc / HA_CenterGravity を削除、`run_chain.py --raw --compression` |
| `5343695` | 設定ファイルの名前(`smile3_tpc_only` = TPC だけ TOT_Skewness まで、`smile3_reconstruct`)、PSA_Calibration の `temp_coef`・`temp_ref` を削除、`calib/tpc/`、test2/test3/run_all を削除 |
| `4a55616`, `7d7ab22` | TOT_Skewness に SMILE-3 の GEM の位置とドリフト速度のファイル(無ければ SMILE-2+ の典型値で警告)、`scripts/calib/make_tpc_drift_velocity.py` |
| `78be476`, `b787120` | CAD の STEP ファイル(`calib/cad/SM3_Inner_3atm.step`、今は `calib/detector_coordinates/`)から `calib/psa/geometry.csv` を作る(`scripts/calib/make_psa_geometry.py`) |
| `42bad19` | `ReadData` の `extra_data_dirs`、`run_chain.py --extra-data-dir`(2 つの storage に分かれたランを読む) |
| `eb99de1` | PSA のエネルギーの較正を Cs-137 662 keV と Co-60 1173/1332 keV の 3 点でピクセルごとに |
| `f946f24` | `Reconstruct` に幾何の α と入射方向(TOT_Skewness の飛跡から)、`smile3_reconstruct` に TOT_Skewness |
| `a1cd979` | PSA_Event のチュートリアルを今の `geometry.csv` とエネルギーの較正で流し直した |

FADC波形の後段積算は `scripts/analysis/make_tpc_fadc_hist.py` と `tutorials/2_Diagnostics/TPC_FADC_Accumulation.ipynb`。全96chをTH2Dとして1つのROOTに保存し、波形数・入力条件はmetadata_jsonに記録。`-n` は複数入力を通じたイベント数上限、-1は全件。rawのカソードは反転済みなので再反転しない。

TPC 1-hitの定義（2026-11-14更新）: `make_tpc_spec1hit.py` は全μPICを通じてアノード1 ASIC・カソード1 ASICだけに電荷があり、同じμPICの組を選ぶ。ボード1-hitも全体で各側1ボードかつ同じμPIC。2026-11-15から、隣り合う2 ASIC（FADC ch 2kと2k+1）の和の1-hit（`Anode_1Hit_2ch_A<i>_C<j>`、i, j = 0〜23）も同じ条件で作る（metadataの `pair2_selection`）。これが無い古いspec1hit.rootは、ゲインマップのノートブックが作り直す。同日から μPIC 単位(そのμPICだけに電荷、12 chの和、`Anode_1Hit_upic_A<p>_C<p>`、metadata の `upic_selection`)も作る。ゲインマップのノートブックは、各ch組に ch組 → 2 ch組 → ボード → μPIC の順で最初に OK だった段の係数を入れる(`gain_map.json` の `calibration_level`)。OK の既定(`OK_CRITERIA = 'converged'`)は、フィットの収束・中心が範囲の端に張り付かない・ピークの高さ ≥ 2 × 誤差。旧「カソードは同じμPIC内だけ数える」条件のスペクトルとゲインマップは比較用として残し、再解析結果は別ファイルに保存する。新ROOTは `selection_metadata_json` に条件を記録。

2026-11-15: SMILE-3 TPCのraw writerを `RawTreeWriter_sm3` から `TPC_RawTreeWriter` に改名。クラス・設定キー・ROOTディレクトリを新名に統一し、ブランチ内容は変更しない。旧名の設定は新名に書き換える。後段解析とチュートリアルは `scripts/analysis/tpc_raw_io.py` で新旧ROOTを判別し、既存ROOTの再変換は不要。新旧両ツリーを含む曖昧な入力はエラー。

2026-11-15: TPC_GainMap.ipynbの2点較正でピーク中心の標準誤差をKa/Kbへ一次伝播し、標準誤差・共分散・相関係数をgain_map.jsonのcalibration_uncertaintyに保存。2ピーク独立・線エネルギー固定の統計誤差であり、系統誤差を含まない。datは4列のまま。

2026-11-15: TPC較正・診断の標準手順を、生データ→TPC_RawTreeWriterのROOT→後段解析の二段階としてREADMEとtutorialsに明記。再構成のROOT入力・PSA生波形保存については後述の実装記録を参照。mkdir.shは旧/usr/local向けビルドを廃止し、scripts/env/build_ericanext.shへの互換入口に変更（通常は直接新スクリプトを使う）。

2026-11-15: 再構成の二段階化を実装。ReadData.root_fileはTPC_RawTreeWriter（旧RawTreeWriter_sm3も可）の全ヒット・全波形を復元し、PSA_ReadData.root_fileは新PSA_RawTreeWriterの生ADC・温度・時刻・present状態を復元する。run_chain.py --root-inputで両readerをROOT入力へ切り替える。第1段階ETCC用configはsmile3_etcc_raw、後段smile3_reconstruct。1ファイル/run。TPC全ヒット・save_threshold=0の全波形が必須、TPCとPSAのID・sevt対応を確認。従来の生データ直接処理は比較検証用として維持。

検証: scripts/check/check_raw_replay.py（20261102_run04先頭200イベント）で直接処理とROOT経由の218オブジェクトが全値一致、TPC/PSA raw4ツリーも往復一致。PSA単独100イベントも18オブジェクト一致。ROOT入力の失敗はAS_CRITICAL_ERROR_TO_FINALIZEで返す（ANLNextは通常のAS_ERRORを非致命的として処理継続するため）。専用手順tutorials/1_DataIO/RawROOT_Pipeline.ipynb、既存モジュール12冊とPSA較正3冊を二段階へ更新。

2026-11-15: SMILE-3の保存モジュールをRawTreeWriterに統合。チェーン内のReadData/PSA_ReadDataを検出してTPCのみ・PSAのみ・両方を保存する。旧TPC_RawTreeWriter/PSA_RawTreeWriterクラスは廃止し、configのchain・TPC用パラメータはRawTreeWriterへ移動。ROOTの保存パスTPC_RawTreeWriter/*、PSA_RawTreeWriter/*は互換性のため維持。両readerがある場合はPSA sync_tpcとイベントID・連番の一致を検証。SMILE-2+のRawTreeWriter_2pは変更しない。

2026-11-15: 不要な互換入口mkdir.shを削除。ビルドはbash scripts/env/build_ericanext.shに統一。

2026-11-15: TPC専用raw保存の設定名をsmile3_rawからsmile3_tpc_rawに変更。PSA専用はsmile3_psa_raw、TPC＋PSAはsmile3_etcc_raw。旧設定名は使用しない。

2026-11-15レビュー反映: SMILE-3設定をconfig/stage1（raw保存）・stage2（ROOT解析）へ移動。ec.config_pathで短名・明示パスを解決。RawTreeWriterのsave_hits/save_fadc/save_threshold/base_window設定を廃止、全ヒット・全チャンネル波形を保存。TPC run_info schema_version=2。旧save_thresholdメタデータのROOTは読み込み互換性を維持。READMEは段階別表・横向き図・役割名のパスに統一。

2026-11-15: SMILE-2+用JSON設定4件をconfig/smile2p/へ集約。短い設定名での実行はec.config_pathの再帰探索により継続可能。

2026-11-15: SMILE-2+用C++モジュール8件をsource/modules/smile2p/配下へ移動。modules/sources.cmakeの登録パスを更新。クラス名・Python公開名・設定名・処理内容は変更しない。

2026-11-16: ReadDataは0バイトの `tpc_enc_*_00000.raw` を警告してボードごと読み飛ばす（DAQはラン開始時に有効な全ボードのファイルを作り、データの来なかったボードは空のまま残る。例: 20261113_run13 の a00〜a02・c00〜c02）。以前は空ファイルで `mod_begin_run` が AS_ERROR を返したあと解析ループに入り、`TPC_Encoder::ReadData` が `raw.eof()` しか見ないため `GetEventID` が無限ループしていた（CPU 100%、`.q` 不可）。`ReadData` は読み込み失敗全般（`!raw`）を次ファイルへの切り替え・終了として扱い、`Init` はヘッダの無いボードのファイル名を表示、`mod_begin_run` の失敗は AS_CRITICAL_ERROR_TO_FINALIZE で返す。検証は `TPC_Encoder` 単体（スタブでビルド）のみ: 2ファイルにまたがる合成データで旧版と出力一致、空ファイル・ファイル無しで停止。ANLNext込みのビルドと実データでの確認は未実施。

2026-11-16: TPC_EnergyCalibration.ipynbを追加。smile3_tpc_energyで1-hit選別せずTPC_Calibrationのイベント全体エネルギーを集積。未較正Ka=Kb=0の寄与は0、イベントは除外しない。3ピーク二次補正datをenergy_correction_fileで適用可能。範囲外は接線延長・負値は0。係数共分散はJSONのみ。基準線確認・fit品質OKまで実データの補正ファイルは出力しない。

2026-11-17: `TPC_Calibration` の同時計数を、`Track_A/C`(1536 × 1024 の TH2C)の外接矩形の総当たり(`GetBinContent`)から、`ReadData` のヒットの一覧(strip, clock)をクロックごとに組にする形に変えた。チェーンの CPU 時間の大半がここで、run15 の `smile3_tpc_energy` で 77 秒 → 20 秒。TPC_Calibration は `Track_A/C` を要求しなくなったので、このチェーンではイベントごとの TH2C の Reset・Fill も無い(TOT_Skewness など、要るモジュールは自分で要求する)。結果は前と同じで、元の走査の細かい動きも再現している: マスの数は 127 で止まる、判定 `hit_a & hit_c & (div_a == div_c)` はビットごとなので「両方の数が奇数」の意味、クロック ≥ 1024 は全部オーバーフローの行に入り、[max(min_d, 1024), max_d] のクロックの数だけ数える。run15 で、同じマスのヒットは 0、クロック ≥ 1024 は 9370 万ヒット中 4。確認: run15 の全 91955 イベントで Spectrum、Coinci_A/C、Image_XY(組 8.3 億)が全ビン一致。

2026-11-17: `ReadData`・`PSA_ReadData` に `root_first_entry`(ROOT 入力で読み始める行)、`run_chain.py` に `--first-entry`、1 つのランを行で分けて同時に流す `scripts/calib/parallel_chain.py`(`run_parallel`。出力は `<名前>.partNNN.root`、条件を `.inputs.json` に残し、違う条件の出力があれば止まる)。`PSA_Energy.ipynb` のチェーンはこれで流す(`CHUNK_EVENTS`、`MAX_JOBS`)。確認: PSA run04 の先頭 60 万イベントを 3 つに分けた出力をつなぐと全 28 列・143 万ヒットが一致、TPC run15 を 3 つに分けたスペクトルと Image_XY の和が全ビン一致。PSA の速さは 1 プロセス約 1 万イベント/秒(波形の読み込みと ZSTD の展開が大半で、TPC のような無駄は無い)。TPC と PSA を一緒に読むチェーン(`smile3_reconstruct`)を分けて流すのはまだ試していない。

2026-11-17: NAS に書いた `HA_raw_20261115_run06.root`(124 GB)に、0 で埋まった 1.3〜2.3 kB の穴が 3 か所あった(どれも 4 kB のページの境目で終わる。書き込みのときにページが抜けたらしい。dmesg に CIFS の `Close unmatched open`)。ROOT はそこの basket を読めず(`fNevBuf=0 ... trying to repair`、GetEntry が -1)、PSA_Energy の分割 3 つが止まった。1 つの穴で、その basket の行(adc で 444 行、time で 1818 行)が全部読めない。
- `ReadData`・`PSA_ReadData` の ROOT 入力は、読めない行を警告して AS_SKIP で飛ばす(`root_skip_unreadable`、既定 true。最初の 20 行は行番号、終わりに数)。TPC と PSA を一緒に読むときは、PSA_ReadData が ReadData の行(`Get_RootRow`)に合わせるので、TPC 側で飛ばしてもずれない(この組み合わせで穴に当たる場合はまだ試していない)。run06 の 6315600 から 100 行で、68 行を飛ばして完走することを確かめた。
- `scripts/check/check_root_holes.py`: ファイル全体を読んで 256 バイト以上の 0 の区間を探し、重なる basket(全ツリー・全ブランチ、ツリーは一番新しい cycle だけ)から読めない行を出す。終了コード 0 = 穴なし、1 = あり。NAS からだと 100 GB で 45 分ほど。
- 本当は raw を作り直すべき。NAS に直接書かず、ローカルに書いてからコピーし、このスクリプトで確かめるのが安全。

2026-11-17: スライド用の飛跡の図 `scripts/analysis/plot_tpc_long_tracks.py`(raw ROOT から。ホットなストリップとイベント中で鳴りっぱなしのストリップを除き、1 つの μPIC に 95 % 以上のヒットがあるイベントで、同じクロックのアノード × カソードの組から 3 次元の点と長さ。`--order long/short/random`、`--min-length`/`--max-length`)。座標は μPIC ごとのローカルで、検出器の座標系ではない。20261115_run03 の先頭 5 万イベントでは upic0 は全ストリップ無音、upic2 はアノード 132 本・カソード 103 本、upic3 はアノード 295 本が 0.1 % 未満しか鳴らない。1 つの μPIC という条件は、無音の upic0 へ抜けた飛跡も通すので、長い飛跡には μPIC の端で切れたものが混じる。`scripts/analysis/compare_drift_edge.py` は SMILE-2+ と SMILE-3 のドリフトケージの壁の近くの飛跡の分布の比較。

2026-11-17: `make_psa_energy.py` が全ヒットを numpy に読み込んでいたため、20261115 の Cs run04〜06 + Co run07(3 億ヒット以上)で WSL のメモリ(16 GB)を使い切り、WSL ごと落ちた。スペクトルを RDataFrame の中で (ボード, ch, ピクセル) ごとの TH2D に詰める形に変えた(ファイルを 2 回読む: peak_id の範囲 → スペクトル)。メモリは入力の量によらず約 0.8 GB。旧版と、Cs・Co 1 ファイルずつの ha02・ha03 で、温度ゲインの表あり・なしとも CSV が全行一致。`make_psa_temp_gain.py` も同じ形にした(peak_id の範囲 → ボードごとの温度の最小・最大 → (ボード, ch, 温度の区間) ごとのスペクトルの 3 回読む。区間の端は numpy の版と同じ値で比べる)。旧版と、ha02・ha03(既定)と ha05(`--temp-width 0.3 --peak-id-window 2 --min-hits 500`)で CSV と QA の図が一致。共通部分は `psa_calib_common.py`(`rdataframe`、`board_peak_ranges` など)。落ちたとき、TPC_EnergyCalibration のチェーン(run17)も同時に流れていた。

2026-11-17: `TPC_Calibration` は、電荷 Q = 0 のアノード ch を足さない(Q = 0 で `Ka Q + Kb` を足すと切片 Kb が残り、ゲインマップを使うと、同時計数はあるがアノードの電荷が無いイベントが Energy = Kb ≠ 0 で `TPC_Ee_Valid` になっていた。`NoCalData: true` では Ka = Kb = 0 なので変わらない)。20261116_run03 の先頭 5000 イベント(`smile3_reconstruct`、20261113 のゲインマップ)で single_hit が 2497 → 1099。落ちた 1398 のうち 1372 は Ke < 0(大半がアノード ch 23 × カソード 14/15 の Kb = −1.756 keV ちょうど)。残ったイベントの 218 は、電荷の無い ch の Kb が抜けた分だけ Ke が変わった(中央値 −0.6 keV)。さらに、Q > 0 でも小さく Ka Q + Kb < 0(Kb < 0 の組で Q < −Kb / Ka)になる ch 組は足さない(負の寄与は 0。全体のエネルギーは負にならず、そういう組だけのイベントは Energy = 0 で `TPC_Ee_Valid` にならない。判定の「≠ 0」は SMILE_tool_box のまま)。同じ 5000 イベントで single_hit は 1099 → 1073(Ke < 0 だった 31 のうち 26 が落ち、残り 5 は負の組を除くと 0.5〜1.1 keV)。81 イベントは負の寄与が抜けた分だけ Ke が増えた(中央値 0.65 keV、最大 14 keV)。Ke < 0 で `cos_alpha_kin` などが NaN になるイベントも無くなった。

2026-11-17: ETCC の事象表示 `scripts/analysis/plot_etcc_event.py`(1 イベントの TPC の飛跡の点、TOT_Skewness の Pe・Ve、HA のヒット点 Pg、Pe → Pg、TPC のドリフト領域と geometry.csv の HA のブロック)。入力は `TOT_Skewness.flag_save_cloud=true` で流した `smile3_reconstruct` の出力。tot_skewness と single_hit は Pe で対応させる(Reconstruct は Ve を単位ベクトルにして書くので、Ve では対応しない)。20261115_run03 の 2 万イベントで見えたこと: μ-PIC に平行な部分(同じクロック)で偽の点が四角く大量に出る、Pe が飛跡から離れた 1 点になることがある(TOT_Skewness は飛跡の端の点を Pe にする)、Ve_z = 0(Pe の周り 10 mm で傾きの fit ができず XY 面の向きだけ)が track ありの 8 %。

※以下の当初の2/3倍・180 mm・Ke=5–400 keVの条件と件数は旧条件の記録。両測定が2気圧であるとの訂正を受け、現行条件は末尾の訂正記録を参照。

2026-11-17: ETCC のカット比較を `scripts/analysis/compare_etcc_cuts.py` に追加。再構成 ROOT の single_hit と tot_skewness をファイルごとの evt_id で結び、Pe も検証。Eg の追加カットなしで、追加カットなし・fiducial のみ・dE/dx のみ・両方・両方＋|Δcosα| < 0.5 のスペクトルと方向図・ARM を比較する。スペクトルは E0 窓なし、方向図は既定630–700 keV。fiducial は全飛跡の外接範囲が壁から5 mm内側、dE/dx は SMILE-2+ の帯を気圧比2/3倍した仮定（Ke=5–400 keV、Len<180 mm併用）。CLI の説明は README。入力と残存数は selection_summary.json。11/16 run03+06 の E0 窓の基準23,114件・上半球20,026件は Claude の図と一致。24×24ビン版も出力した。

11/15 run03 の既存再構成 ROOT は evt_id・track_min/max が tot_skewness にないため、同じ raw ROOT から比較用に再構成。作業用出力は /home/onoda/ericanext_out/etcc_cut_comparison/、ROOT は /home/onoda/ericanext_out/20261115_selected/。再構成を速める一時 SingleHitGate（PSA_SingleHit && TPC_Ee_Valid のときだけ TOT_Skewness 以降を実行）は出力先の replay_helper/ に置いた。一時ゲートはリポジトリのチェーンには組み込んでいない。先頭5000イベントで通常処理と single_hit 全ブランチ、および選ばれた tot_skewness 全ブランチが一致（557件）。この作業用 ROOT の tot_skewness は候補事象に限定される。


2026-11-17 Claude 引き継ぎの補足: 未コミットだった TOT_Skewness の evt_id・track_min/max の保存、plot_etcc_event.py の --min-len/--max-len [mm]・--top・--order points|length を取り込み。前者は保存列の追加のみで、飛跡計算は変えない。実データの3511行で、外接範囲の対角線が Len と一致し、evt_id が一意であることを検証。後者は run03 の点群つき ROOT で --order length --top 1 による図の出力を確認。

ETCC の現時点の較正・判断の注意点（Claude の2026-11-17引き継ぎ）: TPC の較正済みは576組中230組（μPIC0:0、1:144、2:51、3:35）、未較正の寄与は0。全体の二次補正 E′ ≈ 0.68 + 0.888 E + 0.00303 E² は、TPC_Calibration の負の寄与を直す前のスペクトルから決めた。ドリフト速度は run03 の約0.387 mm/clock・offset約133.7の暫定値（上端が鈍く品質判定NG）。PSA は11/15・228 Vの較正を使用し、温度補正なし。Co run07 は Cs run04〜06 より1〜2 °C低く、この差も較正に入っている。TPC/PSAの座標系の同一性も未確認。

線源位置は11/15 run03が (0,0,-152) mm、11/16 run03+06が (-380,-1090,-1412) mm。ARMの中央値の正の偏りは背景だけでも起こる。信号の判断は真の線源−回転した偽線源、または測定時間などで規格化した線源あり−なしで行う。Claude の報告では11/16の Ke>80 keVで |ARM|<10° の真−偽の超過が136±45。今後は線源なしランの背景、ETCC時のHA温度、未較正TPC領域の確認が必要。引き継いだ解析・スライドのスクリプトは /home/onoda/ericanext_out/handover_20261117/ に保持。最新 etcc_image.py の Lambert 正積図も compare_etcc_cuts.py の --projection lambert として選択可能にし、従来の天頂角半径図は --projection zenith（既定）で維持。


2026-11-17: `tutorials/3_Reconstruction/ETCC_CutComparison.ipynb` を追加し、11/15 run03全件（104分割）と11/16 run03+06（12分割）で実行・図を埋め込み。天頂角半径図と Lambert 正積図を併記し、投影法を変えても全5カットの選択件数・上半球件数が不変であることを確認。11/15の基準はE0窓247040件・上半球226773件で既存ROOTと一致し、fiducial+dE/dx+|Δcosα|<0.5では28752件・上半球27446件（ARM中央値28.2°）。11/16の最終段は2742件・上半球2613件（中央値19.9°）。通常のビルドも成功。

TPCのTOT座標の面番号（strip/384）は0:(+X,-Y)、1:(+X,+Y)、2:(-X,+Y)、3:(-X,-Y)。11/15 PSA較正は /mnt/nas_eureka/onoda/analysis2/SMILE3-FM_analysis/20261115/HA_cal/calib_psa に保持。run07のgrid法で12ボード72ch、格子内94.5〜97.6%。Cs run04〜06とCo run07で68chが3点、4ch（ha01 ch3/5、ha09 ch1、ha13 ch0）はCs単点。4608ピクセルのうち4230が3点、378がcs+ch。UVSOR11/03の旧較正はcalib/psaに保持。


2026-11-17: compare_etcc_cuts.py と ETCC_CutComparison.ipynb に dE/dx 確認図を追加。Ke–Len を4 keV×3 mm/bin、空ビンをマスクした LogNorm(vmin=1) の共通スケールで表示。追加カットなし・fiducial・fiducial+dE/dx を、E0窓なし／方向図と同じ窓の2段で比較し、元のSMILE-2+帯・使用する帯・Ke/Lenの境界を重ねる。表示範囲外のイベントも分かるよう、全選択数と表示数を記録。
両ラン全件で実行し、6パネルの全選択数が既存のスペクトル／E0窓の件数と一致することを確認。ノートブックに両ランの図を埋め込み済み。


2026-11-17 訂正: ユーザー確認により SMILE-3 も SMILE-2+ と同じ2気圧。3気圧を仮定した帯の2/3倍と180 mm上限は誤りだった。compare_etcc_cuts.py の --len-scale とチュートリアルの LENGTH_SCALE の既定を1.0にし、帯と飛程上限270 mmをSMILE-2+と同じに戻した。Ke=5–400 keVの選別はユーザー指定で削除し、基準のKe>0のみ維持。高エネルギー側も確認できるようKeの表示範囲は0–600 keVに変更（表示範囲は選別ではない）。旧条件の件数・図と混同しないこと。
訂正後の両ラン・両投影法で ETCC_CutComparison 全体を再実行し、スペクトル・件数・方向図・ARM・dE/dx図を更新。最終段のE0窓の件数は11/16が3820、11/15が41595。追加カットなしのE0窓の件数（23114／247040）は不変で、変更が選別段だけであることを確認。Ke=4/450 keVが通り、Ke=500 keV・Len=269 mmが通って270 mmが落ちるテストも通過。


2026-11-17: ETCC_CutComparison の冒頭を入力ファイル→tree→使用ブランチと用途→イベント対応の順に整理。現在の2気圧・倍率1.0・270 mm・Ke範囲なしを明示し、各カットを独立した短いブロックに分けた。表示が崩れていたMarkdownの表を廃止し、パラメータは箇条書き、残存数の表はHTML表示へ変更。実ファイル一覧と長い実行ログは折りたたみ。再解析は行わず、同じ12/104入力であること、各測定の先頭ROOTで記載ブランチが存在すること、表示セルの実行を検証。


2026-11-17: ETCC_CutComparison.ipynb は埋め込みPNGで6.4 MBになりGitHub表示が重くなったため、10枚の実行例を tutorials/3_Reconstruction/figures/ のPNGに分離し、Markdownの相対参照へ変更。ノートブック本体は画像出力を除いて150 kB未満に制限。計算・入力・選別条件は維持。高解像度の実行結果と元の実行済みノートブックは /home/onoda/ericanext_out/tutorials/ETCC_CutComparison/ に保持。今後コミットするときもPNGのbase64をノートブックに大量埋め込みしないこと。

2026-11-17: ミューオンらしい飛跡の図 `scripts/analysis/plot_muon_tracks.py` とチュートリアル `tutorials/2_Diagnostics/ETCC_MuonTracks.ipynb`（実行例の図は `tutorials/2_Diagnostics/figures/`）。入力は `TOT_Skewness.flag_save_cloud=true` で流した `smile3_reconstruct` の出力。選び方: 重複を除いた点 ≥ 50、PCA の直線方向の長さ（点の1〜99 %）≥ 200 mm、直線からの RMS ≤ 5 mm（z の範囲・HA のヒットは条件にしない）。HA のヒットは `PSA_TreeWriter/psa` を trig = evt_id で対応させる。ミューオンの HA のヒットは HG が飽和（`satu_h`）していることが多く、エネルギーは較正範囲外の外挿で下限の目安にすぎない。20261115 run03 の2万イベントで、飛跡の z はドリフト領域（10〜317 mm）から約 ±10 mm はみ出す（ドリフト速度の誤差）。


2026-11-18: `calib/` を `config/stage2/smile3_reconstruct.json` が NAS から読んでいた較正ファイルに置き換え、設定ファイルを `{repo}/calib/...` に戻した。`calib/tpc/` に `gain_map.dat`(+ `.json`)、`tpc_energy_correction.dat`(+ `.json`、どちらも 20261113 run13〜17)、`drift_velocity.dat`(20261115 run03、天板の端がなだらかで確認には通っていない)。`calib/psa/` の位置・エネルギーは 20261115 の HA_cal の 12 ボードに置き換え、温度ゲインの表と ha00・ha16 の表は消した(git の履歴にある)。`smile3_psa.json` の `temp_gain_dir` も空に。`smile3_tpc_only.json` もゲインマップ・エネルギーの補正・ドリフト速度に `calib/tpc` を使う(エネルギーの補正は前は空だった)。補正を作る `smile3_tpc_energy.json` は補正なしのまま。11-17 夜の ETCC のチュートリアル(ETCC_CutComparison、ETCC_MuonTracks)の再構成の出力は、全パートのログで補正を読んでいる。etcc_raw_20261115_run03 の 2 万イベントで、全ファイルを読んで完走することを確かめた。