#include "ble_client_manager.h"
#include "ble_peripheral_manager.h" // Added back
#include "config.h"
#include "logger.h"
#include <math.h> // For roundf

// Instances of callback classes are global in .ino
extern BikeClientCallbacks myBikeClientCallbacks_global; 
extern MyNimBLEAdvertisedDeviceCallbacks myAdvertisedDeviceCallbacks_global; 

// Global Sensor Data Variables (defined in .ino)
extern uint16_t currentCadence;
extern uint16_t currentPower;
extern uint16_t currentSpeed;
extern uint32_t bikeMachineFeatures;     
extern uint32_t bikeTargetSettingFeatures; 
extern uint16_t bikeRawCaloriesX10;
extern volatile uint8_t currentBikeResistanceLevel_Apparent;

// --- bikeFTMSDataParse Implementation (minimal, logging reduced) ---
void bikeFTMSDataParse(uint8_t* pData, size_t length, const char* source) {
    if (length < 2) {
        return;
    }
    uint16_t flags;
    memcpy(&flags, pData, 2);
}

// --- bikeIndoorDataNotificationCallback Implementation (for bike's FTMS 0x2ACC if ever used) ---
void bikeIndoorDataNotificationCallback(NimBLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    bikeFTMSDataParse(pData, length, "Notif_Bike_0x2ACC");
}

// --- ftmsFeatureNotificationCallback Implementation (for bike's FTMS Feature 0x2AD2) ---
void ftmsFeatureNotificationCallback(NimBLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    ts_log_printf("--- BIKE's FTMS Feature-like Notif (Bike's 0x2AD2), Len: %d ---", length);
    
    char dataStr[length * 3 + 1];
    dataStr[length*3] = '\0';
    for (size_t i = 0; i < length; i++) {
        sprintf(dataStr + i * 3, "%02X ", pData[i]);
    }
    ts_log_printf("    Raw Data from Bike's 0x2AD2: %s", dataStr);

    // Resistance parsing from specific notified FTMS Feature packet from Merach S26
    if (length == 11 && pData[0] == 0x75) { 
        uint8_t potentialResistance = pData[7]; 
        if (potentialResistance >= 1 && potentialResistance <= 8) { 
            currentBikeResistanceLevel_Apparent = potentialResistance;
            ts_log_printf("    >> Updated Apparent Resistance: %u (from Bike's 0x2AD2 NOTIFY, type 0x75, byte 7)", currentBikeResistanceLevel_Apparent);
        } else {
            ts_log_printf("    >> Potential Resistance from Bike's 0x2AD2 (type 0x75, byte 7) out of range (1-8): %u", potentialResistance);
        }
    } else if (length == 12 && pData[0] == 0x00 && pData[1] == 0x0B) {
        uint8_t potentialResistance = pData[7]; 
        if (potentialResistance >= 1 && potentialResistance <= 8) {
             currentBikeResistanceLevel_Apparent = potentialResistance;
             ts_log_printf("    >> Updated Apparent Resistance: %u (from Bike's 0x2AD2 NOTIFY, type 0x000B, byte 7)", currentBikeResistanceLevel_Apparent);
        } else {
             ts_log_printf("    >> Potential Resistance from Bike's 0x2AD2 (type 0x000B, byte 7) out of range (1-8): %u", potentialResistance);
        }
    } else {
        ts_log_printf("    Bike's 0x2AD2 NOTIFY - Unhandled packet format for resistance parsing (len %d, first byte 0x%02X).", length, pData[0]);
    }
    
    // FORWARD THIS RAW DATA TO THE APP's FTMS FEATURE (0x2AD2) on the ESP32 peripheral side.
    sendRawFTMSFeatureDataToApp(pData, length); 
}

// --- parseCustomBikeData Implementation (for bike's proprietary service 0xFFF1) ---
void parseCustomBikeData(uint8_t* pData, size_t length) {
    if (length > 0 && pData[0] == 0x02) { 
        if (pData[1] == 0x42 && length >= 11) { 
            uint16_t rawSpeed = (pData[4] << 8) | pData[3]; 
            currentSpeed = rawSpeed; 

            uint16_t actualRPM_x2_from_bike = (pData[7] << 8) | pData[6]; 
            currentCadence = actualRPM_x2_from_bike; 

            uint16_t rawPowerTimes10 = (pData[10] << 8) | pData[9];
            currentPower = (uint16_t)roundf((float)rawPowerTimes10 / 10.0f); 

        } else if (pData[1] == 0x43 && length >= 8) { 
            bikeRawCaloriesX10 = (pData[6] << 8) | pData[7]; 
        }
    }
}

// --- customDataNotificationCallback Implementation (for bike's proprietary service 0xFFF1) ---
void customDataNotificationCallback(NimBLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    parseCustomBikeData(pData, length);
}

// --- BikeClientCallbacks Implementation ---
void BikeClientCallbacks::onConnect(NimBLEClient* pClient_param) {
    ts_log_printf("****** BIKE Sensor device CONNECTED! ******");
    delay(100); 

    bikeSensorConnected = true;
    bikeAttemptingConnection = false; 

    if (!discoverBikeServicesAndCharacteristics(pClient_param)) {
        ts_log_printf("[onConnect] Failed to discover BIKE services/chars. Disconnecting.");
        pClient_param->disconnect(); 
    } else {
        ts_log_printf("[onConnect] Bike services/characteristics discovered.");
        if (pBikeFTMSControlPointCharacteristic != nullptr) {
            ts_log_printf("[onConnect] Sending FTMS control commands to bike (if applicable)...");
            sendFTMSControlCommandToBike(0x00); 
            delay(250); 
            sendFTMSControlCommandToBike(0x07); 
        }
    }
}

void BikeClientCallbacks::onDisconnect(NimBLEClient* pClient_param) {
    ts_log_printf("****** BIKE Sensor device DISCONNECTED ******");
    bikeSensorConnected = false;
    bikeAttemptingConnection = false; 

    pBikeFTMSDataCharacteristic = nullptr;
    pBikeFTMSControlPointCharacteristic = nullptr;
    pBikeFTMSFeatureCharacteristic = nullptr;
    pBikeCustomDataCharacteristic = nullptr;

    ftmsDataNotificationsEnabled = false;
    customDataNotificationsEnabled = false;

    currentPower = 0;
    currentCadence = 0;
    currentSpeed = 0;
    bikeRawCaloriesX10 = 0;
    currentBikeResistanceLevel_Apparent = 0; 
    bikeMachineFeatures = 0;     
    bikeTargetSettingFeatures = 0; 

    ts_log_printf("BIKE Sensor data reset.");
}

uint32_t BikeClientCallbacks::onPassKeyRequest() {
    ts_log_printf("BIKE Client Security: onPassKeyRequest");
    return 123456; 
}

bool BikeClientCallbacks::onConfirmPIN(uint32_t pass_key) {
    ts_log_printf("BIKE Client Security: onConfirmPIN: %lu", pass_key);
    return true; 
}

void BikeClientCallbacks::onAuthenticationComplete(ble_gap_conn_desc* desc) {
    if(!desc) {
        ts_log_printf("BIKE Client Security: onAuthenticationComplete - descriptor is NULL");
        return;
    }
    if(desc->sec_state.encrypted) {
        ts_log_printf("BIKE Client Security: Link encrypted");
    }
    if(desc->sec_state.authenticated) {
        ts_log_printf("BIKE Client Security: Link authenticated");
    }
    if (!desc->sec_state.encrypted && !desc->sec_state.authenticated) {
        ts_log_printf("BIKE Client Security: Link NOT secure");
    }
}

// --- MyNimBLEAdvertisedDeviceCallbacks Implementation ---
void MyNimBLEAdvertisedDeviceCallbacks::onResult(NimBLEAdvertisedDevice* advertisedDevice) {
    NimBLEAddress bikeAddress(BIKE_MAC_ADDRESS);
    if (advertisedDevice->getAddress().equals(bikeAddress)) {
        ts_log_printf("[ScanCallback] Found TARGET bike: Name=%s, Addr=%s",
                      advertisedDevice->getName().c_str(),
                      advertisedDevice->getAddress().toString().c_str());

        NimBLEScan* pScan = NimBLEDevice::getScan();
        if (pScan != nullptr && pScan->isScanning()) {
            ts_log_printf("[ScanCallback] Stopping current scan.");
             pScan->stop(); 
        }
        
        if (pTargetBikeDevice != nullptr) {
             delete pTargetBikeDevice; 
             pTargetBikeDevice = nullptr;
        }
        pTargetBikeDevice = new NimBLEAdvertisedDevice(*advertisedDevice); 

        ts_log_printf("[ScanCallback] Target bike details stored. Press button to connect.");
    }
}


// --- discoverBikeServicesAndCharacteristics Implementation ---
bool discoverBikeServicesAndCharacteristics(NimBLEClient* pClient_local) {
    if (!pClient_local || !pClient_local->isConnected()) {
        ts_log_printf("[discoverBikeServicesAndCharacteristics] Client not connected.");
        return false;
    }

    ts_log_printf("[discoverBikeServicesAndCharacteristics] Discovering services for BIKE...");
    bool dataPathEstablished = false; 

    NimBLERemoteService* pRemoteFTMSService = nullptr;
    try {
        pRemoteFTMSService = pClient_local->getService(BIKE_FTMS_SERVICE_UUID_STR);
    } catch (const std::exception& e) {
        ts_log_printf("[discoverBikeServicesAndCharacteristics] Exception getting FTMS service: %s", e.what());
    }

    if (pRemoteFTMSService) {
        ts_log_printf("  Found BIKE's FTMS Service (0x1826).");

        pBikeFTMSFeatureCharacteristic = pRemoteFTMSService->getCharacteristic(BIKE_FTMS_FEATURE_CHAR_UUID_STR); 
        if (pBikeFTMSFeatureCharacteristic) {
            ts_log_printf("    Found BIKE's FTMS Feature-like Char (Bike's 0x2AD2)."); 
            if (pBikeFTMSFeatureCharacteristic->canNotify()) {
                if (pBikeFTMSFeatureCharacteristic->subscribe(true, ftmsFeatureNotificationCallback, false)) { 
                     ts_log_printf("      Subscribed to BIKE's FTMS Feature-like (Bike's 0x2AD2) notifications.");
                } else {
                     ts_log_printf("      FAILED to subscribe to BIKE's FTMS Feature-like (Bike's 0x2AD2) notifications.");
                     pBikeFTMSFeatureCharacteristic->unsubscribe(); 
                }
            }
        } else {
            ts_log_printf("    BIKE's FTMS Feature-like Char (Bike's 0x2AD2) NOT found.");
        }

        pBikeFTMSControlPointCharacteristic = pRemoteFTMSService->getCharacteristic(BIKE_FTMS_CONTROL_POINT_CHAR_UUID_STR);
        if (pBikeFTMSControlPointCharacteristic) {
            ts_log_printf("    Found BIKE's FTMS Control Point Char (0x2AD9). Writable: %s, Indicable: %s",
                          pBikeFTMSControlPointCharacteristic->canWrite() ? "Yes" : "No",
                          pBikeFTMSControlPointCharacteristic->canIndicate() ? "Yes" : "No");
        } else {
            ts_log_printf("    BIKE's FTMS Control Point Char (0x2AD9) NOT found.");
        }

        pBikeFTMSDataCharacteristic = pRemoteFTMSService->getCharacteristic(BIKE_FTMS_INDOOR_BIKE_DATA_CHAR_UUID_STR); 
        if (pBikeFTMSDataCharacteristic) {
            ts_log_printf("    Found BIKE's FTMS Indoor Bike Data / Feature Char (Bike's 0x2ACC)."); 
            if (pBikeFTMSDataCharacteristic->canRead()) {
                std::string value = pBikeFTMSDataCharacteristic->readValue();
                if (!value.empty()) {
                    ts_log_printf("      Value of Bike's 0x2ACC (FTMS Feature on Merach):");
                    char dataStr[value.length() * 3 + 1];
                    dataStr[value.length()*3] = '\0';
                    for (size_t i = 0; i < value.length(); i++) {
                        sprintf(dataStr + i * 3, "%02X ", (uint8_t)value[i]);
                    }
                    ts_log_printf("        Raw: %s", dataStr);
                    if (value.length() >= 4) memcpy(&bikeMachineFeatures, value.data(), 4);
                    if (value.length() >= 8) memcpy(&bikeTargetSettingFeatures, (uint8_t*)value.data() + 4, 4);
                    ts_log_printf("        Parsed Bike Machine Features: 0x%08X, Target Setting Features: 0x%08X", bikeMachineFeatures, bikeTargetSettingFeatures);
                }
            }
        } else {
            ts_log_printf("    BIKE's FTMS Indoor Bike Data / Feature Char (Bike's 0x2ACC) NOT found.");
        }

    } else {
        ts_log_printf("  BIKE's FTMS Service (0x1826) NOT found.");
    }

    NimBLERemoteService* pCustomService = nullptr;
    try {
        pCustomService = pClient_local->getService(CUSTOM_SERVICE_UUID_STR);
    } catch (const std::exception& e) {
        ts_log_printf("[discoverBikeServicesAndCharacteristics] Exception getting Custom service: %s", e.what());
    }
    
    if (pCustomService) {
        ts_log_printf("  Found BIKE's Custom Service (0xFFF0).");
        pBikeCustomDataCharacteristic = pCustomService->getCharacteristic(CUSTOM_DATA_CHAR_UUID_STR); 
        if (pBikeCustomDataCharacteristic) {
            ts_log_printf("    Found BIKE's Custom Data Char (0xFFF1)."); 
            if (pBikeCustomDataCharacteristic->canNotify()) {
                if (pBikeCustomDataCharacteristic->subscribe(true, customDataNotificationCallback, false)) {
                    customDataNotificationsEnabled = true;
                    dataPathEstablished = true; 
                    ts_log_printf("      Subscribed to BIKE's Custom Data notifications (0xFFF1 - Primary Data Path).");
                } else {
                    ts_log_printf("      FAILED to subscribe to BIKE's Custom Data notifications (0xFFF1).");
                    pBikeCustomDataCharacteristic->unsubscribe(); 
                }
            } else {
                ts_log_printf("      BIKE's Custom Data Char (0xFFF1) cannot notify.");
            }
        } else {
            ts_log_printf("    BIKE's Custom Data Char (0xFFF1) NOT found.");
        }
    } else {
        ts_log_printf("  BIKE's Custom Service (0xFFF0) NOT found. This is critical for data from Merach S26.");
    }

    if (!dataPathEstablished) {
        ts_log_printf("    CRITICAL: FAILED to establish primary data path (Custom Service 0xFFF1 notifications)!");
    }
    return dataPathEstablished; 
}


// --- sendFTMSControlCommandToBike Implementation ---
void sendFTMSControlCommandToBike(uint8_t command) {
    if (!bikeSensorConnected || pBikeFTMSControlPointCharacteristic == nullptr) {
        return;
    }

    if (pBikeFTMSControlPointCharacteristic->canWrite()) {
        ts_log_printf("  Sending FTMS Control Command 0x%02X to bike...", command);
        if (pBikeFTMSControlPointCharacteristic->writeValue(command, true)) { 
            ts_log_printf("    Command 0x%02X sent successfully to bike.", command);
        } else {
            ts_log_printf("    Failed to send command 0x%02X to bike.", command);
        }
    } else {
        ts_log_printf("  Bike's FTMS Control Point characteristic (0x2AD9) is not writable.");
    }
}

// --- Task Functions (startBikeScanTask_func, connectToBikeDeviceTask_func) ---
void startBikeScanTask_func(void *pvParameters) {
    ts_log_printf("[BikeScanTask:%s] Task started.", pcTaskGetName(NULL));

    NimBLEScan* pBLEScan = NimBLEDevice::getScan();
    if (pBLEScan == nullptr) {
        ts_log_printf("[BikeScanTask:%s] FATAL: Failed to get NimBLEScan object! Deleting task.", pcTaskGetName(NULL));
        bleScanTaskHandle = NULL; 
        vTaskDelete(NULL);        
        return;
    }

    if (pTargetBikeDevice != nullptr) {
        delete pTargetBikeDevice;
        pTargetBikeDevice = nullptr;
    }

    pBLEScan->setAdvertisedDeviceCallbacks(&myAdvertisedDeviceCallbacks_global, true); 
    pBLEScan->setActiveScan(true);  
    pBLEScan->setInterval(100);     
    pBLEScan->setWindow(99);        
    pBLEScan->setFilterPolicy(BLE_HCI_SCAN_FILT_NO_WL); 

    ts_log_printf("[BikeScanTask:%s] Starting NimBLE scan (duration 0 - continuous until device found & stopped by callback)...", pcTaskGetName(NULL));
    
    if (!pBLEScan->start(0, nullptr, false)) { 
        ts_log_printf("[BikeScanTask:%s] CRITICAL: Failed to start scan. Deleting task.", pcTaskGetName(NULL));
    } else {
        ts_log_printf("[BikeScanTask:%s] Scan started. Callback will stop scan upon finding target.", pcTaskGetName(NULL));
    }
    
    ts_log_printf("[BikeScanTask:%s] Scan process initiated. Task will delete self.", pcTaskGetName(NULL));
    bleScanTaskHandle = NULL; 
    vTaskDelete(NULL);        
}

void connectToBikeDeviceTask_func(void *pvParameters) {
    ts_log_printf("[ConnectTask:%s] Task started. Attempting to connect.", pcTaskGetName(NULL));

    if (pBikeClient == NULL || pTargetBikeDevice == NULL) {
        ts_log_printf("[ConnectTask:%s] pBikeClient or pTargetBikeDevice is NULL. Cannot connect.", pcTaskGetName(NULL));
        bikeAttemptingConnection = false; 
        bleConnectTaskHandle = NULL;      
        vTaskDelete(NULL);                
        return;
    }

    if (pBikeClient->isConnected()) {
         ts_log_printf("[ConnectTask:%s] pBikeClient is ALREADY connected. Exiting.", pcTaskGetName(NULL));
         bleConnectTaskHandle = NULL;
         vTaskDelete(NULL);
         return;
    }
    
    pBikeClient->setConnectionParams(12, 24, 0, 500); 
    pBikeClient->setConnectTimeout(10); 

    ts_log_printf("[ConnectTask:%s] Calling pBikeClient->connect(pTargetBikeDevice)... Addr: %s",
                  pcTaskGetName(NULL), pTargetBikeDevice->getAddress().toString().c_str());

    bool success = false;
    try {
        success = pBikeClient->connect(pTargetBikeDevice);
    } catch (const std::exception& e) {
        ts_log_printf("[ConnectTask:%s] Exception during connect: %s", pcTaskGetName(NULL), e.what());
        success = false;
    }


    if (success) {
        ts_log_printf("[ConnectTask:%s] connect() returned true. Connection process initiated. BikeClientCallbacks::onConnect will handle state.", pcTaskGetName(NULL));
    } else {
        ts_log_printf("[ConnectTask:%s] connect() FAILED. Resetting attempt flag.", pcTaskGetName(NULL));
        bikeAttemptingConnection = false; 
    }

    ts_log_printf("[ConnectTask:%s] Task finished. Deleting self.", pcTaskGetName(NULL));
    bleConnectTaskHandle = NULL; 
    vTaskDelete(NULL);           
}
