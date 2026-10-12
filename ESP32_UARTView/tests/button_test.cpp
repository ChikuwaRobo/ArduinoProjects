// 押下バウンス、長押し、再押下、起動時押下、millisの桁あふれを検証する。
#include "../Button.h"
#include <assert.h>
#include <stdio.h>

int main() {
  uartview::Button button(25);
  button.begin(false, 0);
  assert(!button.update(true, 10));
  assert(!button.update(false, 15));
  assert(!button.update(true, 20));
  assert(!button.update(true, 44));
  assert(button.update(true, 45));
  assert(!button.update(true, 1000));
  assert(!button.update(false, 1010));
  assert(!button.update(false, 1035));
  assert(!button.update(true, 1040));
  assert(button.update(true, 1065));
  button.begin(true, 2000);
  assert(!button.update(true, 2100));
  assert(!button.update(false, 2200));
  assert(!button.update(false, 2225));
  assert(!button.update(true, 2300));
  assert(button.update(true, 2325));
  button.begin(false, 0xfffffff0U);
  assert(!button.update(true, 0xfffffff8U));
  assert(!button.update(true, 0x10U));
  assert(button.update(true, 0x11U));
  puts("button tests passed");
}
