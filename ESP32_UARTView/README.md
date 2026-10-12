# ESP32_UARTView

Waveshare **ESP32-S3-LCD-1.69** にSTM32のデバッグUARTを表示するArduinoスケッチ。
LFで確定した1行を1画面に折り返し、ANSIの色・太字を文字ごとに適用する。

## 接続

| 信号 | 4WS_MainFW | ESP32-S3-LCD-1.69 |
| --- | --- | --- |
| デバッグTX（LPUART1） | PC1 | RXD / GPIO44 |
| GND | GND | GND |
| 電源 | 各基板を個別給電 | USB Type-C |

3.3V TTL UART、**2,000,000 baud、8N1、フロー制御なし**。RS-232や5V信号は直結しない。
ESP32のTXは未使用で、STM32のRXへの接続は不要。
参考機 `G474_Orion_main` を接続するときは **PA2（LPUART1_TX）** を使用する。
基板のリビジョンと実際のコネクタ表示を確認して接続する。

`4WS_MainFW`はLPUART1の設定があるが、デバッグ文字列出力は未実装。
このプロジェクトの追加だけでは同STM32から文字列は出力されない。

## セットアップ

Arduino IDEで `ESP32_UARTView.ino` を開く。

- ボード: **ESP32S3 Dev Module**
- esp32 by Espressif Systems: **3.2.0**（ビルド確認用の版）
- ライブラリ: **GFX Library for Arduino 1.6.3**
- Flash Size: **16MB**
- PSRAM: **OPI PSRAM**（手元の基板仕様と照合）
- USB Mode: **Hardware CDC and JTAG**
- USB CDC On Boot: **Enabled**
- 書き込み先: 対象基板のUSBポート

CLIで依存関係を用意する場合:

```powershell
arduino-cli core update-index --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core install esp32:esp32@3.2.0 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli lib install "GFX Library for Arduino@1.6.3"
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1
```

`build.ps1`はコンパイルのみ。CLIのフルパスを使う場合は `-Cli "C:\path\arduino-cli.exe"`。
FW更新は [doc/firmware-update.md](doc/firmware-update.md) に従う。
初回は `tools/update.ps1 -Initial`、通常は `tools/update.ps1` でビルド・USB書き込み・起動確認を実行する。
通常更新ではBOOT／RESET操作を不要とし、Hardware CDCを維持する。

## 表示

- 起動時は `Waiting UART`。LF受信後に画面を更新。
- 初めは2倍文字（約18文字×14行）。長い行は等倍（約37文字×29行）。
- 空白・カンマ・スラッシュの後で折り返し、長い単語は幅で分割。
- 最大20fpsで最新の完成行を表示。古い行の履歴保存はしない。
- フッター `TRUNC` は画面容量または2,048文字の行バッファを超えたことを示す。
- `RXerr` はUARTドライバが通知した受信異常の回数。異常後は次のLFまで破棄して再同期。
- `Config.h` の `demo = true` でSTM32なしの表示確認が可能。通常は `false`。

仕様・対応ANSI・制限は [doc/design.md](doc/design.md)、確認手順は [doc/verification.md](doc/verification.md)。

## ファイル

| ファイル | 責務 |
| --- | --- |
| `ESP32_UARTView.ino` | LCD初期化、受信タスク、最新行共有、描画 |
| `Config.h` | UART/LCDピン、通信速度、更新周期、デモ選択 |
| `Terminal.h` | ANSI解析、文字属性、行バッファ、折り返し |
| `tests/terminal_test.cpp` | ホスト上の解析・境界テスト |
| `tools/build.ps1` | ESP32向けコンパイル |
| `tools/update.ps1` / `tools/usb_update.py` | USB自動更新と起動確認 |

## 参照

- [Waveshare公式ドキュメント](https://docs.waveshare.com/ESP32-S3-LCD-1.69)
- [Waveshare旧Wiki・サンプル配布](https://www.waveshare.com/wiki/ESP32-S3-LCD-1.69)
- [公式V2回路図](https://files.waveshare.com/wiki/ESP32-S3-LCD-1.69/ESP32-S3-LCD-1.69-sch_V2.pdf)
- [Arduino_GFXのST7789設定](https://github.com/moononournation/Arduino_GFX/wiki/Display-Class)
- [Arduino CLI導入](https://docs.arduino.cc/arduino-cli/installation)
