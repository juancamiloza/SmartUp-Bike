#ifndef CONFIG_H
#define CONFIG_H

// --- Button Setup ---
const int PAIR_BUTTON_PIN = 14; // GPIO pin for the pairing button (ensure this is correct for your ESP32 board)

// --- Bike Sensor (Central Role - ESP32 connects to Bike) ---
#define BIKE_MAC_ADDRESS "24:00:0C:A0:4B:4B" // YOUR BIKE'S ACTUAL MAC ADDRESS

// Service and Characteristic UUIDs for the BIKE (if it uses standard FTMS or known custom ones)
#define BIKE_FTMS_SERVICE_UUID_STR "00001826-0000-1000-8000-00805f9b34fb" // Standard FTMS
#define BIKE_FTMS_INDOOR_BIKE_DATA_CHAR_UUID_STR "00002ACC-0000-1000-8000-00805f9b34fb"
#define BIKE_FTMS_FEATURE_CHAR_UUID_STR "00002AD2-0000-1000-8000-00805f9b34fb"
#define BIKE_FTMS_CONTROL_POINT_CHAR_UUID_STR "00002AD9-0000-1000-8000-00805f9b34fb"

// Custom service and characteristic for Merach bike data (primary data source)
#define CUSTOM_SERVICE_UUID_STR "0000fff0-0000-1000-8000-00805f9b34fb"
#define CUSTOM_DATA_CHAR_UUID_STR "0000fff1-0000-1000-8000-00805f9b34fb"


// --- ESP32 Peripheral Role (Advertised Services & Characteristics to MyWhoosh) ---
// These are standard 16-bit UUIDs, often represented as such in code.
// Full 128-bit UUIDs are "0000XXXX-0000-1000-8000-00805f9b34fb"

// Standard BLE Service UUIDs (16-bit)
#define GENERIC_ACCESS_UUID_SHORT            0x1800
#define GENERIC_ATTRIBUTE_UUID_SHORT         0x1801
#define DEVICE_INFORMATION_SERVICE_UUID_SHORT 0x180A
#define FTMS_SERVICE_UUID_SHORT              0x1826 // Fitness Machine Service

// Standard BLE Characteristic UUIDs (16-bit)
// GAS Characteristics
#define DEVICE_NAME_UUID_SHORT               0x2A00
#define APPEARANCE_UUID_SHORT                0x2A01
// GATT Characteristics
#define SERVICE_CHANGED_UUID_SHORT           0x2A05
// DIS Characteristics
#define MANUFACTURER_NAME_UUID_SHORT         0x2A29
#define MODEL_NUMBER_UUID_SHORT              0x2A24
#define FIRMWARE_REVISION_UUID_SHORT         0x2A26
#define SOFTWARE_REVISION_UUID_SHORT         0x2A28 // Added for completeness if used
#define HARDWARE_REVISION_UUID_SHORT         0x2A27 // Added for completeness if used
#define SYSTEM_ID_UUID_SHORT                 0x2A23 // ADDED for System ID
// FTMS Characteristics
#define FTMS_FEATURE_UUID_SHORT              0x2AD2
#define FTMS_INDOOR_BIKE_DATA_UUID_SHORT     0x2ACC
#define FTMS_TRAINING_STATUS_UUID_SHORT      0x2AD3
#define FTMS_SUPPORTED_SPEED_RANGE_UUID_SHORT 0x2AD4
#define FTMS_SUPPORTED_INCLINATION_RANGE_UUID_SHORT 0x2AD5
#define FTMS_SUPPORTED_RESISTANCE_RANGE_UUID_SHORT 0x2AD6
#define FTMS_SUPPORTED_HEART_RATE_RANGE_UUID_SHORT 0x2AD7 
#define FTMS_SUPPORTED_POWER_RANGE_UUID_SHORT 0x2AD8
#define FTMS_CONTROL_POINT_UUID_SHORT        0x2AD9
#define FTMS_STATUS_UUID_SHORT               0x2ADA


// Descriptor UUIDs
#define CCCD_UUID_SHORT                      0x2902 // Client Characteristic Configuration Descriptor

#endif // CONFIG_H
