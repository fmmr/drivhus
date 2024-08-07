void setupPins() {
  pinMode(BUZZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(BLUE_LED_PIN, OUTPUT);
  pinMode(SENSOR_POWER_PIN, OUTPUT);
  pinMode(POST_SWITCH_PIN, INPUT_PULLUP);

  digitalWrite(SENSOR_POWER_PIN, HIGH);
}

void connectToWiFi() {
  int retries = 0;
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("  Initializing WiFi...");

  while (WiFi.status() != WL_CONNECTED && retries < WIFI_MAX_RETRIES) {
    delay(400);
    Serial.print(".");
    retries++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("    WiFi: OK");
    Serial.print(", IP: ");
    Serial.print(WiFi.localIP());
    Serial.print(", RSSI: ");
    Serial.println(WiFi.RSSI());
  } else {
    Serial.println("    WiFi: FAILED");
    flashLED(RED_LED_PIN, FLASH_WIFI_CONNECT_FAILURE);
  }

  lcd.setCursor(0, 1);
  lcd.print(WiFi.RSSI());
  lcd.print(" ...");
  lcd.print(WiFi.localIP().toString().substring(8));

  dispPrint(WiFi.localIP().toString() + "   " + WiFi.RSSI());

  Serial.println("  Wifi Initialized");
}

void initDisplays() {
  Serial.println("  Initializing displays...");
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 allocation failed");
    dispPrint("    D init fail");
    flashLED(RED_LED_PIN, FLASH_DISPLAY_INIT_FAILURE);
  } else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
    dispPrint("DriMon - FmR - 2024");
    Serial.println("    SSD1306 Initialized");
  }

  lcd.begin(false);
  lcd.clear();
  lcd.backlight();
  lcd.home();
  lcd.print("DriMon FmR 2024");
  Serial.println("    LCD Initialized");
  Serial.println("  Displays Initialized");
}