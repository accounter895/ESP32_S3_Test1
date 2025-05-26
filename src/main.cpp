#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "Ticker.h"

/****** WIFI和Web设置 ******/
AsyncWebServer server(80);
// 使用端口号80可以直接输入IP访问，使用其它端口需要输入IP:端口号访问
// 一个储存网页的数组
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
    <meta charset="utf-8">
</head>
<body>
     <h2>自行车智能码表</h2>
     <!-- 创建一个ID位Code_table的盒子用于显示获取到的数据 -->
     <div id="Code_table">
     </div>
</body>
<script>
     // 设置一个定时任务, 1000ms执行一次
     setInterval(function () {
        var xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function () {
            if (this.readyState == 4 && this.status == 200) {
                // 此代码会搜索ID为Code_table的组件，然后使用返回内容替换组件内容
                document.getElementById("Code_table").innerHTML = this.responseText;
            }
        };
        // 使用GET的方式请求 /Code_table
        xhttp.open("GET", "/Code_table", true);
        xhttp.send();
    }, 100)
</script>)rawliteral";
const char *ssid = "Redmi Note 12 Turbo";
const char *password = "12345678";

/****** I2C引脚定义 ******/
#define BOARD_I2C_SCL   9
#define BOARD_I2C_SDA   10

/****** 定时器中断参数 ******/
Ticker ticker1;
uint16_t interrupt_time = 10;
uint8_t timer_intert_flag = 0;

/****** 编码器引脚及参数 ******/
#define PI        3.1415
#define Ancoder_1 17
#define Ancoder_2 18
float speed_threshold = 5.0;
volatile long encoder_counter_1 = 0, encoder_counter_2 = 0;

/****** 蜂鸣器引脚 ******/
#define BUZZER_PIN 21

/****** 按键引脚 ******/
#define BUTTON_SET_SPEED 4

/****** 函数声明 ******/
void timerls();
float readEncoder();
void counter_encoder_1();
void counter_encoder_2();

void checkSpeedAlarm(float current_speed);
void checkButtonPress();

void display_move_frame();
void displayWelcome();
void displaySpeed(float speed);

String Speed_Data(void);

// 初始化OLED
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, /* clock=*/ BOARD_I2C_SCL, /* data=*/ BOARD_I2C_SDA, /* reset=*/ U8X8_PIN_NONE);

// 累积数据
unsigned long total_pulses = 0;   // 累计脉冲数
float show_Speed = 0.0;                // 当前速度
float total_distance = 0.0;       // 累计距离
float average_speed = 0.0;        // 平均速度
unsigned long total_calories = 0; // 累计卡路里
unsigned long trip_count = 0;     // 累计行程

void setup() {
  Serial.begin(115200);
  Serial.println("Init u8g2 ....");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid,password);
  if(WiFi.status() != WL_CONNECTED) {
    Serial.println("Connecting to WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
  }
  Serial.println("Connected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });
  server.on("/Code_table", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/plain", Speed_Data().c_str());
  });
  server.begin();

  u8g2.setFont(u8g2_font_wqy12_t_gb2312);
  u8g2.begin();
  u8g2.enableUTF8Print();
  displayWelcome();

  pinMode(Ancoder_1, INPUT_PULLUP);
  pinMode(Ancoder_2, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(Ancoder_1), counter_encoder_1, CHANGE);
  attachInterrupt(digitalPinToInterrupt(Ancoder_2), counter_encoder_2, CHANGE);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  pinMode(BUTTON_SET_SPEED, INPUT_PULLUP);

  ticker1.attach_ms(interrupt_time, timerls);
  interrupts();
  delay(100);
}

void loop() {
  if (timer_intert_flag == 1) {
    timer_intert_flag = 0;
    show_Speed = readEncoder();
    displaySpeed(show_Speed);
    checkSpeedAlarm(show_Speed);
  }

  checkButtonPress();
  delay(1);
}

void checkSpeedAlarm(float current_speed) {
  if (current_speed > speed_threshold) {
    digitalWrite(BUZZER_PIN, HIGH);
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }
}

void checkButtonPress() {
  if (digitalRead(BUTTON_SET_SPEED) == LOW) {
    delay(200);
    speed_threshold += 1.0;
    if (speed_threshold > 60.0) speed_threshold = 10.0;
    Serial.print("New Speed Threshold: ");
    Serial.println(speed_threshold);
    delay(500);
  }
}

/****** OLED显示相关 ******/
void display_move_frame() {
  u8g2.clearBuffer();
  u8g2.drawFrame(0, 0, 128, 64);
  u8g2.sendBuffer();
}

void displayWelcome() {
  char *str = "正在启动...";
  u8g2.clearBuffer();
  u8g2.drawUTF8(u8g2.getDisplayWidth() / 2 - u8g2.getUTF8Width(str) / 2,
                u8g2.getDisplayHeight() / 2 + u8g2.getMaxCharHeight() / 2, str);
  u8g2.sendBuffer();
}

void displaySpeed(float current_speed) {
  u8g2.clearBuffer();
  u8g2.setCursor(0, 16);
  u8g2.printf("Cur: %.2f km/h", current_speed);

  u8g2.setCursor(0, 32);
  u8g2.printf("Avg: %.2f km/h", average_speed);

  u8g2.setCursor(0, 48);
  u8g2.printf("Dist: %.2f km", total_distance);

  u8g2.setCursor(0, 64);
  u8g2.printf("Cal: %lu kcal", total_calories);

  u8g2.sendBuffer();
}

/****** 定时器中断函数 ******/
void IRAM_ATTR timerls() {
  timer_intert_flag = 1;
}

/****** 编码器计数 ******/
float wheel_circumference_km = (2 * PI * 0.035) / 100000; // 轮胎半径 ~0.035m => 周长约 0.22m => 0.0002199 km 这是个计算轮胎周长的公式

float readEncoder() {
  float pulses = (encoder_counter_1 + encoder_counter_2) / 2.0;
  float speed = pulses / 444.5 / 7.0 * 2 * PI * 3.6;
  //Serial.print("encoder_counter_1: "); Serial.println(encoder_counter_1);
  total_pulses += (long)pulses;
  total_distance = total_pulses * wheel_circumference_km;

  average_speed = total_distance / ((float)(millis()) / 3600000); // 小时单位
  total_calories = (unsigned long)(average_speed * total_distance * 1.0); // 粗略估算公式
  encoder_counter_1 = 0;
  encoder_counter_2 = 0;

  return speed;
}

void counter_encoder_1() {
  encoder_counter_1++;
}

void counter_encoder_2() {
  encoder_counter_2++;
}

/******* 打包上传网页的数据 ******/
String Speed_Data(void){
  // 将全部码表数据打包成JSON格式
  String dataBuffer = "<p>";
  dataBuffer += "<h1>码表数据</h1>";
  dataBuffer += "<b>当前速度:</b>";
  dataBuffer += String(show_Speed);
  dataBuffer += " km/h<br>";
  dataBuffer += "<b>平均速度:</b>";
  dataBuffer += String(average_speed);
  dataBuffer += " km/h<br>";
  dataBuffer += "<b>总里程:</b>";
  dataBuffer += String(total_distance);
  dataBuffer += " km<br>";
  dataBuffer += "<b>消耗的卡路里:</b>";
  dataBuffer += String(total_calories);
  dataBuffer += " kcal<br>";

  return dataBuffer;
}