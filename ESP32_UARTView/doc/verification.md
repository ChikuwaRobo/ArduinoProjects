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
4. 電源を切ってSTM32 TXとConfig.h指定のRX（現在GPIO18）、GNDを接続し、USB給電で起動する。
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

## 検証状態（2026-10-12、FW 0.2.8）

| 項目 | 確認結果 |
| --- | --- |
| ANSI解析・折り返し | PCテスト成功 |
| ボタン処理 | チャタリング、長押し、再押下、起動時押下、時刻桁あふれのPCテスト成功 |
| USB更新判定 | 対象選択と書き込み失敗判定のテスト成功 |
| ESP32ビルド | esp32 core 3.2.0、GFX Library for Arduino 1.6.3で成功 |
| メモリ使用量 | Flash 417,994 byte、静的RAM 84,540 byte。実行時確保分は含まない |
| 実機 | ESP32-S3 revision v0.2、Flash 16 MB、PSRAM 8 MB |
| USB | COM37、VID=303A/PID=1001、シリアル `1C:DB:D4:79:78:94` |
| 通常更新 | ボタン操作なしの書き込み、4領域のハッシュ検証、自動再起動、INFOによる起動確認に成功 |
| 内部ループバック | ANSI付き34 byteを受信し、UART通知エラー0。緑・太字の20文字をLCD上で確認 |
| STM32接続 | G474_Orion_main、PA2 TX→GPIO18、PA3 RX←GPIO17、GND直接接続。他機器のTX/RX接続なし |
| 継続受信 | 約61.8秒で951,060 byte・3,861完成行。UART通知エラーは全種類0 |
| 表示 | STM32ログと文字属性を確認。ユーザーによる画面の目視確認済み |
| BOOT操作 | 送信回数の増加とSTM32のMOUSE→VEL thetaページ変更を確認 |
| TX待機状態 | 各観測時点でTXOE=0 |
| ログ保存 | `doc/measurements/`へ保存し、Gitのignore対象とする |

## 残る確認事項

- 一部の行に文字化け・文字欠落が残る。UART通知エラー0は無誤りを保証しない。
- 1押下につき1IDXの厳密な照合、LF送信波形、PUの立ち上がり時間は未測定。
- 実機は外部PUなしでSTM32内蔵PUを利用し、ページ変更が動作した。外部PUを備えた設計条件での波形と通信品質は未検証。
- 並列送信機、長い配線、別電源条件、バッテリーでの電源保持は未検証。
- 画面上限・行上限からの復帰、下線・反転、ちらつきの網羅的な実機確認は未実施。
- `4WS_MainFW`はデバッグ出力・IDX処理が未実装のため、結合動作は未確認。
