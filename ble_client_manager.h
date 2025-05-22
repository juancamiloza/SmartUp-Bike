#ifndef BLE_CLIENT_MANAGER_H
#define BLE_CLIENT_MANAGER_H

#include <NimBLEDevice.h>
#include "config.h"
#include "logger.h"
#include "ble_peripheral_manager.h" // Added back for sendRawFTMSFeatureDataToApp

// --- External Global Data Variables (defined in .ino or other .cpp files) ---
extern uint16_t currentCadence;
extern uint16_t currentPower;
extern uint16_t currentSpeed;
extern uint32_t totalDistance; 
extern uint32_t bikeMachineFeatures;
extern uint32_t bikeTargetSettingFeatures;
extern uint16_t bikeRawCaloriesX10;
extern volatile uint8_t currentBikeResistanceLevel_Apparent;

// --- Global Variables related to Client (defined in .ino) ---
extern NimBLEClient* pBikeClient;
extern NimBLEAdvertisedDevice* pTargetBikeDevice;
extern bool bikeSensorConnected;
extern volatile bool bikeAttemptingConnection;

extern NimBLERemoteCharacteristic* pBikeFTMSDataCharacteristic;
extern NimBLERemoteCharacteristic* pBikeFTMSControlPointCharacteristic;
extern NimBLERemoteCharacteristic* pBikeFTMSFeatureCharacteristic;
extern NimBLERemoteCharacteristic* pBikeCustomDataCharacteristic;

extern bool ftmsDataNotificationsEnabled; 
extern bool customDataNotificationsEnabled; 

extern TaskHandle_t bleScanTaskHandle;
extern TaskHandle_t bleConnectTaskHandle;

// --- Callback Class Declarations (Instances will be global in .ino) ---
class BikeClientCallbacks : public NimBLEClientCallbacks {
public:
    void onConnect(NimBLEClient* pClient) override;
    void onDisconnect(NimBLEClient* pClient) override;
    uint32_t onPassKeyRequest() override;
    bool onConfirmPIN(uint32_t pass_key) override;
    void onAuthenticationComplete(ble_gap_conn_desc* desc) override;
};

class MyNimBLEAdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks {
public:
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) override;
};

// --- Function Declarations (defined in ble_client_manager.cpp) ---
bool discoverBikeServicesAndCharacteristics(NimBLEClient* pClient);
void sendFTMSControlCommandToBike(uint8_t command);
void startBikeScanTask_func(void *pvParameters);    
void connectToBikeDeviceTask_func(void *pvParameters); 

// Notification Callbacks & Data Parsing (defined in ble_client_manager.cpp)
void ftmsFeatureNotificationCallback(NimBLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);
void bikeIndoorDataNotificationCallback(NimBLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);
void customDataNotificationCallback(NimBLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);
void bikeFTMSDataParse(uint8_t* pData, size_t length, const char* source); 
void parseCustomBikeData(uint8_t* pData, size_t length); 

#endif // BLE_CLIENT_MANAGER_H
