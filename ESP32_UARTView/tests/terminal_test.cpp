// 実際のANSI行、分割受信、異常入力と折り返し境界をPC上で検証する。
#include "../Terminal.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
using namespace uartview;
static Parser parser;
static bool send(const char *text) {
  bool done = false;
  while (*text) done = parser.feed(static_cast<uint8_t>(*text++)) || done;
  return done;
}
int main() {
  assert(!send("\x1b[1;3"));
  assert(send("2mOK\x1b[0m VEL\r\n"));
  assert(parser.line().count == 6);
  assert(parser.line().glyphs[0].style.bold);
  assert(parser.line().glyphs[0].style.fg == palette[2]);
  assert(!parser.line().glyphs[2].style.bold);
  parser.nextLine();
  assert(!send("\n\r\n"));
  assert(send("\x1b[33mA\n"));
  parser.nextLine();
  assert(send("B\x1b[4;7mC\x1b[24;27;39mD\n"));
  assert(parser.line().glyphs[0].style.fg == palette[3]);
  assert(parser.line().glyphs[1].style.underline && parser.line().glyphs[1].style.inverse);
  assert(!parser.line().glyphs[2].style.underline && !parser.line().glyphs[2].style.inverse);
  assert(parser.line().glyphs[2].style.fg == palette[7]);
  parser.reset();
  assert(send("\x1b[4A\x1b]0;hidden\aX\x1b]hidden\x1b\\Y\n"));
  assert(parser.line().count == 2);
  parser.reset();
  assert(send("\x1b[38;2;1;4;7mX\n"));
  assert(!parser.line().glyphs[0].style.bold && !parser.line().glyphs[0].style.underline);
  parser.reset();
  send("\x1b[");
  for (int i = 0; i < 100; ++i) parser.feed('1');
  assert(send("mZ\n"));
  assert(parser.line().count == 1 && !parser.line().glyphs[0].style.bold);
  parser.reset();
  for (size_t i = 0; i < maxGlyphs + 100; ++i) parser.feed('x');
  assert(parser.feed('\n') && parser.line().truncated && parser.line().count == maxGlyphs);
  parser.nextLine();
  assert(send("OK\n") && !parser.line().truncated && parser.line().count == 2);
  parser.reset();
  assert(send("VEL X +12.3, Y -4.5 / loop 12345678901234567890\n"));
  Row rows[64];
  size_t count = wrap(parser.line(), 12, rows, 64), cursor = 0;
  for (size_t i = 0; i < count; ++i) {
    assert(rows[i].start == cursor);
    assert(rows[i].end > rows[i].start && rows[i].end - rows[i].start <= 12);
    cursor = rows[i].end;
  }
  assert(cursor == parser.line().count);
  assert(wrap(parser.line(), 12, rows, 1) == count);
  assert(wrap(parser.line(), 0, rows, 64) == 0);
  puts("terminal tests passed");
}
