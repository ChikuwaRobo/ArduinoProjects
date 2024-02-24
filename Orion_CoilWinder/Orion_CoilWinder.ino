#include <SparkFunAutoDriver.h>
#include <SPI.h>

#define PIN_SW 2
#define PIN_POT A0

#define PIN_STEPPER_BUSY 9
#define PIN_STEPPER_RESET 8
#define PIN_STEPPER_SPI_CS 10

// AutoDriver(int position, int CSPin, int resetPin, int busyPin);
// positionを0にしないとメモリぶっ壊される
AutoDriver driver(0, PIN_STEPPER_SPI_CS, PIN_STEPPER_RESET, PIN_STEPPER_BUSY);

void setup()
{

  pinMode(PIN_STEPPER_SPI_CS, OUTPUT);
  pinMode(PIN_STEPPER_RESET, OUTPUT);
  pinMode(PIN_STEPPER_BUSY, OUTPUT);

  pinMode(PIN_SW, INPUT_PULLUP);
  pinMode(PIN_POT, INPUT);

  SPI.begin();
  SPI.setDataMode(SPI_MODE3);

  // select Stepper driver
  digitalWrite(PIN_STEPPER_SPI_CS, HIGH);

  // リセット
  digitalWrite(PIN_STEPPER_RESET, LOW);
  delay(1);
  digitalWrite(PIN_STEPPER_RESET, HIGH);

  Serial.begin(115200);
  Serial.println("start");
  delay(1);

  driver.SPIPortConnect(&SPI);

  driver.configSyncPin(BUSY_PIN, 0);

  driver.configStepMode(STEP_FS);
  driver.setAcc(1000); // 実際の負荷としてはもっと高くできるが､使用感としてこれぐらいがいい｡脱調対策にもなる
  driver.setDec(10000);
  driver.setFullSpeed(10000); // 実際にはここまで出せない
  driver.setMaxSpeed(10000);
  driver.setOCThreshold(OC_375mA); // 大出力な用途じゃないのでこれで十分
  driver.setSlewRate(SR_530V_us);
  driver.setPWMFreq(PWM_DIV_1, PWM_MUL_2); // DIVを大きくすると変調音が聞こえてくるさい
  driver.setOCShutdown(OC_SD_DISABLE);     // 用途的になくていい
  driver.setVoltageComp(VS_COMP_ENABLE);
  driver.setSwitchMode(SW_HARD_STOP);
  driver.setOscMode(INT_16MHZ); // 開発ボードならまずこれでいいはず

  // 加速･減速･等速回転中･停止中の出力
  // 128にすると電流流杉･うるさすぎ
  driver.setAccKVAL(32);
  driver.setDecKVAL(32);
  driver.setRunKVAL(32);
  driver.setHoldKVAL(32);
}

void loop()
{
  Serial.print("analog : ");
  Serial.print(analogRead(PIN_POT));
  Serial.print(" sw : ");
  Serial.print(digitalRead(PIN_SW));
  Serial.println();
  if (digitalRead(PIN_SW) == 0)
  {
    driver.run(REV, analogRead(PIN_POT) * 2); // 2000step/s出すには20Vぐらい必要だった
  }
  else
  {
    driver.softStop();
  }
  delay(50);
}