/*
 * PROJECT: IoT-Enabled Smart Posture Correction System with Real-Time Activity Monitoring and Analysis
 * AUTHOR: Justin Jeam Crisostomo
 * HARDWARE: Seeed Studio XIAO ESP32-S3, 2x MPU-6050, Active Buzzer
 * DESCRIPTION: Upgraded for differential analysis between Neck and Spine sensors.
 */

// ================================================================
// 1. BLYNK CONFIGURATION (LIVE)
// ================================================================
#define BLYNK_TEMPLATE_ID "TMPL6cws0IBHu"
#define BLYNK_TEMPLATE_NAME "Smart Posture Corrector"
#define BLYNK_AUTH_TOKEN "z78wYM7OS53a_aGUs-QrEOcB0_hh3_Cj"

// ================================================================
// 2. LIBRARIES
// ================================
#include <WiFi.h>
#include <HTTPClient.h>
#include <BlynkSimpleEsp32.h> // Blynk is now active!
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// ================================================================
// 3. HARDWARE CONFIGURATION
// ================================================================
const int BUZZER_PIN = 4; // GPIO 4 (D3 on XIAO)
Adafruit_MPU6050 mpuNeck;  // Default I2C address: 0x68
Adafruit_MPU6050 mpuSpine; // Bridged I2C address: 0x69 (AD0 to VCC)

// ================================================================
// 4. WI-FI & CLOUD CREDENTIALS
// ================================================================
char ssid[] = "Wokwi-GUEST";      
char pass[] = ""; 

String apiKey = "PCGMNVIU22O69KF5"; 
String serverName = "http://api.thingspeak.com/update"; 

// ================================================================
// 5. POSTURE LOGIC VARIABLES
// ================================================================
const float SLOUCH_THRESHOLD = 25.0; 
float baselineNeck = 0;
float baselineSpine = 0;

// --- NEW STANDBY VARIABLE ---
int systemActive = 1; // 1 = ON, 0 = PAUSED
// ----------------------------

// ================================================================
// 6. TIMER VARIABLES
// ================================================================
unsigned long lastThingSpeakTime = 0;    
const unsigned long thingspeakInterval = 20000; 

unsigned long lastBlynkTime = 0;
const unsigned long blynkInterval = 1000; 


// ================================================================
// 7. NEW BLYNK FUNCTIONS (CLOUD -> HARDWARE)
// ================================================================
// This grabs the switch state from the cloud when it boots up
BLYNK_CONNECTED() {
  Blynk.syncVirtual(V2); 
}

// This runs instantly whenever you press the switch on your phone
BLYNK_WRITE(V2) {
  systemActive = param.asInt(); // Reads the 0 or 1 from the app
  if (systemActive == 0) {
    digitalWrite(BUZZER_PIN, LOW); // Force buzzer off immediately
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

  Serial.println("--- DUAL-IMU POSTURE SYSTEM STARTING ---");

  Wire.begin(5, 6); 

  if (!mpuNeck.begin(0x68)) {
    Serial.println("Neck Found Error!");
    while (1) { yield(); }
  }
  Serial.println("Neck Found!");

  if (!mpuSpine.begin(0x69)) {
    Serial.println("Spine Found Error!");
    while (1) { yield(); }
  }
  Serial.println("Spine Found!");
  
  mpuNeck.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpuNeck.setFilterBandwidth(MPU6050_BAND_21_HZ);
  mpuSpine.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpuSpine.setFilterBandwidth(MPU6050_BAND_21_HZ);

  // --- BLYNK CLOUD & WI-FI UPLINK ---
  Serial.print("Connecting to Blynk Cloud...");
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  Serial.println("\nBlynk Connected!");
  // ----------------------------------
  
  calibratePosture();  
}

void loop() {
  Blynk.run(); // Keeps the connection to your app alive

  // 1. READ BOTH SENSORS
  sensors_event_t nA, nG, nTemp;
  sensors_event_t sA, sG, sTemp;
  mpuNeck.getEvent(&nA, &nG, &nTemp);
  mpuSpine.getEvent(&sA, &sG, &sTemp);

  // 2. CALCULATE ANGLES
  float currentNeckAngle = atan2(nA.acceleration.x, nA.acceleration.z) * 57.29578; 
  float currentSpineAngle = atan2(sA.acceleration.x, sA.acceleration.z) * 57.29578;

  // 3. DIFFERENTIAL ANALYSIS
  float neckSlouch = currentNeckAngle - baselineNeck;
  float spineSlouch = currentSpineAngle - baselineSpine;
  float netSlouch = neckSlouch - spineSlouch;


  // --- LOGIC SPLIT BASED ON SYSTEM ACTIVE STATUS ---
  if (systemActive == 1) {
    
    // 4. LIVE SERIAL PLOTTER DATA
    Serial.print("Neck:"); Serial.print(neckSlouch);
    Serial.print(", Spine:"); Serial.print(spineSlouch);
    Serial.print(", Net_Slouch:"); Serial.println(netSlouch);

    // 5. HAPTIC FEEDBACK
    if (netSlouch > SLOUCH_THRESHOLD) {
      digitalWrite(BUZZER_PIN, HIGH);
    } else {
      digitalWrite(BUZZER_PIN, LOW);
    }

    // 6. BLYNK DASHBOARD UPLOAD (Fast Update: 1 second)
    if (millis() - lastBlynkTime > blynkInterval) {
      Blynk.virtualWrite(V0, netSlouch); 
      Blynk.virtualWrite(V1, (netSlouch > SLOUCH_THRESHOLD) ? "SLOUCHING DETECTED!" : "GOOD POSTURE");
      lastBlynkTime = millis();
    }

    // 7. THINGSPEAK UPLOAD (Slow Update: 20 seconds)
    if (millis() - lastThingSpeakTime > thingspeakInterval) {
      sendToThingSpeak(neckSlouch, spineSlouch, netSlouch);
      lastThingSpeakTime = millis(); 
    }

  } else {
    // IF SYSTEM IS PAUSED
    if (millis() - lastBlynkTime > blynkInterval) {
      Blynk.virtualWrite(V0, 0); // Drop gauge to zero
      Blynk.virtualWrite(V1, "SYSTEM PAUSED"); // Update label to show standby mode
      lastBlynkTime = millis();
    }
  }
  
  delay(100); 
}

void calibratePosture() {
  Serial.println(">>> CALIBRATION: Stand Straight for 3 Seconds... <<<");
  digitalWrite(BUZZER_PIN, HIGH); delay(100); digitalWrite(BUZZER_PIN, LOW);
  delay(3000); 
  
  sensors_event_t nA, nG, nT;
  sensors_event_t sA, sG, sT;
  mpuNeck.getEvent(&nA, &nG, &nT);
  mpuSpine.getEvent(&sA, &sG, &sT);
  
  baselineNeck = atan2(nA.acceleration.x, nA.acceleration.z) * 57.29578;
  baselineSpine = atan2(sA.acceleration.x, sA.acceleration.z) * 57.29578;
  
  Serial.print("Baselines set - Neck: "); Serial.print(baselineNeck);
  Serial.print(" | Spine: "); Serial.println(baselineSpine);
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
    int httpResponseCode = http.GET();
    if (httpResponseCode > 0) {
      Serial.print(">>> ThingSpeak Upload Success! Code: "); Serial.println(httpResponseCode);
    }
    http.end();
  }
}
