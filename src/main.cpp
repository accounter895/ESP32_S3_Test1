#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include "Ticker.h"

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

// 初始化OLED
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, /* clock=*/ BOARD_I2C_SCL, /* data=*/ BOARD_I2C_SDA, /* reset=*/ U8X8_PIN_NONE);

// 累积数据
unsigned long total_pulses = 0;
float total_distance = 0.0;
float average_speed = 0.0;
unsigned long total_calories = 0;
unsigned long trip_count = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("Init u8g2 ....");

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
    float speed = readEncoder();
    displaySpeed(speed);
    checkSpeedAlarm(speed);
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