# TPC の較正ファイル

TPC の較正ファイルの置き場です。PSA の `calib/psa/` と同じく、較正のランから作ったファイルをここに置き、チェーンはそれを読むだけにします。

**今は、SMILE-3 の TPC の較正ファイルはまだありません。**

| ファイル(予定) | 中身 | 読むモジュール | 今の扱い |
| --- | --- | --- | --- |
| ゲインマップ | 1 行に `アノード ch  カソード ch  Ka [keV/pC]  Kb [keV]`。ch は FADC のチャンネル(= ストリップ / 32、0〜47)。同時計数は同じパネルの中でしか取らないので、意味のある組は 1 パネル 12 × 12、4 パネルで 576 組 | `TPC_Calibration`(`ParameterFile`、`NoCalData: false`) | 無い。`NoCalData: true` で、エネルギー = 電荷 [ADU] |
| ドリフト速度 | z = `pitch_z_mm_per_clock` × (クロック − `clock_offset`) + 27 mm の 2 つの値 | `TOT_Skewness` | 無い。設定ファイルに SMILE-2+ の値(0.5 mm/clock、90 clock)を書いている |

作り方は、EricaNext_try の `tutorials_caldata/4_MakeCalibration/GainMap.ipynb`、`DriftVelocity.ipynb` と同じ方法にする予定です。
作るスクリプトは `scripts/calib/` に置きます。最初の手順の 1-hit スペクトルは `scripts/calib/make_tpc_spec1hit.py`(生データのツリー `smile3_raw` から)で作れます。
