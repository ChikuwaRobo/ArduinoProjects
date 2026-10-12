// Waveshare基板の配線と表示・受信の設定。STM32側の設定に合わせて変更する。
#pragma once
#include <stddef.h>
#include <stdint.h>
namespace config {
constexpr const char *firmwareVersion = "0.1.0";
constexpr uint32_t baud = 2000000;
constexpr int rx = 44;
constexpr int lcdDc = 4, lcdCs = 5, lcdSck = 6, lcdMosi = 7;
constexpr int lcdReset = 8, lcdBacklight = 15;
constexpr int powerEnable = 41;
constexpr uint32_t refreshMs = 50; // 最大20fps、受信は別タスクで継続
constexpr size_t rxBufferBytes = 16384;
constexpr bool demo = false; // trueならSTM32なしで色・折り返しを確認
}
