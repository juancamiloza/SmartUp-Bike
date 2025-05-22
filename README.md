ESP32 FTMS Bike Bridge (SmartUp Bike)
Project Goal

This project aims to transform a "dumb" fitness bike (specifically tested with a Merach S26) into a smart trainer by using an ESP32 (LilyGo T-Deck S3) as a bidirectional bridge. The ESP32 reads data (power, cadence, speed) from the bike and relays it to fitness applications (e.g., MyWhoosh) using the standard Fitness Machine Service (FTMS) protocol. Conversely, it receives terrain or resistance commands from the app and will eventually control a stepper motor to physically adjust the bike's resistance knob, simulating the virtual environment.

Current Status & Key Achievements (As of May 2024)

-The ESP32 successfully connects to the Merach S26 bike via BLE and reads its proprietary data.
-It advertises as an FTMS device named "DIY FTMS Bike."
-Fitness apps (MyWhoosh, nRF Connect) can connect to the ESP32.
-MyWhoosh successfully subscribes to the Indoor Bike Data characteristic (0x2ACC) and displays live power, speed, cadence, calories, and resistance.
-The ESP32 forwards non-standard notifications from the bike's FTMS Feature characteristic (0x2AD2) to the connected app.
-Initial values for Training Status (0x2AD3) and Fitness Machine Status (0x2ADA) are set appropriately.
-The system can receive target inclination and resistance level commands from MyWhoosh.
-The ESP32's display shows current bike data, app connection status, target resistance/inclination from the app, and whether the bike's current resistance matches the app's target.

Key Features

-Bike Data Acquisition: Reads power, speed, and cadence from the connected fitness bike.
-FTMS Peripheral: Emulates a standard FTMS smart trainer, allowing compatibility with various fitness apps.
-Bidirectional Communication: Sends bike data to apps and receives control commands (target resistance, inclination) from apps.
-Real-time Display: Utilizes the LilyGo T-Deck S3's built-in screen to show vital statistics, connection status, and target values.
-Resistance/Inclination Reception: Parses and stores target resistance and inclination values sent by the fitness app.
-(Planned) Stepper Motor Control: Future development will include controlling a stepper motor to physically adjust the bike's resistance based on app commands.

Hardware

-Microcontroller: ESP32 (specifically LilyGo T-Deck S3 with built-in screen and keyboard)
-Fitness Bike: Initially developed with Merach S26. Aims to be adaptable to other bikes transmitting similar data.
-(Future Addition): Stepper motor (e.g., NEMA 17) and stepper motor driver (e.g., A4988 or DRV8825).

Software & Development Environment

-IDE: Arduino IDE
-ESP32 Core: Version 2.0.17
-BLE Library: NimBLE Arduino library (Version 1.4.0)
-Display Library: TFT_eSPI
-Operating System: FreeRTOS (via Arduino Core for ESP32)

Functionality Overview

The system operates in two primary BLE roles:

-BLE Client (Central Role): The ESP32 scans for and connects to the specified fitness bike (Merach S26 using its MAC address). It subscribes to relevant characteristics to receive data like power, speed, and cadence. For the Merach S26, this involves a custom service (0xFFF0) for primary data and also interaction with its FTMS-like characteristics.
-BLE Peripheral (Server Role): The ESP32 advertises itself as an FTMS device. Fitness apps like MyWhoosh can connect to it. The ESP32 serves the bike's data through standard FTMS characteristics (e.g., Indoor Bike Data 0x2ACC) and receives commands through the FTMS Control Point (0x2AD9) for target resistance and inclination.

The FTMS_test.ino sketch orchestrates these roles, manages the display, and handles user input (button presses for pairing).

Code Structure

The project is organized into several key files:

-FTMS_test.ino: The main Arduino sketch. Handles initialization, the main loop, display updates, button input, and global variable definitions.
-ble_client_manager.h & ble_client_manager.cpp: Manages the BLE client connection to the fitness bike, including scanning, connecting, discovering services/characteristics, and handling notifications from the bike.
-ble_peripheral_manager.h & ble_peripheral_manager.cpp: Manages the BLE peripheral (server) that advertises as an FTMS device. Defines services, characteristics, and callbacks for interactions with fitness apps.
-config.h: Contains compile-time configurations such as the bike's MAC address, UUIDs for BLE services and characteristics, and pin definitions.
-logger.h & logger.cpp: Provides a simple timestamped logging utility for debugging output to the Serial monitor.

Next Steps & Future Enhancements

-Stepper Motor Integration: Implement the control logic for a stepper motor to physically adjust the bike's resistance knob based on commands from the fitness app.
-Calibration for Stepper Motor: Develop a calibration routine for the stepper motor to map its movement to the bike's resistance levels.
-Virtual Gears: Explore adding virtual gear shifting capabilities.
-ERG Mode Support: Implement proper ERG mode functionality where the ESP32 maintains a target power level.
-Robust Pairing System: Enhance the pairing process, possibly using a QR code displayed on the ESP32 screen to initiate a Wi-Fi hotspot portal for bike selection and MAC address input.
-Wider Bike Compatibility: Investigate and add support for other "dumb" bikes.

Basic Setup & Usage

-Clone Repository: git clone <repository-url>
-Install Arduino IDE: Download and install the latest Arduino IDE.
-Install ESP32 Core: Follow instructions to add ESP32 board support to the Arduino IDE (Version 2.0.17 recommended).
-Install Libraries:
    -NimBLE-Arduino (Version 1.4.0)
    -TFT_eSPI (ensure it's configured for your specific ESP32 T-Deck S3 display)
-Configure:
    -Open config.h.
    -Crucially, update BIKE_MAC_ADDRESS with the MAC address of your fitness bike.
-Upload:
    -Connect your ESP32 LilyGo T-Deck S3 to your computer.
    -Select the correct board and port in the Arduino IDE.
    -Compile and upload the FTMS_test.ino sketch.
-Operation:
    -The ESP32 will start. The display will show "Bike: SCAN (BTN)".
    -Press the button defined by PAIR_BUTTON_PIN in config.h (GPIO14 by default on the T-Deck) to start scanning for your bike.
    -Once the bike is found, the display will show "Bike: PAIR (BTN)". Press the button again to connect.
    -Once connected, the ESP32 will advertise as "DIY FTMS Bike".
    -Open your fitness app (e.g., MyWhoosh) and connect to "DIY FTMS Bike".

Contributing

Contributions are welcome! If you'd like to contribute, please feel free to fork the repository, make your changes, and submit a pull request. For major changes, please open an issue first to discuss what you would like to change.

License

This project is licensed under the Apache 2.0 License.
