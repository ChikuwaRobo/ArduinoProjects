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

## 実機での確認手順

1. `Config.h` の `demo = true` でビルドしてUSBから書き込む。
2. 画面位置・色・太字・下線・反転・折り返し・ちらつきを確認する。
3. `demo = false` に戻して再ビルド・書き込みする。
4. 電源を切ってSTM32 TXとESP32 GPIO44、GNDを接続し、USB給電で起動する。
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
