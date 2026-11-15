# TPC の較正ファイル

TPC の較正ファイルの置き場です。

**今は、SMILE-3 の TPC の較正ファイルはまだありません。**

| ファイル(予定) | 中身 | 読むモジュール | 今の扱い |
| --- | --- | --- | --- |
| ゲインマップ | 1 行に `アノード ch  カソード ch  Ka [keV/(ADU × sample)]  Kb [keV]`。ch は FADC のチャンネル(= ストリップ / 32、0〜47)。同時計数は同じパネルの中でしか取らないので、意味のある組は 1 パネル 12 × 12、4 パネルで 576 組 | `TPC_Calibration`(`ParameterFile`、`NoCalData: false`) | 無い。`NoCalData: true` で、エネルギー = 電荷 [ADU × sample] |
| ドリフト速度 `drift_velocity.dat` | `clock_offset <クロック>` と `velocity <mm/クロック>` の 2 行(`#` はコメント)。z = velocity × (クロック − clock_offset) + `z_gem_mm` | `TOT_Skewness`(`drift_velocity_file`) | 無い(ドリフト速度を測れる測定がまだ)。設定ファイルの `pitch_z_mm_per_clock` = 0.383、`clock_offset` = 85(SMILE-2+ の 2019-07-01 Per02/Per03 の典型値)を使い、TOT_Skewness が「較正ファイルが要る」と警告を出す |

## TPC の寸法とドリフト速度

- μ-PIC から GEM 面まで 10 mm（`z_gem_mm`）、ドリフト長（μ-PIC から天板まで）317 mm です（[detector_coordinates](../detector_coordinates/README.md)）。
- ドリフト速度は、全ヒットのクロックの分布（箱の形）から決めます。GEM 側の端を `clock_offset` とし、天板側の端までのクロック数で 317 − 10 = 307 mm を割ったものを `velocity` にします。

## 作り方

入力はどちらも `TPC_RawTreeWriter` の ROOT（生データのツリー `smile3_tpc_raw`）です。

- **ゲインマップ**: [TPC_GainMap.ipynb](../../tutorials/4_MakeCalibration/TPC_GainMap.ipynb)
  - 1-hit スペクトルを `scripts/calib/make_tpc_spec1hit.py` で作り、確認した2本の線をガウス＋直線の背景でフィットして係数を決めます。
  - 線源・エネルギー・フィット範囲はノートブックで指定します。較正できなかった組は 0 にし、作り方とフィット結果は別の JSON に保存します。
  - SMILE-3 の電荷は pC ではなく ADU × sample なので、係数の単位（keV/(ADU × sample)）に注意してください。
- **ドリフト速度**: [TPC_DriftVelocity.ipynb](../../tutorials/4_MakeCalibration/TPC_DriftVelocity.ipynb)、または次のコマンド

  ```bash
  /usr/bin/python3 scripts/calib/make_tpc_drift_velocity.py -o calib/tpc/drift_velocity.dat raw.root   # qa/drift_velocity.png も書く
  ```

  クロックの分布が箱の形でない（天板側の端が見えない）など、確認に通らないときはファイルを書きません（`--force` で書く）。
