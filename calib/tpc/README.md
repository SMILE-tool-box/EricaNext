# TPC の較正ファイル

TPC の較正ファイルの置き場です。

今ある SMILE-3 の較正ファイル(2026-11-18 に、`config/stage2/smile3_reconstruct.json` が使っていた NAS のファイルをコピーした):

| ファイル | 中身 | 読むモジュール | 元のラン |
| --- | --- | --- | --- |
| `gain_map.dat`(`gain_map.json` に作り方・fit の結果・誤差) | 1 行に `アノード ch  カソード ch  Ka [keV/(ADU × sample)]  Kb [keV]`。ch は FADC のチャンネル(= ストリップ / 32、0〜47)。較正できなかった組は Ka = Kb = 0 | `TPC_Calibration`(`ParameterFile`、`NoCalData: false`) | 20261113 run13〜17(TPC_GainMap.ipynb) |
| `tpc_energy_correction.dat`(`tpc_energy_correction.json` に係数の共分散・fit の結果) | ゲインマップ適用後のイベント全体のエネルギーの二次補正 | `TPC_Calibration`(`energy_correction_file`) | 20261113 run13〜17(TPC_EnergyCalibration.ipynb) |
| `drift_velocity.dat` | `clock_offset <クロック>` と `velocity <mm/クロック>` の 2 行(`#` はコメント)。z = velocity × (クロック − clock_offset) + `z_gem_mm` | `TOT_Skewness`(`drift_velocity_file`) | 20261115 run03(ETCC)。2026-11-20 に z_gem = 5 mm で作り直した(端のクロックは同じで velocity だけ変わる。TPC_DriftVelocity.ipynb の `tracks.root` のクロック分布から)。天板側の端がなだらか(幅 36.7 クロック)で確認には通らず、`--force` で書いたもの |

`qa/` は作ったときの確認の図のコピー(git には入れない)。

## TPC の寸法とドリフト速度

- μ-PIC から GEM 面まで 5 mm（`z_gem_mm`。GEM 1 枚の今の構成。2 枚目の GEM を入れた構成では 10 mm で、CAD の値はこちら）、ドリフト長（μ-PIC から天板まで）317 mm です（[detector_coordinates](../detector_coordinates/README.md)）。
- ドリフト速度は、全ヒットのクロックの分布（箱の形）から決めます。GEM 側の端を `clock_offset` とし、天板側の端までのクロック数で 317 − 5 = 312 mm を割ったものを `velocity` にします。

## 作り方

入力はどちらも `TPC_RawTreeWriter` の ROOT（生データのツリー `smile3_tpc_raw`）です。

- **ゲインマップ**: [TPC_GainMap.ipynb](../../tutorials/4_MakeCalibration/TPC_GainMap.ipynb)
  - 1-hit スペクトルを `scripts/calib/make_tpc_spec1hit.py` で作り、確認した2本の線をガウス＋直線の背景でフィットして係数を決めます。
  - 線源・エネルギー・フィット範囲はノートブックで指定します。較正できなかった組は 0 にし、作り方とフィット結果は別の JSON に保存します。
  - SMILE-3 の電荷は pC ではなく ADU × sample なので、係数の単位（keV/(ADU × sample)）に注意してください。
  - ガス増幅率のマップは、`gain_map.dat` から `scripts/calib/plot_tpc_gas_gain.py` で描けます（G = Q(E) × W / E / e / G_board。電荷の換算・W・G_board は SMILE-2+ の値）。
- **ドリフト速度**: [TPC_DriftVelocity.ipynb](../../tutorials/4_MakeCalibration/TPC_DriftVelocity.ipynb)、または次のコマンド

  ```bash
  /usr/bin/python3 scripts/calib/make_tpc_drift_velocity.py -o calib/tpc/drift_velocity.dat raw.root   # qa/drift_velocity.png も書く
  ```

  クロックの分布が箱の形でない（天板側の端が見えない）など、確認に通らないときはファイルを書きません（`--force` で書く）。

## TPC全体のエネルギー補正

[TPC_EnergyCalibration](../../tutorials/4_MakeCalibration/TPC_EnergyCalibration.ipynb)で、ゲインマップ適用後のイベント全体のスペクトルを作り、Cu・Baの3ピークから二次補正を求めます。
1-hitの選別はしません。未較正の組はKa=Kb=0として寄与0にし、イベントは残します。
`TPC_Calibration.energy_correction_file` に出力datを指定します（空なら従来どおり）。ピーク中心の標準誤差から伝播した係数共分散は別JSONに保存します。
3点間は二次式、範囲外は端点の接線で延長し、負の補正値は0にします。基準値とfit結果を確認するまでファイルは保存しません。
