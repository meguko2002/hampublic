//https://programresource.net/2020/02/21/2912.html

#include "Ambient.h"
#include <EEPROM.h>   //ssid,pwdを不可視にする処置
#include <HTTPClient.h>   //iffftを使うため（センサ異常をLINEに通知するため）

#define sensorPin 36
#define ledPin 22

//ssid,pwdなどは不可視にするためにEEOROMに書き込んでおく /arduino/hide_ssie_pwd/write_to_EEPROM
struct DATA_SET {  // ②EEPROMで利用する型（構造体）を宣言
  char ssid[64];
  char pass[64];
  char writeKey[64];
  char maker_Event[64];
  char maker_Key[64];
  int counter;
};
DATA_SET buf;                  //ssid,pwdを不可視にする処置

WiFiClient client;
Ambient ambient;
HTTPClient http;

unsigned int channelId =  30898; // AmbientのチャネルID
const int thd = 2000;  //sensor閾値0～4096, 全反射4096
unsigned long previousMillis = 0;
const int interval = 60000; //送信間隔[msec]
int counter;
String url;

//データをEEPROMから読み込む。保存データが無い場合デフォルトにする。
void load_data() {
  EEPROM.get<DATA_SET>(0, buf);
}

//EEPROMへの保存
void save_data() {
  EEPROM.put<DATA_SET>(0, buf);
  EEPROM.commit(); //大事
}

void setup() {
  pinMode(ledPin, OUTPUT);
  pinMode(sensorPin, INPUT);
  digitalWrite(ledPin, LOW);
  Serial.begin(115200);
  EEPROM.begin(1024); //1kbサイズ,構造体のサイズよりも大きいこと
  load_data();
  if (buf.counter != 0)counter = buf.counter;
  else counter = 0;
  Serial.println(counter);
  Serial.println(buf.ssid);
  Serial.println(buf.pass);
  Serial.println(buf.writeKey);
  Serial.println(buf.maker_Event);
  Serial.println(buf.counter);

  WiFi.begin ( buf.ssid, buf.pass );
  while (WiFi.status() != WL_CONNECTED) {  //  Wi-Fi AP接続待ち
    Serial.print(".");
    delay(100);
  }

  Serial.print("WiFi connected\r\nIP address: ");
  Serial.println(WiFi.localIP());
  pinMode(21, INPUT_PULLUP);
  ambient.begin(channelId, buf.writeKey, &client); // チャネルIDとライトキーを指定してAmbientの初期化

  // センサ異常をiffft経由でLINEに通知する設定
  url = "https://maker.ifttt.com/trigger/" + String(buf.maker_Event) + "/with/key/" + String(buf.maker_Key);
  http.begin(url);
}

void loop() {
  unsigned long currentMillis = millis();
  int sensor_value, state;
  static int pre_state = 0;
  static int alert = 0;

  sensor_value = analogRead(sensorPin);
  state = (sensor_value > thd) ? 1 : 0;
  if (state) {
    digitalWrite(ledPin, HIGH);
    if (pre_state == 0) {
      counter += 1;
      buf.counter = counter;
      save_data();
    }
  }
  else digitalWrite(ledPin, LOW);
  Serial.print(sensor_value);
  Serial.print(",");
  Serial.println(counter);
  pre_state = state;
  if (sensor_value == 0 && alert == 0) { // センサ異常ならiffft経由で通知
    http.GET();
    alert = 1;
  }
  if (sensor_value != 0) alert = 0; // センサ正常ならalert解除
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    if (sensor_value != 0) {
      ambient.set(1, String(counter).c_str());
      ambient.send();
    }
  }
  delay(100);
}
