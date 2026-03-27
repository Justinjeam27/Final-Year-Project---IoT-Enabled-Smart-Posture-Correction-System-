/*
 * PROJECT: IoT-Enabled Smart Posture Correction System with Real-Time Activity Monitoring and Analysis
 * AUTHOR: Justin Jeam Crisostomo
 * HARDWARE: Seeed Studio XIAO ESP32-S3, MPU-6050, Active Buzzer
 * DESCRIPTION: Upgraded from PDE3116. Native Wi-Fi, ThingSpeak HTTP client, and Blynk App Integration.
 * NOTE: Strict algorithmic filtering used. No AI or Machine Learning implemented.
 */

// ================================================================
// 1. BLYNK CONFIGURATION (MUST BE AT THE VERY TOP)
// ================================================================
#define BLYNK_TEMPLATE_ID "TMPL_Your_ID_Here"
#define BLYNK_TEMPLATE_NAME "Smart Posture Corrector"
#define BLYNK_AUTH_TOKEN "Your_Blynk_Auth_Token_Here"

// ================================================================
// 2. LIBRARIES
// ================================================================
#include <WiFi.h>
#include <HTTPClient.h>
#include <BlynkSimpleEsp32.h> // Blynk library for ESP32
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// ================================================================
// 3. HARDWARE CONFIGURATION
// ================================================================
// On the XIAO ESP32-S3, Pin D3 corresponds to GPIO 4
const int BUZZER_PIN = 4; 
Adafruit_MPU6050 mpu;   

// ================================================================
// 4. WI-FI & CLOUD CREDENTIALS
// ================================================================
char ssid[] = "Type_Wifi_Name_here";      
char pass[] = "Type_Wifi_Password_here"; 

// ThingSpeak Settings
String apiKey = "Type_your_API_Key"; 
String serverName = "http://api.thingspeak.com/update"; 

// ================================================================
// 5. POSTURE LOGIC VARIABLES
// ================================================================
const float SLOUCH_THRESHOLD = 25.0; // Angle (degrees) that triggers the alarm
float baselineAngle = 0;             // Stores the user's "perfect posture" angle

// ================================================================
// 6. TIMER VARIABLES (Non-blocking delay)
// ================================================================
unsigned long lastThingSpeakTime = 0;    
const unsigned long thingspeakInterval = 20000; // 20 seconds for ThingSpeak

unsigned long lastBlynkTime = 0;
const unsigned long blynkInterval = 1000;       // 1 second refresh for Blynk app

void setup() {
  Serial.begin(115200);
  
  // Initialize Output Pins
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  Serial.println("--- SMART POSTURE CORRECTOR STARTING ---");

  // Initialize the MPU-6050 Sensor
  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");
    while (1) { 
      digitalWrite(BUZZER_PIN, HIGH); delay(100); 
      digitalWrite(BUZZER_PIN, LOW); delay(100); 
    }
  }
  Serial.println("MPU6050 Found!");
  
  // Set Sensor Sensitivity
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  // Connect to Wi-Fi and Blynk simultaneously
  Serial.println("Connecting to Wi-Fi and Blynk Cloud...");
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  // Set the baseline posture
  calibratePosture();  
}

void loop() {
  // Keep the Blynk connection alive
  Blynk.run(); 

  // 1. READ SENSOR DATA
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  // 2. CALCULATE POSTURE ANGLE
  // Converts X and Z acceleration into an angle measuring neck/spine tilt
  float currentAngle = atan2(a.acceleration.x, a.acceleration.z) * 57.29578; 
  float slouchAngle = abs(currentAngle - baselineAngle);

  // 3. HAPTIC FEEDBACK LOGIC
  if (slouchAngle > SLOUCH_THRESHOLD) {
    digitalWrite(BUZZER_PIN, HIGH); // Bad Posture: Buzz ON
  } else {
    digitalWrite(BUZZER_PIN, LOW);  // Good Posture: Buzz OFF
  }

  // 4. BLYNK APP UPLOAD (Every 1 second for real-time feel)
  if (millis() - lastBlynkTime > blynkInterval) {
    // Send live angle to Virtual Pin V0
    Blynk.virtualWrite(V0, slouchAngle); 
    
    // Send Text Status to Virtual Pin V1
    if (slouchAngle > SLOUCH_THRESHOLD) {
      Blynk.virtualWrite(V1, "SLOUCHING DETECTED!");
    } else {
      Blynk.virtualWrite(V1, "GOOD POSTURE");
    }
    lastBlynkTime = millis();
  }

  // 5. THINGSPEAK CLOUD UPLOAD (Every 20 seconds)
  if (millis() - lastThingSpeakTime > thingspeakInterval) {
    sendToThingSpeak(slouchAngle);
    lastThingSpeakTime = millis(); 
  }
  
  delay(50); // Small stability delay
}

// ================================================================
// HELPER FUNCTIONS
// ================================================================

void calibratePosture() {
  Serial.println(">>> CALIBRATION: Stand Straight for 3 Seconds... <<<");
  digitalWrite(BUZZER_PIN, HIGH); delay(100); digitalWrite(BUZZER_PIN, LOW);
  delay(3000); 
  
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  baselineAngle = atan2(a.acceleration.x, a.acceleration.z) * 57.29578;
  
  Serial.print("Baseline set to: "); Serial.println(baselineAngle);
  digitalWrite(BUZZER_PIN, HIGH); delay(600); digitalWrite(BUZZER_PIN, LOW);
}

void sendToThingSpeak(float angle) {
  // Check if connected to Wi-Fi before sending
  if(WiFi.status() == WL_CONNECTED){
    HTTPClient http;
    
    // Construct the full URL with your API key and data
    String serverPath = serverName + "?api_key=" + apiKey + "&field1=" + String(angle);
    
    // Begin HTTP connection and send GET request
    http.begin(serverPath.c_str());
    int httpResponseCode = http.GET();
    
    if (httpResponseCode > 0) {
      Serial.print("ThingSpeak Upload Success. HTTP Response code: ");
      Serial.println(httpResponseCode);
    } else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
    }
    // Free resources
    http.end();
  } else {
    Serial.println("Wi-Fi Disconnected. Cannot send to ThingSpeak.");
  }
}