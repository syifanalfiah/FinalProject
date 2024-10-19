#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHTesp.h>
#include <ESP32Servo.h>
#include <RTClib.h>
#include <Wire.h>

/* OLED Display */
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

/* Servo */
const int servoPin = 4;
Servo servo;
int servoPos = 0;

/* Push Buttons */
#define BUTTON_1_PIN 19
#define BUTTON_2_PIN 18
#define BUTTON_3_PIN 5

/* LEDs */
#define GREEN_LED_PIN 16
#define RED_LED_PIN 17

/* RTC Clock */
RTC_DS1307 rtc;
char daysOfWeek[7][12] = {"Minggu", "Senin", "Selasa", "Rabu", "Kamis", "Jumat", "Sabtu"};

/* DHT Sensor */
const int DHT_PIN = 15;
DHTesp dhtSensor;

/* LDR Sensor */
#define LDR_PIN 13
const float GAMMA = 0.7;
const float RL10 = 50;

/* Buzzer */
#define BUZZER_PIN 2

/* Global Variables */
int currentMode = 0;
int menuOption = 0;
unsigned long lastButtonPress = 0;
unsigned long alarmTime = 0;

void setup() {
  Serial.begin(9600);

  /* Setup OLED */
  if (!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Failed to start SSD1306 OLED"));
    while (1);
  }
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(WHITE);
  oled.setCursor(0, 0);
  oled.println("VI-ROSE");
  oled.display();

  /* Setup Servo */
  servo.attach(servoPin, 500, 2400);

  /* Setup Buttons */
  pinMode(BUTTON_1_PIN, INPUT_PULLUP);
  pinMode(BUTTON_2_PIN, INPUT_PULLUP);
  pinMode(BUTTON_3_PIN, INPUT_PULLUP);

  /* Setup LEDs */
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);

  /* Setup RTC Clock */
  if (!rtc.begin()) {
    Serial.println("RTC not found");
    while (1);
  }
  if (!rtc.isrunning()) {
    Serial.println("RTC is NOT running!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  /* Setup DHT Sensor */
  dhtSensor.setup(DHT_PIN, DHTesp::DHT22);

  /* Setup LDR */
  pinMode(LDR_PIN, INPUT);

  /* Setup Buzzer */
  pinMode(BUZZER_PIN, OUTPUT);
}

void loop() {
  checkButtons();
  switch (currentMode) {
    case 0:
      displayTime();
      break;
    case 1:
      displayDHTData();
      break;
    case 2:
      displayLDRData();
      break;
    case 3:
      setAlarm();
      break;
  }
  checkAlarm();
  if (millis() - lastButtonPress > 30000) {
    currentMode = 0;
  }
}

void checkButtons() {
  if (digitalRead(BUTTON_1_PIN) == LOW) {
    delay(50);  // Debounce
    if (digitalRead(BUTTON_1_PIN) == LOW) {
      unsigned long pressStart = millis();
      while (digitalRead(BUTTON_1_PIN) == LOW) {
        if (millis() - pressStart > 1000) {
          // Long press
          currentMode = 0;
          return;
        }
      }
      // Short press
      currentMode = (currentMode + 1) % 4;
      lastButtonPress = millis();
    }
  }

  if (digitalRead(BUTTON_2_PIN) == LOW) {
    delay(50);  // Debounce
    if (digitalRead(BUTTON_2_PIN) == LOW) {
      menuOption--;
      if (menuOption < 0) menuOption = 2;
      lastButtonPress = millis();
    }
  }

  if (digitalRead(BUTTON_3_PIN) == LOW) {
    delay(50);  // Debounce
    if (digitalRead(BUTTON_3_PIN) == LOW) {
      menuOption++;
      if (menuOption > 2) menuOption = 0;
      lastButtonPress = millis();
    }
  }
}

void displayTime() {
  DateTime now = rtc.now();
  
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.print(daysOfWeek[now.dayOfTheWeek()]);
  oled.print(", ");
  oled.print(now.day());
  oled.print('/');
  oled.print(now.month());
  oled.print('/');
  oled.println(now.year());
  
  oled.setTextSize(2);
  oled.setCursor(20, 30);
  oled.print(now.hour());
  oled.print(':');
  oled.print(now.minute());
  oled.print(':');
  oled.print(now.second());
  
  oled.display();
}

void displayDHTData() {
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.println("DHT Sensor Data");
  oled.print("Temp: ");
  oled.print(data.temperature, 1);
  oled.println(" C");
  oled.print("Humidity: ");
  oled.print(data.humidity, 1);
  oled.println("%");
  
  String status;
  if (data.temperature < 20) status = "Dingin";
  else if (data.temperature < 25) status = "Netral";
  else if (data.temperature < 30) status = "Hangat";
  else status = "Panas";
  
  oled.println(status);
  oled.display();

  // Control servo and LEDs based on temperature
  int servoAngle = map(data.temperature, 0, 50, 0, 180);
  servo.write(servoAngle);

  if (data.temperature < 25) {
    digitalWrite(RED_LED_PIN, HIGH);
    digitalWrite(GREEN_LED_PIN, LOW);
  } else {
    digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(GREEN_LED_PIN, HIGH);
  }

  for (int i = 0; i < SCREEN_WIDTH; i += 4) {
    oled.drawLine(i, SCREEN_HEIGHT - 1, i, SCREEN_HEIGHT - 1 - (data.temperature / 50.0 * SCREEN_HEIGHT), WHITE);
    oled.display();
    delay(10);
  }
}

void displayLDRData() {
  int analogValue = analogRead(LDR_PIN);
  float voltage = analogValue / 1024. * 5;
  float resistance = 2000 * voltage / (1 - voltage / 5);
  float lux = pow(RL10 * 1e3 * pow(10, GAMMA) / resistance, (1 / GAMMA));
  
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.println("LDR Sensor Data");
  oled.print("Light: ");
  oled.print(lux, 2);
  oled.println(" lux");
  
  String status = (lux < 50) ? "Gelap" : "Terang";
  oled.println(status);
  oled.display();

  // Control servo and LEDs based on light intensity
  int servoAngle = map(lux, 0, 1000, 0, 180);
  servo.write(servoAngle);

  if (lux < 50) {
    digitalWrite(RED_LED_PIN, HIGH);
    digitalWrite(GREEN_LED_PIN, LOW);
  } else {
    digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(GREEN_LED_PIN, HIGH);
  }

  float middleLux = 500; // Sesuaikan dengan kebutuhan
  if (lux < middleLux) {
    digitalWrite(RED_LED_PIN, HIGH);
    digitalWrite(GREEN_LED_PIN, LOW);
  } else {
    digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(GREEN_LED_PIN, HIGH);
  }
}

void setAlarm() {
  DateTime now = rtc.now();
  
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.println("Set Alarm");
  oled.print("Current: ");
  oled.print(now.hour());
  oled.print(':');
  oled.println(now.minute());
  
  int alarmHour = (alarmTime / 3600) % 24;
  int alarmMinute = (alarmTime / 60) % 60;
  
  oled.print("Alarm: ");
  oled.print(alarmHour);
  oled.print(':');
  oled.println(alarmMinute);
  
  oled.println(menuOption == 0 ? "> Hour" : "  Hour");
  oled.println(menuOption == 1 ? "> Minute" : "  Minute");
  oled.println(menuOption == 2 ? "> Save" : "  Save");
  
  oled.display();

  if (digitalRead(BUTTON_2_PIN) == LOW) {
    if (menuOption == 0) alarmHour = (alarmHour + 1) % 24;
    if (menuOption == 1) alarmMinute = (alarmMinute + 1) % 60;
    if (menuOption == 2) alarmTime = alarmHour * 3600 + alarmMinute * 60;
  }

  if (menuOption == 2 && digitalRead(BUTTON_2_PIN) == LOW) {
    alarmTime = alarmHour * 3600 + alarmMinute * 60;
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setCursor(0, 20);
    oled.println("Alarm Saved!");
    oled.display();
    delay(2000);
  }
}

void checkAlarm() {
  DateTime now = rtc.now();
  unsigned long currentTime = now.hour() * 3600 + now.minute() * 60 + now.second();
  if (currentTime == alarmTime) {
    oled.clearDisplay();
    oled.setTextSize(2);
    oled.setCursor(20, 20);
    oled.println("Alarm!");
    oled.display();
    playAlarmMelody();
    delay(5000); // Display for 5 seconds
  }
}

void playAlarmMelody() {
  int melody[] = {262, 330, 392, 523};
  int noteDurations[] = {4, 4, 4, 4};

  for (int i = 0; i < 4; i++) {
    int noteDuration = 1000 / noteDurations[i];
    tone(BUZZER_PIN, melody[i], noteDuration);
    int pauseBetweenNotes = noteDuration * 1.30;
    delay(pauseBetweenNotes);
    noTone(BUZZER_PIN);
  }
}