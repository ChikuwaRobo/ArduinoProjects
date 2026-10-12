# 確認方法と検証状態

## PC上のテスト

プロジェクトフォルダで実行する。LLVMまたはC++17対応コンパイラが必要。

```powershell
& "C:\Program Files\LLVM\bin\clang++.exe" -std=c++17 -Wall -Wextra -Werror .\tests\terminal_test.cpp -o .\tests\terminal_test.exe
if ($LASTEXITCODE -eq 0) { & .\tests\terminal_test.exe }
```

SGRの分割受信、色・太字・リセット、改行を越える属性、下線・反転、CRLFと空行、
CSI/OSCの除去、未対応拡張色、長すぎるCSI、行上限後の復帰、折り返しの文字欠落なしを確認する。

USB更新ツールの誤選択・失敗判定テスト（pyserialが必要）:

```powershell
python -m unittest discover -s .\tests -p test_usb_update.py
```

複数機器の自動選択禁止、VID/PIDの確認、未完了書き込みの失敗判定、
4領域の検証後に限るリセット時USB切断の扱いを確認する。

ボタン処理のテスト:

```powershell
& "C:\Program Files\LLVM\bin\clang++.exe" -std=c++17 -Wall -Wextra -Werror .\tests\button_test.cpp -o .\tests\button_test.exe
if ($LASTEXITCODE -eq 0) { & .\tests\button_test.exe }
```

押下バウンス、25 msの確定時間、長押し、再押下、起動時の押しっぱなし、millisの桁あふれを確認する。

## ボタン・TXの実機確認手順

1. GPIO17→STM32 RX（4WSではPC0、参考機ではPA3）を接続し、共有GNDと3.3Vへの外部PUを用意する。
2. 起動後にUSB INFOの `NEXT=0 TXOE=0` を確認する。
3. BOOTを1回押して、`NEXT` が1増え、STM32のIDXが1つ進むことを確認する。
4. 長押しで繰り返さず、放して再び押すともう1回進むことを確認する。
5. ロジックアナライザーで2 Mbps/8N1、送信内容がLF 1 byteであることを確認する。
6. オシロスコープでHIGHの立ち上がりが0.5 µsのbit時間に対して十分速いことを確認する。
7. 待機時にGPIO17がHIGHを能動駆動せず、送信終了後も `TXOE=0` へ戻ることを確認する。
8. 他の送信機から単独送信できることを確認する。同時送信は行わない。

## 実機での確認手順

1. `Config.h` の `demo = true` でビルドしてUSBから書き込む。
2. 画面位置・色・太字・下線・反転・折り返し・ちらつきを確認する。
3. `demo = false` に戻して再ビルド・書き込みする。
4. 電源を切ってSTM32 TXとESP32 GPIO18、GNDを接続し、USB給電で起動する。
5. STM32または3.3V USB-UARTから2 Mbps/8N1で以下を送る。
   `ESC` は実際のbyte 0x1Bを使用する（文字列「ESC」ではない）。

```text
ESC[0mVEL theta +123.4 ESC[1;32mOK ESC[0mloop 123456\n
ESC[31mUNDER_VOLTAGE ESC[33mWARN ESC[36mCAM ESC[0m\r\n
```

6. 100～800文字の行で文字サイズ切り替えと折り返しを確認する。
7. 画面に収まらない行・2,048文字超の行で `TRUNC`、次の短い行で正常復帰を確認する。
8. 実際の周期・最大長で連続送信し、最新状態の表示と `RXerr` を確認する。
9. RXを外したときに最後の完成行が残ることを確認する。

## 検証記録（2026-10-12）

- PC解析テスト: 成功（LLVM clang++、C++17、警告をエラーとして扱う）。
- ESP32ビルド: 成功。esp32 core 3.2.0、GFX Library for Arduino 1.6.3。
  `tools/build.ps1` 経由でも成功。USB管理口追加後はFlash 415,690 byte、静的RAM 84,492 byte。
  キャンバス等の実行時確保は静的RAMの数値に含まれない。
- 実機: COM37、USB Serial/JTAG VID=303A/PID=1001、USBシリアル `1C:DB:D4:79:78:94`。
  ESP32-S3 revision v0.2、Flash 16 MB、PSRAM 8 MBを読出し確認。
- 初回書き込み: 4領域のハッシュ検証成功。esptool 5.5.0のウォッチドッグリセットで
  手動ROM起動から通常起動へ移行。Windowsの再列挙時にツールのポート例外が出たが、
  続けてUSB INFOでFW 0.1.0・Flash 16 MB・PSRAM 8 MB・稼働時間を取得。
  この条件を更新スクリプトで扱うよう修正し、判定テストを追加。
- 通常更新: `tools/update.ps1 -Port COM37` で成功（終了コード0）。
  ボタン操作なしでUSBからROMへ移行、4領域の書き込み検証、再起動まで完了。
  `Boot verified` 応答でFW 0.1.0、Flash 16 MB、PSRAM 8 MB、再起動後の稼働時間899 msを確認。
- 更新判定テスト: 5件成功。公式Windows版esptoolのZIPも配布元のSHA-256に一致。
- LCDの目視・UART 2 Mbps連続受信・バッテリーでの電源保持: 未確認。
- `4WS_MainFW`との結合: STM32出力が未実装のため未確認。

### FW 0.2.0追加分

- ボタン処理テスト成功。既存の解析テストとUSB更新判定5件も成功。
- ビルド成功: Flash 416,354 byte、静的RAM 84,508 byte。
- COM37へUSB自動書き込み成功、4領域のハッシュ検証と通常再起動を確認。
  USB INFOでFW 0.2.0、PSRAM 8 MB、`NEXT=0 TXOE=0` を取得。
- Espressif IDF v5.4のGPIOドライバも確認し、ODを設定してからUART出力を接続する順序にした。
  `gpio_set_direction()` のGPIO Matrix再設定と、OD有効化前の出力を避ける。
- 物理配線の追加、実ボタンによるSTM32 IDX切り替え、送信波形、並列送信機との電気的確認は未実施。

### FW 0.2.1追加分

- RXをGPIO44からGPIO18へ変更。TXはGPIO17のまま。
- ビルド・COM37への書き込み・4領域のハッシュ検証・自動再起動が成功。
  USB INFOでFW 0.2.1、`NEXT=0 TXOE=0` を取得。
- GPIO18への配線変更と実UART受信、STM32ログ送信中のROM書き込みは未確認。
