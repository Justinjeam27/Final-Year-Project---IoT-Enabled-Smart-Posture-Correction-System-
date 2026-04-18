/*
 * PROJECT: IoT-Enabled Smart Posture Correction System with Real-Time Activity Monitoring and Analysis - Physical Prototype
 * AUTHOR: Justin Jeam Crisostomo
 * HARDWARE: Seeed Studio XIAO ESP32-S3, MPU-6050, Active Buzzer
 * DESCRIPTION: Upgraded from PDE3116. Native Wi-Fi, ThingSpeak HTTP client, and Blynk App Integration.
 * NOTE: Strict algorithmic filtering used. No AI or Machine Learning implemented.
 */

// ================================================================
// 1. BLYNK CONFIGURATION
// ================================================================
#define BLYNK_TEMPLATE_ID "ENTER_TEMPLATE_ID"
#define BLYNK_TEMPLATE_NAME "ENTER_TEMPLATE_NAME"
#define BLYNK_AUTH_TOKEN "ENTER_AUTH_TOKEN"

// ================================================================
// 2. LIBRARIES
// ================================================================
#include <WiFi.h>
#include <HTTPClient.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
// NOTE: Adafruit Libraries completely removed to bypass clone chip block.

// ================================================================
// 3. HARDWARE CONFIGURATION
// ================================================================
const int BUZZER_PIN = 4; // GPIO 4 (D3 on XIAO)
const uint8_t NECK_ADDR = 0x68;
const uint8_t SPINE_ADDR = 0x69;

// ================================================================
// 4. PHYSICAL WI-FI CREDENTIALS
// ================================================================
char ssid[] = "ENTER_WI-FI_SSID"; 
char pass[] = "ENTER_WI-FI_PASSWORD"; 

String apiKey = "ENTER_API_KEY"; 
String serverName = "ENTER_THINKSPEAK_SERVERNAME"; 

// ================================================================
// 5. POSTURE LOGIC VARIABLES
// ================================================================
const float SLOUCH_THRESHOLD = 25.0; 
float baselineNeck = 0;
float baselineSpine = 0;
int systemActive = 1; // 1 = ON, 0 = PAUSED

// ================================================================
// 6. TIMER VARIABLES
// ================================================================
unsigned long lastThingSpeakTime = 0;
const unsigned long thingspeakInterval = 20000; 

unsigned long lastBlynkTime = 0;
const unsigned long blynkInterval = 1000;

// ================================================================
// 7. BLYNK INTERRUPTS
// ================================================================
BLYNK_CONNECTED() {
  Blynk.syncVirtual(V2); 
}

BLYNK_WRITE(V2) {
  systemActive = param.asInt();
  if (systemActive == 0) {
    digitalWrite(BUZZER_PIN, LOW); 
    Serial.println(">>> SYSTEM PAUSED VIA MOBILE APP <<<");
  } else {
    Serial.println(">>> SYSTEM RESUMED VIA MOBILE APP <<<");
  }
}

// ================================================================
// RAW I2C SENSOR READING FUNCTION
// ================================================================
float getRawAngle(uint8_t address, bool isNeck) {
  Wire.beginTransmission(address);
  Wire.write(0x3B); // Starting register for Accelerometer
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)address, (uint8_t)6);
  
  // Combine High and Low bytes
  int16_t ax = Wire.read() << 8 | Wire.read();
  int16_t ay = Wire.read() << 8 | Wire.read();
  int16_t az = Wire.read() << 8 | Wire.read();
  
  if (isNeck) {
    return atan2(ax, az) * 57.29578;
  } else {
    return atan2(ay, az) * 57.29578;
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial); 

  Serial.println("\n--- INITIATING SYSTEM (RAW I2C MODE) ---");

  Wire.begin(); 
  Wire.setClock(100000); 
  delay(500); 

  // Wake up Neck Sensor
  Wire.beginTransmission(NECK_ADDR);
  Wire.write(0x6B); // Power Management Register
  Wire.write(0x00); // Wake up command
  Wire.endTransmission(true);
  delay(50);
  
  // Set Neck to 8G Range
  Wire.beginTransmission(NECK_ADDR);
  Wire.write(0x1C);
  Wire.write(0x10);
  Wire.endTransmission(true);

  // Wake up Spine Sensor
  Wire.beginTransmission(SPINE_ADDR);
  Wire.write(0x6B);
  Wire.write(0x00); 
  Wire.endTransmission(true);
  delay(50);
  
  // Set Spine to 8G Range
  Wire.beginTransmission(SPINE_ADDR);
  Wire.write(0x1C);
  Wire.write(0x10);
  Wire.endTransmission(true);

  Serial.println("Sensors Awake! Adafruit Library Bypassed.");

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  Serial.print("Connecting to Blynk Cloud...");
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  Serial.println("\nBlynk Connected!");
  
  calibratePosture();  
}

void loop() {
  Blynk.run(); 

  // Read Raw Angles
  float currentNeckAngle = getRawAngle(NECK_ADDR, true);
  float currentSpineAngle = getRawAngle(SPINE_ADDR, false);

  // Differential Calculation
  float neckSlouch = currentNeckAngle - baselineNeck;
  float spineSlouch = currentSpineAngle - baselineSpine;
  float netSlouch = abs(neckSlouch - spineSlouch);

  if (systemActive == 1) {
    Serial.print("Neck:"); Serial.print(neckSlouch);
    Serial.print(", Spine:"); Serial.print(spineSlouch);
    Serial.print(", Net_Slouch:"); Serial.println(netSlouch);

    // Haptic Logic
    if (netSlouch > SLOUCH_THRESHOLD) {
      digitalWrite(BUZZER_PIN, HIGH);
    } else {
      digitalWrite(BUZZER_PIN, LOW);
    }

    // Blynk Upload
    if (millis() - lastBlynkTime > blynkInterval) {
      Blynk.virtualWrite(V0, netSlouch);
      Blynk.virtualWrite(V1, (netSlouch > SLOUCH_THRESHOLD) ? "SLOUCHING DETECTED!" : "GOOD POSTURE");
      lastBlynkTime = millis();
    }

    // ThingSpeak Upload
    if (millis() - lastThingSpeakTime > thingspeakInterval) {
      sendToThingSpeak(neckSlouch, spineSlouch, netSlouch);
      lastThingSpeakTime = millis(); 
    }
  } else {
    if (millis() - lastBlynkTime > blynkInterval) {
      Blynk.virtualWrite(V0, 0);
      Blynk.virtualWrite(V1, "SYSTEM PAUSED"); 
      lastBlynkTime = millis();
    }
  }
  delay(100);
}

void calibratePosture() {
  Serial.println(">>> CALIBRATION: Stand Straight for 5 seconds <<<");
  digitalWrite(BUZZER_PIN, HIGH); delay(100); digitalWrite(BUZZER_PIN, LOW);
  delay(5000);

  baselineNeck = getRawAngle(NECK_ADDR, true);
  baselineSpine = getRawAngle(SPINE_ADDR, false);
  
  digitalWrite(BUZZER_PIN, HIGH); delay(600); digitalWrite(BUZZER_PIN, LOW);
}

void sendToThingSpeak(float neck, float spine, float net) {
  if(WiFi.status() == WL_CONNECTED){
    HTTPClient http;
    String serverPath = serverName + "?api_key=" + apiKey 
                      + "&field1=" + String(neck) 
                      + "&field2=" + String(spine) 
                      + "&field3=" + String(net);
    http.begin(serverPath.c_str());
    http.GET();
    http.end();
  }
}
