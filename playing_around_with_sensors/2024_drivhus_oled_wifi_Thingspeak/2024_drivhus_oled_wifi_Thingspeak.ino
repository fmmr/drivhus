#include <WiFi.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_AHTX0.h>
#include <ThingSpeak.h>
#include <esp_sleep.h>
#include <DFRobot_MAX17043.h>
#include <Wire.h>
#include <BH1750.h>
#include <EEPROM.h>
#include <LCD_I2C.h>
#include <Adafruit_BMP085.h>

// secrets
#define WIFI_SSID "CHANGEME"
#define WIFI_PASSWORD "CHANGEME"
const char* thingSpeakAPIKey = "CHANGEME";  // ThingSpeak channel write API key

unsigned long thingSpeakChannel = 2568299;  // ThingSpeak channel number

// Error-handling constants
#define FLASH_WIFI_CONNECT_FAILURE 2
#define FLASH_THINGSPEAK_UPDATE_FAILURE 3
#define FLASH_BATTERY_MONITOR_INIT_FAILURE 5
#define FLASH_DISPLAY_INIT_FAILURE 6
#define FLASH_DHT20_INIT_FAILURE 7
#define FLASH_BH1750_INIT_FAILURE 8
#define FLASH_BMP_INIT_FAILURE 9

// Other constants
#define FLASH_INGREENHOUSE_TRUE 4
#define FLASH_INGREENHOUSE_FALSE 2
#define DISPLAY_DURATION 2000      // Duration to show the display before going back to deep sleep (milliseconds)
#define INIT_DISPLAY_DURATION 800  // Duration to show the initial display message
#define WIFI_MAX_RETRIES 3         // Maximum number of Wi-Fi connection retries
#define NUM_READINGS 2             // Number of readings to average
#define SLEEP_BETWEEN_READINGS 15  // Number of ms to sleep between each reading
#define DEGREE 223
#define seaLevelPressure_hPa 1013.25

// lux-dependent deep sleep
#define NIGHT_LEVEL 2
#define DUSK_LEVEL 700
#define SHADE_LEVEL 6000
#define SLEEP_DURATION_DUSK 300
#define SLEEP_DURATION_DAY 300
#define SLEEP_DURATION_NIGHT 500

// pins
#define GREEN_LED_PIN 2          // GPIO pin for the green LED
#define RED_LED_PIN 4            // GPIO pin for the red LED
#define BLUE_LED_PIN 16          // GPIO pin for the blue LED
#define SENSOR_POWER_PIN 13      // GPIO pin to control power to sensors
#define BUTTON_PIN 15            // GPIO pin for the button
#define SECONDARY_BUTTON_PIN 27  // GPIO pin for the secondary button
#define SOIL_MOISTURE_PIN 34     // GPIO pin for the soil moisture sensor
#define BATTERY_VOLTAGE_PIN 35   // GPIO pin to monitor the voltage of the batteries
#define TRIGGER_PIN 25           // GPIO pin for the HC-SR04 trigger
#define ECHO_PIN 26              // GPIO pin for the HC-SR04 echo

// EEPROM addresses
#define EEPROM_SIZE 1
#define EEPROM_ADDRESS_INGREENHOUSE 0

// instances
Adafruit_AHTX0 aht;               // Create an instance of the DHT20 sensor
DFRobot_MAX17043 batteryMonitor;  // Create an instance of the DFR0563 battery monitor
BH1750 lightMeter;                // Create an instance of the BH1750 light sensor
WiFiClient client;
Adafruit_SSD1306 display(128, 64, &Wire, -1);
LCD_I2C lcd(0x27, 16, 2);
Adafruit_BMP085 bmp;

// variable to store inGreenHouse state
bool inGreenHouse = false;

struct SensorData {
  float temperature;
  float humidity;
  float ahtTemperature;
  float bmpTemperature;
  float pressure;
  int32_t rssi;
  int soilMoisture;
  float batteryVoltage;
  int batteryPercentage;
  int batVoltageCharger;
  float distance;
  float lux;
  String status;
};

void flashLED(int pin, int times, int delayTime = 180) {
  for (int i = 0; i < times; i++) {
    digitalWrite(pin, HIGH);
    delay(delayTime);
    digitalWrite(pin, LOW);
    delay(delayTime);
  }
}

void connectToWiFi() {
  int retries = 0;
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi...");

  while (WiFi.status() != WL_CONNECTED && retries < WIFI_MAX_RETRIES) {
    delay(400);
    Serial.print(".");
    retries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected to WiFi");
  } else {
    Serial.println("Failed to connect to WiFi");
    flashLED(RED_LED_PIN, FLASH_WIFI_CONNECT_FAILURE);
  }
}

void initializeDisplays() {
  Serial.println("Initializing displays...");
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 allocation failed");
    display.setCursor(0, 0);
    display.print("D init fail");
    display.display();
    flashLED(RED_LED_PIN, FLASH_DISPLAY_INIT_FAILURE);
  } else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.print("Drivhus - FmR - 2024");
    display.display();
  }

  // Initialize the LCD
  lcd.begin(false);
  lcd.clear();
  lcd.backlight();
  lcd.home();
}

void initializeSensors() {
  if (!aht.begin()) {
    Serial.println("Could not find a valid DHT20 sensor, check wiring!");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("DHT20 fail");
    display.display();
    flashLED(RED_LED_PIN, FLASH_DHT20_INIT_FAILURE);
  }

  if (!lightMeter.begin()) {
    Serial.println("Could not find a valid BH1750 sensor, check wiring!");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("BH1750 fail");
    display.display();
    flashLED(RED_LED_PIN, FLASH_BH1750_INIT_FAILURE);
  }
  if (!bmp.begin()) {
    Serial.println("Could not find a valid BMP085 sensor, check wiring!");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("BH1750 fail");
    display.display();
    flashLED(RED_LED_PIN, FLASH_BMP_INIT_FAILURE);
  }
  while (batteryMonitor.begin() != 0) {
    delay(100);
    Serial.println("gauge begin faild!");
    flashLED(RED_LED_PIN, 5);
    delay(100);
  }
}

void gatherData(SensorData& data) {
  data.rssi = WiFi.RSSI();

  float totalAhtTemp = 0, totalHumidity = 0;
  float totalBmpTemp = 0, totalPressure = 0;
  int totalSoilMoisture = 0;
  float totalBatteryVoltage = 0;
  int totalBatteryPercentage = 0;
  int totalBatVoltageCharger = 0;
  float totalDistance = 0;
  float totalLux = 0;

  for (int i = 0; i < NUM_READINGS; i++) {
    sensors_event_t humidity, temp;
    aht.getEvent(&humidity, &temp);
    totalAhtTemp += temp.temperature;
    totalHumidity += humidity.relative_humidity;

    totalBmpTemp += bmp.readTemperature();
    totalPressure += bmp.readSealevelPressure(31.0) / 100.0;

    int rawSoilMoisture = analogRead(SOIL_MOISTURE_PIN);
    totalSoilMoisture += (100 - ((rawSoilMoisture / 4095.00) * 100));  // Convert to percentage using the new formula

    int batVoltage = analogRead(BATTERY_VOLTAGE_PIN);
    totalBatVoltageCharger += batVoltage;

    totalBatteryVoltage += batteryMonitor.readVoltage() / 1000.0;  // Convert millivolts to volts
    totalBatteryPercentage += static_cast<int>(batteryMonitor.readPercentage());

    // Measure distance using HC-SR04
    digitalWrite(TRIGGER_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIGGER_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIGGER_PIN, LOW);
    long duration = pulseIn(ECHO_PIN, HIGH);
    totalDistance += duration * 0.0343 / 2;  // Calculate distance in cm

    totalLux += lightMeter.readLightLevel();  // Read light level in lux

    delay(SLEEP_BETWEEN_READINGS);  // Short delay between readings
  }

  data.temperature = ((totalAhtTemp + totalBmpTemp) / NUM_READINGS) / 2.0;
  data.humidity = totalHumidity / NUM_READINGS;
  data.ahtTemperature = totalAhtTemp / NUM_READINGS;
  data.bmpTemperature = totalBmpTemp / NUM_READINGS;
  data.pressure = totalPressure / NUM_READINGS;
  data.soilMoisture = totalSoilMoisture / NUM_READINGS;
  data.batVoltageCharger = totalBatVoltageCharger / NUM_READINGS;
  data.batteryVoltage = totalBatteryVoltage / NUM_READINGS;
  data.batteryPercentage = totalBatteryPercentage / NUM_READINGS;
  data.distance = totalDistance / NUM_READINGS;
  data.lux = totalLux / NUM_READINGS;

  // Calculate the status string
  data.status = "";
  if (data.temperature < 14) data.status += "T-COLD";
  else if (data.temperature > 40) data.status += "T-HOT";
  else data.status += "T-OK";

  if (data.lux < 2) data.status += "_NIGHT";
  else if (data.lux < DUSK_LEVEL) data.status += "_DUSK";
  else if (data.lux < SHADE_LEVEL) data.status += "_SHADE";
  else data.status += "_SUN";

  if (data.distance < 3) data.status += "_W-CLOSE";
  else data.status += "_W-OPEN";

  if (data.batteryPercentage < 50) data.status += "_B-LOW";
  else data.status += "_B-OK";

  if (data.pressure < 990) data.status += "_P-LOW";
  else if (data.pressure > 1025) data.status += "_P-HIGH";
  else data.status += "_P-OK";
}

void displayData(const SensorData& data) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print(WiFi.localIP());
  display.print(" ");
  display.print(data.rssi);
  display.print("dBm");
  display.display();
  delay(500);  // Short delay to ensure the display is updated

  display.setCursor(0, 10);
  display.print("T:");
  display.print(data.temperature, 1);
  display.print(" T1:");
  display.print(data.ahtTemperature, 0);
  display.print(" T2:");
  display.print(data.bmpTemperature, 0);
  display.setCursor(0, 20);
  display.print("H:");
  display.print(data.humidity, 0);
  display.print(" S:");
  display.print(data.soilMoisture);
  display.print(" D:");
  display.print(data.distance, 0);
  display.setCursor(0, 30);
  display.print("B:");
  display.print(data.batteryVoltage, 2);
  display.print("v ");
  display.print(data.batteryPercentage);
  display.print("% ");
  display.setCursor(0, 40);
  display.print("P:");
  display.print(data.pressure, 0);
  display.print(" L:");
  display.print(data.lux, 0);
//  display.setCursor(0, 50);
//  display.print(data.status);
  display.display();

  // Update LCD
  lcd.clear();
  lcd.home();
  lcd.print(F("FmR 2024 L:"));
  lcd.print(data.lux, 0);


  int tempInt = data.temperature < 0 ? data.temperature - 0.5 : data.temperature + 0.5;
   int pressureInt = data.pressure + 0.5;
  // int humidityInt = data.humidity + 0.5;

  lcd.setCursor(0, 1);
  lcd.print(data.temperature,1);
//  lcd.print((char)DEGREE);
  lcd.print(" ");
  lcd.print(data.batteryVoltage, 1);
  lcd.print(" ");
  lcd.print(data.batteryPercentage, 0);
  lcd.print(" ");
  lcd.print(pressureInt);
}

void postToThingSpeak(const SensorData& data) {
  Serial.println("Setting fields to ThingSpeak...");
  ThingSpeak.setField(1, data.temperature);
  ThingSpeak.setField(2, data.humidity);
  ThingSpeak.setField(3, data.rssi);
  ThingSpeak.setField(4, data.soilMoisture);
  ThingSpeak.setField(5, data.batteryVoltage);
  ThingSpeak.setField(6, data.batteryPercentage);
  ThingSpeak.setField(7, data.pressure);
  ThingSpeak.setField(8, data.lux);

  // Set the status
  ThingSpeak.setStatus(data.status);

  Serial.println("Attempting to update ThingSpeak...");
  int result = ThingSpeak.writeFields(thingSpeakChannel, thingSpeakAPIKey);

  if (result == 200) {
    Serial.println("Channel update successful.");
  } else {
    Serial.println("Problem updating channel. HTTP error code " + String(result));
    flashLED(RED_LED_PIN, 3);
  }
}

void postToSerial(const SensorData& data) {
  Serial.print("RSSI: ");
  Serial.print(data.rssi);
  Serial.println(" dBm");

  Serial.print("Temp: ");
  Serial.println(data.temperature);
  Serial.print("Temp AHT: ");
  Serial.println(data.ahtTemperature);
  Serial.print("Temp BMP: ");
  Serial.println(data.bmpTemperature);
  Serial.print("Pressure: ");
  Serial.print(data.pressure);
  Serial.println("hPa");
  Serial.print("Humidity: ");
  Serial.println(data.humidity);
  Serial.print("Soil Moisture: ");
  Serial.println(data.soilMoisture);
  Serial.print("Battery Voltage: ");
  Serial.print(data.batteryVoltage, 2);
  Serial.println("V");
  Serial.print("Battery Percentage: ");
  Serial.print(data.batteryPercentage);
  Serial.println("%");
  Serial.print("Battery Voltage Charger: ");
  Serial.print(data.batVoltageCharger);
  Serial.println("V");
  Serial.print("Distance: ");
  Serial.print(data.distance, 1);
  Serial.println(" cm");
  Serial.print("Light Intensity: ");
  Serial.print(data.lux, 1);
  Serial.println(" lx");
  Serial.print("Status: ");
  Serial.println(data.status);
}

void writeInGreenHouse(bool state) {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(EEPROM_ADDRESS_INGREENHOUSE, state);
  EEPROM.commit();
  EEPROM.end();
}

void toggleInGreenHouse() {
  inGreenHouse = !inGreenHouse;
  Serial.print("Setting inGreenHouse to ");
  Serial.println(inGreenHouse);
  writeInGreenHouse(inGreenHouse);
  if (inGreenHouse) {
    flashLED(BLUE_LED_PIN, 3);
  } else {
    flashLED(GREEN_LED_PIN, 3);
  }
}

void enterDeepSleep(int sleepDuration, bool inGreenHouse) {
  // Turn off the power to the sensors
  digitalWrite(SENSOR_POWER_PIN, LOW);

  // Turn off all LEDs
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(BLUE_LED_PIN, LOW);

  esp_sleep_enable_ext0_wakeup(GPIO_NUM_15, 0);  // Wake up on button press (LOW)

  if (inGreenHouse) {
    // Go to deep sleep with timer wakeup if in greenhouse
    Serial.print("Going to deep sleep for ");
    Serial.print(sleepDuration);
    Serial.println(" seconds...");
    esp_sleep_enable_timer_wakeup(sleepDuration * 1000000);
  } else {
    Serial.println("Not adding wake up timer...");
  }
  esp_deep_sleep_start();
}


int getSleepDuration(float lux) {
  if (lux < NIGHT_LEVEL) {
    return SLEEP_DURATION_NIGHT;
  } else if (lux < DUSK_LEVEL) {
    return SLEEP_DURATION_DUSK;
  } else {
    return SLEEP_DURATION_DAY;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(SECONDARY_BUTTON_PIN, INPUT_PULLUP);
  pinMode(SOIL_MOISTURE_PIN, INPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(BLUE_LED_PIN, OUTPUT);
  pinMode(SENSOR_POWER_PIN, OUTPUT);
  pinMode(TRIGGER_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Read inGreenHouse state from EEPROM
  EEPROM.begin(EEPROM_SIZE);
  inGreenHouse = EEPROM.read(EEPROM_ADDRESS_INGREENHOUSE);
  EEPROM.end();

  // Check if wakeup was caused by deep sleep
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  bool displayOn = false;
  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
    if (digitalRead(SECONDARY_BUTTON_PIN) == LOW) {
      toggleInGreenHouse();
      enterDeepSleep(0, inGreenHouse);  // No timer, just deep sleep
    }
    Serial.println("Woke up from deep sleep by button press");
    displayOn = true;
  } else if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
    Serial.println("Woke up from deep sleep by timer");
  } else {
    Serial.println("Starting fresh");
    displayOn = true;
  }

  digitalWrite(inGreenHouse ? BLUE_LED_PIN : GREEN_LED_PIN, HIGH);
  digitalWrite(SENSOR_POWER_PIN, HIGH);
  delay(50);
  // Initialize the OLED and LCD displays if required
  if (displayOn) {
    initializeDisplays();
    delay(INIT_DISPLAY_DURATION);  // Show initial display message
  }

  // Initialize the DHT20 sensor
  initializeSensors();

  // Connect to Wi-Fi
  connectToWiFi();

  // Gather and display data
  SensorData data;
  gatherData(data);
  if (displayOn) {
    displayData(data);
  }

  // Post data to Serial
  postToSerial(data);

  // Post data to ThingSpeak
  if (inGreenHouse) {
    Serial.println("Initializing ThingSpeak...");
    ThingSpeak.begin(client);
    postToThingSpeak(data);
  } else {
    Serial.println("Not in greenhouse mode, skipping ThingSpeak update.");
  }

  // Keep the display on for DISPLAY_DURATION seconds if it was turned on
  if (displayOn) {
    delay(inGreenHouse ? DISPLAY_DURATION * 3 : DISPLAY_DURATION);
    // Turn off the display to save power
    display.ssd1306_command(SSD1306_DISPLAYOFF);
  }

  int sleepDuration = getSleepDuration(data.lux);
  enterDeepSleep(sleepDuration, inGreenHouse);
}

void loop() {
  // This will not be called
}
