#define DHT22_PIN 27
#include <WiFi.h>
#include <LiquidCrystal_I2C.h>
#include <SparkFun_ENS160.h>
#include <SparkFun_Qwiic_Humidity_AHT20.h>
#include <DHT.h>
#include <PubSubClient.h>

unsigned long wifiTaskPrevMillis = 0;
unsigned long wifiTaskCheckInterval = 10000;
unsigned long mqttTaskPrevMillis = 0;
unsigned long mqttTaskCheckInterval = 1000;
unsigned long ntpTaskPrevMillis = 0;
unsigned long ntpTaskCheckInterval = 100;
unsigned long ensTaskPrevMillis = 0;
unsigned long ensTaskCheckInterval = 1000;
unsigned long ahtTaskPrevMillis = 0;
unsigned long ahtTaskCheckInterval = 10000;
unsigned long dhtTaskPrevMillis = 0;
unsigned long dhtTaskCheckInterval = 1000;
unsigned long lcdTaskPrevMillis = 0;
unsigned long lcdTaskCheckInterval = 100;

//Sleep
bool sleepMode = false;
unsigned long sleepTaskPrevMillis = 0;
unsigned long sleepTaskCheckInterval = 60000;
uint8_t sleepTaskCount = 0;
uint8_t sleepTaskTarget = 0;

//WiFi
const char* wifiSsid  = "ssid";
const char* wifiPass = "password";
uint8_t wifiStatus;

//NTP
const char* ntpServer = "pool.ntp.org";
const long ntpOffset = 10800;
struct tm ntpTime;

//MQTT
const char* mqttBroker = "broker";
const char* mqttInTopic = "inTopic";
const char* mqttOutTopic = "outTopic";
const char* mqttUser = "user";
const char* mqttPassword = "password";
const int mqttPort = 1883;
bool mqttStatus;

//WIFI
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
char mqttBuffer[400];

//LCD
LiquidCrystal_I2C lcd(0x27,16,2);
byte lcd_char_n1[8] = {B11111, B11011, B11011, B11011, B11011, B11011, B11011, B11011};
byte lcd_char_n2[8] = {B11011, B11011, B11011, B11011, B11011, B11011, B11011, B11111};
byte lcd_char_n3[8] = {B00011, B00011, B00011, B00011, B00011, B00011, B00011, B00011};
byte lcd_char_n4[8] = {B11111, B00011, B00011, B00011, B00011, B00011, B00011, B11111};
byte lcd_char_n5[8] = {B11111, B11000, B11000, B11000, B11000, B11000, B11000, B11111};
byte lcd_char_n6[8] = {B11111, B00011, B00011, B00011, B00011, B00011, B00011, B00011};
byte lcd_char_n7[8] = {B11111, B11011, B11011, B11011, B11011, B11011, B11011, B11111};
byte lcd_char_aqiInit[8] = {B00100, B01100, B00100, B01110, B00000, B01110, B01010, B01110};
byte lcd_char_aqiWarm[8] = {B00000, B00100, B01010, B10101, B11011, B10101, B01110, B00000};
byte lcd_char_aqiError[8] = {B11111, B10000, B10000, B11111, B10000, B10000, B11111, B00000};
byte lcd_char_aqi1[8] = {B00000, B00000, B01010, B01010, B00000, B10001, B01110, B01100};
byte lcd_char_aqi2[8] = {B00000, B00000, B01010, B01010, B00000, B10001, B01110, B00000};
byte lcd_char_aqi3[8] = {B00000, B00000, B01010, B01010, B00000, B00000, B11111, B00000};
byte lcd_char_aqi4[8] = {B00000, B00000, B01010, B01010, B00000, B01110, B10001, B00000};
byte lcd_char_aqi5[8] = {B00000, B00000, B11011, B01010, B00000, B01110, B10001, B00000};
uint8_t lcd_digits_up[12] = {1, 3, 4, 4, 2, 5, 5, 6, 7, 7, 46, 32};
uint8_t lcd_digits_dn[12] = {2, 3, 5, 4, 6, 4, 7, 3, 7, 4, 46, 32};
bool lcdBacklightStatus = false;

//Modules
SparkFun_ENS160 ens;
AHT20 aht; 
DHT dht(DHT22_PIN, DHT22);

int8_t ensStatus = -1; //Status Flag (-1 - No connection, 0 - Standard, 1 - Warm up, 2 - Initial Start Up)
uint8_t ensAQI;        //Air Quality Index (1-5)
uint16_t ensTVOC;      //Total Volatile Organic Compounds in ppb
uint16_t ensECO2;      //CO2 concentration in ppm
int8_t ahtStatus = -1; //Status Flag (-1 - No connection, 0 - Standard)
float ahtTemp;         //Temperature in Celsius
float ahtHum;          //Humidity in % RH
float dhtTemp;         //Temperature in Celsius
float dhtHum;          //Humidity in % RH

void modeSleep(uint8_t timeInMinutes) {
  sleepTaskPrevMillis = millis();
  sleepTaskCount = 0;
  sleepTaskTarget = timeInMinutes;
  sleepMode = true;
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

void modeWork() {
  sleepMode = false;
  WiFi.disconnect(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSsid, wifiPass);
}

void lcdInit() {
  lcd.init();
  lcd.backlight();
  lcdBacklightStatus = true;
}

void lcdBacklightOn() {
  lcd.backlight();
  lcdBacklightStatus = true;
}

void lcdBacklightOff() {
  lcd.noBacklight();
  lcdBacklightStatus = false;
}

void lcdPrintConnectingWifi() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("CONNECTING");
  lcd.setCursor(0, 1);
  lcd.print("TO WIFI");
}

void lcdPrintGettingTime() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("GETTING TIME");
  lcd.setCursor(0, 1);
  lcd.print("FROM SERVER");
}

void lcdPrintReady() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("READY");
}

void lcdPrintInitENS() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Initialize");
  lcd.setCursor(0, 1);
  lcd.print("ENS160");
}

void lcdPrintInitAHT() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Initialize");
  lcd.setCursor(0, 1);
  lcd.print("AHT21");
}

void lcdPrintInitDHT() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Initialize");
  lcd.setCursor(0, 1);
  lcd.print("DHT22");
}

void lcdPrepareMain() {
  lcd.createChar(1, lcd_char_n1);
  lcd.createChar(2, lcd_char_n2);
  lcd.createChar(3, lcd_char_n3);
  lcd.createChar(4, lcd_char_n4);
  lcd.createChar(5, lcd_char_n5);
  lcd.createChar(6, lcd_char_n6);
  lcd.createChar(7, lcd_char_n7);
  lcd.clear();
}

void lcdPrintMain(float temp, float hum, uint16_t co2, uint8_t wifiStatus, uint8_t aqi, int8_t ensStatus) {
  lcdPrintTime(0);
  lcdPrintTemperature(temp, 9, 0);
  lcdPrintHumidity(hum, 6, 0);
  lcdPrintCO2(co2, 9, 1);
  lcdPrintAlarm(wifiStatus, mqttStatus, 6, 1);
  lcdPrintAQI(aqi, ensStatus, 7, 1);
}

void lcdPrintTime(uint8_t pos) {
  lcdPrintNumber(ntpTime.tm_hour / 10, pos);
  lcdPrintNumber(ntpTime.tm_hour % 10, pos + 1);
  lcdPrintDelimiter(ntpTime.tm_sec % 2, pos + 2);
  lcdPrintNumber(ntpTime.tm_min / 10, pos + 3);
  lcdPrintNumber(ntpTime.tm_min % 10, pos + 4);
}

void lcdPrintNumber(uint8_t number, uint8_t pos) {
  lcd.setCursor(pos, 0);
  lcd.write(lcd_digits_up[number]);
  lcd.setCursor(pos, 1);
  lcd.write(lcd_digits_dn[number]);
}

void lcdPrintDelimiter(bool draw, uint8_t pos) {
  lcd.setCursor(pos, 0);
  lcd.write(lcd_digits_up[draw ? 10 : 11]);
  lcd.setCursor(pos, 1);
  lcd.write(lcd_digits_dn[draw ? 10 : 11]);
}

void lcdPrintTemperature(float temp, uint8_t posX, uint8_t posY) {
  lcd.setCursor(posX, posY);
  char str[6];
  dtostrf(temp, 5, 1, str);
  lcd.print(str);
  lcd.write(165);
  lcd.print("C");
}

void lcdPrintHumidity(float hum, uint8_t posX, uint8_t posY) {
  lcd.setCursor(posX, posY);
  char str[3];
  dtostrf(hum, 2, 0, str);
  lcd.print(str);
  lcd.print("%");
}

void lcdPrintCO2(uint16_t co2, uint8_t posX, uint8_t posY) {
  lcd.setCursor(posX, posY);
  char str[4];
  dtostrf(co2, 4, 0, str);
  lcd.print(str);
  lcd.print("ppm");
}

void lcdPrintAlarm(uint8_t wifiStatus, bool mqttStatus, uint8_t posX, uint8_t posY) {
  lcd.setCursor(posX, posY);
  if (wifiStatus != WL_CONNECTED) {
    lcd.write(33);
  } else if (!mqttStatus) {
    lcd.write(42);
  } else {
    lcd.write(32);
  }
}

void lcdPrintAQI(uint8_t aqi, int8_t ensStatus, uint8_t posX, uint8_t posY) {
  switch (ensStatus) {
    case 0:
      switch (aqi) {
        case 1: lcd.createChar(8, lcd_char_aqi1); break;
        case 2: lcd.createChar(8, lcd_char_aqi2); break;
        case 3: lcd.createChar(8, lcd_char_aqi3); break;
        case 4: lcd.createChar(8, lcd_char_aqi4); break;
        case 5: lcd.createChar(8, lcd_char_aqi5); break;
      }
      break;
    case 1: lcd.createChar(8, lcd_char_aqiWarm); break;
    case 2: lcd.createChar(8, lcd_char_aqiInit); break;
    case -1: lcd.createChar(8, lcd_char_aqiError); break;
  }
  lcd.setCursor(posX, posY);
  lcd.write(8);
}

void wifiInit() {
  wifiStatus = WiFi.status();
  WiFi.begin(wifiSsid, wifiPass);
  while (wifiStatus != WL_CONNECTED) {
    delay(500);
    wifiCheck();
  }
  delay(1000);
}

void wifiCheck() {
  wifiStatus = WiFi.status();
}

void mqttInit() {
  mqttClient.setBufferSize(400);
  mqttClient.setServer(mqttBroker, mqttPort);
  mqttClient.setCallback(mqttCallback);
}

void mqttCheck() {
  if (!mqttClient.connected()) {
    mqttStatus = 0;
  }
  if (wifiStatus == WL_CONNECTED && mqttStatus == 0) {
    if (!mqttClient.connect("DesktopClock", mqttUser, mqttPassword)) {
      delay(500);
    }
  }
  if (mqttClient.connected()) {
    if (mqttStatus == 0) {
      mqttStatus = 1;
      mqttClient.subscribe(mqttInTopic);
    }
    mqttClient.loop();
    mqttSend();
  }
}

void mqttSend() {
  snprintf(mqttBuffer, sizeof(mqttBuffer),
  "{\n"
  "  \"year\": %d,\n"
  "  \"month\": %d,\n"
  "  \"day\": %d,\n"
  "  \"hour\": %d,\n"
  "  \"minute\": %d,\n"
  "  \"second\": %d,\n"
  "  \"weekDay\": %d,\n"
  "  \"yearDay\": %d,\n"
  "  \"ensStatus\": %d,\n"
  "  \"ensAQI\": %d,\n"
  "  \"ensTVOC\": %d,\n"
  "  \"ensECO2\": %d,\n"
  "  \"ahtStatus\": %d,\n"
  "  \"ahtTemp\": %0.1f,\n"
  "  \"ahtHum\": %0.1f,\n"
  "  \"dhtTemp\": %0.1f,\n"
  "  \"dhtHum\": %0.1f,\n"
  "  \"backlight\": %d\n"
  "}"
  ,
  ntpTime.tm_year + 1900, ntpTime.tm_mon + 1, ntpTime.tm_mday, ntpTime.tm_hour, ntpTime.tm_min, ntpTime.tm_sec, ntpTime.tm_wday + 1, ntpTime.tm_yday + 1
  , ensStatus, ensAQI, ensTVOC, ensECO2, ahtStatus, ahtTemp, ahtHum, dhtTemp, dhtHum, lcdBacklightStatus);
  mqttClient.publish(mqttOutTopic, mqttBuffer);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  if (message == "backlight off") {
    lcdBacklightOff();
  } else if (message == "backlight on") {
    lcdBacklightOn();
  } else if (message == "ens reset") {
    ensReset();
  } else if (message.startsWith("sleep")) {
    int param = message.substring(6).toInt();
    modeSleep(param);
  }
}

void ntpInit() {
  configTime(ntpOffset, 0, ntpServer);
  while(!getLocalTime(&ntpTime)) {
    delay(500);
  }
}

void ntpRead()
{
  getLocalTime(&ntpTime);
}

void ensInit() {
  if(ens.begin()) {
    ens.setOperatingMode(SFE_ENS160_STANDARD);
  } else {
    ensStatus = -1;
  }
}

void ensReset() {
  if (ens.setOperatingMode(SFE_ENS160_RESET)) {
    delay(100);
    ens.setOperatingMode(SFE_ENS160_STANDARD);
  }
}

void ahtInit() {
  if (aht.begin()) {
    ahtStatus = 0;
  } else {
    ahtStatus = -1;
  }
}

void dhtInit() {
  dht.begin();
}

void ensRead() {
  if (ens.checkDataStatus()) {
    ensAQI = ens.getAQI();
    ensTVOC = ens.getTVOC();
    ensECO2 = ens.getECO2();
    ensStatus = ens.getFlags();
  }
}

void ahtRead() {
  if (aht.available()) {
    ahtTemp = aht.getTemperature();
    ahtHum = aht.getHumidity();
  }
}

void dhtRead() {
  float tempVal = dht.readTemperature();
  float humVal = dht.readHumidity();
  if (!isnan(tempVal) || !isnan(humVal)) {
    dhtTemp = tempVal;
    dhtHum = humVal;
    ensWrite();
  }
}

void ensWrite() {
  ens.setTempCompensationCelsius(dhtTemp);
  ens.setRHCompensationFloat(dhtHum);
}

void setup() {
  lcdInit();
  lcdPrintConnectingWifi();
  wifiInit();
  mqttInit();
  lcdPrintGettingTime();
  ntpInit();
  lcdPrintInitENS();
  ensInit();
  lcdPrintInitAHT();
  ahtInit();
  lcdPrintInitDHT();
  dhtInit();
  lcdPrintReady();
  delay(1000);
  lcdPrepareMain();
}

void loop() {
  unsigned long currMillis = millis();
  if (sleepMode) {
    if (currMillis - sleepTaskPrevMillis > sleepTaskCheckInterval) {
      sleepTaskCount++;
      if (sleepTaskCount >= sleepTaskTarget) {
        modeWork();
      }
      sleepTaskPrevMillis = currMillis;
    }
  }
  if (currMillis - wifiTaskPrevMillis > wifiTaskCheckInterval) {
    wifiCheck();
    wifiTaskPrevMillis = currMillis;
  }
  if (currMillis - mqttTaskPrevMillis > mqttTaskCheckInterval) {
    mqttCheck();
    mqttTaskPrevMillis = currMillis;
  }
  if (currMillis - ntpTaskPrevMillis > ntpTaskCheckInterval) {
    ntpRead();
    ntpTaskPrevMillis = currMillis;
  }
  if (currMillis - ensTaskPrevMillis > ensTaskCheckInterval) {
    ensRead();
    ensTaskPrevMillis = currMillis;
  }
  if (currMillis - ahtTaskPrevMillis > ahtTaskCheckInterval) {
    ahtRead();
    ahtTaskPrevMillis = currMillis;
  }
  if (currMillis - dhtTaskPrevMillis > dhtTaskCheckInterval) {
    dhtRead();
    dhtTaskPrevMillis = currMillis;
  }
  if (currMillis - lcdTaskPrevMillis > lcdTaskCheckInterval) {
    lcdPrintMain(dhtTemp, dhtHum, ensECO2, wifiStatus, ensAQI, ensStatus);
    lcdTaskPrevMillis = currMillis;
  }
}
