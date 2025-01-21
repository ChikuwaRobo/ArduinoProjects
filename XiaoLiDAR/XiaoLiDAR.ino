/*
  AnalogReadSerial

  Reads an analog input on pin 0, prints the result to the Serial Monitor.
  Graphical representation is available using Serial Plotter (Tools > Serial Plotter menu).
  Attach the center pin of a potentiometer to pin A0, and the outside pins to +5V and ground.

  This example code is in the public domain.

  https://www.arduino.cc/en/Tutorial/BuiltInExamples/AnalogReadSerial
*/

// the setup routine runs once when you press reset:
void setup()
{
  // initialize serial communication at 9600 bits per second:
  Serial.begin(9600);
  Serial0.begin(230400);
}

// the loop routine runs over and over again forever:
void loop()
{
  static uint8_t header_cnt = 0;
  if (Serial0.available())
  {
    uint8_t tmp = Serial0.read();
    if (tmp == 0x55 && header_cnt == 0)
    {
      header_cnt++;
    }
    if (tmp == 0xaa && header_cnt == 1)
    {

      header_cnt = 0;
      Serial.printf("\n");
    }
    Serial.printf("0x%02x ", tmp);
  }
}
