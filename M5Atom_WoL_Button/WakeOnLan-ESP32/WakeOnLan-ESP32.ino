#include <WiFi.h>
#include <WiFiUdp.h>

#include <WakeOnLan.h>

#include "M5Atom.h"

WiFiUDP UDP;
WakeOnLan WOL(UDP);

const char* ssid     = "aterm-09449e-g";
const char* password = "400e19ffa976a";

void wakeMyPC() {
  //const char *MACAddress = "04:D9:F5:86:06:37"; // Desktop
  const char *MACAddress = "90:2E:16:BD:E9:27"; // ThinkPad Eher
  

  WOL.sendMagicPacket(MACAddress); // Send Wake On Lan packet with the above MAC address. Default to port 9.
}

void setup()
{

  M5.begin(true, false,
           true);  // Init Atom-Matrix(Initialize serial port, LED).  初始化
  // ATOM-Matrix(初始化串口、LED点阵)
  M5.dis.drawpix(0, 0xfff000);  // YELLOW
  // 00ff00(Atom-Matrix has only one light).
  // 以指定RGB颜色0x00ff00点亮第0个LED
  pinMode(G39, INPUT);
  Serial.begin(115200);


  WOL.setRepeat(3, 100); // Optional, repeat the packet three times with 100ms between. WARNING delay() is used between send packet function.

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  M5.dis.drawpix(0, 0x0000f0);  // BLUE  蓝色
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  WOL.calculateBroadcastAddress(WiFi.localIP(), WiFi.subnetMask()); // Optional  => To calculate the broadcast address, otherwise 255.255.255.255 is used (which is denied in some networks).

  M5.dis.drawpix(0, 0x00ff00);  // GREEN  绿色
}


void loop()
{
  if (digitalRead(G39) == 0) {
    Serial.print("pushed!!\n\r");
    M5.dis.drawpix(0, 0x0000f0);  // BLUE  蓝色
    wakeMyPC();
    delay(500);
    M5.dis.drawpix(0, 0x00ff00);  // GREEN  绿色
  }
}
