// チャタリングを除去し、押下ごとに1回だけイベントを出す。起動時の押しっぱなしは無視。
#pragma once
#include <stdint.h>

namespace uartview {
class Button {
 public:
  explicit Button(uint32_t debounceMs) : debounceMs_(debounceMs) {}
  void begin(bool pressed, uint32_t now) {
    raw_ = stable_ = pressed;
    changedAt_ = now;
  }
  bool update(bool pressed, uint32_t now) {
    if (pressed != raw_) { raw_ = pressed; changedAt_ = now; }
    if (stable_ == raw_ || uint32_t(now - changedAt_) < debounceMs_) return false;
    stable_ = raw_;
    return stable_;
  }
 private:
  uint32_t debounceMs_, changedAt_ = 0;
  bool raw_ = false, stable_ = false;
};
}
