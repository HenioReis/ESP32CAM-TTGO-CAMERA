/*
Autor: Hênio reis
Proj: Servedor web com esp32cam e ttgo camera 
Ano: 15/01/2020
*/

#include <WiFi.h>
#include "Esp32CamNew.h"
//#include "SSD1306.h"
//#include "OLEDDisplayUi.h"

#define PWDN_GPIO_NUM 26
//#define ENABLE_OLED

const char *ssid = "ESP32NET";
const char *password = "acdb3572sczx213";

void START_ESP32CAM_SERVER();

void setup()
{   
    pinMode(PWDN_GPIO_NUM, PULLUP);
    digitalWrite(PWDN_GPIO_NUM, HIGH);

	Serial.begin(115200);
	Serial.setDebugOutput(true);
	Serial.println();
	
	START_ESP32CAM();

	WiFi.begin(ssid, password);
	while (WiFi.status() != WL_CONNECTED)
	{
		delay(500);
		Serial.print(".");
	}
	Serial.println("");
	Serial.println("WiFi connected");
	START_ESP32CAM_SERVER();
	Serial.print("Camera Ready! Use 'http://");
	Serial.print(WiFi.localIP());
	Serial.println("' to connect");
}

void loop()
{

}
