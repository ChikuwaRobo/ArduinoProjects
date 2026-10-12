// STM32のデバッグUARTを受信し、ANSI属性付きの1行をLCDの1画面として表示する。
// 受信タスクと描画を分離し、完成行の最新値だけを共有する。
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <driver/gpio.h>
#include <driver/uart.h>
#include <esp_rom_gpio.h>
#include <esp_cpu.h>
#include <soc/gpio_sig_map.h>
#include <soc/gpio_struct.h>
#include "Config.h"
#include "Terminal.h"
#include "Button.h"

#if ARDUINO_USB_MODE != 1 || ARDUINO_USB_CDC_ON_BOOT != 1
#error "Enable Hardware CDC and JTAG and USB CDC On Boot for automatic USB updates"
#endif

HardwareSerial debugUart(1);
uartview::Button nextButton(config::debounceMs);
uint32_t nextRequests = 0;
Arduino_ESP32SPI bus(config::lcdDc, config::lcdCs, config::lcdSck,
                     config::lcdMosi, GFX_NOT_DEFINED);
Arduino_ST7789 display(&bus, config::lcdReset, 0, true, 240, 280, 0, 20, 0, 20);
Arduino_Canvas canvas(240, 280, &display);
uartview::Parser parser;
uartview::Frame latest, shown;
SemaphoreHandle_t frameMutex;
uint32_t uartErrors = 0;
uint32_t uartErrorTypes[UART_PARITY_ERROR + 1] = {};
uint32_t receivedBytes = 0, lastReceiveMs = 0;
bool resync = false;
portMUX_TYPE errorMux = portMUX_INITIALIZER_UNLOCKED;

// USB経由で更新後の起動と実メモリ構成を確認する。UARTログとは独立した管理口。
void usbInfo() {
  uint32_t bytes, errors, receivedAt, sequence;
  uint32_t errorTypes[UART_PARITY_ERROR + 1];
  portENTER_CRITICAL(&errorMux);
  bytes = receivedBytes;
  errors = uartErrors;
  receivedAt = lastReceiveMs;
  memcpy(errorTypes, uartErrorTypes, sizeof(errorTypes));
  portEXIT_CRITICAL(&errorMux);
  xSemaphoreTake(frameMutex, portMAX_DELAY);
  sequence = latest.sequence;
  xSemaphoreGive(frameMutex);
  Serial.printf("ESP32_UARTView FW=%s BUILD=%s_%s FLASH=%lu PSRAM=%lu UPTIME=%lu NEXT=%lu TXOE=%u RXBYTES=%lu FRAMES=%lu SHOWN=%lu RXERR=%lu RXAGE=%lu RXLEVEL=%d TXLEVEL=%d BAUD=%lu RXMAP=%u BREAKERR=%lu BUFERR=%lu FIFOERR=%lu FRAMEERR=%lu PARITYERR=%lu\n",
                config::firmwareVersion, __DATE__, __TIME__,
                static_cast<unsigned long>(ESP.getFlashChipSize()),
                static_cast<unsigned long>(ESP.getPsramSize()),
                static_cast<unsigned long>(millis()),
                static_cast<unsigned long>(nextRequests),
                unsigned((GPIO.enable >> config::tx) & 1U),
                static_cast<unsigned long>(bytes), static_cast<unsigned long>(sequence),
                static_cast<unsigned long>(shown.sequence), static_cast<unsigned long>(errors),
                static_cast<unsigned long>(bytes ? millis() - receivedAt : UINT32_MAX),
                gpio_get_level(static_cast<gpio_num_t>(config::rx)),
                gpio_get_level(static_cast<gpio_num_t>(config::tx)),
                static_cast<unsigned long>(debugUart.baudRate()),
                unsigned(GPIO.func_in_sel_cfg[U1RXD_IN_IDX].func_sel),
                static_cast<unsigned long>(errorTypes[UART_BREAK_ERROR]),
                static_cast<unsigned long>(errorTypes[UART_BUFFER_FULL_ERROR]),
                static_cast<unsigned long>(errorTypes[UART_FIFO_OVF_ERROR]),
                static_cast<unsigned long>(errorTypes[UART_FRAME_ERROR]),
                static_cast<unsigned long>(errorTypes[UART_PARITY_ERROR]));
}

// 画面へ渡した行をPCから確認する。LCD実画素の読戻しではなく描画元のスナップショット。
void usbSnapshot() {
  size_t styled = 0;
  for (size_t p = 0; p < shown.count; ++p) {
    const auto &s = shown.glyphs[p].style;
    if (s.fg != uartview::palette[7] || s.bg != 0 || s.bold || s.underline || s.inverse) ++styled;
  }
  Serial.printf("SNAP SEQ=%lu CHARS=%u TRUNC=%u STYLED=%u\nTEXT ",
                static_cast<unsigned long>(shown.sequence), unsigned(shown.count),
                unsigned(shown.truncated), unsigned(styled));
  for (size_t p = 0; p < shown.count; ++p) Serial.write(shown.glyphs[p].ch);
  Serial.write('\n');
}

// GPIO17を駆動せずUART内部のTX→RX接続だけで解析・描画経路を確認する。
void usbSelfTest() {
  if (config::demo || ((GPIO.enable >> config::tx) & 1U)) {
    Serial.println("SELFTEST BLOCKED");
    return;
  }
  ESP_ERROR_CHECK(uart_set_loop_back(UART_NUM_1, true));
  const char test[] = "\x1b[0;1;32mUARTVIEW_LOOPBACK_OK\x1b[0m\n";
  debugUart.write(reinterpret_cast<const uint8_t *>(test), sizeof(test) - 1);
  debugUart.flush(true);
  delay(20); // RXタイムアウト割込みと受信タスクが行を確定する時間を確保
  ESP_ERROR_CHECK(uart_set_loop_back(UART_NUM_1, false));
  Serial.println("SELFTEST SENT_INTERNAL");
}

void serviceNextButton(uint32_t now) {
  if (!nextButton.update(digitalRead(config::nextButton) == LOW, now) || config::demo) return;
  const auto txPin = static_cast<gpio_num_t>(config::tx);
  // 出力有効化前にGPIOラッチを解放側に置き、ODを先に設定してからUARTへ接続する。
  // gpio_set_direction()はGPIO Matrixを既定に戻すため、UART接続後には呼ばない。
  ESP_ERROR_CHECK(gpio_set_level(txPin, 1));
  gpio_config_t txConfig = {};
  txConfig.pin_bit_mask = 1ULL << config::tx;
  txConfig.mode = GPIO_MODE_INPUT_OUTPUT_OD;
  txConfig.pull_up_en = GPIO_PULLUP_ENABLE;
  ESP_ERROR_CHECK(gpio_config(&txConfig));
  esp_rom_gpio_connect_out_signal(config::tx, U1TXD_OUT_IDX, false, false);
  debugUart.write(uint8_t('\n')); // CRLFは参考STM32では2回進むためLFだけ送る
  debugUart.flush(true); // ストップビット完了まで待つ。RXバッファは消去しない。
  ESP_ERROR_CHECK(gpio_set_direction(txPin, GPIO_MODE_INPUT));
  ++nextRequests;
}

// RXを駆動せず、20 ms以内の入力エッジ間隔をCPUで採取する。割込みによる測定誤差を含む。
void usbPulse() {
  uint32_t durations[256];
  uint8_t levels[256];
  size_t count = 0;
  const uint32_t mhz = getCpuFrequencyMhz();
  const uint32_t start = esp_cpu_get_cycle_count();
  uint32_t previous = start;
  bool level = gpio_get_level(static_cast<gpio_num_t>(config::rx));
  bool first = true;
  while (count < 256 && uint32_t(esp_cpu_get_cycle_count() - start) < mhz * 20000U) {
    const bool next = config::rx < 32 ? ((GPIO.in >> config::rx) & 1U)
                                   : ((GPIO.in1.val >> (config::rx - 32)) & 1U);
    if (next != level) {
      const uint32_t now = esp_cpu_get_cycle_count();
      if (!first) {
        durations[count] = now - previous;
        levels[count++] = level;
      }
      first = false;
      previous = now;
      level = next;
    }
  }
  Serial.printf("PULSE MHZ=%lu COUNT=%u UNIT=ns\n", static_cast<unsigned long>(mhz), unsigned(count));
  for (size_t i = 0; i < count; ++i)
    Serial.printf("%u:%lu%s", unsigned(levels[i]), static_cast<unsigned long>(uint64_t(durations[i]) * 1000 / mhz), i + 1 == count ? "\n" : " ");
  if (!count) Serial.println("NO_EDGES");
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
      else if (strcmp(command, "SNAP") == 0) usbSnapshot();
      else if (strcmp(command, "SELFTEST") == 0) usbSelfTest();
      else if (strcmp(command, "PULSE") == 0) usbPulse();
      used = 0;
    } else if (used < sizeof(command) - 1) command[used++] = static_cast<char>(byte);
  }
}

void receiveError(hardwareSerial_error_t error) {
  portENTER_CRITICAL(&errorMux);
  ++uartErrors;
  if (error <= UART_PARITY_ERROR) ++uartErrorTypes[error];
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
    uint32_t bytes = 0;
    // 受信が連続してもFreeRTOSに実行時間を返す。バッファはこの間の受信を保持。
    for (size_t n = 0; n < 4096 && debugUart.available(); ++n) {
      int byte = debugUart.read();
      if (byte >= 0) { publishByte(static_cast<uint8_t>(byte)); ++bytes; }
    }
    if (bytes) {
      portENTER_CRITICAL(&errorMux);
      receivedBytes += bytes;
      lastReceiveMs = millis();
      portEXIT_CRITICAL(&errorMux);
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
  // 他の送信機との並列接続を想定し、初期化・待機中は出力を有効にしない。
  const auto txPin = static_cast<gpio_num_t>(config::tx);
  ESP_ERROR_CHECK(gpio_set_direction(txPin, GPIO_MODE_INPUT));
  ESP_ERROR_CHECK(gpio_set_pull_mode(txPin, GPIO_PULLUP_ONLY));
  pinMode(config::nextButton, INPUT_PULLUP);
  nextButton.begin(digitalRead(config::nextButton) == LOW, millis());
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
    // UART1 RXをconfigのGPIOへ割当。TXは自動割当せず、押下時だけGPIO17へ接続する。
    debugUart.setRxBufferSize(config::rxBufferBytes);
    debugUart.begin(config::baud, SERIAL_8N1, config::rx, -1);
    // GPIO18はUART1専用IOMUX経路が選ばれる。入力・PUとGPIO Matrix経路を明示する。
    // UARTドライバのピン所有情報はbegin()のまま維持し、実際の入力接続だけを確定する。
    gpio_config_t rxConfig = {};
    rxConfig.pin_bit_mask = 1ULL << config::rx;
    rxConfig.mode = GPIO_MODE_INPUT;
    rxConfig.pull_up_en = GPIO_PULLUP_ENABLE;
    ESP_ERROR_CHECK(gpio_config(&rxConfig));
    esp_rom_gpio_connect_in_signal(config::rx, U1RXD_IN_IDX, false);
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
  serviceNextButton(now);
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
