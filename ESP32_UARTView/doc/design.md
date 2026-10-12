# UART表示仕様

## STM32側の確認結果

参照元: `C:\Users\hiroyuki\STM32CubeIDE\workspace_1.17.0\G474_Orion_main`。

- `Core/Src/usart.c`: LPUART1、2 Mbps、8N1、PA2 TX / PA3 RX。
- `Core/Src/main.c`: `__io_putchar()` はLPUART1へ送信。
- 周期デバッグは `p()` で約2,000 byteのバッファに追記し、LFを付けて `HAL_UART_Transmit_DMA()`。
- `setTextRed/Yellow/Green/Magenta/Cyan/Normal/Bold()` はSGRを出力する。
- 通常ページは1行に複数フィールド。DRIVE_LOGはCSVで長い行を出力する。
- ページ切り替え時は名前の一覧も別の行として送信される。
- TUI試作にはカーソル移動 `ESC[4A` がある。本ビューアでは無視する。

接続先: `C:\Users\hiroyuki\Documents\CubeProjects\4WS_MainFW`。
`Core/Src/usart.c` にLPUART1（PC1 TX / PC0 RX）、UART4（PC10 TX / PC11 RX）があり、どちらも2 Mbps。
デバッグ送信先の実装はまだないため、LPUART1を使用する前提をESP32側の初期設定とする。
将来STM32が別のUART・速度を使う場合は配線と `Config.h` を合わせる。
STM32プロジェクトには変更を加えていない。

## LCD

ST7789V2、240×280、縦置き。SPI DC=4、CS=5、SCK=6、MOSI=7、RST=8、BL=15。
ST7789の240×320 RAMに対して縦オフセット20を指定する。
電源保持GPIO41をHIGHにする。USB給電を最初の確認条件とし、バッテリー電源や電源ボタン操作は実機確認対象。
タッチ、RTC、IMU、ブザーは使用しない。

USB Serial/JTAGを維持してFWを自動更新する。GPIO19/20はUSB専用とし、USBへ `INFO` + LFを送ると
FW版・ビルド日時・メモリ容量・稼働時間を返す。USB待ちによる起動停止は行わない。
初回・通常更新・復旧の仕様は [firmware-update.md](firmware-update.md) を参照。

RGB565キャンバス（134,400 byte）に全画面を描いてからSPI転送する。
丸角を避けて文字領域を左右8 px内側に配置する。
本文領域は224×232 px、ヘッダーとフッターは等倍文字。
文字はArduino_GFXの標準ASCII固定幅フォント。日本語・UTF-8は未対応で、非ASCII byteは `?` に置換する。

## 行と属性

1. LFで非空行を確定。CRは無視し、CRLF・LFCRも扱う。空行は表示を消さない。
2. ANSI属性は改行を越えて維持し、SGR 0でリセットする。
3. タブは4文字のタブストップまで空白を追加。BSは直前の文字を削除する。
4. 改行なしの途中行は表示しない。送信停止時は最後の完成行が残る。
5. 行バッファを超えた文字はLFまで捨てるが、その間のSGRは解析する。

| SGR | 動作 |
| --- | --- |
| 0 / 空パラメータ | 全属性リセット |
| 1 / 22 | 太字ON / OFF（1 pxずらして重ね描き） |
| 4 / 24 | 下線ON / OFF |
| 7 / 27 | 前景・背景の反転ON / OFF |
| 30–37 / 90–97 / 39 | 前景8色 / 明色8色 / デフォルト |
| 40–47 / 100–107 / 49 | 背景8色 / 明色8色 / デフォルト |

色はLCD上の可読性を考えたRGB565パレット。SGR 2/3/5/8/9、256色、TrueColorは未対応。
未対応SGRは無視し、38/48の色引数は他の属性として解釈しない。
カーソル移動・画面消去等のCSIは消費して無視する。OSCはBELまたはESC+バックスラッシュまで消費する。
CSIは最大63 byteで、それ以上は終端まで破棄する。壊れた制御列はLFでも解析状態を戻す。
端末画面の再現ではなく、受信行のスナップショットを表示する仕様。

## 負荷とデータ欠落

UART1のRXをGPIO18へ割り当てる。TXはボタン操作時だけGPIO17へ接続する。
GPIO44（U0RXD）は起動ストラップではなく、通常起動時にログ入力で起動モードが変わるピンではない。
ただしROM書き込みモードではUART0の受信端子となるため、STM32の連続ログをROMへ入力しない構成とする。
FW 0.2.1以降は基板のGPIO18端子へ配線し、U0RXDは使用しない。USB更新はGPIO19/20で継続する。
UART受信バッファ16 KiB、専用受信タスク（core 0、優先度2）で解析。
最新の完成行をmutexで描画側へ渡す。描画はArduino loop（通常core 1）で最大20fps。
描画中もUARTを読み進めるが、表示周期の間に完成した中間行は画面に出ない。
したがって各受信行が必ず1回表示される保証はない。高速デバッグの最新状態を見るための設計。
複数行の起動ログやエラーも後続行が来れば置き換わり、履歴やエラー保持は行わない。

2 Mbpsは8N1で最大約200 kbyte/s。16 KiBは約82 ms相当。
受信タスクは4,096 byteごとに1 tick待ってOSへ実行時間を返す。
ドライバのFIFO/リングバッファ欠落やフレーミング等の通知を `RXerr` で示す。
異常後は進行中の行とANSI状態をリセットし、次のLFまで捨てる。
チェックサムのない文字列のため、通知されない欠落・誤りまで検知する保証はない。
画面容量超過は本文の先頭を表示し `TRUNC` を出す。スクロールやページ送りは初期版では実装しない。

## ボタンと送信線（FW 0.2.0）

BOOT（GPIO0、LOWで押下）で次のIDXを要求する。PWRの割当はない。
25 msの状態安定後に押下イベントを1回出し、長押しでは繰り返さない。
起動時に押されていた状態ではイベントを出さず、放した後の再押下を受け付ける。
起動・リセット時にBOOTを押すとROM書き込みモードに入る従来のHW機能は維持する。

参考FWのLPUART1受信処理はLFとCRのそれぞれで `setPrintPage(debug.print_idx + 1)` を呼ぶ。
二重進行を避けるため、LF（0x0A）だけを2 Mbps/8N1で送る。CRLFは送らない。
1 byteの線上時間は5 µsで、IDXの実状態はSTM32側に任せる。ESP32にはIDX値を保持しない。
`4WS_MainFW`の受信処理は変更していないので、同FWでのIDX切り替えは未実装。

送信ピンは基板のGPIO17端子を使う。GPIO43の「TX」端子はROMのUART0出力があり、
リセット・FW更新中まで出力解放を保証できないため使わない。
通常はGPIO17を `GPIO_MODE_INPUT`、`GPIO_PULLUP_ONLY` とする。
送信時だけGPIOラッチを1（ODでの解放側）に置き、`gpio_config()` でODを出力有効化より先に設定する。
その後GPIO MatrixでUART1 TXを接続する。
`gpio_set_direction()` は出力有効化時にもGPIO Matrixを既定へ戻すため、UART接続後には呼ばない。
UARTのストップビット完了まで `flush(true)` で待ち、入力モードへ戻してUART出力を切り離す。
Arduino HALの `flush(true)` はUART TX idleを待ち、RXバッファは破棄しない。
HIGHは送信中も出力トランジスタで駆動せず、PUで作る。
設定根拠: [Espressif GPIO API](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/gpio.html)。

GPIO17→STM32 RXの共有線を3.3Vへ1～2.2 kΩで外部プルアップする。
この抵抗値は初期設計値で、配線容量と実測立ち上がり時間により調整する。
2 Mbpsの1 bitは0.5 µsのため、内部PUのみで受信品質は保証しない。
他の送信機も待機中は高インピーダンスまたはODのHIGH（解放）とする。
同時送信の調停・衝突検出・自動再送はない。外部機器側が常時プッシュプルHIGHの場合は、
送信出力の三態化や絶縁用回路が別途必要で、このFWだけでは並列接続の競合を防げない。

USB INFOの `NEXT` は実際にLFを送った回数（STM32受信成功数ではない）。
`TXOE` はGPIO出力有効レジスタから読む待機時の診断値で、0が正常。
このレジスタ確認は外部回路・波形の実測の代わりにはならない。
