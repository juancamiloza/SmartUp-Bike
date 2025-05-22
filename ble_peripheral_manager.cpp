#include "ble_peripheral_manager.h"
#include "config.h"
#include "logger.h"
#include <math.h> // For roundf
#include <stdio.h> // For sprintf

// Instances of callback classes (defined in .ino if global, or local if only used here)
extern MyWhooshNimBLEServerCallbacks myServerCallbacks_global; // Defined in .ino

// Local instances for characteristic callbacks, used only in this file's scope for peripheral characteristics
static MyWhooshNimBLEControlPointCallbacks myControlPointCallbacks_instance_local;
static IndoorBikeDataCallbacks myIndoorBikeDataCallbacks_instance_local;
static TrainingStatusCallbacks myTrainingStatusCallbacks_instance_local;
static FitnessMachineStatusCallbacks myFitnessMachineStatusCallbacks_instance_local;
static FTMSFeatureCallbacks myFTMSFeatureCallbacks_instance_local;
static ServiceChangedCallbacks myServiceChangedCallbacks_instance_local;

// Global sensor data variables (defined in .ino, read by this module for sending to app)
extern uint16_t currentSpeed;
extern uint16_t currentCadence; // Assumed to be ACTUAL RPM from bike (x2 for 0.5 RPM resolution)
extern uint16_t currentPower;
extern std::string globalDeviceName; // From .ino, used for advertising

// --- Global Variables for Target Data from App (defined in .ino, written by this module) ---
extern int16_t  targetInclinationPercentX100;
extern uint8_t  targetResistanceLevel_App;


// --- MyWhooshNimBLEServerCallbacks Implementation (Peripheral Role) ---
void MyWhooshNimBLEServerCallbacks::onConnect(NimBLEServer* pSrv, ble_gap_conn_desc* desc) {
    mywhooshConnected = true;
    ts_log_printf("App Connected to ESP32. Conn Handle: %d, Peer Address: %s. 'mywhooshConnected' flag SET TO TRUE.",
                  desc->conn_handle, NimBLEAddress(desc->peer_ota_addr).toString().c_str());
}

void MyWhooshNimBLEServerCallbacks::onDisconnect(NimBLEServer* pSrv, ble_gap_conn_desc* desc) {
    mywhooshConnected = false;
    ts_log_printf("App Disconnected from ESP32. Conn Handle: %d. 'mywhooshConnected' flag SET TO FALSE.", desc->conn_handle);

    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    if(pAdvertising && !pAdvertising->isAdvertising()) {
        ts_log_printf("[PeripheralCallbacks] Attempting to restart advertising after app disconnect...");
        if (pAdvertising->start()) {
             ts_log_printf("[PeripheralCallbacks] Peripheral advertising restarted successfully.");
        } else {
             ts_log_printf("[PeripheralCallbacks] FAILED to restart peripheral advertising.");
        }
    }
}

// --- MyWhooshNimBLEControlPointCallbacks Implementation (Peripheral Role - FTMS Control Point) ---
void MyWhooshNimBLEControlPointCallbacks::onWrite(NimBLECharacteristic* pChar, ble_gap_conn_desc* desc) {
    NimBLEAttValue value_att = pChar->getValue();
    std::string value_str = (std::string)value_att;
    const uint8_t* pData = (const uint8_t*)value_str.data();
    size_t length = value_str.length();

    ts_log_printf(">>> App -> Wrote to ESP32's Control Point (0x2AD9), Length: %d <<<", length);

    std::string hexStr;
    for(size_t i = 0; i < length; ++i) {
        char buf[4];
        sprintf(buf, "%02X ", pData[i]);
        hexStr += buf;
    }
    ts_log_printf("    Raw CP Data from App: %s", hexStr.c_str());

    if (length > 0) {
        uint8_t opCode = pData[0];
        ts_log_printf("    Received OpCode: 0x%02X", opCode);

        uint8_t response[3]; 
        response[0] = 0x80; 
        response[1] = opCode; 

        switch (opCode) {
            case 0x00: // Request Control
                ts_log_printf("    CP Handler: Request Control (0x00)");
                response[2] = 0x01; // Success
                pChar->setValue(response, 3);
                pChar->indicate();
                ts_log_printf("    CP Response to App: Sent Success (0x01) for Request Control.");
                sendTrainingStatusUpdate(0x0D, true); 
                sendFitnessMachineStatusUpdate(0x02, true); 
                break;

            case 0x01: // Reset
                ts_log_printf("    CP Handler: Reset (0x01)");
                response[2] = 0x01; // Success
                pChar->setValue(response, 3);
                pChar->indicate();
                ts_log_printf("    CP Response to App: Sent Success for Reset.");
                targetInclinationPercentX100 = 0; 
                targetResistanceLevel_App = 0;
                sendTrainingStatusUpdate(0x01, true); 
                sendFitnessMachineStatusUpdate(0x01, true); 
                break;

            case 0x03: // Set Target Inclination
                ts_log_printf("    CP Handler: Set Target Inclination (0x03)");
                if (length >= 3) { 
                    int16_t rawInclination;
                    memcpy(&rawInclination, &pData[1], sizeof(rawInclination));
                    targetInclinationPercentX100 = rawInclination;
                    ts_log_printf("      Raw Inclination Bytes: %02X %02X", pData[1], pData[2]);
                    ts_log_printf("      Parsed targetInclinationPercentX100: %d (%.2f%%)",
                                  targetInclinationPercentX100, (float)targetInclinationPercentX100 / 100.0f);
                    response[2] = 0x01; // Success
                } else {
                    ts_log_printf("      ERROR: Insufficient data length (%d). Expected 3 for Set Target Inclination.", length);
                    response[2] = 0x04; // Invalid Parameter
                }
                pChar->setValue(response, 3);
                pChar->indicate();
                break;

            case 0x04: // Set Target Resistance Level
                ts_log_printf("    CP Handler: Set Target Resistance Level (0x04)");
                if (length >= 2) { 
                    uint8_t rawResistanceValueFromApp = pData[1]; 
                    ts_log_printf("      Raw Resistance Byte from App: %02X (Value: %u)", pData[1], rawResistanceValueFromApp);
                    
                    uint8_t processedLevel = 0;
                    if (rawResistanceValueFromApp == 0) {
                        processedLevel = 0;
                    } else {
                        processedLevel = (uint8_t)roundf((float)rawResistanceValueFromApp / 10.0f);
                    }

                    if (processedLevel == 0 && rawResistanceValueFromApp == 0) { 
                        targetResistanceLevel_App = 0; 
                    } else if (processedLevel < 1) {
                        targetResistanceLevel_App = 1; 
                    } else if (processedLevel > 8) { 
                        targetResistanceLevel_App = 8; 
                    } else {
                        targetResistanceLevel_App = processedLevel;
                    }
                                        
                    ts_log_printf("      Processed targetResistanceLevel_App (1-8 scale): %u", targetResistanceLevel_App);
                    response[2] = 0x01; // Success
                } else {
                    ts_log_printf("      ERROR: Insufficient data length (%d). Expected 2 for Set Target Resistance.", length);
                    response[2] = 0x04; // Invalid Parameter
                }
                pChar->setValue(response, 3);
                pChar->indicate();
                break;
            
            case 0x05: // Set Target Power
                ts_log_printf("    CP Handler: Set Target Power (0x05)");
                if (length >= 3) { 
                    int16_t rawPower;
                    memcpy(&rawPower, &pData[1], sizeof(rawPower));
                    ts_log_printf("      Received Target Power command: %d W. (Currently not acted upon)", rawPower);
                    response[2] = 0x01; 
                } else {
                    ts_log_printf("      ERROR: Insufficient data length (%d). Expected 3 for Set Target Power.", length);
                    response[2] = 0x04; 
                }
                pChar->setValue(response, 3);
                pChar->indicate();
                break;
            
            case 0x07: // Start or Resume
                ts_log_printf("    CP Handler: Start/Resume (0x07)");
                response[2] = 0x01; // Success
                pChar->setValue(response, 3);
                pChar->indicate();
                ts_log_printf("    CP Response to App: Sent Success for Start/Resume.");
                sendFitnessMachineStatusUpdate(0x04, true); 
                break;

            case 0x08: // Stop or Pause
                ts_log_printf("    CP Handler: Stop/Pause (0x08)");
                if (length >= 2 && pData[1] == 0x01) { 
                     ts_log_printf("      Parameter: Stop (0x01)");
                     sendFitnessMachineStatusUpdate(0x02, true); 
                } else if (length >=2 && pData[1] == 0x02) { 
                     ts_log_printf("      Parameter: Pause (0x02)");
                     sendFitnessMachineStatusUpdate(0x07, true); 
                } else {
                     ts_log_printf("      Parameter: Unknown/None for Stop/Pause");
                     sendFitnessMachineStatusUpdate(0x02, true);
                }
                response[2] = 0x01; // Success
                pChar->setValue(response, 3);
                pChar->indicate();
                ts_log_printf("    CP Response to App: Sent Success for Stop/Pause.");
                break;

            default:
                ts_log_printf("    CP Handler: Unrecognized Op Code: 0x%02X", opCode);
                response[2] = 0x02; 
                pChar->setValue(response, 3);
                pChar->indicate();
                ts_log_printf("    CP Response to App: Sent 'Op Code Not Supported'.");
                break;
        }
    } else {
        ts_log_printf("    Received empty Control Point write (length 0). Ignoring.");
    }
}

// --- onSubscribe Callbacks ---
void IndoorBikeDataCallbacks::onSubscribe(NimBLECharacteristic* pCharacteristic, ble_gap_conn_desc* desc, uint16_t subValue) {
    std::string subValStr;
    char cccdValHex[7]; 
    if (subValue == 0x0001) subValStr = "NOTIFICATIONS ENABLED";
    else if (subValue == 0x0000) subValStr = "Notifications DISABLED";
    else {
        sprintf(cccdValHex, "0x%04X", subValue);
        subValStr = "UNKNOWN CCCD value: ";
        subValStr += cccdValHex;
    }
    NimBLEAddress peerAddr(desc->peer_ota_addr);
    ts_log_printf("App (%s) %s for ESP32's Indoor Bike Data (0x2ACC). CCCD Raw Value: 0x%04X",
                  peerAddr.toString().c_str(), subValStr.c_str(), subValue);
}

void TrainingStatusCallbacks::onSubscribe(NimBLECharacteristic* pCharacteristic, ble_gap_conn_desc* desc, uint16_t subValue) {
    std::string subValStr;
    char cccdValHex[7];
    if (subValue == 0x0001) subValStr = "NOTIFICATIONS ENABLED";
    else if (subValue == 0x0000) subValStr = "Notifications DISABLED";
    else {
        sprintf(cccdValHex, "0x%04X", subValue);
        subValStr = "UNKNOWN CCCD value: ";
        subValStr += cccdValHex;
    }
    NimBLEAddress peerAddr(desc->peer_ota_addr);
    ts_log_printf("App (%s) %s for ESP32's Training Status (0x2AD3). CCCD Raw Value: 0x%04X",
                  peerAddr.toString().c_str(), subValStr.c_str(), subValue);
    if (subValue == 0x0001) { 
        sendTrainingStatusUpdate(pCharacteristic->getValue<uint8_t>(), true); 
    }
}

void FitnessMachineStatusCallbacks::onSubscribe(NimBLECharacteristic* pCharacteristic, ble_gap_conn_desc* desc, uint16_t subValue) {
    std::string subValStr;
    char cccdValHex[7];
    if (subValue == 0x0001) subValStr = "NOTIFICATIONS ENABLED";
    else if (subValue == 0x0000) subValStr = "Notifications DISABLED";
    else {
        sprintf(cccdValHex, "0x%04X", subValue);
        subValStr = "UNKNOWN CCCD value: ";
        subValStr += cccdValHex;
    }
    NimBLEAddress peerAddr(desc->peer_ota_addr);
    ts_log_printf("App (%s) %s for ESP32's Fitness Machine Status (0x2ADA). CCCD Raw Value: 0x%04X",
                  peerAddr.toString().c_str(), subValStr.c_str(), subValue);
    if (subValue == 0x0001) { 
        sendFitnessMachineStatusUpdate(pCharacteristic->getValue<uint8_t>(), true); 
    }
}

void FTMSFeatureCallbacks::onSubscribe(NimBLECharacteristic* pCharacteristic, ble_gap_conn_desc* desc, uint16_t subValue) {
    std::string subValStr;
    char cccdValHex[7];
    if (subValue == 0x0001) subValStr = "NOTIFICATIONS ENABLED"; 
    else if (subValue == 0x0000) subValStr = "Notifications DISABLED";
    else {
        sprintf(cccdValHex, "0x%04X", subValue);
        subValStr = "UNKNOWN CCCD value: ";
        subValStr += cccdValHex;
    }
    NimBLEAddress peerAddr(desc->peer_ota_addr);
    ts_log_printf("App (%s) %s for ESP32's FTMS Feature (0x2AD2). CCCD Raw Value: 0x%04X",
                  peerAddr.toString().c_str(), subValStr.c_str(), subValue);
}

void ServiceChangedCallbacks::onSubscribe(NimBLECharacteristic* pCharacteristic, ble_gap_conn_desc* desc, uint16_t subValue) {
    std::string subValStr;
    char cccdValHex[7];
    if (subValue == 0x0002) subValStr = "INDICATIONS ENABLED"; 
    else if (subValue == 0x0000) subValStr = "Indications DISABLED";
    else {
        sprintf(cccdValHex, "0x%04X", subValue);
        subValStr = "UNKNOWN CCCD value: ";
        subValStr += cccdValHex;
    }
    NimBLEAddress peerAddr(desc->peer_ota_addr);
    ts_log_printf("App (%s) %s for ESP32's Service Changed (0x2A05). CCCD Raw Value: 0x%04X",
                  peerAddr.toString().c_str(), subValStr.c_str(), subValue);
    if (subValue == 0x0002) { 
         ts_log_printf("  >>> App SUBSCRIBED to INDICATIONS for Service Changed! <<<");
         indicateServiceChanged();
    }
}

// --- sendDataToMyWhoosh Implementation (Corrected FTMS Flags) ---
void sendDataToMyWhoosh() {
  if (!mywhooshConnected || pIndoorBikeDataCharacteristic_Peripheral == nullptr) {
    return;
  }
  if (pIndoorBikeDataCharacteristic_Peripheral->getSubscribedCount() == 0) {
    return;
  }

  uint16_t ftms_flags = 0;
  // FTMS Indoor Bike Data Packet Flags:
  // Bit 0: More Data (0 for false)
  // Bit 1: Average Speed Present (0 for false, means Instantaneous Speed field is present)
  // Bit 2: Instantaneous Cadence Present (1 = true)
  // Bit 3: Average Cadence Present (0 for false)
  // Bit 4: Total Distance Present (0 for false)
  // Bit 5: Resistance Level Present (0 for false - we are not sending it here)
  // Bit 6: Instantaneous Power Present (1 = true)
  // Bit 7: Average Power Present (0 for false)
  // Other flags (HR, Metabolic, Elapsed Time, Remaining Time) are 0 for this basic implementation.

  bool cadence_present = true; 
  bool power_present = true;   

  // Instantaneous Speed is always present after flags if Bit 1 is 0.
  
  if (cadence_present) {
    ftms_flags |= (1 << 2); 
  }
  if (power_present) {
    ftms_flags |= (1 << 6); 
  }

  uint8_t payload[8]; 
  int offset = 0;

  memcpy(payload + offset, &ftms_flags, 2); offset += 2;
  memcpy(payload + offset, &currentSpeed, 2); offset += 2; 

  if (cadence_present) {
    memcpy(payload + offset, &currentCadence, 2); offset += 2;
  }
  if (power_present) { 
    int16_t powerForFtms_val = (int16_t)currentPower; 
    memcpy(payload + offset, &powerForFtms_val, 2); offset += 2;
  }

  pIndoorBikeDataCharacteristic_Peripheral->setValue(payload, offset);
  pIndoorBikeDataCharacteristic_Peripheral->notify();
}

// --- sendTrainingStatusUpdate, sendFitnessMachineStatusUpdate, sendRawFTMSFeatureDataToApp, indicateServiceChanged ---
void sendTrainingStatusUpdate(uint8_t status_code, bool force_notify) {
    if (mywhooshConnected && pTrainingStatusCharacteristic_Peripheral != nullptr) {
        if (status_code != pTrainingStatusCharacteristic_Peripheral->getValue<uint8_t>() || force_notify) {
             pTrainingStatusCharacteristic_Peripheral->setValue(&status_code, 1);
            if (pTrainingStatusCharacteristic_Peripheral->getSubscribedCount() > 0) {
                pTrainingStatusCharacteristic_Peripheral->notify();
                ts_log_printf("[BLE Peripheral] Sent Training Status (0x2AD3) Update to App: 0x%02X", status_code);
            }
        }
    }
}

void sendFitnessMachineStatusUpdate(uint8_t status_code, bool force_notify) {
    if (mywhooshConnected && pFitnessMachineStatusCharacteristic_Peripheral != nullptr) {
         if (status_code != pFitnessMachineStatusCharacteristic_Peripheral->getValue<uint8_t>() || force_notify) {
            pFitnessMachineStatusCharacteristic_Peripheral->setValue(&status_code, 1);
            if (pFitnessMachineStatusCharacteristic_Peripheral->getSubscribedCount() > 0) {
                pFitnessMachineStatusCharacteristic_Peripheral->notify();
                ts_log_printf("[BLE Peripheral] Sent Fitness Machine Status (0x2ADA) Update to App: 0x%02X", status_code);
            }
        }
    }
}

// This function will now be used to forward bike's 0x2AD2 data to app via ESP32's 0x2AD2
void sendRawFTMSFeatureDataToApp(const uint8_t* data, size_t length) {
    if (mywhooshConnected && pFTMSFeatureCharacteristic_Peripheral != nullptr) {
        if (pFTMSFeatureCharacteristic_Peripheral->getSubscribedCount() > 0) {
            pFTMSFeatureCharacteristic_Peripheral->setValue(data, length);
            pFTMSFeatureCharacteristic_Peripheral->notify();
            // char dataStr[length * 3 + 1];
            // dataStr[length*3] = '\0';
            // for (size_t i = 0; i < length; i++) {
            //     sprintf(dataStr + i * 3, "%02X ", data[i]);
            // }
            // ts_log_printf("[BLE Peripheral] Forwarded data to App via 0x2AD2 (len %d): %s", length, dataStr);
        }
    }
}

void indicateServiceChanged() {
    if (pServiceChangedCharacteristic_Peripheral != nullptr && mywhooshConnected) {
        if (pServiceChangedCharacteristic_Peripheral->getSubscribedCount() > 0) {
            uint16_t range[] = {0x0001, 0xFFFF}; 
            pServiceChangedCharacteristic_Peripheral->setValue((uint8_t*)range, sizeof(range));
            pServiceChangedCharacteristic_Peripheral->indicate();
            ts_log_printf("[BLE Peripheral] Indicated Service Changed (0x2A05) to App. Range: 0x%04X-0x%04X", range[0], range[1]);
        }
    }
}

// --- blePeripheralSetupTask_func Implementation ---
void blePeripheralSetupTask_func(void *pvParameters) {
    ts_log_printf("[BLE Peripheral Task:%s] Task started on core %d.", pcTaskGetName(NULL), xPortGetCoreID());
    vTaskDelay(pdMS_TO_TICKS(200));

    pServer_Peripheral = NimBLEDevice::createServer();
    if (!pServer_Peripheral) { 
        ts_log_printf("FATAL: Failed to create server in blePeripheralSetupTask_func");
        vTaskDelete(NULL); return;
    }
    pServer_Peripheral->setCallbacks(&myServerCallbacks_global);

    pFTMSService_Peripheral = pServer_Peripheral->createService(NimBLEUUID((uint16_t)FTMS_SERVICE_UUID_SHORT));
    NimBLEService* pDISService = pServer_Peripheral->createService(NimBLEUUID((uint16_t)DEVICE_INFORMATION_SERVICE_UUID_SHORT));
    NimBLEService* pGenericAccessService = pServer_Peripheral->createService(NimBLEUUID((uint16_t)GENERIC_ACCESS_UUID_SHORT));
    NimBLEService* pGattService = pServer_Peripheral->createService(NimBLEUUID((uint16_t)GENERIC_ATTRIBUTE_UUID_SHORT));


    if (pFTMSService_Peripheral) {
        ts_log_printf("  Configuring Fitness Machine Service (0x1826)...");

        // FTMS Feature (0x2AD2) - Now dynamic, properties NOTIFY
        pFTMSFeatureCharacteristic_Peripheral = pFTMSService_Peripheral->createCharacteristic(
                                                    NimBLEUUID((uint16_t)FTMS_FEATURE_UUID_SHORT),
                                                    NIMBLE_PROPERTY::NOTIFY); // Properties set to NOTIFY
        if(pFTMSFeatureCharacteristic_Peripheral){
            // No static value set here, it will be updated by sendRawFTMSFeatureDataToApp
            pFTMSFeatureCharacteristic_Peripheral->setCallbacks(&myFTMSFeatureCallbacks_instance_local); 
            ts_log_printf("    FTMS Feature (0x2AD2) created. Properties: NOTIFY (Dynamically updated)");
        } else {ts_log_printf("    FAILED to create FTMS Feature (0x2AD2).");}

        pIndoorBikeDataCharacteristic_Peripheral = pFTMSService_Peripheral->createCharacteristic(
                                                    NimBLEUUID((uint16_t)FTMS_INDOOR_BIKE_DATA_UUID_SHORT), 
                                                    NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ ); // Properties NOTIFY, READ
        if(pIndoorBikeDataCharacteristic_Peripheral) {
            pIndoorBikeDataCharacteristic_Peripheral->setCallbacks(&myIndoorBikeDataCallbacks_instance_local);
            ts_log_printf("    Indoor Bike Data (0x2ACC) created. Properties: NOTIFY, READ");
        } else {ts_log_printf("    FAILED to create Indoor Bike Data (0x2ACC).");}
        
        pTrainingStatusCharacteristic_Peripheral = pFTMSService_Peripheral->createCharacteristic(
                                                    NimBLEUUID((uint16_t)FTMS_TRAINING_STATUS_UUID_SHORT), NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ );
        if(pTrainingStatusCharacteristic_Peripheral){
            pTrainingStatusCharacteristic_Peripheral->setCallbacks(&myTrainingStatusCallbacks_instance_local);
            uint8_t initialTrainingStatus = 0x0D; 
            pTrainingStatusCharacteristic_Peripheral->setValue(&initialTrainingStatus, 1);
            ts_log_printf("    Training Status (0x2AD3) created.");
        } else {ts_log_printf("    FAILED to create Training Status (0x2AD3).");}
        
        uint8_t speedRangePayload[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; 
        pSupportedSpeedRangeCharacteristic_Peripheral = pFTMSService_Peripheral->createCharacteristic(
                                                    NimBLEUUID((uint16_t)FTMS_SUPPORTED_SPEED_RANGE_UUID_SHORT), NIMBLE_PROPERTY::READ);
        if (pSupportedSpeedRangeCharacteristic_Peripheral) {
            pSupportedSpeedRangeCharacteristic_Peripheral->setValue(speedRangePayload, sizeof(speedRangePayload));
            ts_log_printf("    Supported Speed Range (0x2AD4) set to all zeros.");
        } else {ts_log_printf("    FAILED to create Supported Speed Range (0x2AD4).");}

        uint8_t inclinationRangePayload[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; 
        pSupportedInclinationRangeCharacteristic_Peripheral = pFTMSService_Peripheral->createCharacteristic(
                                                    NimBLEUUID((uint16_t)FTMS_SUPPORTED_INCLINATION_RANGE_UUID_SHORT), NIMBLE_PROPERTY::READ);
        if (pSupportedInclinationRangeCharacteristic_Peripheral) {
            pSupportedInclinationRangeCharacteristic_Peripheral->setValue(inclinationRangePayload, sizeof(inclinationRangePayload));
            ts_log_printf("    Supported Inclination Range (0x2AD5) set to all zeros.");
        } else {ts_log_printf("    FAILED to create Supported Inclination Range (0x2AD5).");}

        uint8_t resistanceRangePayload[6] = {0x0A, 0x00, 0x50, 0x00, 0x0A, 0x00};  
        pSupportedResistanceRangeCharacteristic_Peripheral = pFTMSService_Peripheral->createCharacteristic(
                                                    NimBLEUUID((uint16_t)FTMS_SUPPORTED_RESISTANCE_RANGE_UUID_SHORT), NIMBLE_PROPERTY::READ);
        if (pSupportedResistanceRangeCharacteristic_Peripheral) {
            pSupportedResistanceRangeCharacteristic_Peripheral->setValue(resistanceRangePayload, sizeof(resistanceRangePayload));
             ts_log_printf("    Supported Resistance Level Range (0x2AD6) set: Min:1.0, Max:8.0, Inc:1.0 (0.1 resolution)");
        } else { ts_log_printf("    FAILED to create Supported Resistance Level Range (0x2AD6)."); }

        int16_t min_power = 0; int16_t max_power = 1000; uint16_t inc_power = 1;   
        uint8_t powerRangePayload[6]; 
        memcpy(powerRangePayload, &min_power, 2);
        memcpy(powerRangePayload + 2, &max_power, 2);
        memcpy(powerRangePayload + 4, &inc_power, 2);
        pSupportedPowerRangeCharacteristic_Peripheral = pFTMSService_Peripheral->createCharacteristic(
                                                    NimBLEUUID((uint16_t)FTMS_SUPPORTED_POWER_RANGE_UUID_SHORT), NIMBLE_PROPERTY::READ);
        if (pSupportedPowerRangeCharacteristic_Peripheral) {
            pSupportedPowerRangeCharacteristic_Peripheral->setValue(powerRangePayload, sizeof(powerRangePayload));
             ts_log_printf("    Supported Power Range (0x2AD8) created.");
        } else {ts_log_printf("    FAILED to create Supported Power Range (0x2AD8).");}
        
        uint8_t hrRangePayload[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        pSupportedHeartRateRangeCharacteristic_Peripheral = pFTMSService_Peripheral->createCharacteristic(
                                                    NimBLEUUID((uint16_t)FTMS_SUPPORTED_HEART_RATE_RANGE_UUID_SHORT),NIMBLE_PROPERTY::READ);
        if(pSupportedHeartRateRangeCharacteristic_Peripheral) {
            pSupportedHeartRateRangeCharacteristic_Peripheral->setValue(hrRangePayload, sizeof(hrRangePayload));
            ts_log_printf("    Supported Heart Rate Range (0x2AD7) set to all zeros.");
        } else {ts_log_printf("    FAILED to create Supported Heart Rate Range (0x2AD7).");}

        pControlPointCharacteristic_Peripheral = pFTMSService_Peripheral->createCharacteristic(NimBLEUUID((uint16_t)FTMS_CONTROL_POINT_UUID_SHORT), NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::INDICATE);
        if(pControlPointCharacteristic_Peripheral) {
            pControlPointCharacteristic_Peripheral->setCallbacks(&myControlPointCallbacks_instance_local);
            ts_log_printf("    FTMS Control Point (0x2AD9) created.");
        } else {ts_log_printf("    FAILED to create FTMS Control Point (0x2AD9).");}

        pFitnessMachineStatusCharacteristic_Peripheral = pFTMSService_Peripheral->createCharacteristic(NimBLEUUID((uint16_t)FTMS_STATUS_UUID_SHORT), NIMBLE_PROPERTY::NOTIFY);
        if (pFitnessMachineStatusCharacteristic_Peripheral) {
            pFitnessMachineStatusCharacteristic_Peripheral->setCallbacks(&myFitnessMachineStatusCallbacks_instance_local);
            uint8_t initialFMStatus = 0x02; 
            pFitnessMachineStatusCharacteristic_Peripheral->setValue(&initialFMStatus, 1);
            ts_log_printf("    Fitness Machine Status (0x2ADA) created.");
        } else {ts_log_printf("    FAILED to create Fitness Machine Status (0x2ADA).");}

    } else { 
        ts_log_printf("FATAL: Failed to create FTMS Service in blePeripheralSetupTask_func");
        vTaskDelete(NULL); return;
    }

    if (pDISService) {
        ts_log_printf("  Configuring Device Information Service (0x180A)...");
        NimBLECharacteristic* pManuf = pDISService->createCharacteristic(NimBLEUUID((uint16_t)MANUFACTURER_NAME_UUID_SHORT), NIMBLE_PROPERTY::READ);
        if(pManuf) pManuf->setValue("DIY Project: ESP32 Bridge"); 

        NimBLECharacteristic* pModel = pDISService->createCharacteristic(NimBLEUUID((uint16_t)MODEL_NUMBER_UUID_SHORT), NIMBLE_PROPERTY::READ);
        if(pModel) pModel->setValue("ESP32-FTMS-S26-Bridge-v5"); 

        NimBLECharacteristic* pFirm = pDISService->createCharacteristic(NimBLEUUID((uint16_t)FIRMWARE_REVISION_UUID_SHORT), NIMBLE_PROPERTY::READ);
        if(pFirm) pFirm->setValue("1.1.3"); 

        NimBLECharacteristic* pSysId = pDISService->createCharacteristic(NimBLEUUID((uint16_t)SYSTEM_ID_UUID_SHORT), NIMBLE_PROPERTY::READ);
        if (pSysId) {
            uint8_t systemIdPayload[] = {0x49, 0xB6, 0xE2, 0x3C, 0x01, 0xAB, 0x00, 0x00}; 
            pSysId->setValue(systemIdPayload, sizeof(systemIdPayload));
            ts_log_printf("    System ID (0x2A23) created with Merach value.");
        } else {ts_log_printf("    FAILED to create System ID (0x2A23).");}

    } else {ts_log_printf("  FAILED to create Device Information Service (0x180A).");}


    if(pGenericAccessService){
        ts_log_printf("  Configuring Generic Access Service (0x1800)...");
        NimBLECharacteristic* pDevNameChar = pGenericAccessService->createCharacteristic(NimBLEUUID((uint16_t)DEVICE_NAME_UUID_SHORT), NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
        if(pDevNameChar) pDevNameChar->setValue(globalDeviceName);
        
        NimBLECharacteristic* pAppearanceChar = pGenericAccessService->createCharacteristic(NimBLEUUID((uint16_t)APPEARANCE_UUID_SHORT), NIMBLE_PROPERTY::READ);
        if(pAppearanceChar) {
            uint16_t appearanceValue = 0x0741; // Indoor Bike (reverted from 0x0000)
            pAppearanceChar->setValue(appearanceValue);
            ts_log_printf("    Appearance (0x2A01) set to 0x%04X (Indoor Bike).", appearanceValue);
        } else {ts_log_printf("    FAILED to create Appearance (0x2A01).");}

    } else {ts_log_printf("  FAILED to create Generic Access Service (0x1800).");}

     if(pGattService){
        ts_log_printf("  Configuring Generic Attribute Service (0x1801)...");
        pServiceChangedCharacteristic_Peripheral = pGattService->createCharacteristic(NimBLEUUID((uint16_t)SERVICE_CHANGED_UUID_SHORT), NIMBLE_PROPERTY::INDICATE);
        if(pServiceChangedCharacteristic_Peripheral) {
            pServiceChangedCharacteristic_Peripheral->setCallbacks(&myServiceChangedCallbacks_instance_local);
            ts_log_printf("    Service Changed (0x2A05) created.");
        } else {ts_log_printf("    FAILED to create Service Changed (0x2A05).");}
     } else {ts_log_printf("  FAILED to create Generic Attribute Service (0x1801).");}

    if (pFTMSService_Peripheral) pFTMSService_Peripheral->start();
    if (pDISService) pDISService->start();
    if (pGenericAccessService) pGenericAccessService->start();
    if (pGattService) pGattService->start();
    vTaskDelay(pdMS_TO_TICKS(100)); 

    NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
    if (!pAdvertising) { 
        ts_log_printf("FATAL: Failed to get advertising object in blePeripheralSetupTask_func");
        vTaskDelete(NULL); return;
     }
    if(pAdvertising->isAdvertising()) pAdvertising->stop(); 

    NimBLEAdvertisementData advertisementData;
    advertisementData.setFlags(0x06); 
    advertisementData.setCompleteServices(NimBLEUUID((uint16_t)FTMS_SERVICE_UUID_SHORT)); 
    
    uint16_t appearanceValueForAdv = 0x0741; // Indoor Bike
    advertisementData.setAppearance(appearanceValueForAdv);
    
    advertisementData.setName(globalDeviceName); 
    
    pAdvertising->setAdvertisementData(advertisementData);
    
    NimBLEAdvertisementData scanResponseData; 
    pAdvertising->setScanResponseData(scanResponseData);
    
    pAdvertising->setMinPreferred(0x06);  
    pAdvertising->setMaxPreferred(0x12);

    if (pAdvertising->start()) {
        ts_log_printf("[BLE Peripheral Task] BLE Advertising started as '%s'. Appearance: 0x%04X", globalDeviceName.c_str(), appearanceValueForAdv);
    } else {
        ts_log_printf("[BLE Peripheral Task] FAILED to start BLE Advertising.");
    }

    ts_log_printf("[BLE Peripheral Task] Peripheral setup complete. Task idling.");
    while(1) { vTaskDelay(pdMS_TO_TICKS(10000)); } 
}
