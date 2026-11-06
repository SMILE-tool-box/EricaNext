# 検出器の CAD

| ファイル | 中身 |
| --- | --- |
| `SM3_Inner_3atm.step` | SMILE-3 の内部(3 気圧の容器の中: TPC、μ-PIC、GEM、ドリフトケージ、底面と側面の HA のモジュール)の組み立て。2026-11-06 に Autodesk の CAD から書き出した STEP(AP214) |

- 使い方: `scripts/calib/make_psa_geometry.py` がこのファイルから `calib/psa/geometry.csv`(PSA_Event のジオメトリ)を作ります。
  STEP の組み立ての木は `scripts/calib/step_reader.py` で読みます(CAD のライブラリは要らない)。部品と大きさの一覧は
  `/usr/bin/python3 scripts/calib/step_reader.py calib/cad/SM3_Inner_3atm.step '<部品の名前の正規表現>'` で見られます。
- CAD の座標は z が上向きで、原点は底板です。EricaNext の座標(原点は μ-PIC の中央、Z は下向き)へは
  X = y_CAD、Y = x_CAD、Z = 427.04 − z_CAD(427.04 mm は μ-PIC の基板の下の面)。
- CAD で確かめた寸法: GEM は μ-PIC から 10.0 mm、ドリフトケージの天板の内側の面は 317.0 mm。
- シミュレーター(Geant4)の配置も、このファイルからそろえる予定です(AGENTS.md の残りの仕事)。
