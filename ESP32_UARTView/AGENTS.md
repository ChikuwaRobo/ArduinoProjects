# 作業方針

- 応答・仕様文書は日本語、ファイルはUTF-8で保存する。
- STM32側のプロジェクトは参照元。ESP32の整備に伴って変更しない。
- 基板はWaveshare ESP32-S3-LCD-1.69。Touch版や別サイズのピン設定を混用しない。
- UART受信は描画から独立させる。ANSI属性を文字ごとに保持する。
- 仕様変更はREADMEとdoc/design.mdに反映し、実装済み・ビルド済み・実機確認済みを区別する。
- 解析や折り返しの変更はtests/terminal_test.cppを実行し、スケッチ変更はtools/build.ps1でビルドする。
- 実機への書き込みは対象ポートとユーザーの指示を確認して行う。
- PowerShellスクリプトはWindows PowerShell 5.1でも読めるようASCIIにするかUTF-8 BOM付きで保存する。
