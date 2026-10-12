# HW仕様とFW更新仕様

## 方針

通常のFW更新ではBOOT、RESET、電源ボタンの操作を不要にする。
PCにデータ通信対応USBケーブルで接続した状態で、更新コマンド1回で
ビルド、対象確認、書き込み、再起動、起動確認を行う。
初回は手動で書き込みモードにした状態を使用する。
STM32のデバッグUARTとは別のUSB Serial/JTAG経由で更新する。

## HW確認

[公式V2回路図](https://files.waveshare.com/wiki/ESP32-S3-LCD-1.69/ESP32-S3-LCD-1.69-sch_V2.pdf)
をPDFから読み取り、回路図画像でもUSB・KEYS・電源回路を確認した。
実機のリビジョン表記自体は未確認で、V2回路図に基づく配線情報と実機読出しを区別する。

| 項目 | 確認した仕様 | 更新への影響 |
| --- | --- | --- |
| MCU | 実機: ESP32-S3、revision v0.2 | ROMローダーによるUSB書き込みが可能 |
| PSRAM | 実機チップ情報: 内蔵8 MB、3.3V | OPI PSRAM設定を使用。アプリ起動後にも容量を確認 |
| Flash | 実機: 16 MB、3.3V、Quad、ID 20/4018 | Flash Size=16MB。実機品種を回路図のW25Q128と同一とは断定しない |
| USB Type-C | 回路図H2: D−→GPIO19、D＋→GPIO20 | USB-UARTブリッジを介さずESP32-S3のUSBへ直結 |
| USB機器 | 実機: VID=303A、PID=1001 | USB Serial/JTAG。通常の更新経路 |
| BOOT | 回路図Key1: GPIO0をGNDへ接続、10 kΩプルアップ | GPIO0 LOWでリセットするとROM書き込みモード |
| RESET | 回路図Key3: CHIP_PUをGNDへ接続 | 手動のフルリセット。通常更新では使用しない |
| UART端子 | 回路図P2: TX=GPIO43、RX=GPIO44 | ROM UART0との共有を避け、表示用UART1はGPIO18 RX、GPIO17 TXを使う。U0TXD/U0RXDは接続しない |
| 電源 | USB VBUSとバッテリーの電源経路、SYS_EN=GPIO41 | 初回・更新確認はUSB給電。バッテリー単独ではPC更新不可 |

実機検出時のポートはCOM37、USBシリアルは `1C:DB:D4:79:78:94`。
COM番号はWindowsの割当で変わるため、更新コードには固定しない。

## 自動更新を維持するFW条件

- ArduinoのUSB Modeは **Hardware CDC and JTAG**。
- **USB CDC On Boot=Enabled**、Upload Modeは **UART0 / Hardware CDC**。
- GPIO19/20を別用途へ割り当てず、USB Serial/JTAGを無効にしない。
- Deep Sleepへ入らない。ホスト接続を `while (!Serial)` で待たない。
- 更新用の特殊なUARTコマンド、独自ブートローダー、Wi-Fi OTAは追加しない。
- `Serial.begin(115200)` でUSB管理口を使用する。USBの115200設定はSTM32 UARTの2 Mbpsとは独立。
- USBへ `INFO` + LFを送ると機器識別、FW版、ビルド日時、Flash/PSRAM容量、稼働時間を返す。
- 必須USB設定が違うとコンパイルを失敗させ、更新経路を失うFWを誤って作らない。

[EspressifのUSB Serial/JTAG仕様](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/usb-serial-jtag-console.html)
に基づき、書き込みモードへの移行はUSBコントローラーに任せる。

## 初回と通常更新の違い

| 処理 | 初回 `-Initial` | 通常更新 |
| --- | --- | --- |
| 開始状態 | BOOTでROM書き込みモードに起動済み | UARTViewが通常動作中 |
| 書き込み前 | `no-reset` で接続 | `usb-reset` で自動的にROMへ移行 |
| 書き込み後 | `watchdog-reset` でフルリセット | `hard-reset` でUSB経由のリセット |
| 人間の操作 | 初回の書き込みモード起動のみ。BOOTボタンは放しておく | USB接続とPCの更新コマンド実行のみ |

USB Serial/JTAGの通常リセットはコアリセットで、起動ストラップを再読出ししない。
手動BOOT起動直後は通常リセットだけではROMに残る場合がある。
初回はウォッチドッグによるフルリセットでGPIO0を再読出しして通常起動へ戻す。
[esptoolのリセット仕様](https://docs.espressif.com/projects/esptool/en/latest/esp32s3/esptool/advanced-options.html)
を参照。GPIO0が物理的にLOW固定のままなら、ソフトウェアだけでは通常起動へ戻せない。

Arduino core 3.2.0付属ツールは配置フォルダ名が4.9.dev3だが、実行表示はesptool 4.8.1。
このツールには `watchdog-reset` 選択肢がないため、更新専用に **esptool 5.5.0** を固定して用意する。
Arduino coreの入替えは不要。

## PCの準備（初回のみ）

以下は `ESP32_UARTView` フォルダで実行する。Python 3.13で確認する構成。
Arduino core・GFXライブラリはREADMEの設定を使用する。

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\setup-flash.ps1
```

Arduino CLIがPATHにない場合は、更新コマンドへ `-Cli "C:\path\arduino-cli.exe"` を付ける。
`.tools/arduino-cli.exe` があればスクリプトが自動選択する。
PythonはPATHのpythonを使用する。別環境を使う場合は `-Python "C:\path\python.exe"`。
準備スクリプトは公式GitHubのWindows版esptool 5.5.0をダウンロードしてSHA-256を検証し、
Pythonのpyserialがなければ導入する。esptool本体のPython依存パッケージは不要。

## 初回書き込み

現在ユーザーが準備した書き込みモードのまま、以下を実行する。
ここでRESETを押して準備状態を変える必要はない。

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\update.ps1 -Initial
```

複数のEspressif USB機器が接続されている場合は `-Port COM37` のように対象を指定する。
ツールは候補が複数あると書き込まずに停止する。

## 2回目以降

UARTViewが動作している状態で、BOOTやRESETを押さずに実行する。

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\update.ps1
```

更新スクリプトは次の順に処理する。

1. 設定を固定してスケッチをビルドし、書き込み用の4つのbinを用意する。
2. VID/PIDから対象USB機器を選択し、ESP32-S3・Flash 16 MBをROM経由で確認する。
3. bootloader=0x0、partition table=0x8000、boot_app0=0xe000、application=0x10000へ書き込む。
4. esptoolの書き込み検証が成功した後にリセットする。Flash全消去は行わない。
5. 同じUSBシリアルの機器の再列挙と `INFO` 応答を最大20秒待つ。
6. FW版、Flash 16 MB、PSRAM 8 MBを確認して `Boot verified` を出す。

書き込みが成功しても起動確認できなければ、更新コマンドは失敗として終了する。
Windowsではリセット時のUSB切断がesptoolのポート例外になる場合がある。
4領域のハッシュ検証完了とリセット段階への到達を確認できる場合だけ起動確認を続け、
実際のFW応答が得られたときに成功とする。書き込み中の異常はこの扱いにしない。
FWの版は `Config.h` の `firmwareVersion` に保持し、リリース時に更新する。
書き込みなしの情報確認:

```powershell
python .\tools\usb_update.py --check
```

## 人間の操作が必要な復旧条件

USBの結線や給電が失われた場合、GPIO19/20を別用途にしたFW、USBを無効化したFW、
Deep Sleep中など、PCからUSBへ接続できない状態はこの経路では更新できない。
まずケーブル・電源・ポート占有を確認する。
FWが原因で接続できない場合のみ、BOOTを押したままRESETを押して放し、最後にBOOTを放す。
新しいCOMポートを確認して `-Initial` で復旧する。
通常更新のボタン操作不要という要件と、USBを失った故障時の手動復旧は区別する。

## 実測結果

結果は [verification.md](verification.md) に記録する。
