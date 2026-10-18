# ESP32 IMU-based Digital Setting Circles 
An ESP32-based real-time orientation tracking system designed for telescopes with a Vixen dovetail socket. The project utilizes an IMU sensor suite, the Madgwick fusion filter, and a FreeRTOS architecture to broadcast current telescope coordinates over Wi-Fi/TCP onto a Python-based bridge script which transmits the signal to Stellarium over LX200.

**WORK IN PROGRESS** <br>
Current stage: fixing I2C connectivity issues (possibly poor solder connection) <br>
### Working software features:
- Madgwick filter based sensor fusion
- Sensor calibration
- FreeRTOS architecture
- The board advertises itself as dsc.local via mDNS and runs a TCP server on port 5000.
- Python bridge that connects to the board which calculates RA/DEC and connects to Stellarium's Telescope Control plugin. The bridge also receives logs and can send commands to the board
- 6 commands that can be sent via serial or TCP connection

### Known issues
- mDNS sometimes crashes without any errors requiring restart
- sensors sometimes fail to intialize (physical connectivity issue)
- the board sometimes freezes on calibration (physical connectivity issue)

## Hardware
- **MCU:** ESP-WROOM-32 DEVKIT V1
- **Accelerometer/Gyroscope:** MPU-6515 (or other MPU-6050 derivatives)
- **Magnetometer:** QMC5883L (or other HMC5883L derivatives)
- **Buzzer module:** CAR9012 (or any passive buzzer module that works on 3.3V)
- **Batteries:** 4x AAA
- **Peripherals:** Red, yellow and green LEDs, a two-state switch
- **Passives:** 3x 220Ω resistors, 100μF 16V capacitor

## Pin connections
| Pin | Connection |
| --- | ---------- |
| 22 | SCL | 
| 21 | SDA |
| 19 | Red LED |
| 18 | Yellow LED |
| 17 | Green LED |
| 16 | Buzzer I/O |

## Setup and installation
**IMPORTANT** <br>
The FastIMU library has wrong values for MPU6515 constants in file src/sensors/F_MPU6515.hpp: <br>
`MPU6515_WHO_AM_I_MPU6515` macro should be modified to be `0x75` <br>
`MPU6515_WHOAMI_DEFAULT_VALUE` macro should be modified to be `0x74`

**1. Prerequisites:**
- Arduino IDE 2.x or PlatformIO
- ESP32 Board Package installed (v2.x or v3.x)
- Libraries for the sketch: `FastIMU`, `QMC5883LCompass`, `MadgwickAHRS`, `ArduinoJson`
- Libraries for the Python bridge: `astropy`

**2. Configuration:**
- Open `source.ino` and update Wi-Fi credentials:
  ```
  const char* ssid = "YOUR_SSID";
  const char* password = "YOUR_PASSWORD";
  ```
- Adjust magnetic declination for your location:
  ```
  float magneticDeclinationDegrees = 6.38; // Example for Krakow
  ```
- Open the `bridge.py` script and modify the coordinate variables to match your location:
  ```
  # placeholder values
  latitude = 21   # degrees
  longitude = 37  # degrees
  elevation = 67  # meters
  ```
  
**3. Compilation & Upload:**
- Select ESP32 Dev Module in Arduino IDE
- Upload the sketch
- Open Serial Monitor at 115200 baud (optional)

**4. Connecting to the Bridge and Stellarium:**
- Open the Bridge using `python3 bridge.py` command
- Open Stellarium Telesope Control plugin
- Create new LX200 type telescope
- Select TCP connection on IP 127.0.0.1 and port 10001

## Commands
| Key | Command |
| --- | ------- |
| c | Calibrate QMC5883L |
| m | Calibrate MPU6515 |
| r | Clear NVS (calibration reset) |
| b{n} | Change brightness to `n` |
| e{n} | Trigger error code `n` |
| h | List all available commands |


## 3D prints
All 3D prints except the dovetail adapter are designed for a generic ABS box I bought for this project.
- **dovetailAdapter:** Working as intended. Designed for a SkyWatcher dobson dovetail but should fit with other Vixen style dovetails 
- **boxMount:** Working as intended. Connects to the dovetail adapter and is designed to be connected to the bottom of the box via bolts and nuts
- **internalSupport:** Needs correction. Designed to be screwed in to the inside of the box. The top side has space for a 5x7cm perfboard with most of the components. There is enough spacing on the bottom side for a 4xAAA battery holder.


