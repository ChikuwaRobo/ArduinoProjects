// UARTバイト列をANSI属性付きの完成行へ変換し、画面幅に応じて折り返す。
// Arduinoに依存させず、分割受信や破損入力をPC上でも検証する。
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace uartview {
constexpr size_t maxGlyphs = 2048;
constexpr uint16_t palette[16] = {
    0x0000, 0xf986, 0x3666, 0xff46, 0x5b7f, 0xe35f, 0x3fff, 0xdedb,
    0x8410, 0xfbae, 0x77ef, 0xffef, 0x9cff, 0xfbbf, 0x9fff, 0xffff};
struct Style {
  uint16_t fg = palette[7], bg = palette[0];
  bool bold = false, underline = false, inverse = false;
};
struct Glyph { char ch; Style style; };
struct Frame {
  Glyph glyphs[maxGlyphs];
  size_t count = 0;
  bool truncated = false;
  uint32_t sequence = 0;
};

class Parser {
 public:
  // LFで確定。CRはCRLF/LFCRのどちらでも重複行を作らない。
  bool feed(uint8_t byte) {
    if (byte == '\n') {
      state_ = State::Text;
      if (!line_.count && !line_.truncated) return false;
      line_.sequence = ++sequence_;
      return true;
    }
    if (byte == '\r') return false;
    if (byte == 0x1b) {
      if (state_ == State::Osc) state_ = State::OscEscape;
      else { state_ = State::Escape; used_ = 0; }
      return false;
    }
    if (state_ == State::OscEscape) {
      state_ = byte == '\\' ? State::Text : State::Osc;
      return false;
    }
    if (state_ == State::Osc) {
      if (byte == 7) state_ = State::Text;
      return false;
    }
    if (state_ == State::Escape) {
      state_ = byte == '[' ? State::Csi : byte == ']' ? State::Osc : State::Text;
      return false;
    }
    if (state_ == State::Csi || state_ == State::DiscardCsi) {
      if (byte >= 0x40 && byte <= 0x7e) {
        if (state_ == State::Csi && byte == 'm') applySgr();
        state_ = State::Text;
      } else if (state_ == State::Csi) {
        if (used_ < sizeof(csi_) - 1) csi_[used_++] = static_cast<char>(byte);
        else state_ = State::DiscardCsi;
      }
      return false;
    }
    if (byte == '\b') { if (line_.count) --line_.count; }
    else if (byte == '\t') {
      size_t spaces = 4 - line_.count % 4;
      while (spaces--) append(' ');
    } else if (byte >= 32 && byte <= 126) append(static_cast<char>(byte));
    else if (byte >= 128) append('?'); // 参考FWはASCII。UTF-8フォントは未対応
    return false;
  }
  const Frame &line() const { return line_; }
  void nextLine() { line_.count = 0; line_.truncated = false; }
  // UART欠落時は行の残りを捨て、次のLFから再同期する側で呼び出す。
  void reset() { nextLine(); style_ = Style{}; state_ = State::Text; used_ = 0; }
 private:
  enum class State { Text, Escape, Csi, DiscardCsi, Osc, OscEscape };
  State state_ = State::Text;
  Style style_;
  Frame line_;
  uint32_t sequence_ = 0;
  char csi_[64] = {};
  size_t used_ = 0;
  void append(char ch) {
    if (line_.count < maxGlyphs) line_.glyphs[line_.count++] = Glyph{ch, style_};
    else line_.truncated = true;
  }
  void applySgr() {
    unsigned values[24] = {}, n = 1;
    for (size_t i = 0; i < used_; ++i) {
      if (csi_[i] == ';') { if (n == 24) return; ++n; }
      else if (csi_[i] >= '0' && csi_[i] <= '9') {
        if (values[n - 1] > 999) return;
        values[n - 1] = values[n - 1] * 10 + csi_[i] - '0';
      } else return;
    }
    for (unsigned i = 0; i < n; ++i) {
      const unsigned v = values[i];
      // 拡張色の引数を太字等として誤解釈しない。拡張色自体は未対応。
      if (v == 38 || v == 48) {
        if (i + 1 >= n) break;
        unsigned mode = values[++i];
        if (mode == 5) { if (i + 1 >= n) break; ++i; }
        else if (mode == 2) { if (i + 3 >= n) break; i += 3; }
        else break;
      } else if (v == 0) style_ = Style{};
      else if (v == 1) style_.bold = true;
      else if (v == 22) style_.bold = false;
      else if (v == 4) style_.underline = true;
      else if (v == 24) style_.underline = false;
      else if (v == 7) style_.inverse = true;
      else if (v == 27) style_.inverse = false;
      else if (v == 39) style_.fg = palette[7];
      else if (v == 49) style_.bg = palette[0];
      else if (v >= 30 && v <= 37) style_.fg = palette[v - 30];
      else if (v >= 40 && v <= 47) style_.bg = palette[v - 40];
      else if (v >= 90 && v <= 97) style_.fg = palette[v - 90 + 8];
      else if (v >= 100 && v <= 107) style_.bg = palette[v - 100 + 8];
    }
  }
};

struct Row { size_t start, end; };
// 空白・CSVカンマ・スラッシュの後を優先。表示内容と各文字の属性は保持。
inline size_t wrap(const Frame &frame, size_t columns, Row *rows, size_t capacity) {
  if (!columns) return 0;
  size_t start = 0, count = 0;
  while (start < frame.count) {
    size_t end = start + columns;
    if (end > frame.count) end = frame.count;
    if (end < frame.count) {
      for (size_t p = end; p > start + columns / 2; --p) {
        char ch = frame.glyphs[p - 1].ch;
        if (ch == ' ' || ch == ',' || ch == '/') { end = p; break; }
      }
    }
    if (count < capacity) rows[count] = Row{start, end};
    ++count;
    start = end;
  }
  return count;
}
}
