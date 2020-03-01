#include "ESP32SERVO.h"
#include "myStdlib.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Ticker.h>
#include <Adafruit_NeoPixel.h>

#include <stdlib.h>
#include <time.h>

// Ticker
Ticker tickerWeatherUpdate;
Ticker tickerTimeUpdate;


// WiFi setting
//const char* ssid = "kutzoa_iphone";
//const char* password =  "57D7C2DTenmo";
const char* ssid = "aterm-3fd676-g";
const char* password =  "90bdd380f6653";

// Set web server port number to 80
WiFiServer server(80);

// Variable to store the HTTP request
String header;

// Decode HTTP GET value
String valueString = String(5);
int pos1 = 0;
int pos2 = 0;


// NTP
struct tm timeInfo;
char nowDate[40];
int ntp_now_hour, ntp_now_month;


// Weather
const int weather_load_num = 5;
/*
   0...現在  1...3時間後 2...6時間後 3...9時間後 4...12時間後
*/
float forecast_temp[weather_load_num];
float forecast_temp_min[weather_load_num];
float forecast_temp_max[weather_load_num];
String forecast_weather[weather_load_num];
int forecast_weather_id[weather_load_num];
String forecast_weather_desc[weather_load_num];
float forecast_clouds_ratio[weather_load_num];
String forecast_weatherTime[weather_load_num];

// OpenWeatherMap
const String endpoint = "http://api.openweathermap.org/data/2.5/weather?q=nagoya,jp&units=metric&APPID=";
const String endpoint_forecast = "http://api.openweathermap.org/data/2.5/forecast?q=nagoya,jp&units=metric&cnt=10&APPID=";
const String key = "94ac0f885314944655b13c1a3f71980a";


// Servo
#define SERVO_NUM 4
static const int servosPins[SERVO_NUM] = {4, 16, 18, 19};
ESP32SERVO servos[SERVO_NUM];

// NeoPixel
#define NEOPIXEL_PIN 27
#define NUMPIXELS 4
Adafruit_NeoPixel pixels(NUMPIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// deviceStatus
const int servo_angle[3] = {0, 90, -90};
int servo_status[SERVO_NUM] = {0, 0, 0, 0};

#define LED_COLOR_NUM 15
#define LED_OFF     0
#define LED_RED     1
#define LED_GREEN   2
#define LED_BLUE    3
#define LED_PINK    4
#define LED_ORANGE  5
#define LED_GOLD    6
#define LED_YELLOW  7
#define LED_SKYBLUE 8
#define LED_DARKSKY 9
#define LeD_PURPLE  10
#define LED_WHITE   11
#define LED_SILVER  12
#define LED_GRAY    13
#define LED_DARK    14
const int led_color[LED_COLOR_NUM][3] = { {  0,   0,   0},   // off
                              {255,   0,   0},   // red 
                              {  0, 255,   0},   // green
                              {  0,   0, 255},   // blue
                              {255, 192, 203},   // pink
                              {255, 165,   0},   // orange
                              {255, 215,   0},   // gold
                              {255, 255,   0},   // yellow
                              {135, 206, 235},   // skyblue
                              {  0,   0, 139},   // darksky
                              {128,   0, 128},   // purple
                              {255, 255, 255},   // white
                              {192, 192, 192},   // silver
                              {128, 128, 128},    // gray
                              { 72,  61, 139}    // dark
                            };
int led_status[NUMPIXELS][3] = { {0, 0, 0},
                                {0, 0, 0},
                               {0, 0, 0},
                               {0, 0, 0} };   
/*
   天気予報を取得
   引数 ep ... 天気APIのアドレス
*/
void get_forecast_weather(const String ep)
{
  HTTPClient http;

  http.begin(ep + key); // URLを指定
  int httpCode = http.GET(); // GETリクエストを送信

  if ( httpCode > 0 ) {
    String payload = http.getString(); // 返答を取得(JSON形式)
    Serial.print("httpCode: ");
    Serial.println(httpCode);
    //Serial.println(payload);

    // JSONオブジェクトを作成
    DynamicJsonDocument doc(10000);
    String json = payload;
    DeserializationError error = deserializeJson(doc, json); // JSONを解析

    // パース( deserializeJson() )が成功したか確認
    if ( error ) {
      Serial.print(F("deserializeJson() failed with code "));
      Serial.println(error.c_str());
      return;
    }

    for ( int i = 1; i < weather_load_num; i++ ) {
      const double temp = doc["list"][i - 1]["main"]["temp"].as<double>();
      const double temp_min = doc["list"][i - 1]["main"]["temp_min"].as<double>();
      const double temp_max = doc["list"][i - 1]["main"]["temp_max"].as<double>();
      const char* weather = doc["list"][i - 1]["weather"][0]["main"].as<char*>();
      const int weather_id = doc["list"][i - 1]["weather"][0]["id"].as<int>();
      const char* weather_description = doc["list"][i - 1]["weather"][0]["description"].as<char*>();
      const double clouds_ratio = doc["list"][i - 1]["clouds"]["all"].as<double>();
      const char* weatherTime = doc["list"][i - 1]["dt_txt"].as<char*>();

      forecast_temp[i] = temp;
      forecast_temp_min[i] = temp_min;
      forecast_temp_max[i] = temp_max;
      forecast_weather[i] = weather;
      forecast_weather_id[i] = weather_id;
      forecast_weather_desc[i] = weather_description;
      forecast_clouds_ratio[i] = clouds_ratio;
      forecast_weatherTime[i] = weatherTime;
    }
  } else {
    Serial.println("Error on HTTP request.");
  }
  http.end();
}

/*
   指定時間の天気を取得(3時間毎)
   引数 ep ... 天気APIのアドレス
       get_time ... 0->現在, 1->3時間後, 2->6時間後
*/
void get_time_weather(const String ep, int get_time = 0)
{
  HTTPClient http;

  http.begin(ep + key); // URLを指定
  int httpCode = http.GET();  // GETリクエストを送信

  if (httpCode > 0) { // 返答がある場合

    String payload = http.getString();  // 返答（JSON形式）を取得
    Serial.print("httpCode: ");
    Serial.println(httpCode);
    //Serial.println(payload);

    if ( get_time == 0 ) {
      // jsonオブジェクトの作成
      DynamicJsonDocument doc(1024);
      String json = payload;
      DeserializationError error = deserializeJson(doc, json);

      // パース( deserializeJson() )が成功したかどうかを確認
      if (error) {
        Serial.print(F("deserializeJson() failed with code "));
        Serial.println(error.c_str());
        return;
      }

      // 各データを抜き出し

      const double temp = doc["main"]["temp"].as<double>();
      const double temp_min = doc["main"]["temp_min"].as<double>();
      const double temp_max = doc["main"]["temp_max"].as<double>();
      const char* weather = doc["weather"][0]["main"].as<char*>();
      const int weather_id = doc["weather"][0]["id"].as<int>();
      const char* weather_description = doc["weather"][0]["description"].as<char*>();
      const double clouds_ratio = doc["clouds"]["all"].as<double>();
      const char* weatherTime = doc["dt_txt"].as<char*>();

      forecast_temp[get_time] = temp;
      forecast_temp_min[get_time] = temp_min;
      forecast_temp_max[get_time] = temp_max;
      forecast_weather[get_time] = weather;
      forecast_weather_id[get_time] = weather_id;
      forecast_weather_desc[get_time] = weather_description;
      forecast_clouds_ratio[get_time] = clouds_ratio;
      forecast_weatherTime[get_time] = weatherTime;
    } else {
      // jsonオブジェクトの作成
      DynamicJsonDocument doc(10000);
      String json = payload;
      DeserializationError error = deserializeJson(doc, json);

      // パース( deserializeJson() )が成功したかどうかを確認
      if (error) {
        Serial.print(F("deserializeJson() failed with code "));
        Serial.println(error.c_str());
        return;
      }

      // 各データを抜き出し
      const double temp = doc["list"][get_time]["main"]["temp"].as<double>();
      const double temp_min = doc["list"][get_time]["main"]["temp_min"].as<double>();
      const double temp_max = doc["list"][get_time]["main"]["temp_max"].as<double>();
      const char* weather = doc["list"][get_time]["weather"][0]["main"].as<char*>();
      const int weather_id = doc["list"][get_time]["weather"][0]["id"].as<int>();
      const char* weather_description = doc["list"][get_time]["weather"]["description"].as<char*>();
      const double clouds_ratio = doc["list"][get_time]["clouds"]["all"].as<double>();
      const char* weatherTime = doc["list"][get_time]["dt_txt"].as<char*>();

      forecast_temp[get_time] = temp;
      forecast_temp_min[get_time] = temp_min;
      forecast_temp_max[get_time] = temp_max;
      forecast_weather[get_time] = weather;
      forecast_weather_id[get_time] = weather_id;
      forecast_weather_desc[get_time] = weather_description;
      forecast_clouds_ratio[get_time] = clouds_ratio;
      forecast_weatherTime[get_time] = weatherTime;
    }
  } else {
    Serial.println("Error on HTTP request");
  }

  http.end(); //Free the resources
}

/*
   全ての天気予報を取得
*/
void get_all_weather()
{
  get_time_weather(endpoint, 0);
  get_forecast_weather(endpoint_forecast);
}

/*
   現在時間を取得
*/
void get_now_time()
{
  getLocalTime(&timeInfo);
  ntp_now_hour = timeInfo.tm_hour;
  ntp_now_month = timeInfo.tm_mon + 1;
  sprintf(nowDate, "%04d/%02d/%02d %02d:%02d:%02d",
          timeInfo.tm_year + 1900, timeInfo.tm_mon + 1, timeInfo.tm_mday, timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);
}

/* print関連 */
void printWeather()
{
  for ( int i = 0; i < weather_load_num; i++ ) {
    Serial.println(forecast_weatherTime[i]);
    Serial.print("Weather: ");
    Serial.println(forecast_weather[i]);
    Serial.print("Temp: ");
    Serial.println(forecast_temp[i]);
    Serial.print("Weather description: ");
    Serial.println(forecast_weather_desc[i]);
    Serial.print("Weather ID: ");
    Serial.println(forecast_weather_id[i]);
    Serial.println();
  }
}
void printTime()
{
  Serial.println(nowDate);
}

void set_led_status(int status, int color)
{
  led_status[status][0] = led_color[color][0];
  led_status[status][1] = led_color[color][1];
  led_status[status][2] = led_color[color][2];
}
int searchWeatherId_badWeather() {
  int status = 999;

  // 優位度が高い値を抽出
  int max_id = 0;
  for ( int i = 0; i < weather_load_num; i++) {
    int id = forecast_weather_id[i];
    int id_digit[3] = {0, 0, 0};
    //id_digit[0] = id % 10;
    //id_digit[1] = (id % 100) / 10;
    id_digit[2] = id / 100;

    if ( id_digit[2] == 2 ) {
      if ( max_id != 2 ) status = id;
      else if ( status < id ) status = id;
      
      max_id = id_digit[2];
    } else {
      if ( id_digit[2] == 5 || id_digit[2] == 6 ) { // 500-699
        if ( status <= 500 || status >= 700 ) status = id;
        else if ( status % 100 < id % 100 ) status = id;
        else if ( status % 100 == id % 100 && status < id ) status = id;
      } 
      else if ( id < 500 && (status < id || status >= 700) ) status = id; // 300-499
      else if ( status >= 700 && status < id ) status = id; // 700-

      max_id = status / 100; 
    }
  }
  return status;
}

void setDeviceStatus1()
{
  int deviceID = 0;
  int status1 = searchWeatherId_badWeather();

  for( int i = 0; i < SERVO_NUM; i++ ) {
    servo_status[i] = servo_angle[1];
    set_led_status(0, LED_OFF);
  }
  // status1 (default : Umbrella and parasol)
  switch (status1 / 100) {
    case 2: // Thunderstorm
      // 傘
      // NeoPixel -> DarkBlue
      servo_status[deviceID] = servo_angle[1];
      set_led_status(deviceID, LED_DARKSKY);
      break;
    case 3: // Drizzle 30 seconds to 15 minutes
      // 折り畳み傘
      // NeoPixel -> Blue
      servo_status[deviceID] = servo_angle[2];
      set_led_status(deviceID, LED_BLUE);
      break;
    case 5: // Rain
      if ( status1 == 500 ) {
        // 折り畳み傘
        // NeoPixel -> Blue
        servo_status[deviceID] = servo_angle[2];
        set_led_status(deviceID, LED_BLUE);
      } else {
        // 傘
        // NeoPixel -> Blue
        servo_status[deviceID] = servo_angle[1];
        set_led_status(deviceID, LED_BLUE);
      }
      break;
    case 6: // Snow
      if (status1 == 600 ) {
        // 折り畳み傘
        // NeoPixel -> white
        servo_status[deviceID] = servo_angle[2];
        set_led_status(deviceID, LED_WHITE);
      } else {
        // 傘
        // NeoPixel -> white
        servo_status[deviceID] = servo_angle[1];
        set_led_status(deviceID, LED_WHITE);
      }
      break;
    case 7: // Atmosphere
      break;
    case 8: // Clear(800) or Clouds(>801)
      if ( status1 == 800 && ntp_now_month >= 7 && ntp_now_month <= 9 ){
        servo_status[deviceID] = servo_angle[1];
        set_led_status(deviceID, LED_ORANGE);
      } 
      break;
    default:
      Serial.println("error : weatherID is undefiend.");
  }
}
/* 26℃以上で半袖 
   21-25℃、曇りの日、長袖。晴れの日、半袖
   16-22℃　薄手のジャケット、長袖
   12-15℃
    7-11℃　厚手のコート、長袖
 */
void setDeviceStatus2()
{
  // status2 (default : coat)
  int deviceID = 1;
  int status2 = searchWeatherId_badWeather();
  int max_temp = maxArray(forecast_temp_max, weather_load_num);
  if ( max_temp >= 30 ) {
    // servo_status[1] = servo_angle[2];
    // set_led_status(1, LED_RED);
  } else if ( max_temp >= 26 ) {
    //servo_status[1] = servo_angle[2];
    //set_led_status(1, LED_ORANGE);
  } else if ( max_temp >= 21 ) {
    if ( status2 == 800 || status2 == 801 || status2 == 802 ) {
      // 薄手の上着、オレンジ色
      servo_status[deviceID] = servo_angle[2];
      set_led_status(deviceID, LED_ORANGE);
    }
  } else if ( max_temp >= 16 ) {
    // 薄手の上着、黄色
    servo_status[deviceID] = servo_angle[2];
    set_led_status(deviceID, LED_YELLOW);
  } else if ( max_temp >= 12 ) {
    // 軽めのアウター、薄手のコート、ダウン
    servo_status[deviceID] = servo_angle[2];
    set_led_status(deviceID, LED_BLUE);
  } else if ( max_temp >= 7 ) {
    // 厚手のコート、BLUE
    servo_status[deviceID] = servo_angle[1];
    set_led_status(deviceID, LED_BLUE);
  } else {
    // 厚手のコート、DARKSKY
    servo_status[deviceID] = servo_angle[1];
    set_led_status(deviceID, LED_DARKSKY);
  }
}
void setDeviceStatus3()
{
  // status3 (default : clothes)
  int deviceID = 2;
  int status3 = searchWeatherId_badWeather();
  int max_temp = maxArray(forecast_temp_max, weather_load_num);
  if ( max_temp >= 30 ) {
    // 半袖, 赤色
    servo_status[deviceID] = servo_angle[2];
    set_led_status(deviceID, LED_RED);
  } else if ( max_temp >= 26 ) {
    // 半袖、オレンジ色
    servo_status[deviceID] = servo_angle[2];
    set_led_status(deviceID, LED_ORANGE);
  } else if ( max_temp >= 21 ) {
    if ( status3 == 800 || status3 == 801 || status3 == 802 ) {
      // 半袖、オレンジ色
      servo_status[deviceID] = servo_angle[2];
      set_led_status(deviceID, LED_ORANGE);
    }
  } else if ( max_temp >= 16 ) {
    // 長袖
    servo_status[deviceID] = servo_angle[1];
    set_led_status(deviceID, LED_SKYBLUE);
  } else if ( max_temp >= 12 ) {
    // 長袖、セータ、ダウン、DARKSKY
    servo_status[deviceID] = servo_angle[1];
    set_led_status(deviceID, LED_DARKSKY);
  } else if ( max_temp >= 7 ) {
    // セータ、ダウン、DARKSKY
    servo_status[deviceID] = servo_angle[1];
    set_led_status(deviceID, LED_DARKSKY);
  } else {
    // セータ、ダウン、DARKSKY
    servo_status[deviceID] = servo_angle[2];
    set_led_status(deviceID, LED_DARKSKY);
  }
}

void setDeviceStatus4()
{
  // status4 (default : fortune and color)
  int deviceID = 3;
  srand((unsigned int)time(NULL));
  int lucky_face = rand() % 3;
  int lucky_color = rand() % LED_COLOR_NUM;

  servo_status[deviceID] = servo_angle[lucky_face];
  set_led_status(deviceID, lucky_color);
}
void runDevice(){

}

void setup() {
  Serial.begin(115200);
  while ( !Serial );
  Serial.println("");

  // Servo関連
  for (int i = 0; i < SERVO_NUM; ++i) {
    if (!servos[i].begin(servosPins[i], 15 - i)) {
      Serial.print("Servo ");
      Serial.print(i);
      Serial.println("attach error");
    }
  }

  // NeoPixel関連
  pixels.begin();

  // WiFi関連
  WiFi.begin(ssid, password);
  Serial.print("WiFi connecting");
  while ( WiFi.status() != WL_CONNECTED ) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  // server.begin();

  // NTP
  configTime(9 * 3600L, 0, "ntp.nict.jp", "time.google.com", "ntp.jst.mfeed.ad.jp"); //NTPの設定

  // Tickerの設定
  tickerWeatherUpdate.attach(600, get_all_weather);
  tickerTimeUpdate.attach(60, get_now_time);

  // 初期ロード
  get_all_weather();
  get_now_time();
}

void loop() {
  setDeviceStatus1();
  setDeviceStatus2();
  setDeviceStatus3();
  setDeviceStatus4();
  Serial.println("#############################################################");
  printWeather();
  printTime();

  delay(30000);
}
