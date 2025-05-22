#ifndef BLE_PERIPHERAL_MANAGER_H
#define BLE_PERIPHERAL_MANAGER_H

#include <NimBLEDevice.h>
#include "config.h"
#include "logger.h"
#include <string>

// --- External Global Data Variables (defined in .ino, read by this module) ---
extern uint16_t currentSpeed;
extern uint16_t currentCadence;
extern uint16_t currentPower;
extern std::string globalDeviceName;

// --- NEW: External Global Data Variables for Target Values (defined in .ino, written by this module) ---
extern int16_t  targetInclinationPercentX100; // Target inclination from app (e.g., 550 means 5.50%)
extern uint8_t  targetResistanceLevel_App;    // Target resistance level from app (e.g., 1-8, after processing)


// --- Global Variables related to Peripheral (defined in .ino) ---
extern NimBLEServer* pServer_Peripheral;
extern NimBLEService* pFTMSService_Peripheral;
// FTMS Characteristics
extern NimBLECharacteristic* pFTMSFeatureCharacteristic_Peripheral;
extern NimBLECharacteristic* pControlPointCharacteristic_Peripheral;
extern NimBLECharacteristic* pIndoorBikeDataCharacteristic_Peripheral;
extern NimBLECharacteristic* pTrainingStatusCharacteristic_Peripheral;
extern NimBLECharacteristic* pFitnessMachineStatusCharacteristic_Peripheral;
extern NimBLECharacteristic* pSupportedSpeedRangeCharacteristic_Peripheral;
extern NimBLECharacteristic* pSupportedInclinationRangeCharacteristic_Peripheral;
extern NimBLECharacteristic* pSupportedResistanceRangeCharacteristic_Peripheral;
extern NimBLECharacteristic* pSupportedPowerRangeCharacteristic_Peripheral;
extern NimBLECharacteristic* pSupportedHeartRateRangeCharacteristic_Peripheral;


// GATT Service Characteristics
extern NimBLECharacteristic* pServiceChangedCharacteristic_Peripheral;

extern volatile bool mywhooshConnected;
extern TaskHandle_t blePeripheralTaskHandle;

// --- Callback Class Declarations ---
class MyWhooshNimBLEServerCallbacks : public NimBLEServerCallbacks {
public:
    void onConnect(NimBLEServer* pServer, ble_gap_conn_desc* desc) override;
    void onDisconnect(NimBLEServer* pServer, ble_gap_conn_desc* desc) override;
};

class MyWhooshNimBLEControlPointCallbacks : public NimBLECharacteristicCallbacks {
public:
    void onWrite(NimBLECharacteristic* pChar, ble_gap_conn_desc* desc) override;
};

class IndoorBikeDataCallbacks : public NimBLECharacteristicCallbacks {
public:
    void onSubscribe(NimBLECharacteristic* pCharacteristic, ble_gap_conn_desc* desc, uint16_t subValue) override;
};

class TrainingStatusCallbacks : public NimBLECharacteristicCallbacks {
public:
    void onSubscribe(NimBLECharacteristic* pCharacteristic, ble_gap_conn_desc* desc, uint16_t subValue) override;
};

class FitnessMachineStatusCallbacks : public NimBLECharacteristicCallbacks {
public:
    void onSubscribe(NimBLECharacteristic* pCharacteristic, ble_gap_conn_desc* desc, uint16_t subValue) override;
};

// ADDED: Callback for FTMS Feature characteristic subscriptions
class FTMSFeatureCallbacks : public NimBLECharacteristicCallbacks {
public:
    void onSubscribe(NimBLECharacteristic* pCharacteristic, ble_gap_conn_desc* desc, uint16_t subValue) override;
};

class ServiceChangedCallbacks : public NimBLECharacteristicCallbacks {
public:
    void onSubscribe(NimBLECharacteristic* pCharacteristic, ble_gap_conn_desc* desc, uint16_t subValue) override;
};


// --- Function Declarations ---
void blePeripheralSetupTask_func(void *pvParameters);
void sendDataToMyWhoosh();
void sendTrainingStatusUpdate(uint8_t status_code, bool force_notify = false);
void sendFitnessMachineStatusUpdate(uint8_t status_code, bool force_notify = false);
void indicateServiceChanged();

// ADDED: Function to send raw FTMS Feature data
void sendRawFTMSFeatureDataToApp(const uint8_t* data, size_t length);


#endif // BLE_PERIPHERAL_MANAGER_H
