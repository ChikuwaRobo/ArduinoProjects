#include <SparkFunAutoDriver.h>
#include <SPI.h>

#define PIN_LED

#define PIN_BUSY 9
#define PIN_RESET 8
#define PIN_SPI_CS 10

AutoDriver board(PIN_SPI_CS, PIN_RESET, PIN_BUSY);

void setup()
{

  pinMode(PIN_SPI_CS, OUTPUT);
  pinMode(PIN_RESET, OUTPUT);
  pinMode(PIN_BUSY, OUTPUT);
  Serial.begin(115200);

  board.setAcc(10);
  board.setDec(20);
  board.setFullSpeed(10);
}

void loop()
{
}