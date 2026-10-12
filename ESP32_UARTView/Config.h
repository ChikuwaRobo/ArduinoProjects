// Waveshare基板の配線と表示・受信の設定。STM32側の設定に合わせて変更する。
#pragma once
#include <stddef.h>
#include <stdint.h>
namespace config {
constexpr const char *firmwareVersion = "0.2.1";
constexpr uint32_t baud = 2000000;
// ROMのUART0 RX（GPIO44）へSTM32のログを入れないため、外部GPIO端子を使う。
constexpr int rx = 18;
// GPIO43はROMのUART0 TX。起動中の競合を避けるため外部GPIO端子を使う。
constexpr int tx = 17;
constexpr int nextButton = 0; // BOOT、押下時LOW
constexpr uint32_t debounceMs = 25;
constexpr int lcdDc = 4, lcdCs = 5, lcdSck = 6, lcdMosi = 7;
constexpr int lcdReset = 8, lcdBacklight = 15;
constexpr int powerEnable = 41;
constexpr uint32_t refreshMs = 50; // 最大20fps、受信は別タスクで継続
constexpr size_t rxBufferBytes = 16384;
constexpr bool demo = false; // trueならSTM32なしで色・折り返しを確認
}
