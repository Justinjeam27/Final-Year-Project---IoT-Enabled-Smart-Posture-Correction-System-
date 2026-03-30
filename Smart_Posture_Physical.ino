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
#define BLYNK_TEMPLATE_ID "TMPL6cws0IBHu"
#define BLYNK_TEMPLATE_NAME "Smart Posture Corrector"
#define BLYNK_AUTH_TOKEN "z78wYM7OS53a_aGUs-QrEOcB0_hh3_Cj"

// ================================================================
// 2. LIBRARIES
// ================================================================
#include <WiFi.h>
#include <HTTPClient.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// ================================================================
// 3. HARDWARE CONFIGURATION
// ================================================================
const int BUZZER_PIN = 4;  // GPIO 4 (D3 on XIAO)
Adafruit_MPU6050 mpuNeck;  // Address 0x68
Adafruit_MPU6050 mpuSpine; // Address 0x69 (AD0 to VCC)

// ================================================================
// 4. PHYSICAL WI-FI CREDENTIALS
// ================================================================
char ssid[] = "YOUR_WIFI_NAME";      // <--- Update this to your real WiFi Name
char pass[] = "YOUR_WIFI_PASSWORD";  // <--- Update this to your real WiFi Password

String apiKey = "PCGMNVIU22O69KF5"; 
String serverName = "http://api.thingspeak.com/update"; 

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

void setup() {
  Serial.begin(115200);
  delay(1000); 
  
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  Serial.println("--- PHYSICAL DUAL-IMU SYSTEM STARTING ---");

  // SDA = GPIO 5 (D4), SCL = GPIO 6 (D5)
  Wire.begin(5, 6); 

  if (!mpuNeck.begin(0x68)) {
    Serial.println("Neck Found Error!");
    while (1) { yield(); }
  }
  if (!mpuSpine.begin(0x69)) {
    Serial.println("Spine Found Error!");
    while (1) { yield(); }
  }
  
  mpuNeck.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpuNeck.setFilterBandwidth(MPU6050_BAND_21_HZ);
  mpuSpine.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpuSpine.setFilterBandwidth(MPU6050_BAND_21_HZ);

  // Connection handshake
  Serial.print("Connecting to Blynk Cloud...");
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  Serial.println("\nBlynk Connected!");
  
  calibratePosture();  
}

void loop() {
  Blynk.run(); 

  sensors_event_t nA, nG, nTemp;
  sensors_event_t sA, sG, sTemp;
  mpuNeck.getEvent(&nA, &nG, &nTemp);
  mpuSpine.getEvent(&sA, &sG, &sTemp);

  // Calculation for Physical Orientation
  float currentNeckAngle = atan2(nA.acceleration.x, nA.acceleration.z) * 57.29578; 
  float currentSpineAngle = atan2(sA.acceleration.x, sA.acceleration.z) * 57.29578;

  // Fixed Differential Analysis with abs()
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
  Serial.println(">>> CALIBRATION: Stand Straight... <<<");
  digitalWrite(BUZZER_PIN, HIGH); delay(100); digitalWrite(BUZZER_PIN, LOW);
  delay(3000); 
  
  sensors_event_t nA, nG, nT;
  sensors_event_t sA, sG, sT;
  mpuNeck.getEvent(&nA, &nG, &nT);
  mpuSpine.getEvent(&sA, &sG, &sT);
  
  baselineNeck = atan2(nA.acceleration.x, nA.acceleration.z) * 57.29578;
  baselineSpine = atan2(sA.acceleration.x, sA.acceleration.z) * 57.29578;
  
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
