// STM32のデバッグUARTを受信し、ANSI属性付きの1行をLCDの1画面として表示する。
// 受信タスクと描画を分離し、完成行の最新値だけを共有する。
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "Config.h"
#include "Terminal.h"

#if ARDUINO_USB_MODE != 1 || ARDUINO_USB_CDC_ON_BOOT != 1
#error "Enable Hardware CDC and JTAG and USB CDC On Boot for automatic USB updates"
#endif

HardwareSerial debugUart(1);
Arduino_ESP32SPI bus(config::lcdDc, config::lcdCs, config::lcdSck,
                     config::lcdMosi, GFX_NOT_DEFINED);
Arduino_ST7789 display(&bus, config::lcdReset, 0, true, 240, 280, 0, 20, 0, 20);
Arduino_Canvas canvas(240, 280, &display);
uartview::Parser parser;
uartview::Frame latest, shown;
SemaphoreHandle_t frameMutex;
uint32_t uartErrors = 0;
bool resync = false;
portMUX_TYPE errorMux = portMUX_INITIALIZER_UNLOCKED;

// USB経由で更新後の起動と実メモリ構成を確認する。UARTログとは独立した管理口。
void usbInfo() {
  Serial.printf("ESP32_UARTView FW=%s BUILD=%s_%s FLASH=%lu PSRAM=%lu UPTIME=%lu\n",
                config::firmwareVersion, __DATE__, __TIME__,
                static_cast<unsigned long>(ESP.getFlashChipSize()),
                static_cast<unsigned long>(ESP.getPsramSize()),
                static_cast<unsigned long>(millis()));
}

void serviceUsb() {
  static char command[16];
  static size_t used = 0;
  // 巨大入力で描画・受信を占有しない。CDC接続を待たずに動作する。
  for (size_t n = 0; n < 64 && Serial.available(); ++n) {
    int byte = Serial.read();
    if (byte == '\r') continue;
    if (byte == '\n') {
      command[used] = 0;
      if (strcmp(command, "INFO") == 0) usbInfo();
      used = 0;
    } else if (used < sizeof(command) - 1) command[used++] = static_cast<char>(byte);
  }
}

void receiveError(hardwareSerial_error_t) {
  portENTER_CRITICAL(&errorMux);
  ++uartErrors;
  resync = true;
  portEXIT_CRITICAL(&errorMux);
}

void publishByte(uint8_t byte) {
  bool discard;
  portENTER_CRITICAL(&errorMux);
  discard = resync;
  if (discard && byte == '\n') resync = false;
  portEXIT_CRITICAL(&errorMux);
  if (discard) { parser.reset(); return; }
  if (parser.feed(byte)) {
    xSemaphoreTake(frameMutex, portMAX_DELAY);
    latest = parser.line();
    xSemaphoreGive(frameMutex);
    parser.nextLine();
  }
}

void receiveTask(void *) {
  for (;;) {
    // 受信が連続してもFreeRTOSに実行時間を返す。バッファはこの間の受信を保持。
    for (size_t n = 0; n < 4096 && debugUart.available(); ++n) {
      int byte = debugUart.read();
      if (byte >= 0) publishByte(static_cast<uint8_t>(byte));
    }
    vTaskDelay(1);
  }
}

void renderFrame(uint32_t errors) {
  constexpr int left = 8, top = 28, width = 224, height = 232;
  uartview::Row rows[29];
  int scale = 2;
  size_t count = uartview::wrap(shown, width / 12, rows, 29);
  if (count > height / 16) {
    scale = 1;
    count = uartview::wrap(shown, width / 6, rows, 29);
  }
  const size_t maxRows = height / (8 * scale);
  bool clipped = shown.truncated || count > maxRows;
  canvas.fillScreen(0);
  canvas.setTextSize(1);
  canvas.setTextColor(0x9fff);
  canvas.setCursor(8, 12);
  canvas.printf("UART 2M  #%lu", static_cast<unsigned long>(shown.sequence));
  canvas.setTextSize(scale);
  for (size_t r = 0; r < count && r < maxRows; ++r) {
    int x = left, y = top + r * 8 * scale;
    for (size_t p = rows[r].start; p < rows[r].end; ++p) {
      const auto &g = shown.glyphs[p];
      uint16_t fg = g.style.inverse ? g.style.bg : g.style.fg;
      uint16_t bg = g.style.inverse ? g.style.fg : g.style.bg;
      canvas.fillRect(x, y, 6 * scale, 8 * scale, bg);
      canvas.setTextColor(fg);
      canvas.setCursor(x, y);
      canvas.write(g.ch);
      if (g.style.bold) {
        canvas.setCursor(x + 1, y);
        canvas.write(g.ch);
      }
      if (g.style.underline) canvas.drawFastHLine(x, y + 7 * scale, 6 * scale, fg);
      x += 6 * scale;
    }
  }
  canvas.setTextSize(1);
  canvas.setTextColor(clipped || errors ? 0xf986 : 0x8410);
  canvas.setCursor(8, 264);
  canvas.printf("%s RXerr:%lu", clipped ? "TRUNC" : "LIVE ", static_cast<unsigned long>(errors));
  canvas.flush();
}

void setup() {
  // Hardware CDCを維持し、USBの自動書き込みモード移行を可能にする。
  Serial.begin(115200);
  pinMode(config::powerEnable, OUTPUT);
  digitalWrite(config::powerEnable, HIGH);
  pinMode(config::lcdBacklight, OUTPUT);
  digitalWrite(config::lcdBacklight, LOW);
  frameMutex = xSemaphoreCreateMutex();
  if (!frameMutex || !canvas.begin(40000000)) {
    for (;;) delay(1000);
  }
  canvas.setTextWrap(false);
  canvas.fillScreen(0);
  canvas.setTextSize(2);
  canvas.setTextColor(0xdedb);
  canvas.setCursor(16, 110);
  canvas.print("Waiting UART");
  canvas.flush();
  digitalWrite(config::lcdBacklight, HIGH);
  if (!config::demo) {
    // UART1をGPIO44へ割り当てる。TXは既定ピンにも出さず無効化。
    debugUart.setRxBufferSize(config::rxBufferBytes);
    debugUart.begin(config::baud, SERIAL_8N1, config::rx, -1);
    debugUart.setPins(config::rx, -1);
    debugUart.onReceiveError(receiveError);
    if (xTaskCreatePinnedToCore(receiveTask, "uart-rx", 4096, nullptr, 2, nullptr, 0) != pdPASS) {
      canvas.fillScreen(0);
      canvas.setCursor(16, 110);
      canvas.print("RX task failed");
      canvas.flush();
      for (;;) delay(1000);
    }
  }
}

void loop() {
  serviceUsb();
  static uint32_t lastDraw = 0, lastDemo = 0, lastErrors = 0;
  uint32_t now = millis();
  if (config::demo && now - lastDemo >= 1000) {
    lastDemo = now;
    const char *sample = "\x1b[0mVEL theta +123.4 \x1b[1;32mOK\x1b[0m accLc X +12.3, Y -4.5, "
      "\x1b[33mKP 1.2 KD 0.3\x1b[0m Tar +10.0 +20.0 +30.0 +40.0 "
      "\x1b[36mReal +9.8 +19.9 +30.1 +39.8\x1b[0m "
      "\x1b[4;31mUNDER_VOLTAGE\x1b[0m \x1b[7mCHECK\x1b[0m loop 123456\n";
    while (*sample) publishByte(static_cast<uint8_t>(*sample++));
  }
  if (now - lastDraw >= config::refreshMs) {
    lastDraw = now;
    uint32_t errors;
    portENTER_CRITICAL(&errorMux);
    errors = uartErrors;
    portEXIT_CRITICAL(&errorMux);
    xSemaphoreTake(frameMutex, portMAX_DELAY);
    bool changed = latest.sequence != shown.sequence;
    if (changed) shown = latest;
    xSemaphoreGive(frameMutex);
    if (changed || errors != lastErrors) renderFrame(errors);
    lastErrors = errors;
  }
  delay(1);
}
