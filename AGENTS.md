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
- `--data-dir` を指定しても `ericanext_config: WARNING: no storage_g*/ in .../mount_data` が出る。`mount_data/` が空なだけなので、気にしなくてよい(以前からの動き)。
- `build/`、`install/`、`mount_data/`、`root_data/` は `.gitignore` に入っている。

## 3. データの場所

| データ | 場所 |
| --- | --- |
| SMILE-3 FM の生データ(NAS) | `/mnt/nas_eureka/SMILE-3FM_data/<日付>/<storage>/<日付>_runNN/`。`calltt` = storage_g0(アノード `tpc_enc_a*`、ha06〜ha16)、`killua` = storage_g1(カソード `tpc_enc_c*`、ha00〜ha05)。モードは `run_meta.json` の `mode`(TPC_cal、HA_cal、ETCC) |
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
| `PSA_ReadData` | `ha<NN>_<番号>.dat` を全ボード読む。`ReadData` があればそのトリガーカウンタに合わせて読み(`sync_tpc`)、無ければボード同士をトリガーカウンタでそろえる。中身の読み取りは `PSA_Board` |
| `PSA_CalcFADC` | ペデスタル(最初の 23 サンプル)、ピーク(LG の和の最大)、台形積分(前 4〜後 9)、`hg`・`lg`、温度 |
| `PSA_CenterGravity` | 4 隅の重心 `x_hg`, `y_hg`, `x_lg`, `y_lg` |
| `PSA_Calibration` | 64 点の薄板スプラインで位置を直し、ピクセル (n, m) とエネルギーを出す |
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

| ファイル | 由来 | 状態 |
| --- | --- | --- |
| `psa_param.csv` | `PSA_analysis/param/psa_param_20261029.csv` のコピー(`HA` 列のある版) | あり。ha09、ha10、ha12、ha13、ha15 の温度較正は仮の値(`CAL_NOTE` 列) |
| `position_haNN.csv` | `PSA_analysis/peak_runs_run01-11_haNN/position_haNN.csv`(peak_search の結果、2026-11-05)に `run_script/psa_position_nm.py` で n, m を足したもの | 14 ボード(ha00〜07、09、10、12、13、15、16) |
| `energy_haNN.csv` | 未定(`ch,n,m,p0,p1`) | **無い**。今は `energy = -1` |
| `geometry.csv` | 未定(形式は `geometry_template.csv` と `config/README.md`) | **無い**。今は `PSA_Event` がヒットを使わない |

- `peak_search.py` をやり直したら、`psa_position_nm.py` で `position_haNN.csv` を作り直す。
- n, m の付け方は grid_spect.py に合わせてある(x で 8 列に分け、列ごとに y で並べる)。値が同じピークがあると C++ と numpy で並びが変わるので、numpy の並びを書き込んでいる。

## 6. 残りの仕事(上ほど優先)

1. **エネルギーの較正**(`energy_haNN.csv`)
   - 線源のランで、`PSA_Calibration/PixelHG_haNN`(ピクセルごとの hg スペクトル)の光電ピークをフィットして `p0, p1` を出すスクリプトを作る。
   - どのランが何の線源か(エネルギー)は人に確かめる。
   - 温度補正 `temp_coef` は今 0。
2. **ジオメトリ**(`geometry.csv`)
   - 必要な値: ボードの位置(底面 HA0〜5、側面 N/W/S/E の HA6〜17)、モジュールの中の ch(TL〜BR)の位置、ピクセルの間隔、n, m の向き。
   - 値は人(検出器の担当)に聞く。`0_Introduction` の配置図は模式図なので、そのまま使わない。
3. **イベントの選び方の確認**
   - `PSA_Event` の「1 ヒット」は「ピクセルとエネルギー > `energy_min` があり、geometry にあるヒットが 1 つ」。ノイズのヒットで MultiHit になりすぎないかを、実データで見る。
   - `CalcFADC` が FADC にヒットの無いイベントを飛ばすので、`smile3_reconstruct_psa` では TPC にヒットのあるイベントしか PSA を処理しない(意図どおりか確かめる)。
4. **`ReadData` が複数のディレクトリを読めるようにする**(`PSA_ReadData.extra_data_dirs` と同じ形)。2 つの storage に分かれたランをシンボリックリンク無しで読めるようにする。
5. ノートブック `tutorials/` に PSA のチュートリアル(フラッドマップ、ピクセル、スペクトル)を足す。`0_Introduction` の 6 節は、まだ「解析は EricaNext の外」と書いてあるので直す。
6. `psa_param.csv` には IP アドレスと基板のシリアル番号が入っている。リポジトリに入れたままでよいか、人に確かめる(今は入れて push 済み)。

## 7. 経緯(コミット)

| コミット | 中身 |
| --- | --- |
| `0ca45c8` | PSA_ReadData〜PSA_TreeWriter、`smile3_psa.json`、`calib/psa/`(psa_param、position) |
| `c85494a` | PSA_Event、Reconstruct が PSA を使う、`smile3_reconstruct_psa.json`、`geometry_template.csv`、`scripts/check/compare_psa_python.py`、この AGENTS.md |
