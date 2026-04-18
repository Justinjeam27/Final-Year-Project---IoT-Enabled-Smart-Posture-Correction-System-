/*
 * PROJECT: IoT-Enabled Smart Posture Correction System with Real-Time Activity Monitoring and Analysis
 * AUTHOR: Justin Jeam Crisostomo
 * HARDWARE: Seeed Studio XIAO ESP32-S3, Dual MPU-6050, Active Buzzer
 * DESCRIPTION: Hardware implementation using pure kinematic logic (No AI/ML). 
 * Utilizes low-level I2C commands to bypass standard library limitations for identical sensor addresses.
 */

// ================================================================
// 1. BLYNK CLOUD CONFIGURATION (REDACTED FOR REPOSITORY)
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
// Note: Standard Adafruit MPU6050 libraries removed to prevent WHO_AM_I register conflicts.

// ================================================================
// 3. HARDWARE CONFIGURATION
// ================================================================
const int BUZZER_PIN = 4; // Haptic feedback pin (GPIO 4 / D3)
const uint8_t NECK_ADDR = 0x68;  // Default I2C address
const uint8_t SPINE_ADDR = 0x69; // Bridged I2C address (AD0 pulled HIGH)

// ================================================================
// 4. WI-FI & THINGSPEAK CREDENTIALS (REDACTED)
// ================================================================
char ssid[] = "ENTER_WI-FI_SSID"; 
char pass[] = "ENTER_WI-FI_PASSWORD"; 
String apiKey = "ENTER_API_KEY"; 
String serverName = "ENTER_THINKSPEAK_SERVERNAME"; 

// ================================================================
// 5. POSTURE LOGIC VARIABLES
// ================================================================
const float SLOUCH_THRESHOLD = 25.0; // Angle differential limit
float baselineNeck = 0;
float baselineSpine = 0;
int systemActive = 1; // Tracks Blynk manual interrupt: 1 = ON, 0 = PAUSED

// ================================================================
// 6. TIMER VARIABLES (Non-blocking delays)
// ================================================================
unsigned long lastThingSpeakTime = 0;
const unsigned long thingspeakInterval = 20000; // 20-second cloud push

unsigned long lastBlynkTime = 0;
const unsigned long blynkInterval = 1000; // 1-second app refresh

// ================================================================
// 7. BLYNK CLOUD INTERRUPTS
// ================================================================
BLYNK_CONNECTED() {
  Blynk.syncVirtual(V2); // Syncs switch state on boot
}

BLYNK_WRITE(V2) {
  systemActive = param.asInt(); // Reads virtual switch from mobile app
  if (systemActive == 0) {
    digitalWrite(BUZZER_PIN, LOW); // Failsafe: silence buzzer if paused
    Serial.println(">>> SYSTEM PAUSED VIA MOBILE APP <<<");
  } else {
    Serial.println(">>> SYSTEM RESUMED VIA MOBILE APP <<<");
  }
}

// ================================================================
// 8. RAW I2C SENSOR READING FUNCTION
// ================================================================
// Extracts accelerometer data directly via I2C registers
float getRawAngle(uint8_t address, bool isNeck) {
  Wire.beginTransmission(address);
  Wire.write(0x3B); // Starting register for Accelerometer
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)address, (uint8_t)6);
  
  // Combine High and Low bytes
  int16_t ax = Wire.read() << 8 | Wire.read();
  int16_t ay = Wire.read() << 8 | Wire.read();
  int16_t az = Wire.read() << 8 | Wire.read();
  
  // Convert acceleration vectors to degrees
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
  Wire.setClock(100000); // Standard I2C speed
  delay(500); 

  // Wake up & configure Neck Sensor (0x68)
  Wire.beginTransmission(NECK_ADDR);
  Wire.write(0x6B); // Power Management Register
  Wire.write(0x00); // Wake up command
  Wire.endTransmission(true);
  delay(50);
  Wire.beginTransmission(NECK_ADDR);
  Wire.write(0x1C);
  Wire.write(0x10); // Set to 8G Range
  Wire.endTransmission(true);

  // Wake up & configure Spine Sensor (0x69)
  Wire.beginTransmission(SPINE_ADDR);
  Wire.write(0x6B);
  Wire.write(0x00); 
  Wire.endTransmission(true);
  delay(50);
  Wire.beginTransmission(SPINE_ADDR);
  Wire.write(0x1C);
  Wire.write(0x10); // Set to 8G Range
  Wire.endTransmission(true);

  Serial.println("Sensors Awake! Custom I2C bridge active.");

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  Serial.print("Connecting to Blynk Cloud...");
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  Serial.println("\nBlynk Connected!");
  
  calibratePosture();  
}

void loop() {
  Blynk.run(); // Maintain IoT connection

  // Read current angles from both physical sensors
  float currentNeckAngle = getRawAngle(NECK_ADDR, true);
  float currentSpineAngle = getRawAngle(SPINE_ADDR, false);

  // PURE LOGIC DIFFERENTIAL CALCULATION
  float neckSlouch = currentNeckAngle - baselineNeck;
  float spineSlouch = currentSpineAngle - baselineSpine;
  float netSlouch = abs(neckSlouch - spineSlouch); // Cancels out whole-body bending

  if (systemActive == 1) {
    Serial.print("Neck:"); Serial.print(neckSlouch);
    Serial.print(", Spine:"); Serial.print(spineSlouch);
    Serial.print(", Net_Slouch:"); Serial.println(netSlouch);

    // Deterministic Haptic Feedback
    if (netSlouch > SLOUCH_THRESHOLD) {
      digitalWrite(BUZZER_PIN, HIGH);
    } else {
      digitalWrite(BUZZER_PIN, LOW);
    }

    // Fast Update: Push to Blynk Dashboard
    if (millis() - lastBlynkTime > blynkInterval) {
      Blynk.virtualWrite(V0, netSlouch);
      Blynk.virtualWrite(V1, (netSlouch > SLOUCH_THRESHOLD) ? "SLOUCHING DETECTED!" : "GOOD POSTURE");
      lastBlynkTime = millis();
    }

    // Slow Update: Push to ThingSpeak Database
    if (millis() - lastThingSpeakTime > thingspeakInterval) {
      sendToThingSpeak(neckSlouch, spineSlouch, netSlouch);
      lastThingSpeakTime = millis(); 
    }
  } else {
    // If system is paused via app
    if (millis() - lastBlynkTime > blynkInterval) {
      Blynk.virtualWrite(V0, 0);
      Blynk.virtualWrite(V1, "SYSTEM PAUSED"); 
      lastBlynkTime = millis();
    }
  }
  delay(100);
}

// Establishes the 'zero' coordinate baseline upon boot
void calibratePosture() {
  Serial.println(">>> CALIBRATION: Stand Straight for 5 seconds <<<");
  digitalWrite(BUZZER_PIN, HIGH); delay(100); digitalWrite(BUZZER_PIN, LOW);
  delay(5000);

  baselineNeck = getRawAngle(NECK_ADDR, true);
  baselineSpine = getRawAngle(SPINE_ADDR, false);
  
  digitalWrite(BUZZER_PIN, HIGH); delay(600); digitalWrite(BUZZER_PIN, LOW);
}

// Constructs and executes the HTTP GET request to ThingSpeak
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
