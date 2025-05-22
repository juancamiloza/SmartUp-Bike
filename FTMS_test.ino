// NimBLE Core Library
#include <NimBLEDevice.h>
#include <NimBLELog.h>

// TFT Display Library
#include <SPI.h>
#include <TFT_eSPI.h> // Main TFT library

// Custom Headers
#include "config.h" 
#include "logger.h"
#include "ble_peripheral_manager.h"
#include "ble_client_manager.h"

// --- Global Device Name ---
std::string globalDeviceName; 

// --- Global Peripheral BLE Objects & Status ---
NimBLEServer* pServer_Peripheral = NULL;
NimBLEService* pFTMSService_Peripheral = NULL;
NimBLECharacteristic* pFTMSFeatureCharacteristic_Peripheral = NULL;
NimBLECharacteristic* pControlPointCharacteristic_Peripheral = NULL;
NimBLECharacteristic* pIndoorBikeDataCharacteristic_Peripheral = NULL;
NimBLECharacteristic* pTrainingStatusCharacteristic_Peripheral = NULL;
NimBLECharacteristic* pFitnessMachineStatusCharacteristic_Peripheral = NULL;
NimBLECharacteristic* pSupportedSpeedRangeCharacteristic_Peripheral = NULL;
NimBLECharacteristic* pSupportedInclinationRangeCharacteristic_Peripheral = NULL;
NimBLECharacteristic* pSupportedResistanceRangeCharacteristic_Peripheral = NULL;
NimBLECharacteristic* pSupportedPowerRangeCharacteristic_Peripheral = NULL;
NimBLECharacteristic* pSupportedHeartRateRangeCharacteristic_Peripheral = NULL;
NimBLECharacteristic* pServiceChangedCharacteristic_Peripheral = NULL;
volatile bool mywhooshConnected = false;
TaskHandle_t blePeripheralTaskHandle = NULL;

// --- Global Client BLE Objects & Status ---
NimBLEClient* pBikeClient = NULL;
NimBLEAdvertisedDevice* pTargetBikeDevice = nullptr;
bool bikeSensorConnected = false;
volatile bool bikeAttemptingConnection = false;
NimBLERemoteCharacteristic* pBikeFTMSDataCharacteristic = NULL;
NimBLERemoteCharacteristic* pBikeFTMSControlPointCharacteristic = NULL;
NimBLERemoteCharacteristic* pBikeFTMSFeatureCharacteristic = NULL;
NimBLERemoteCharacteristic* pBikeCustomDataCharacteristic = NULL;
bool ftmsDataNotificationsEnabled = false; 
bool customDataNotificationsEnabled = false; 
TaskHandle_t bleScanTaskHandle = NULL;
TaskHandle_t bleConnectTaskHandle = NULL;

// --- Global Sensor Data Variables ---
uint16_t currentCadence = 0;
uint16_t currentPower = 0;   
uint16_t currentSpeed = 0;   
uint32_t totalDistance = 0;  
uint32_t bikeMachineFeatures = 0;     
uint32_t bikeTargetSettingFeatures = 0; 
uint16_t bikeRawCaloriesX10 = 0; 
volatile uint8_t currentBikeResistanceLevel_Apparent = 0; 

// --- Global Variables for Target Data from App ---
int16_t  targetInclinationPercentX100 = 0; 
uint8_t  targetResistanceLevel_App = 0;    
bool     targetResistanceMatchesBike = false; 

// --- Global Callback Instances ---
MyWhooshNimBLEServerCallbacks myServerCallbacks_global;
BikeClientCallbacks myBikeClientCallbacks_global;
MyNimBLEAdvertisedDeviceCallbacks myAdvertisedDeviceCallbacks_global;

// --- TFT Display Object & Sprite ---
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft); 
bool displayInitialized = false;

// --- Button State ---
bool buttonPressedLastState = false;

// --- Display Functions ---
void initDisplay() {
    tft.init();
    tft.setRotation(0); 
    spr.createSprite(tft.width(), tft.height());
    spr.fillSprite(TFT_BLACK); 
    spr.setTextColor(TFT_WHITE, TFT_BLACK); 
    spr.setTextSize(2); 
    spr.setCursor(10, 80);
    spr.println("SMARTUP BIKE");
    spr.pushSprite(0, 0); 
    ts_log_printf("TFT Display Initialized. Sprite created (%dx%d).", spr.width(), spr.height());
    displayInitialized = true;
}

void updateDisplay() {
    if (!displayInitialized) return;

    spr.fillSprite(TFT_BLACK); 
    spr.setTextWrap(false);    

    // Define positions and sizes
    int16_t yPos = 0;
    int16_t xPosLabel = 5; 
    int16_t xPosValue = tft.width() * 2 / 3 -15; // Shift values further right
    int16_t labelHeight = 15; 
    int16_t valueHeight = 17;  
    int16_t lineSpacing = 3;   

    // Header
    spr.setTextSize(2);
    spr.setTextColor(TFT_CYAN, TFT_BLACK);
    spr.setCursor(xPosLabel, yPos); // Use xPosLabel for header too
    spr.println("SMARTUP BIKE");
    yPos += valueHeight + 4; 

    // Bike Connection Status
    spr.setTextSize(1); 
    spr.setCursor(xPosLabel, yPos);
    if (!pTargetBikeDevice && !bikeSensorConnected && !bikeAttemptingConnection) {
        spr.setTextColor(TFT_ORANGE, TFT_BLACK);
        spr.print("Bike: SCAN (BTN)");
    } else if (pTargetBikeDevice && !bikeSensorConnected && !bikeAttemptingConnection) {
        spr.setTextColor(TFT_YELLOW, TFT_BLACK);
        spr.print("Bike: PAIR (BTN)");
    } else if (bikeAttemptingConnection) {
        spr.setTextColor(TFT_BLUE, TFT_BLACK);
        spr.print("Bike: CONNECTING...");
    } else if (bikeSensorConnected) {
        spr.setTextColor(TFT_GREEN, TFT_BLACK);
        spr.print("Bike: CONNECTED");
    } else { 
        spr.setTextColor(TFT_RED, TFT_BLACK);
        spr.print("Bike: OFFLINE");
    }
    yPos += labelHeight + 1;

    // MyWhoosh App Connection Status
    spr.setCursor(xPosLabel, yPos);
    bool currentMyWhooshStatus = mywhooshConnected; 
    spr.setTextColor(currentMyWhooshStatus ? TFT_GREEN : TFT_RED, TFT_BLACK);
    spr.printf("App:  %s", currentMyWhooshStatus ? "CONNECTED" : "OFFLINE");
    yPos += labelHeight + lineSpacing + 2; 

    // Resistance (Bike's current)
    spr.setTextColor(TFT_WHITE, TFT_BLACK); 
    spr.setCursor(xPosLabel, yPos);
    spr.setTextSize(1); spr.print("Bike Res:");
    spr.setTextSize(2); spr.setCursor(xPosValue, yPos); spr.printf("%u", currentBikeResistanceLevel_Apparent);
    yPos += valueHeight + lineSpacing;

    // Target Resistance (from App)
    spr.setTextColor(TFT_WHITE, TFT_BLACK);
    spr.setCursor(xPosLabel, yPos);
    spr.setTextSize(1); spr.print("Tgt Res:"); // Shortened label
    spr.setTextColor(TFT_GOLD, TFT_BLACK); 
    spr.setTextSize(2); spr.setCursor(xPosValue, yPos); spr.printf("%u", targetResistanceLevel_App);
    yPos += valueHeight + lineSpacing;

    // Target Inclination (from App)
    spr.setTextColor(TFT_WHITE, TFT_BLACK);
    spr.setCursor(xPosLabel, yPos);
    spr.setTextSize(1); spr.print("Tgt Inc:"); // Shortened label
    spr.setTextColor(TFT_VIOLET, TFT_BLACK); 
    spr.setTextSize(2); spr.setCursor(xPosValue, yPos);
    char inclBuffer[10];
    sprintf(inclBuffer, "%.1f%%", (float)targetInclinationPercentX100 / 100.0f); // Display with 1 decimal for space
    spr.print(inclBuffer);
    yPos += valueHeight + lineSpacing;

    // Resistance Match Status
    if (mywhooshConnected && bikeSensorConnected && targetResistanceLevel_App > 0) { 
        targetResistanceMatchesBike = (currentBikeResistanceLevel_Apparent == targetResistanceLevel_App);
    } else {
        targetResistanceMatchesBike = false; 
    }
    spr.setTextColor(TFT_WHITE, TFT_BLACK);
    spr.setCursor(xPosLabel, yPos);
    spr.setTextSize(1); spr.print("Match:"); // Shortened label
    spr.setTextSize(2); spr.setCursor(xPosValue, yPos);
    if (mywhooshConnected && bikeSensorConnected && targetResistanceLevel_App > 0) {
        spr.setTextColor(targetResistanceMatchesBike ? TFT_GREEN : TFT_RED, TFT_BLACK);
        spr.print(targetResistanceMatchesBike ? "YES" : "NO");
    } else {
        spr.setTextColor(TFT_DARKGREY, TFT_BLACK);
        spr.print("N/A");
    }
    yPos += valueHeight + lineSpacing;

    // Speed
    spr.setTextColor(TFT_WHITE, TFT_BLACK);
    spr.setCursor(xPosLabel, yPos);
    spr.setTextSize(1); spr.print("Speed:");
    spr.setTextSize(2); spr.setTextColor(TFT_GREENYELLOW, TFT_BLACK); spr.setCursor(xPosValue, yPos);
    char speedBuffer[12]; sprintf(speedBuffer, "%.1f", (float)currentSpeed / 100.0); spr.print(speedBuffer);
    yPos += valueHeight + lineSpacing;

    // Cadence
    spr.setTextColor(TFT_WHITE, TFT_BLACK);
    spr.setCursor(xPosLabel, yPos);
    spr.setTextSize(1); spr.print("Cadence:");
    spr.setTextSize(2); spr.setTextColor(TFT_ORANGE, TFT_BLACK); spr.setCursor(xPosValue, yPos);
    spr.printf("%.0f", (float)currentCadence); // Display currentCadence directly
    yPos += valueHeight + lineSpacing;

    // Power
    spr.setTextColor(TFT_WHITE, TFT_BLACK);
    spr.setCursor(xPosLabel, yPos);
    spr.setTextSize(1); spr.print("Power:");
    spr.setTextSize(2); spr.setTextColor(TFT_MAGENTA, TFT_BLACK); spr.setCursor(xPosValue, yPos);
    spr.printf("%u", currentPower);
    yPos += valueHeight + lineSpacing;

    // Calories
    spr.setTextColor(TFT_WHITE, TFT_BLACK);
    spr.setCursor(xPosLabel, yPos);
    spr.setTextSize(1); spr.print("Calories:");
    spr.setTextSize(2); spr.setTextColor(TFT_SKYBLUE, TFT_BLACK); spr.setCursor(xPosValue, yPos);
    spr.printf("%.1f", (float)bikeRawCaloriesX10 / 10.0); 
    
    spr.pushSprite(0, 0); 
}

// --- Handle Button Press ---
void handleButtonPress() {
    ts_log_printf("[handleButtonPress] Button Pressed!");
    if (bikeSensorConnected || bikeAttemptingConnection) {
        ts_log_printf("[handleButtonPress] Already connected or attempting connection to bike.");
        if(bikeSensorConnected && pBikeClient != nullptr) {
            ts_log_printf("[handleButtonPress] Disconnecting from bike due to button press while connected.");
            pBikeClient->disconnect(); 
        }
        targetInclinationPercentX100 = 0;
        targetResistanceLevel_App = 0;
        targetResistanceMatchesBike = false;
        return;
    }

    if (pTargetBikeDevice == nullptr) { 
        ts_log_printf("[handleButtonPress] No target bike known. Starting scan...");
        if (bleScanTaskHandle != NULL) { 
             eTaskState taskState = eTaskGetState(bleScanTaskHandle);
             if (taskState != eDeleted && taskState != eInvalid) {
                vTaskDelete(bleScanTaskHandle);
             }
             bleScanTaskHandle = NULL; 
        }
        xTaskCreatePinnedToCore(startBikeScanTask_func, "BikeScanBtn", 8192, NULL, 1, &bleScanTaskHandle, 0);
    }
    else if (pTargetBikeDevice != nullptr && !bikeSensorConnected && !bikeAttemptingConnection) { 
        ts_log_printf("[handleButtonPress] Target bike known. Attempting to connect...");
        if (pBikeClient == nullptr) { 
            pBikeClient = NimBLEDevice::createClient();
            if (pBikeClient == nullptr) {
                ts_log_printf("[handleButtonPress] FATAL: Failed to create new pBikeClient!");
                return; 
            }
            pBikeClient->setClientCallbacks(&myBikeClientCallbacks_global); 
        }
        bikeAttemptingConnection = true; 
        updateDisplay(); 
        if (bleConnectTaskHandle != NULL) { 
             eTaskState taskState = eTaskGetState(bleConnectTaskHandle);
             if (taskState != eDeleted && taskState != eInvalid) {
                vTaskDelete(bleConnectTaskHandle);
             }
             bleConnectTaskHandle = NULL; 
        }
        xTaskCreatePinnedToCore(connectToBikeDeviceTask_func, "ConnectBikeBtn", 8192, NULL, 2, &bleConnectTaskHandle, 0);
    }
}

// --- MAIN SETUP ---
void setup() {
  Serial.begin(115200);
  unsigned long setupStartTime = millis();
  while (!Serial && (millis() - setupStartTime < 3000));
  ts_log_printf("\n[%08.3fs] Starting ESP32 FTMS BLE Bridge...", millis()/1000.0);

  initDisplay(); 
  pinMode(PAIR_BUTTON_PIN, INPUT_PULLUP); 
  globalDeviceName = "DIY FTMS Bike"; 
  NimBLEDevice::init("");
  NimBLEDevice::setMTU(247); 
  delay(500); 
  
  BaseType_t peripheralTaskStatus = xTaskCreatePinnedToCore(
                                      blePeripheralSetupTask_func, "BLEPeripheralSetup", 
                                      20480, NULL, 1, &blePeripheralTaskHandle, 0);                         
  if (peripheralTaskStatus != pdPASS) {
    ts_log_printf("Failed to create BLE Peripheral Setup Task. Error: %d", peripheralTaskStatus);
  }
  updateDisplay(); 
}

// --- MAIN LOOP ---
void loop() {
  static unsigned long lastButtonCheck = 0;
  if (millis() - lastButtonCheck > 50) { 
    bool currentButtonState = (digitalRead(PAIR_BUTTON_PIN) == LOW); 
    if (currentButtonState && !buttonPressedLastState) { 
      handleButtonPress();
    }
    buttonPressedLastState = currentButtonState;
    lastButtonCheck = millis();
  }

  static bool oldMywhooshConnected_loop = false;
  bool currentMyWhooshStatus = mywhooshConnected; 
  if (currentMyWhooshStatus != oldMywhooshConnected_loop) {
    ts_log_printf("MyWhoosh app connection status (loop): %s", currentMyWhooshStatus ? "CONNECTED" : "DISCONNECTED");
    oldMywhooshConnected_loop = currentMyWhooshStatus;
    if (!currentMyWhooshStatus) {
        targetInclinationPercentX100 = 0;
        targetResistanceLevel_App = 0;
        targetResistanceMatchesBike = false;
    }
    updateDisplay(); 
  }

  static unsigned long lastDataSendToMyWhooshTime = 0;
  if (mywhooshConnected && bikeSensorConnected) {
    if (millis() - lastDataSendToMyWhooshTime > 250) { 
        sendDataToMyWhoosh(); 
        lastDataSendToMyWhooshTime = millis();
    }
  }

  static unsigned long lastDisplayUpdateTime = 0;
  if (millis() - lastDisplayUpdateTime > 500) { 
      updateDisplay(); 
      lastDisplayUpdateTime = millis();
  }
  vTaskDelay(pdMS_TO_TICKS(10)); 
}

