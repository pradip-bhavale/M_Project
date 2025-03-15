#include<Arduino.h>
#include <WiFi.h>
#include <EEPROM.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// WiFi credentials
const char* ssid = "vivo 1935";
const char* password = "";

// Firebase credentials
#define DATABASE_URL "https://ioitive-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define API_KEY "AIzaSyDXgOB3Dfuf3AwOPvoUWL7ChoHfs6AYZm8"
#define USER_NAME "bhavalepradip@gmail.com"
#define USER_PASS "Bhavale@123"

// Firebase objects
FirebaseData firebaseData;
FirebaseAuth firebaseAuth;
FirebaseConfig firebaseConfig;

// Device Pin Configuration
#define portCount 6
int portNumber[portCount] = {12, 13, 14, 25, 26, 27};
bool localStates[portCount] = {false};
const char* applianceNames[portCount] = {"appliance_1", "appliance_2", "appliance_3", "appliance_4", "appliance_5", "appliance_6"};

// EEPROM Configuration
#define EEPROM_SIZE 128 
#define FLAG_START 0
#define STATE_START 1 
#define SSID_START 11 
#define PASS_START 51 

void saveStateToEEPROM(int index, bool State) {
  EEPROM.write(STATE_START + index, State ? 1 : 0);
  EEPROM.commit();
  Serial.printf("Saved Data of %s is: %s\n", applianceNames[index], State ? "ON" : "OFF");
}

bool readStateFromEEPROM(int index) {
  return EEPROM.read(STATE_START + index) == 1; 
}

void controlDevice(String devicePath, bool State) {
  int index = -1;
  for (int i = 0; i < portCount; i++) {
    if (devicePath.equals(applianceNames[i])) {
      index = i;
      break;
    }
  }

  if (index != -1) {
    digitalWrite(portNumber[index], State ? HIGH : LOW);
    Serial.printf("The %s is: %s\n", applianceNames[index], State ? "ON" : "OFF");
    localStates[index] = State;
    saveStateToEEPROM(index, State);
  } else {
    Serial.println("Invalid device path: " + devicePath);
  }
}

bool fetchLastStates() {
  Serial.println("Fetching latest states from Firebase...");
  if (!Firebase.ready()) {
    Serial.println("Firebase is not ready...");
    return false;
  }
  
  if (Firebase.RTDB.getJSON(&firebaseData, "/admin/admin_id/user/user_id/board/board_id/appliances/")) {
    FirebaseJson& json = firebaseData.jsonObject();
    FirebaseJsonData jsonData;

    for (int i = 0; i < portCount; i++) {
      if (json.get(jsonData, applianceNames[i])) {
        bool newState = jsonData.boolValue;
        controlDevice(applianceNames[i], newState);
      }
    }
    return true;
  } else {
    Serial.println("Failed to fetch states: " + firebaseData.errorReason());
    return false;
  }
}

void streamCallBack(FirebaseStream data) {
  String devicePath = data.dataPath().substring(data.dataPath().lastIndexOf("/") + 1);
  Serial.println("Device path: " + devicePath);
  bool State = data.boolData();
  controlDevice(devicePath, State);
}

void streamTimeoutCallBack(bool timeout) {
  if (timeout) {
    Serial.println("Stream timeout, reconnecting...");
    Firebase.RTDB.beginStream(&firebaseData, "/admin/");
  }
}

void resetFirebase() {
  Serial.println("Reinitializing Firebase...");
  firebaseData.clear(); 
  firebaseAuth = FirebaseAuth();
  firebaseConfig = FirebaseConfig();

  firebaseConfig.api_key = API_KEY;
  firebaseConfig.database_url = DATABASE_URL;
  firebaseAuth.user.email = USER_NAME;
  firebaseAuth.user.password = USER_PASS;

  Firebase.begin(&firebaseConfig, &firebaseAuth);
  Firebase.reconnectWiFi(true);

  Serial.println("Firebase Reinitialized.");
  
  int retryCount = 5;
  while (!Firebase.ready() && retryCount-- > 0) {
    Serial.println("Waiting for Firebase...");
    delay(1000);
  }

  if (Firebase.ready()) {
    fetchLastStates();
    if (Firebase.RTDB.beginStream(&firebaseData, "/admin/")) {
      Firebase.RTDB.setStreamCallback(&firebaseData, streamCallBack, streamTimeoutCallBack);
    } else {
      Serial.println("Failed to Start Stream: " + firebaseData.errorReason());
    }
  }
}

void reConnectToWifi() {
  Serial.print("\nReconnecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    WiFi.disconnect();
    delay(1000);
    WiFi.reconnect();
    delay(5000);
  }
  Serial.println("\nReconnected to WiFi. IP Address: " + WiFi.localIP().toString());
  resetFirebase();
}

void setup() {
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);

  for (int i = 0; i < portCount; i++) {
    pinMode(portNumber[i], OUTPUT);
    localStates[i] = readStateFromEEPROM(i);
    digitalWrite(portNumber[i], localStates[i] ? HIGH : LOW);
  }

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nConnected to WiFi. IP Address: " + WiFi.localIP().toString());

  firebaseConfig.api_key = API_KEY;
  firebaseConfig.database_url = DATABASE_URL;
  firebaseAuth.user.email = USER_NAME;
  firebaseAuth.user.password = USER_PASS;

  Firebase.begin(&firebaseConfig, &firebaseAuth);
  Firebase.reconnectWiFi(true);
  Serial.println("Firebase initialized.");

  fetchLastStates();

  if (Firebase.RTDB.beginStream(&firebaseData, "/admin/")) {
    Firebase.RTDB.setStreamCallback(&firebaseData, streamCallBack, streamTimeoutCallBack);
  } else {
    Serial.println("Failed to Start Stream: " + firebaseData.errorReason());
  }
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    reConnectToWifi();
  }
}