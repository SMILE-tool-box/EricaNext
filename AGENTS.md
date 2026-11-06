# AGENTS.md — EricaNext(SMILE-3)の引き継ぎ

このリポジトリで作業する人と AI エージェント(Codex、Claude Code など)向けの引き継ぎ書です。
使い方の説明は [`README.md`](README.md)、設定ファイルの書き方は [`config/README.md`](config/README.md)、スクリプトは [`run_script/README.md`](run_script/README.md) にあります。
ここには、**それらに書いていない作業の約束・環境の落とし穴・これまでの経緯・残りの仕事**をまとめます。

最終更新: 2026-11-05(ブランチ `feature-restructure`)

---

## 1. 何のリポジトリか

- SMILE-3(ETCC: ガス TPC + シンチレータ)の生データから、イベント再構成までを行う ANLNext のモジュール群です。C++ で書き、SWIG で Python(`EricaPy`)から使います。
- チェーンは `config/<名前>.json` に書き、`run_script/run_chain.py` が組み立てて実行します。モジュールの順番やパラメータをスクリプトに直接書いてはいけません。
- SMILE-2+ 版は [EricaNext_try](https://github.com/SMILE-tool-box/EricaNext_try)(`~/ANLNext/EricaNext_try`)です。構成はそれに合わせています。

| ディレクトリ | 中身 |
| --- | --- |
| `source/modules/<モジュール>/` | ANLNext のモジュール。足したら `source/modules/sources.cmake` と `source/pyext/EricaPy.i` の両方に書く |
| `config/` | チェーンの設定ファイル(`*.json`)と読み込み(`ericanext_config.py`) |
| `calib/psa/` | PSA(シンチレータ)の較正ファイル |
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
- **チェーンの中で `AS_SKIP` を返すと、そのイベントの後ろのモジュールは全部動かない**(ANLNext の仕様)。TPC の `CalcFADC` は 2026-11-06 まで「アノード 1 ch・カソードあり」以外のイベントを `FADC_NoHit` で `AS_SKIP` していた(6 月の TPC_cal で 62%、うち 29% はアノード 2 ch 以上の普通のイベント)。SMILE_tool_box(`~/SMILE_tool_box_v2.20/Erica/CalcFADC`)と EricaNext_try に合わせて、今は飛ばさずに印(`FADC_NoHit`、`FADC_1Hit`)を付けるだけ。`TPC_Calibration` は `TPC_Ee_Valid`(エネルギー ≠ 0)を立て、`Reconstruct` はそれを見る。SMILE_tool_box の TPC_Calibration にある「同時計数が 100 個を超えるパネルだけ足す」条件は、SMILE-3 版にはまだ無い。
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
- `ReadData`(TPC)は `data_dir` を 1 つしか読めない。TPC も読むときは、両方の storage のファイルを 1 つのディレクトリにシンボリックリンクで集める。
  例: `mkdir r; ln -s .../calltt/20261102_run04/*_0*.{dat,raw} .../killua/20261102_run04/*_0*.{dat,raw} r/`

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

`Reconstruct` は、チェーンに `PSA_Event` があればそれを使い、無ければ `HA_Calc`(SMILE-2+)を使います。
`single_hit` には `evt_id`(トリガーカウンタ)のブランチを足しました。

設定ファイル:

- `smile3_psa.json`: PSA だけ。
- `smile3_reconstruct_psa.json`: TPC + PSA → `Reconstruct`。

### 確かめたこと

| データ | 結果 |
| --- | --- |
| 20261029_run06(ha03)、窓はピークの周りと固定の両方 | psa_decode + pixel_id と 15432 ヒットすべて一致。hg などは相対 1e-6 以内(Python 側の float32 の丸め) |
| 20261102_run02 HA_cal(14 ボード、2 つの storage) | ha00、ha06 で一致。x, y の差は \|hg\| < 60 のノイズのようなヒットだけ(割り算の桁落ち)。ピクセルは全部一致 |
| 20261102_run03 / run04 ETCC、TPC と一緒に読む | ヒットのある PSA のフレームは、全ボードで取りこぼし無くツリーに入る |
| `smile3_reconstruct_psa` + ダミーの較正値(テスト専用、リポジトリには入れていない) | single_hit の 20 行すべてで、Eg、Pg が式どおり |

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
| `position_haNN.csv` | `make_psa_position.py --method grid`(既定。x の射影で 8 列、列ごとに y の射影の 2 階微分で 8 行。`--method localmax` は `peak_search.py` と同じで、同じヒストグラムで 64 点が一致することを確かめた) | run12(Co、トリガー時刻 peak_id 26〜29、hg ≥ 3 万、飽和なし)から作った 13 ボード(ha04 以外)。補正後に 8 × 8 に入る割合は 92〜99%。ha01 の ch4 はフラッドマップがにじんでいて 83.6% なので書いていない(`--min-frac 0.9`)。ha04 は run12 に無いので、前の仮のもの(peak_search の結果)のまま |
| `temp_gain_haNN.csv` | `make_psa_temp_gain.py`(光電ピークの探し方とトリガー時刻の範囲は `temp_spectra.py` と同じ) | Cs の run01〜11 から作った 13 ボード(ha04 は Cs のランにほぼ無いので無い)。1 °C ごと。基準の温度(gain = 1)はボードごとにヒットの一番多い区間 |
| `energy_haNN.csv` | `make_psa_energy.py`(`--line E` か `E:lo:hi`) | Cs の run01〜11 を位置と温度ゲインを置いた状態で流し直したもの(`tutorials/cache/psa_cal_*.root`)から、662 keV の光電ピーク 1 本で作った 13 ボード(`p0 = 0`。直線性はまだ確かめていない)。ha04 は Cs のデータが run01 だけで信用できないので置いていない。ha01 の ch4 はピクセルが無いので無い |
| `geometry.csv` | 手で書く(形式は `geometry_template.csv`) | **無い**。今は `PSA_Event` がヒットを使わない |

- 作る順番: 位置、温度ゲイン → (それを置いて線源のランを流す) → エネルギー。エネルギーは `hg_corr` とピクセルを使う。
- `<out>/qa/` に確認の図が出る(git には入れない)。使う前に必ず見る。
- `--method localmax`(peak_search.py と同じ)は、縦に長い中央のスポットから何個もピークを拾って端の列を押し出す(統計を増やしても直らない)。`grid` はそのために作った。中央の列の上下の行は山ではなく列の端の斜面の肩なので、極大ではなく 2 階微分で探している。
- ch ごとに図で確かめ、悪い ch はファイルから行を消す(その ch はピクセルを求めない)。
- 温度ゲインは、39.5〜41.5 °C ではほぼ一定で、42.5 °C で約 15% 下がる(ha02)。HV(設定値もフレームの DAC 値も 15853)と Vth は run01〜11 で同じ。温度依存性は過去にも報告されている。

## 6. 残りの仕事(上ほど優先)

1. **エネルギーの直線性**: 今のエネルギーの較正は Cs の 662 keV 1 本(`p0 = 0`)。Co(run12)の約 5 万の山が光電ピークかコンプトン端か(Notion で未確認)が分かれば、2 本目の線として `--line E:lo:hi` で使える。ha04 の較正(位置・温度ゲイン・エネルギー)は、ha04 のデータのある較正ランが要る。
2. **ジオメトリ**(`geometry.csv`)
   - 必要な値: ボードの位置(底面 HA0〜5、側面 N/W/S/E の HA6〜17)、モジュールの中の ch(TL〜BR)の位置、ピクセルの間隔、n, m の向き。
   - 値は人(検出器の担当)に聞く。`0_Introduction` の配置図は模式図なので、そのまま使わない。
3. **イベントの選び方の確認**
   - `PSA_Event` の「1 ヒット」は「ピクセルとエネルギー > `energy_min` があり、geometry にあるヒットが 1 つ」。ノイズのヒットで MultiHit になりすぎないかを、実データで見る。
4. **`ReadData` が複数のディレクトリを読めるようにする**(`PSA_ReadData.extra_data_dirs` と同じ形)。2 つの storage に分かれたランをシンボリックリンク無しで読めるようにする。
5. **チュートリアル**: モジュールの解説(`tutorials/1_DataIO/modules/1〜12`)と `4_MakeCalibration/PSA_*` はある。まだ無いもの: 1_DataIO の全体の流れの本(EricaNext_try の `DataIO_example`)、TPC のゲインマップとドリフト速度の較正の本(SMILE-3 のデータでは未作成)。`0_Introduction` の 6 節は、まだ「解析は EricaNext の外」と書いてあるので直す。生成スクリプトはリポジトリに無い(ノートブックを直接直す)。
6b. **TPC 側で分かっていて、まだ直していないこと**
   - `TOT_Skewness` の `Ve` は単位ベクトルではない((1/傾き_x, 1/傾き_y, 1) × 符号。傾きが 0 に近いと巨大、求まらないと 0xffffffff)。EricaNext_try と比べて直す。
   - `TPC_Calibration` に SMILE_tool_box の「同時計数が 100 を超えるパネルだけ足す」条件が無い。
   - `Reconstruct` は `Ve` を使った幾何の α、到来方向をまだ計算していない(EricaNext_try にはある)。
   - 2026-06-23 の TPC_cal には、鳴りっぱなしのストリップを拾って `Len` が数百 mm になるイベントが多い。
   - TOT 補正の係数、skewness のオフセット、`clock_offset`・`pitch_z`(ドリフト速度)は SMILE-2+ の値のまま。
   ノートブックの生成と実行: `jupyter nbconvert --to notebook --execute --inplace --ExecutePreprocessor.kernel_name=ericanext <nb>`(`/usr/bin/python3 -m jupyter`)。
7. **SMILE-2+ のデータも同じ EricaNext で扱う**(決定済みの方針): EricaNext_try の SMILE-2+ 用モジュールを、名前に `_2p` を付けて取り込む(ReadData_2p、CalcFADC_2p、TPC_Calibration_2p、HA_CenterGravity_2p、HA_Calc_2p、TOT_Skewness_2p、Reconstruct_2p など)。SMILE-3 のモジュールの名前は変えない。ANLNext は名前でモジュールを探して `static_cast` するので、中身の違う同名クラスは置けない。モジュールの中の `get_module("ReadData")` なども `_2p` の名前に直す。2+ の設定ファイル(`smile2p_*.json`)で EricaNext_try と同じ結果になるかを確かめる。
6. `psa_param.csv` には IP アドレスと基板のシリアル番号が入っている。リポジトリに入れたままでよいか、人に確かめる(今は入れて push 済み)。

## 7. 経緯(コミット)

| コミット | 中身 |
| --- | --- |
| `0ca45c8` | PSA_ReadData〜PSA_TreeWriter、`smile3_psa.json`、`calib/psa/`(psa_param、position) |
| `c85494a` | PSA_Event、Reconstruct が PSA を使う、`smile3_reconstruct_psa.json`、`geometry_template.csv`、`scripts/check/compare_psa_python.py`、この AGENTS.md |
| `daede13` | `smile3_reconstruct_psa` で PSA_* を CalcFADC より前に(CalcFADC の AS_SKIP で PSA が止まらないように) |
| `c132646` | 温度ゲインの表と hg_corr、`scripts/calib/`(位置・温度ゲイン・エネルギー)、PSA のモジュールのチュートリアル |
| `7818fdd` | TPC の CalcFADC がイベントを飛ばさない(SMILE_tool_box と同じ。前はアノード 2 ch 以上の 29% を捨てていた)、TPC_Ee_Valid、NoCalData のエネルギーを和に |
| `fa7e472` | TPC_Calibration の FADC の ch を strip / 32 に(ボードが欠けると範囲外で落ちていた) |
| `c9a8343` | TPC のモジュールのチュートリアル(1_ReadData、7_CalcFADC、8_TPC_Calibration、9_TOT_Skewness、11_Reconstruct、12_RawTreeWriter_sm3) |
| `91afdbd` | 位置の格子の方法、PSA_ReadData がボードのファイルを複数のディレクトリから集める(`recursive`)、UVSOR のデータから作った位置・温度ゲイン・エネルギーの較正ファイル、`4_MakeCalibration/PSA_*` |
