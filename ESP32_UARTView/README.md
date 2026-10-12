# ESP32_UARTView

Waveshare **ESP32-S3-LCD-1.69** にSTM32のデバッグUARTを表示するArduinoスケッチ。
LFで確定した1行を1画面に折り返し、ANSIの色・太字を文字ごとに適用する。

## 接続

| 信号 | 4WS_MainFW | ESP32-S3-LCD-1.69 |
| --- | --- | --- |
| デバッグTX（LPUART1） | PC1 | GPIO18（ESP32が受信） |
| IDX切り替えRX（LPUART1） | PC0 | GPIO17（ESP32から送信） |
| GND | GND | GND |
| 電源 | 各基板を個別給電 | USB Type-C |

3.3V TTL UART、**2,000,000 baud、8N1、フロー制御なし**。RS-232や5V信号は直結しない。
送信線（GPIO17→STM32 RX）は **3.3Vへ1～2.2 kΩで外部プルアップ**する。
ESP32の送信はオープンドレイン。内蔵PUのみでは2 Mbpsの立ち上がり時間を保証できない。
基板の「TX」端子（GPIO43）はROM起動時のUART0出力があるため、この送信線には使わない。
基板の「RX」端子（GPIO44）はROMのUART0 RXと共有されるため、表示用RXにはGPIO18を使う。
参考機 `G474_Orion_main` の現在の接続は **PA2 TX→GPIO18、PA3 RX←GPIO17、GND共通**。
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

## ボタン操作とIDX切り替え

BOOTボタンを押すとLF（0x0A）を1バイト送信する。参考STM32では次のIDXへ進む。
25 msのチャタリング除去、長押しリピートなし。起動時に押されていた場合は、放してからの再押下で送信する。
PWRとRESETにはIDX操作を割り当てない。デモモードでは送信しない。
`4WS_MainFW`側の受信・IDX処理は未実装なので、同FWとの結合動作はまだ未確認。

非送信時のGPIO17は **入力＋PU（出力無効）**。送信中もLOWのみ駆動し、HIGHはプルアップで作る。
並列接続する他の送信機も待機中は出力を解放する必要がある。
通常のプッシュプルUART TXをそのまま並列接続すると、他の送信機のHIGHとLOWが競合する。
同時送信の調停は実装していないため、送信タイミングは機器間で分ける。
USBの `INFO` 応答に送信回数 `NEXT` とGPIO17の出力有効ビット `TXOE` を含める。待機時は `TXOE=0`。

## PCからの受信確認

USBの `INFO` で受信byte数 `RXBYTES`、完成行数 `FRAMES`、描画元の行番号 `SHOWN`、
UARTエラー `RXERR`、最終受信からの時間 `RXAGE`（ms）を取得できる。
実UART速度 `BAUD`、UART入力のGPIO Matrix設定 `RXMAP`、エラー種類別の件数も取得できる。
`SNAP` は描画元の1行をASCIIで返す。`STYLED` はデフォルト以外の文字属性を持つ文字数。
この機能はLCD実画素の読戻しではなく、STM32へコマンドも送らない。

```powershell
python .\tools\inspect_uart.py --samples 5 --interval 2 --output .\doc\measurements\uart-check.json
```

エラー分類は `BREAKERR`（BREAK）、`BUFERR`（RXバッファ満杯）、`FIFOERR`（FIFO欠落）、
`FRAMEERR`（フレーミング）、`PARITYERR`（パリティ）。無受信時の `RXAGE=4294967295` は未受信を表す。

`SELFTEST` はUART内部のTX→RXループバックで緑・太字の
`UARTVIEW_LOOPBACK_OK`を表示する。GPIO17は出力無効のまま、STM32へ送信しない。
約20 msだけ外部RXを内部入力に切り替えるので、この間の実ログは取得できない。
`PULSE` はRXのエッジ間隔を最大256個、最大20 msで採取する。
GPIOを駆動せず、CPUポーリングによる簡易測定（割込み・サンプリング誤差あり）でns値を返す。
電圧・立ち上がり波形を測る機能ではない。`inspect_uart.py --pulse`で記録できる。
GPIO18からSTM32ログを継続受信し、BOOT操作後のページ変更と待機時TXOE=0を確認済み。
UART通知エラーが0でも、一部の受信行に文字化けが残る。電気的品質・並列機器との接続は未検証。

仕様・対応ANSI・制限は [doc/design.md](doc/design.md)、確認手順は [doc/verification.md](doc/verification.md)。

## ファイル

| ファイル | 責務 |
| --- | --- |
| `ESP32_UARTView.ino` | LCD初期化、受信タスク、最新行共有、描画 |
| `Config.h` | UART/LCDピン、通信速度、更新周期、デモ選択 |
| `Terminal.h` | ANSI解析、文字属性、行バッファ、折り返し |
| `Button.h` | チャタリング除去と押下イベント |
| `tests/terminal_test.cpp` | ホスト上の解析・境界テスト |
| `tools/build.ps1` | ESP32向けコンパイル |
| `tools/update.ps1` / `tools/usb_update.py` | USB自動更新と起動確認 |

## 参照

- [Waveshare公式ドキュメント](https://docs.waveshare.com/ESP32-S3-LCD-1.69)
- [公式V2回路図](https://files.waveshare.com/wiki/ESP32-S3-LCD-1.69/ESP32-S3-LCD-1.69-sch_V2.pdf)
- [Arduino_GFXのST7789設定](https://github.com/moononournation/Arduino_GFX/wiki/Display-Class)
- [Arduino CLI導入](https://docs.arduino.cc/arduino-cli/installation)
