# AGENTS.md — EricaNext(SMILE-3)の引き継ぎ

このリポジトリで作業する人と AI エージェント(Codex、Claude Code など)向けの引き継ぎ書です。
使い方の説明は [`README.md`](README.md)、設定ファイルの書き方は [`config/README.md`](config/README.md)、スクリプトは [`run_script/README.md`](run_script/README.md) にあります。
ここには、**それらに書いていない作業の約束・環境の落とし穴・これまでの経緯・残りの仕事**をまとめます。

最終更新: 2026-11-07(ブランチ `feature-restructure`)

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
| `position_haNN.csv` | `make_psa_position.py --method grid`(既定。x の射影で 8 列、列ごとに y の射影の 2 階微分で 8 行。`--method localmax` は `peak_search.py` と同じで、同じヒストグラムで 64 点が一致することを確かめた) | run12(Co、トリガー時刻 peak_id 26〜29、hg ≥ 3 万、飽和なし)から作った 13 ボード(ha04 以外)。補正後に 8 × 8 に入る割合は 92〜99%。ha01 の ch4 はバタフライマップがにじんでいて 83.6% なので書いていない(`--min-frac 0.9`)。ha04 は run12 に無いので、前の仮のもの(peak_search の結果)のまま |
| `temp_gain_haNN.csv` | `make_psa_temp_gain.py`(光電ピークの探し方とトリガー時刻の範囲は `temp_spectra.py` と同じ) | Cs の run01〜11 から作った 13 ボード(ha04 は Cs のランにほぼ無いので無い)。1 °C ごと。基準の温度(gain = 1)はボードごとにヒットの一番多い区間 |
| `energy_haNN.csv` | `make_psa_energy.py`(`--cs` と `--co`。fit は `analysis_sm3_FM` の `energy_calib.py` と同じで、ピクセルごと) | 位置と温度ゲインを置いた状態で流し直した Cs の run01〜11 と Co の run12(`tutorials/cache/psa_cal_*.root`)から、Cs 662 keV と Co 1173 / 1332 keV の 3 点の直線(`p0` は 0 に固定しない)で作った 13 ボード(2026-11-07)。温度が温度ゲインの表の範囲の中のヒットだけを使う(Co は 47〜50 °C で表の端を越えるボードがある)。Co の 2 本は分かれず 1 つの山になるので、1332 keV の中心を 1173 keV の 1.06〜1.16 倍に制限して fit する。ch の和の Co が確認に通らない 11 ch(ha00 ch3、ha01 ch0/1/3/5、ha05 ch0、ha06 ch5、ha09 ch4、ha15 ch0、ha16 ch1/5)は Cs だけ(`p0 = 0`)。Co が使えないピクセルは、ch の和の `p0` とピクセルの Cs の位置から `p1`(`src` 列)。ha04 は無い。ha01 の ch4 はピクセルが無いので無い |
| `geometry.csv` | `make_psa_geometry.py`(CAD の STEP `calib/cad/SM3_Inner_3atm.step`。HA の番号・ch・ピクセルの向きはスクリプトの `HA_LAYOUT`、HA のモジュールの配置図から) | 18 HA × 6 ch = 108 ブロック(2026-11-06)。ピクセルの向き(u, v)は配線でも変わるので、データでの確認がまだ |

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
5. **チュートリアル**: モジュールの解説(`tutorials/1_DataIO/modules/1〜12`)と `4_MakeCalibration/PSA_*` はある。まだ無いもの: 1_DataIO の全体の流れの本(EricaNext_try の `DataIO_example`)、TPC のゲインマップとドリフト速度の較正の本(SMILE-3 のデータでは未作成)。生成スクリプトはリポジトリに無い(ノートブックを直接直す)。
6b. **TPC 側で分かっていて、まだ直していないこと**
   - `TPC_Calibration` には、SMILE_tool_box の 2 つの条件(同時計数が 100 を超える組 (i, j) だけを使う、各チャンネルの 32 ストリップの両端 4 本ずつを数えない)が無い。**今は入れない**と決めた(2026-11-06)。SMILE-3 のゲインマップを作るときに、要るかどうかをもう一度見る。
   - `Reconstruct` の幾何の α・入射方向(2026-11-08): SMILE_tool_box の `Erica/Reconstruct`(TRA、SingleHit)と SMLib `Compton_common`(`recon_vector`、`ARM`、`SPD`)と同じ式で入れた(EricaNext_try の `Reconstruct_2p` には無い)。
     `smile3_reconstruct` に `TOT_Skewness` を足した(AS_SKIP するので `Reconstruct` の直前)。`TOT_Skewness` の `Ve` は長さに意味の無い方向なので、`Reconstruct` で単位ベクトルにする。
     **残り**: TPC の位置(`TOT_Skewness` の `Get_TPC_XYZ`、SMILE-2+ のパネルの並びのまま μ-PIC の中心が原点)と、PSA の `geometry.csv`(X: 東、Y: 南、Z: 下)が同じ座標系かを確かめる。
     TPC のゲインマップが無いので `Ke` は ADU で、角度の値にはまだ意味が無い。
   - 2026-06-23 の TPC_cal には、鳴りっぱなしのストリップを拾って `Len` が数百 mm になるイベントが多い。
   - TOT 補正の係数、skewness のオフセットは SMILE-2+ の値のまま。
   - ドリフト速度: `clock_offset` = 85、`pitch_z_mm_per_clock` = 0.383 は SMILE-2+ の典型値(Per02/Per03)。SMILE-3 の測定はまだで、`drift_velocity_file` が空だと TOT_Skewness が警告を出す。GEM 面は `z_gem_mm` = 10 mm(SMILE-3)、ドリフト長(μ-PIC〜天板)317 mm。6/23 の TPC_cal はクロックの分布が箱の形にならず、ドリフト速度は出せない。ファイルを作るスクリプト `scripts/calib/make_tpc_drift_velocity.py` はある(SMILE-2+ の Per02 で EricaNext_try と同じ値を確認)。測定ができたら流して `calib/tpc/drift_velocity.dat` を置き、`smile3_tpc_only.json` の `drift_velocity_file` に書く。
   ノートブックの生成と実行: `jupyter nbconvert --to notebook --execute --inplace --ExecutePreprocessor.kernel_name=ericanext <nb>`(`/usr/bin/python3 -m jupyter`)。
7. **SMILE-2+ のデータも同じ EricaNext で扱う**: 2026-11-06 に取り込んだ(README の「SMILE-2+ のデータ」)。
   - EricaNext_try の `source/modules/` の 8 つ(ReadData、CalcFADC、TPC_Calibration、TOT_Skewness、HA_CenterGravity、HA_Calc、Reconstruct、RawTreeWriter)を `<名前>_2p/` にコピーし、クラス名・ファイル名・`get_module` の名前・補助クラス(`CP80256`、`TPC_Encoder`、`MDF`)・インクルードガードに `_2p` を付けた。印の名前も `TOT_Skewness_2p` など変わったものがある。ANLNext は名前でモジュールを探して `static_cast` するので、中身の違う同名クラスは置けない。
   - SMILE-3 にあった古い `HA_Calc`、`HA_CenterGravity`(前の版の SMILE-2+ 用のコピー。どの SMILE-3 の設定ファイルも使っていなかった)は消した。SMILE-3 の `Reconstruct` は `PSA_Event` だけを使う。
   - `SaveFile` は 1 つ(EricaNext_try の版。`compression` のパラメータが増えた)。`run_chain.py` に `--raw`、`--compression` を足した。
   - 設定ファイル `smile2p_20190701_Per02/Per03`、`smile2p_sim_d10_recon`、`tpc_caldata_2017_Ba133` は EricaNext_try のものの名前を直しただけ。置き換え文字 `{data_top}`、`{tpc_caldata}`、`{sim_data}` は `$SMILE2P_DATA`(無ければ NAS)。
   - 確かめたこと: Per02 の 3000 イベントの `tot_skewness`、`single_hit`、`--raw` の 2000 イベントの `raw`、`run_info` が EricaNext_try と全部一致。SMILE-3 の `smile3_reconstruct`(run04 の例)はツリー 2 つ、ヒストグラム 213 個が前と一致。
   - まだ: EricaNext_try のチュートリアル(`tutorials_caldata`、`tutorials_simulation`)とシミュレーションの設定(`config/simulation/`、`simulation_source/`)は持ってきていない。EricaNext_try の側を直したら、`_2p` のほうにも同じ直しが要る(二重の管理になる)。
6. `psa_param.csv` には IP アドレスと基板のシリアル番号が入っている。リポジトリに入れたままでよいか、人に確かめる(今は入れて push 済み)。

## 7. 経緯(コミット)

| コミット | 中身 |
| --- | --- |
| `0ca45c8` | PSA_ReadData〜PSA_TreeWriter、`smile3_psa.json`、`calib/psa/`(psa_param、position) |
| `c85494a` | PSA_Event、Reconstruct が PSA を使う、`smile3_reconstruct.json`、`geometry_template.csv`、`scripts/check/compare_psa_python.py`、この AGENTS.md |
| `daede13` | `smile3_reconstruct` で PSA_* を CalcFADC より前に(CalcFADC の AS_SKIP で PSA が止まらないように) |
| `c132646` | 温度ゲインの表と hg_corr、`scripts/calib/`(位置・温度ゲイン・エネルギー)、PSA のモジュールのチュートリアル |
| `7818fdd` | TPC の CalcFADC がイベントを飛ばさない(SMILE_tool_box と同じ。前はアノード 2 ch 以上の 29% を捨てていた)、TPC_Ee_Valid、NoCalData のエネルギーを和に |
| `fa7e472` | TPC_Calibration の FADC の ch を strip / 32 に(ボードが欠けると範囲外で落ちていた) |
| `c9a8343` | TPC のモジュールのチュートリアル(1_ReadData、7_CalcFADC、8_TPC_Calibration、9_TOT_Skewness、11_Reconstruct、12_RawTreeWriter_sm3) |
| `91afdbd` | 位置の格子の方法、PSA_ReadData がボードのファイルを複数のディレクトリから集める(`recursive`)、UVSOR のデータから作った位置・温度ゲイン・エネルギーの較正ファイル、`4_MakeCalibration/PSA_*` |
| `e1c2070` | EricaNext_try の SMILE-2+ のモジュールを `*_2p` で取り込み、`smile2p_*.json`、古い HA_Calc / HA_CenterGravity を削除、`run_chain.py --raw --compression` |
| `5343695` | 設定ファイルの名前(`smile3_tpc_only` = TPC だけ TOT_Skewness まで、`smile3_reconstruct`)、PSA_Calibration の `temp_coef`・`temp_ref` を削除、`calib/tpc/`、test2/test3/run_all を削除 |
