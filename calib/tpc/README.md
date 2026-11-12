# TPC の較正ファイル

TPC の較正ファイルの置き場です。PSA の `calib/psa/` と同じく、較正のランから作ったファイルをここに置き、チェーンはそれを読むだけにします。

**今は、SMILE-3 の TPC の較正ファイルはまだありません。**

| ファイル(予定) | 中身 | 読むモジュール | 今の扱い |
| --- | --- | --- | --- |
| ゲインマップ | 1 行に `アノード ch  カソード ch  Ka [keV/(ADU × sample)]  Kb [keV]`。ch は FADC のチャンネル(= ストリップ / 32、0〜47)。同時計数は同じパネルの中でしか取らないので、意味のある組は 1 パネル 12 × 12、4 パネルで 576 組 | `TPC_Calibration`(`ParameterFile`、`NoCalData: false`) | 無い。`NoCalData: true` で、エネルギー = 電荷 [ADU × sample] |
| ドリフト速度 `drift_velocity.dat` | `clock_offset <クロック>` と `velocity <mm/クロック>` の 2 行(`#` はコメント)。z = velocity × (クロック − clock_offset) + `z_gem_mm` | `TOT_Skewness`(`drift_velocity_file`) | 無い(ドリフト速度を測れる測定がまだ)。設定ファイルの `pitch_z_mm_per_clock` = 0.383、`clock_offset` = 85(SMILE-2+ の 2019-07-01 Per02/Per03 の典型値)を使い、TOT_Skewness が「較正ファイルが要る」と警告を出す |

SMILE-3 の TPC の寸法: μ-PIC から GEM 面まで 10 mm(`z_gem_mm`、SMILE-2+ は 27 mm)、ドリフト長(μ-PIC から天板まで)317 mm。
ドリフト速度は、全ヒットのクロックの分布(箱の形)の GEM 側の端を `clock_offset`、天板側の端までのクロックで 317 − 10 = 307 mm を割ったものを velocity にします
(SMILE-2+ は 305 mm)。2026-06-23 の TPC_cal はクロックの分布が箱の形にならない(天板側の端が見えない)ので、これには使えません。

作り方は [TPC_GainMap.ipynb](../../tutorials/4_MakeCalibration/TPC_GainMap.ipynb) と [TPC_DriftVelocity.ipynb](../../tutorials/4_MakeCalibration/TPC_DriftVelocity.ipynb) にあります。いずれも `RawTreeWriter_sm3` のROOTを入力にします。SMILE-3の電荷はpCではなくADU × sampleなので、ゲイン係数の単位に注意してください。
ドリフト速度のファイルは `scripts/calib/make_tpc_drift_velocity.py` で作ります(生データのツリー `smile3_raw` から):

```bash
/usr/bin/python3 scripts/calib/make_tpc_drift_velocity.py -o calib/tpc/drift_velocity.dat raw.root   # qa/drift_velocity.png も書く
```

箱の形でない(天板側の端が見えない)などの確認に通らないときは、ファイルを書きません(`--force` で書く)。
SMILE-2+ の Per02 の全イベントで試すと、`clock_offset` 85、速度 0.3844 mm/クロックで、EricaNext_try の `drift_velocity.dat`(85、0.383〜0.384)と合います
(`--tree RawTreeWriter/raw --z-gem 27 --drift-length 332 --gem-range 0 200 --top-range 512 1023`)。

ゲインマップはチュートリアル内で、確認した2本の線のガウス＋直線背景fitから作成します。線源・エネルギー・fit範囲を明示して指定し、未較正組は0とします。由来とfit結果は別JSONに保存します。最初の手順の 1-hit スペクトルは `scripts/calib/make_tpc_spec1hit.py`(生データのツリー `smile3_raw` から)で作れます。
