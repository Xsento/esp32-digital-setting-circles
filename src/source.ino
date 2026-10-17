#include "FastIMU.h"
#include <Wire.h>
#include <QMC5883LCompass.h>
#include <Preferences.h>
#include <MadgwickAHRS.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>

// sensor stuff
constexpr int MPU_ADDRESS = 0x68;
constexpr int QMC_ADDRESS = 0x0D;
MPU6515 MPU;
QMC5883LCompass compass;
AccelData accelData;
GyroData gyroData;

// cablibration data
calData MPU_calib = { 0 };  //Calibration data
float QMC_calData[6];

// filter stuff
float magneticDeclinationDegrees = 6.0 + 32.0/60.0;
Madgwick* filterPtr = nullptr;
unsigned long microsPerReading, microsPrevious;
float roll, pitch, heading;

// wifi
const char* ssid = "SSID";
const char* password = "PASSWORD";
const char* mDNSaddress = "dsc";
const int port = 5000;
WiFiServer server(port);

// task handles
TaskHandle_t serialPrintTaskHandle = nullptr;
TaskHandle_t transmitDataTaskHandle = nullptr;
TaskHandle_t filterUpdateTaskHandle = nullptr;
TaskHandle_t commandsTaskHandle = nullptr;
TaskHandle_t errorTaskHandle = nullptr;
TaskHandle_t systemCheckTaskHandle = nullptr;

// semaphores
SemaphoreHandle_t rotationDataMutex = nullptr;
SemaphoreHandle_t serialPrintMutex = nullptr;

// LEDs
#undef LED_BUILTIN
#define LED_BUILTIN 2
constexpr int RED_LED = 19;
constexpr int YELLOW_LED = 18;
constexpr int GREEN_LED = 17; 
bool BUILTIN_LED_state = false;
bool RED_LED_state = false;
bool YELLOW_LED_state = false;
bool GREEN_LED_state = false;
TimerHandle_t yellowBlinkTimer;

// LED PWM
constexpr int RED_LED_CHANNEL = 2;
constexpr int YELLOW_LED_CHANNEL = 1;
constexpr int GREEN_LED_CHANNEL = 0;
const int freq = 5000;
const int resolution = 8; // 2^resolution options 
int brightness = 16; // 0 - 255

// buzzer
constexpr int BUZZER_PIN = 16;
constexpr uint ERROR_TONE = 1000;
constexpr uint CALIB_TONE = 500;
constexpr uint STARTUP_TONE = 100;

// errors
TimerHandle_t errorBlinkTimer = nullptr;
int errorCodeBlinkIntervalMS = 300;
QueueHandle_t xErrorQueue = nullptr;

// other
Preferences preferences;
QueueHandle_t xMessageQueue = nullptr;
QueueHandle_t xCommandQueue = nullptr;


struct LogMessage {
  char text[128];
};

struct commandObj {
  char text[16];
};

#define DISABLE_SERIAL_POSITION_UPDATES

void errorTask(void* parameter){
  int errorCode;
  LogMessage msg;
    
  for (;;) {
    if (xQueueReceive(xErrorQueue, &errorCode, portMAX_DELAY) == pdPASS) {
      if (errorCode < 10) {
        if (serialPrintTaskHandle) xTaskNotifyGive(serialPrintTaskHandle);
        if (filterUpdateTaskHandle) xTaskNotifyGive(filterUpdateTaskHandle);
        if (transmitDataTaskHandle) xTaskNotifyGive(transmitDataTaskHandle);
        
        sendLog("FATAL ERROR");
      }

      switch (errorCode) {
          case 1:
              strncpy(msg.text, "MPU6515 Error", sizeof(msg.text) - 1);
              break;
              
          case 2:
              strncpy(msg.text, "QMC5883L Error", sizeof(msg.text) - 1);
              break;

          case 3:
              strncpy(msg.text, "RTOS Error", sizeof(msg.text) - 1);
              break;

          case 4:
              strncpy(msg.text, "WiFi Connection Failure", sizeof(msg.text) - 1);
              break;

          case 5:
              strncpy(msg.text, "MDNS Setup Failure", sizeof(msg.text) - 1);
              break;
              
          default:
              strncpy(msg.text, "Unknown Error", sizeof(msg.text) - 1);
              break;
      }
      msg.text[sizeof(msg.text) - 1] = '\0';

      auto totalToggles = std::make_unique<int>(errorCode * 2);
      
      TimerHandle_t errorBlinkTimer = xTimerCreate("errBlink", pdMS_TO_TICKS(200), pdTRUE, totalToggles.get(), vErrorBlinkTimerCallback);

      while (1) {
        sendLog(msg.text);
        sendLogFmt("Error code: %d", errorCode);

        *totalToggles = errorCode * 2;
        
        vTimerSetTimerID(errorBlinkTimer, totalToggles.get());        
        xTimerStart(errorBlinkTimer, 0);

        vTaskDelay(pdMS_TO_TICKS(2000 + (*totalToggles * 200)));
      }
    }
  }
}

void serialPrintTask(void* parameter){
  while (true){
    if (ulTaskNotifyTake(pdTRUE, 0) > 0){   // fatal error notification
      break;
    }
    if (xSemaphoreTake(serialPrintMutex, portMAX_DELAY)){
      Serial.print("Heading ");
      Serial.print(heading);
      Serial.print(",");
      Serial.print("Pitch ");
      Serial.print(pitch);
      Serial.print(",");
      Serial.print("Roll ");
      Serial.println(roll);
      xSemaphoreGive(serialPrintMutex);
      vTaskDelay(100/portTICK_PERIOD_MS);
    }
  }
  vTaskDelete(nullptr);
}

void commandsTask(void* parameter){
  while (true){
    char command = '\0';
    char buffer[16]; 
    if (Serial.available() > 0) {      
      int bytesToRead = min(Serial.available(), (int)(sizeof(buffer) - 1));
      int bytesRead = Serial.readBytes(buffer, bytesToRead);
      buffer[bytesRead] = '\0';
      command = buffer[0];
    
    }
    else if (xQueueReceive(xCommandQueue, &buffer, 0) == pdPASS){
      command = buffer[0];
    }

    if (command) {
      switch (command) {
        case 'c':{
          sendLog("--- Received command: Calibrate QMC5883L ---");
          if (serialPrintTaskHandle) vTaskSuspend(serialPrintTaskHandle);
          xTaskNotifyGive(transmitDataTaskHandle);
          vTaskSuspend(filterUpdateTaskHandle);
          calibrateQMC5883L();
          delete filterPtr;
          filterPtr = new Madgwick;
          filterPtr->begin(25);
          if (serialPrintTaskHandle) vTaskResume(serialPrintTaskHandle);
          microsPrevious = micros();
          vTaskResume(filterUpdateTaskHandle);
          break;
        }

        case 'm':{
          sendLog("--- Received command: Calibrate MPU6050 ---");
          if (serialPrintTaskHandle) vTaskSuspend(serialPrintTaskHandle);
          xTaskNotifyGive(transmitDataTaskHandle);
          vTaskSuspend(filterUpdateTaskHandle);
          calibrateMPU6515();
          delete filterPtr;
          filterPtr = new Madgwick;
          filterPtr->begin(25);
          if (serialPrintTaskHandle) vTaskResume(serialPrintTaskHandle);
          microsPrevious = micros();
          vTaskResume(filterUpdateTaskHandle);
          break;
        }

        case 'r':{
          sendLog("--- Received command: Clear NVS (calibration reset) ---");
          preferences.begin("imu", false);
          preferences.clear(); // Kasuje wszystkie zapisane offsety
          preferences.end();
          sendLog("Memory cleared. Reset the ESP32!");
          break;
        }

        case 'e':{
          sendLog("--- Received command: Activate error ---");
          int er = atoi(&buffer[1]);
          xQueueSend(xErrorQueue, &er, portMAX_DELAY);
          break;
        }

        case 'b':{
          sendLog("--- Received command: Change brightness ---");
          int num = atoi(&buffer[1]);   // shifts the buffer by 1 ignoring the first letter
          if (num < 1 || num > 255){
            sendLog("Brightness must be between 1 and 255");
            break;
          }
          brightness = num;
          updateBrightness();
          sendLogFmt("Brightness is now %d", num);
          break;
        }

        case 'h':{
          sendLog("--- AVAILABLE COMMANDS ---");
          sendLog("c - Calibrate Magnetometer (QMC)");
          sendLog("m - Calibrate Accelerometer/Gyroscope (MPU)");
          sendLog("r - Reset/Delete NVS calibration memory");
          sendLog("e{n} - Activate error n");
          sendLog("b{n} - Set LED brightness to n");
          break;
        }

        default:
          sendLogFmt("Unknown command: %s", buffer); 
          break;
      }
    }
    else{
      vTaskDelay(1000/portTICK_PERIOD_MS);
    }
  }
}

void transmitDataTask(void* parameter){
  while (true){
    WiFiClient client = server.available();
    client.setTimeout(1000);
    String output;
    String input;
    LogMessage receivedMsg;
    JsonDocument pos;
    JsonDocument msg;
    volatile bool haltPosUpdate = false; 
    if (client) {
      GREEN_LED_state = 1;
      setLed(GREEN_LED, GREEN_LED_state);
      sendLog("Client connected to ESP32!");
      while (client.connected()) {
        if (ulTaskNotifyTake(pdTRUE, 0) > 0){  // stop sending position updates after fatal error notif
          haltPosUpdate = true;
        }
        if (xQueueReceive(xMessageQueue, &receivedMsg, 0) == pdPASS){
          msg["type"] = "message";
          msg["msg"] = receivedMsg.text;
          serializeJson(msg, output);
          client.println(output);
        }
        if (client.available()){
          output = client.readStringUntil('\n');
          xQueueSend(xCommandQueue, output.c_str(), 0);
        }
        else if (!haltPosUpdate){
          if (xSemaphoreTake(rotationDataMutex, portMAX_DELAY)){
            pos["type"] = "position";
            pos["az"] = heading;
            pos["alt"] = -pitch;
            xSemaphoreGive(rotationDataMutex);

            serializeJson(pos, output);
            client.println(output);
            //sendLogFmt("Heap: %d, output len: %d", ESP.getFreeHeap(), output.length());
          }
          else{
            if (ulTaskNotifyTake(pdTRUE, 0) > 0) haltPosUpdate = false;
          }
        }
        vTaskDelay(100/portTICK_PERIOD_MS);
      }
      client.stop();
      GREEN_LED_state = 0;
      setLed(GREEN_LED, GREEN_LED_state);
      sendLog("Client disconnected.");
    }
    vTaskDelay(500/portTICK_PERIOD_MS);
  }
}

void filterUpdateTask(void* parameter){
  while (true) {
    if (ulTaskNotifyTake(pdTRUE, 0) > 0){   // fatal error notification
      break;
    }

    unsigned long microsNow;

    // update madgwick based on time passed
    microsNow = micros();
    if (microsNow - microsPrevious >= microsPerReading) {
      MPU.update();
      compass.read();
      MPU.getAccel(&accelData);
      MPU.getGyro(&gyroData);

      // COORDINATE SYSTEM:
      // X - FORWARD
      // Y - LEFT
      // Z - UP

      // accelerometer
      float ax = -accelData.accelY;
      float ay = accelData.accelX;
      float az = accelData.accelZ;
      
      // gyro
      float gx = -gyroData.gyroY;
      float gy = gyroData.gyroX;
      float gz = gyroData.gyroZ;
      
      // magnetometer
      float mx = compass.getX();
      float my = compass.getY();
      float mz = -compass.getZ();

      unsigned long actualDt = microsNow - microsPrevious;
      if (actualDt > microsPerReading * 1.5) { // significant delay
        if (xSemaphoreTake(serialPrintMutex, portMAX_DELAY)){
          Serial.print("WARNING - significant delay: ");
          Serial.println(actualDt);
          xSemaphoreGive(serialPrintMutex);
        }
      }

      filterPtr->update(gx,gy,gz,ax,ay,az,mx,my,mz);

      if (xSemaphoreTake(rotationDataMutex, portMAX_DELAY)){
        roll = filterPtr->getRoll();
        pitch = filterPtr->getPitch();
        heading = filterPtr->getYaw();
        heading = 360 - heading;
        heading += magneticDeclinationDegrees; 

        // normalize it again
        if (heading < 0) heading += 360.0;
        if (heading >= 360.0) heading -= 360.0;

        xSemaphoreGive(rotationDataMutex);
      }
      
      // increment previous time, so we keep proper pace
      microsPrevious = microsPrevious + microsPerReading;    
    }
    else{
      unsigned long remainingMicros = microsPerReading - (microsNow - microsPrevious);
      vTaskDelay((remainingMicros/1000) / portTICK_PERIOD_MS);
    } 
  }
  vTaskDelete(nullptr);
}

void vYellowLedTimerCallback(TimerHandle_t xTimer){
  YELLOW_LED_state = !YELLOW_LED_state;
  setLed(YELLOW_LED, YELLOW_LED_state);
}

void vErrorBlinkTimerCallback(TimerHandle_t xTimer){
  int* remainingToggles = (int*)pvTimerGetTimerID(xTimer);

    if (*remainingToggles > 0) {
        RED_LED_state = !RED_LED_state;
        setLed(RED_LED, RED_LED_state);
        if (RED_LED_state) tone(BUZZER_PIN, ERROR_TONE, 200);
        //sendLogFmt("Remaining toggles: %d", *remainingToggles);

        *remainingToggles -= 1;
        vTimerSetTimerID(xTimer, (void*)remainingToggles);
    }

    if (*remainingToggles <= 0) {
        setLed(RED_LED, 0);
        xTimerStop(xTimer, 0);
    }
}

void setup() {
  Wire.begin(); // default I2C pins: SDA=21, SCL=22
  Serial.begin(115200);
  delay(1000);

  pinMode(BUILTIN_LED, OUTPUT);
  digitalWrite(BUILTIN_LED, BUILTIN_LED_state);
  
  pinMode(BUZZER_PIN, OUTPUT);
  noTone(BUZZER_PIN);

  ledcAttach(RED_LED, freq, resolution);
  ledcAttach(YELLOW_LED, freq, resolution);
  ledcAttach(GREEN_LED, freq, resolution);

  ledcWrite(RED_LED, 0);
  ledcWrite(YELLOW_LED, 0);
  ledcWrite(GREEN_LED, 0);


  bool RTOSerror = false;
  // create mutexes
  rotationDataMutex = xSemaphoreCreateMutex();
  serialPrintMutex = xSemaphoreCreateMutex();

  if (!rotationDataMutex) {Serial.println("Failed to create rotationDataMutex"); RTOSerror = true;}
  if (!serialPrintMutex) {Serial.println("Failed to create serialPrintMutex"); RTOSerror = true;}

  // queues
  xErrorQueue = xQueueCreate(5, sizeof(int));
  xMessageQueue = xQueueCreate(15, sizeof(LogMessage));
  xCommandQueue = xQueueCreate(5, sizeof(commandObj));

  yellowBlinkTimer = xTimerCreate("yellowBlinkTimer", pdMS_TO_TICKS(500), pdTRUE, (void*)0, vYellowLedTimerCallback);

  // error task
  xTaskCreatePinnedToCore(errorTask, "errorTask", 4096, nullptr, 1, &errorTaskHandle, 0);
  if (!errorTaskHandle) {sendLog("Failed to create errorTask"); RTOSerror = true;}

  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    attempts++;
    if (attempts >= 8) break;
  }
  if (WiFi.status() != WL_CONNECTED){
    int e = 4;
    xQueueSend(xErrorQueue, &e, portMAX_DELAY);
  }
  else {
    Serial.println("");
    Serial.println("WiFi connected.");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }

  if (!MDNS.begin(mDNSaddress)) {
    int e = 5;
    xQueueSend(xErrorQueue, &e, portMAX_DELAY);
  }
  Serial.print("mDNS started on address: "); Serial.print(mDNSaddress); Serial.println(".local");
  MDNS.addService("dsc", "tcp", port);

  server.begin();
  Serial.print("TCP server started on port"); Serial.println(port);
  
  // I2C timeout
  Wire.setTimeOut(150);

  // init MPU
  int err = MPU.init(MPU_calib, MPU_ADDRESS);
  if (err != 0) {
    sendLog("MPU6515 failed to initialize!");
    int e = 1;
    xQueueSend(xErrorQueue, &e, portMAX_DELAY);
  } else {
    MPUloadCalibration();
  }

  // init QMC
  Wire.beginTransmission(QMC_ADDRESS);
  byte i2c_error = Wire.endTransmission();

  if (i2c_error == 0) {
    compass.init();
    QMCloadCalibration();
  } else {
    sendLogFmt("QMC5883L not found! I2C error: %d", i2c_error);
    int e = 2;
    xQueueSend(xErrorQueue, &e, portMAX_DELAY);
  }

  //compass.setSmoothing(5, true);

  filterPtr = new Madgwick;
  filterPtr->begin(25); // 25 Hz sample frequency
  microsPerReading = 1000000 / 25;
  microsPrevious = micros();
  
  // create tasks
                        //task function, task name, stack size (bytes), parameters, priority, handle, core
  xTaskCreatePinnedToCore(filterUpdateTask, "filterUpdateTask", 4096, nullptr, 3, &filterUpdateTaskHandle, 0);
  xTaskCreatePinnedToCore(commandsTask, "commandsTask", 3072, nullptr, 1, &commandsTaskHandle, 0);
  xTaskCreatePinnedToCore(transmitDataTask, "transmitDataTask", 4096, nullptr, 2, &transmitDataTaskHandle, 1);

  if (!filterUpdateTask) {sendLog("Failed to create filterUpdateTask"); RTOSerror = true;}
  if (!commandsTaskHandle) {sendLog("Failed to create commandsTask"); RTOSerror = true;}
  if (!transmitDataTaskHandle) {sendLog("Failed to create transmitDataTask"); RTOSerror = true;}

  #ifndef DISABLE_SERIAL_POSITION_UPDATES
    xTaskCreatePinnedToCore(serialPrintTask, "serialPrintTask", 2048, nullptr, 1, &serialPrintTaskHandle, 0);
    if (!serialPrintTaskHandle) {sendLog("Failed to create serialPrintTask"); RTOSerror = true;}
  #endif

  if (RTOSerror){
    int e = 3;
    xQueueSend(xErrorQueue, &e, portMAX_DELAY);
  }
  sendLog("Setup Complete");
  BUILTIN_LED_state = true;
  digitalWrite(LED_BUILTIN, BUILTIN_LED_state);
  tone(BUZZER_PIN, STARTUP_TONE, 100);
}

void loop() {

}

void updateBrightness(){
  setLed(RED_LED, RED_LED_state);
  setLed(YELLOW_LED, YELLOW_LED_state);
  setLed(GREEN_LED, GREEN_LED_state);
}

void setLed(const int pin, const bool state){
  (state) ? ledcWrite(pin, brightness) : ledcWrite(pin, 0);
}

void sendLog(const char* text) {
  if (xSemaphoreTake(serialPrintMutex, portMAX_DELAY)){
    Serial.println(text);
    xSemaphoreGive(serialPrintMutex);
  }
  if (xMessageQueue != NULL) {
    LogMessage msg;
    strncpy(msg.text, text, sizeof(msg.text) - 1);
    msg.text[sizeof(msg.text) - 1] = '\0';
    xQueueSend(xMessageQueue, &msg, 0);
  }
}

void sendLogFmt(const char* format, ...) {
  char buffer[128];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  if (xSemaphoreTake(serialPrintMutex, portMAX_DELAY)){
    Serial.println(buffer);
    xSemaphoreGive(serialPrintMutex);
  }
  if (xMessageQueue != NULL) {
    LogMessage msg;
    strncpy(msg.text, buffer, sizeof(msg.text) - 1);
    msg.text[sizeof(msg.text) - 1] = '\0';
    xQueueSend(xMessageQueue, &msg, 0);
  }
}

void calibrateMPU6515() {
  setLed(YELLOW_LED, 1);
  sendLog("MPU6515 calibration will begin in 5 seconds.");
  tone(BUZZER_PIN, CALIB_TONE, 100);
  delay(5000);
  
  sendLog("Keep the board level.");
  Serial.println("TEST 1: Przed tone");
  tone(BUZZER_PIN, CALIB_TONE, 100);

  Serial.println("TEST 2: Przed timerem");
  xTimerStart(yellowBlinkTimer, 0);

  Serial.println("TEST 3: Przed delay");
  delay(3000);

  Serial.println("TEST 4: Przed kalibracja MPU");
  MPU.calibrateAccelGyro(&MPU_calib);

  Serial.println("TEST 5: Po kalibracji");
  tone(BUZZER_PIN, CALIB_TONE, 400);
  xTimerStop(yellowBlinkTimer, 0);
  setLed(YELLOW_LED, 0);
  
  sendLog("MPU6515 calibration done!");
  sendLogFmt("Accel biases X/Y/Z: %.2f, %.2f, %.2f", MPU_calib.accelBias[0], MPU_calib.accelBias[1], MPU_calib.accelBias[2]);
  sendLogFmt("Gyro biases X/Y/Z: %.2f, %.2f, %.2f", MPU_calib.gyroBias[0], MPU_calib.gyroBias[1], MPU_calib.gyroBias[2]);
  
  MPUsaveCalibration();
}

void MPUsaveCalibration() {
  preferences.begin("imu", false); 
  preferences.putBytes("calData", &MPU_calib, sizeof(calData));
  preferences.end();
  sendLog("MPU calibration saved!");
}

void MPUloadCalibration() {
  preferences.begin("imu", true);

  if (preferences.getBytesLength("calData") > 0) {
    preferences.getBytes("calData", &MPU_calib, sizeof(calData));
    sendLog("Loaded MPU calibration.");
    preferences.end();
    //delay(500);
  } else {
    sendLog("No MPU calibration in memory! New calibration required.");
    delay(1000);
    preferences.end();
    calibrateMPU6515();
  }
}

void QMCsaveCalibration() {
  preferences.begin("imu", false); 
  preferences.putBytes("QMC_calData", QMC_calData, sizeof(QMC_calData));
  preferences.end();
  sendLog("QMC calibration saved!");
}

void QMCloadCalibration() {
  preferences.begin("imu", true);
  if (preferences.getBytesLength("QMC_calData") > 0) {
    preferences.getBytes("QMC_calData", QMC_calData, sizeof(QMC_calData));
    compass.setCalibrationOffsets(QMC_calData[0], QMC_calData[1], QMC_calData[2]);
    compass.setCalibrationScales(QMC_calData[3], QMC_calData[4], QMC_calData[5]);
    
    sendLog("Loaded QMC calibration.");
    sendLogFmt("Mag biases X/Y/Z: %.2f, %.2f, %.2f", (float)QMC_calData[0], (float)QMC_calData[1], (float)QMC_calData[2]);
    sendLogFmt("Mag Scale X/Y/Z: %.2f, %.2f, %.2f", (float)QMC_calData[3], (float)QMC_calData[4], (float)QMC_calData[5]);
    
    preferences.end();
    //delay(500);
  } else {
    sendLog("No QMC calibration in memory! New calibration required.");
    delay(1000);
    preferences.end();
    calibrateQMC5883L();
  }
}

void calibrateQMC5883L() {
  setLed(YELLOW_LED, 1);
  tone(BUZZER_PIN, CALIB_TONE, 100);
  sendLog("QMC5883L calibration will begin in 5 seconds.");
  delay(5000);
  sendLog("CALIBRATING. Keep moving your sensor...");
  tone(BUZZER_PIN, CALIB_TONE, 100);
  
  xTimerStart(yellowBlinkTimer, 0);
  compass.calibrate();
  xTimerStop(yellowBlinkTimer, 0);
  setLed(YELLOW_LED, 0);
  tone(BUZZER_PIN, CALIB_TONE, 400);

  sendLog("QMC5883L calibration completed");
  sendLogFmt("Mag biases X/Y/Z: %.2f, %.2f, %.2f", (float)compass.getCalibrationOffset(0), (float)compass.getCalibrationOffset(1), (float)compass.getCalibrationOffset(2));
  sendLogFmt("Mag Scale X/Y/Z: %.2f, %.2f, %.2f", (float)compass.getCalibrationScale(0), (float)compass.getCalibrationScale(1), (float)compass.getCalibrationScale(2));

  QMC_calData[0] = compass.getCalibrationOffset(0);
  QMC_calData[1] = compass.getCalibrationOffset(1);
  QMC_calData[2] = compass.getCalibrationOffset(2);
  QMC_calData[3] = compass.getCalibrationScale(0);
  QMC_calData[4] = compass.getCalibrationScale(1);
  QMC_calData[5] = compass.getCalibrationScale(2); 

  QMCsaveCalibration();
}
