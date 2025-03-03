/***********************************************
 * Student Emotion Recorder with ESP32
 *
 * Hardware Components and Pin Definitions:
 *  - ESP32
 *  - I2C LCD (16x2)
 *      * SDA: GPIO21
 *      * SCL: GPIO22
 *      * I2C Address: 0x27
 *  - SH1106 OLED Display (1.3") on separate I2C bus
 *      * SDA: GPIO25
 *      * SCL: GPIO26
 *      * I2C Address: 0x3C
 *      * Resolution: 128x64
 *  - HW040 Rotary Encoder (KY-040)
 *      * CLK: GPIO18
 *      * DT:  GPIO19
 *      * SW:  GPIO23 (active LOW)
 *
 * Functionality:
 *  1. Student selects their name, class, and mood.
 *  2. Retrieves current date via NTP.
 *  3. Sends a Telegram message.
 *  4. LCD displays the menu; upon submission, it shows:
 *       "Submitted!" and "Thank you!"
 *  5. OLED displays additional information:
 *     - During NAME & CLASS selection: runs RoboEyes idle animation.
 *     - During MOOD selection: shows an emoticon corresponding to the mood.
 *     - During CONFIRMATION: displays a mood-specific message in a smaller font.
 *     - During WiFi connection, displays connection status.
 ***********************************************/

/* ----- Forward Declarations ----- */
void startupAnimation();
void setupWiFi();
String urlEncode(String str);
void sendTelegramMessage(String message);
bool isButtonPressed();
String getFormattedDate();
void displayMenu();
void updateOLED();
void displayEmotion(int moodIndex);
void displaySmallMessage(String msg);
void IRAM_ATTR updateEncoder();

/* ----- I2C Pin Definitions ----- */
#define LCD_I2C_SDA   21    // LCD SDA on default I2C bus
#define LCD_I2C_SCL   22    // LCD SCL on default I2C bus
#define OLED_I2C_SDA  25    // OLED SDA on second I2C bus
#define OLED_I2C_SCL  26    // OLED SCL on second I2C bus

/* ----- LCD Configuration ----- */
#define LCD_I2C_ADDR  0x27
#define LCD_COLS      16
#define LCD_ROWS      2

/* ----- Rotary Encoder Pins ----- */
#define ENCODER_PIN_A 18
#define ENCODER_PIN_B 19
#define BUTTON_PIN    23

/* ----- WiFi & Telegram Configuration ----- */
const char* ssid = "ssid ";
const char* password = "password";
const char* botToken = "token";
const char* chatID   = "id";

/* ----- Include Libraries ----- */
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

// RoboEyes library expects SSD1306_SWITCHCAPVCC defined:
#define SSD1306_SWITCHCAPVCC 0x8

/* ----- Create Second I2C Instance for OLED ----- */
TwoWire I2C_OLED(1);  // Use I2C bus 1 for OLED

/* ----- Create Global OLED Object ----- */
// RoboEyes expects a global variable named "display"
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET   -1
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &I2C_OLED, OLED_RESET);

/* ----- Include FluxGarage RoboEyes Library ----- */
#include <FluxGarage_RoboEyes.h>
roboEyes roboEyes; // Create RoboEyes instance

/* ----- Create LCD Object (using default Wire) ----- */
LiquidCrystal_I2C lcd(LCD_I2C_ADDR, LCD_COLS, LCD_ROWS);

/* ----- NTP Client ----- */
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);

/* ----- State Machine ----- */
enum DeviceState {
  STATE_NAME_SELECTION,
  STATE_CLASS_SELECTION,
  STATE_MOOD_SELECTION,
  STATE_SUBMISSION,
  STATE_CONFIRMATION
};
DeviceState currentState = STATE_NAME_SELECTION;

/* ----- Menu Data ----- */
const char* names[] = {"Tan Jetyu", "Nur Qalisha", "Shum Jia Xiang", "Evelyn"};
const int numNames = sizeof(names) / sizeof(names[0]);
const char* classes[] = {"Class 3", "Class 4", "Class 5"};
const int numClasses = sizeof(classes) / sizeof(classes[0]);
const char* moods[] = {"Happy", "Neutral", "Sad", "Angry", "Excited"};
const int numMoods = sizeof(moods) / sizeof(moods[0]);
const char* moodMessages[] = {
  "yes! Please stay happy !",
  "awh Why ? Try to find something you like to do today !",
  "Well, Its okay to be sad. Try to reach out to people you trust.",
  "Okay! Cool cool! Take a deep breath!",
  "Wow! I'm glad! Try to share your happiness with your friends!"
};

/* ----- Variables for Selections ----- */
int selectedNameIndex = 0;
int selectedClassIndex = 0;
int selectedMoodIndex = 0;

/* ----- Rotary Encoder Variables (Interrupt-Based) ----- */
volatile int encoderPos = 0;
volatile int lastEncoded = 0;

/* ----- Button Debounce Variable ----- */
unsigned long lastButtonPressTime = 0;

/* ----- Tracking Variables for Updates ----- */
DeviceState prevState = (DeviceState)-1;
int prevNameIndex = -1;
int prevClassIndex = -1;
int prevMoodIndex = -1;

/***********************************************
 * Function: startupAnimation()
 ***********************************************/
void startupAnimation() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Welcome!");
  delay(1000);
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Loading.");
  delay(300);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Loading..");
  delay(300);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Loading...");
  delay(300);
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Student Emotion");
  lcd.setCursor(0, 1);
  lcd.print("Recorder");
  delay(1500);
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("By JET");
  delay(1500);
  lcd.clear();
}

/***********************************************
 * Function: setupWiFi()
 ***********************************************/
void setupWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.println("Connecting to WiFi...");
  display.display();
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected");
  
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("WiFi Connected!");
  display.println("IP:");
  display.println(WiFi.localIP());
  display.display();
  delay(2000);
}

/***********************************************
 * Function: urlEncode()
 ***********************************************/
String urlEncode(String str) {
  String encoded = "";
  for (size_t i = 0; i < str.length(); i++) {
    char c = str.charAt(i);
    if (isalnum(c))
      encoded += c;
    else if (c == ' ')
      encoded += "%20";
    else {
      char buf[5];
      sprintf(buf, "%%%02X", c);
      encoded += buf;
    }
  }
  return encoded;
}

/***********************************************
 * Function: sendTelegramMessage()
 ***********************************************/
void sendTelegramMessage(String message) {
  WiFiClientSecure client;
  client.setInsecure();
  
  if (client.connect("api.telegram.org", 443)) {
    String encodedMessage = urlEncode(message);
    String url = "/bot" + String(botToken) + "/sendMessage?chat_id=" + String(chatID) +
                 "&text=" + encodedMessage;
    Serial.println("Sending URL: " + url);
    
    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: api.telegram.org\r\n" +
                 "Connection: close\r\n\r\n");
    
    while (client.connected()) {
      String line = client.readStringUntil('\n');
      if (line.length() > 0)
        Serial.println(line);
      if (line == "\r")
        break;
    }
    String response = client.readString();
    Serial.println("Response:");
    Serial.println(response);
  } else {
    Serial.println("Connection to Telegram failed");
  }
}

/***********************************************
 * Function: updateEncoder() [ISR]
 ***********************************************/
void IRAM_ATTR updateEncoder() {
  int MSB = digitalRead(ENCODER_PIN_A);
  int LSB = digitalRead(ENCODER_PIN_B);
  int encoded = (MSB << 1) | LSB;
  int sum = (lastEncoded << 2) | encoded;
  if (sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011)
    encoderPos++;
  else if (sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000)
    encoderPos--;
  lastEncoded = encoded;
}

/***********************************************
 * Function: isButtonPressed()
 ***********************************************/
bool isButtonPressed() {
  if (digitalRead(BUTTON_PIN) == LOW) {
    unsigned long currentMillis = millis();
    if (currentMillis - lastButtonPressTime > 200) {
      lastButtonPressTime = currentMillis;
      return true;
    }
  }
  return false;
}

/***********************************************
 * Function: getFormattedDate()
 ***********************************************/
String getFormattedDate() {
  unsigned long epochTime = timeClient.getEpochTime();
  time_t rawTime = epochTime;
  struct tm* timeInfo = gmtime(&rawTime);
  char buffer[11];
  sprintf(buffer, "%02d/%02d/%04d", timeInfo->tm_mday, timeInfo->tm_mon + 1, timeInfo->tm_year + 1900);
  return String(buffer);
}

/***********************************************
 * Function: displayMenu()
 * Updates the LCD.
 * In CONFIRMATION state, the LCD is cleared and displays a fixed message.
 ***********************************************/
void displayMenu() {
  lcd.clear();
  switch (currentState) {
    case STATE_NAME_SELECTION:
      lcd.setCursor(0, 0);
      lcd.print("Select Name:");
      lcd.setCursor(0, 1);
      lcd.print(names[selectedNameIndex]);
      break;
    case STATE_CLASS_SELECTION:
      lcd.setCursor(0, 0);
      lcd.print("Select Class:");
      lcd.setCursor(0, 1);
      lcd.print(classes[selectedClassIndex]);
      break;
    case STATE_MOOD_SELECTION:
      lcd.setCursor(0, 0);
      lcd.print("Select Mood:");
      lcd.setCursor(0, 1);
      lcd.print(moods[selectedMoodIndex]);
      break;
    case STATE_CONFIRMATION:
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Submitted!");
      lcd.setCursor(0, 1);
      lcd.print("Thank you!");
      break;
    default:
      break;
  }
}

/***********************************************
 * Function: updateOLED()
 * Updates the OLED based on the current state.
 ***********************************************/
void updateOLED() {
  display.clearDisplay();
  
  switch (currentState) {
    case STATE_NAME_SELECTION:
    case STATE_CLASS_SELECTION:
      // Run RoboEyes idle animation.
      roboEyes.update();
      break;
    case STATE_MOOD_SELECTION:
      // Show emoticon for selected mood.
      displayEmotion(selectedMoodIndex);
      break;
    case STATE_CONFIRMATION:
      // Display the mood message in a smaller font
      displaySmallMessage(moodMessages[selectedMoodIndex]);
      break;
    default:
      display.setCursor(0, 0);
      display.println("Idle");
      display.display();
      break;
  }
}

/***********************************************
 * Function: displayEmotion()
 * Draws an emoticon on the OLED for the selected mood.
 ***********************************************/
void displayEmotion(int moodIndex) {
  display.clearDisplay();
  switch (moodIndex) {
    case 0: // Happy
      display.drawCircle(64, 32, 20, SH110X_WHITE);
      display.fillCircle(56, 26, 2, SH110X_WHITE);
      display.fillCircle(72, 26, 2, SH110X_WHITE);
      display.drawLine(56, 38, 72, 38, SH110X_WHITE);
      break;
    case 1: // Neutral
      display.drawCircle(64, 32, 20, SH110X_WHITE);
      display.fillCircle(56, 26, 2, SH110X_WHITE);
      display.fillCircle(72, 26, 2, SH110X_WHITE);
      display.drawLine(56, 38, 72, 38, SH110X_WHITE);
      break;
    case 2: // Sad
      display.drawCircle(64, 32, 20, SH110X_WHITE);
      display.fillCircle(56, 26, 2, SH110X_WHITE);
      display.fillCircle(72, 26, 2, SH110X_WHITE);
      display.drawLine(56, 42, 72, 42, SH110X_WHITE);
      break;
    case 3: // Angry
      display.drawCircle(64, 32, 20, SH110X_WHITE);
      display.fillCircle(56, 26, 2, SH110X_WHITE);
      display.fillCircle(72, 26, 2, SH110X_WHITE);
      display.drawLine(50, 20, 62, 24, SH110X_WHITE);
      display.drawLine(74, 24, 86, 20, SH110X_WHITE);
      display.drawLine(56, 40, 72, 40, SH110X_WHITE);
      break;
    case 4: // Excited
      display.drawCircle(64, 32, 20, SH110X_WHITE);
      display.fillCircle(56, 26, 3, SH110X_WHITE);
      display.fillCircle(72, 26, 3, SH110X_WHITE);
      display.drawCircle(64, 38, 4, SH110X_WHITE);
      break;
    default:
      break;
  }
  display.display();
}

/***********************************************
 * Function: displaySmallMessage()
 * Displays a long message on the OLED using a smaller text size (size 1).
 * This should help fit the entire message on the screen.
 ***********************************************/
void displaySmallMessage(String msg) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  // Enable automatic text wrap (if supported)
  display.setTextWrap(true);
  display.println(msg);
  display.display();
}

void setup() {
  Serial.begin(115200);
  
  // Initialize default I2C for LCD on SDA=21, SCL=22
  Wire.begin(LCD_I2C_SDA, LCD_I2C_SCL);
  lcd.init();
  lcd.backlight();
  
  // Initialize second I2C for OLED on SDA=25, SCL=26
  I2C_OLED.begin(OLED_I2C_SDA, OLED_I2C_SCL);
  if (!display.begin(0x3C, true)) { // For SH1106, use address 0x3C
    Serial.println("OLED allocation failed");
    for (;;);
  }
  display.clearDisplay();
  display.display();
  
  // Initialize RoboEyes on the OLED
  roboEyes.begin(SCREEN_WIDTH, SCREEN_HEIGHT, 100);
  roboEyes.setAutoblinker(ON, 3, 2);
  roboEyes.setIdleMode(ON, 2, 2);
  
  // Run startup animation on LCD
  startupAnimation();
  
  // Set pin modes for rotary encoder and button
  pinMode(ENCODER_PIN_A, INPUT_PULLUP);
  pinMode(ENCODER_PIN_B, INPUT_PULLUP);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // Attach interrupts for rotary encoder pins
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), updateEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), updateEncoder, CHANGE);
  
  // Connect to WiFi and show status on OLED
  setupWiFi();
  
  // Start NTP client and update time
  timeClient.begin();
  timeClient.update();
  
  // Initialize tracking variables and show initial LCD menu and idle OLED output
  prevState = (DeviceState)-1;
  prevNameIndex = -1;
  prevClassIndex = -1;
  prevMoodIndex = -1;
  displayMenu();
  roboEyes.update();
}

void loop() {
  timeClient.update();
  
  noInterrupts();
  int pos = encoderPos;
  encoderPos = 0;
  interrupts();
  
  if (pos != 0) {
    switch (currentState) {
      case STATE_NAME_SELECTION:
        selectedNameIndex = (selectedNameIndex + pos + numNames) % numNames;
        break;
      case STATE_CLASS_SELECTION:
        selectedClassIndex = (selectedClassIndex + pos + numClasses) % numClasses;
        break;
      case STATE_MOOD_SELECTION:
        selectedMoodIndex = (selectedMoodIndex + pos + numMoods) % numMoods;
        break;
      default:
        break;
    }
  }
  
  if (isButtonPressed()) {
    switch (currentState) {
      case STATE_NAME_SELECTION:
        currentState = STATE_CLASS_SELECTION;
        break;
      case STATE_CLASS_SELECTION:
        currentState = STATE_MOOD_SELECTION;
        break;
      case STATE_MOOD_SELECTION:
        currentState = STATE_SUBMISSION;
        break;
      default:
        break;
    }
    delay(200);
  }
  
  if (currentState != prevState ||
      (currentState == STATE_NAME_SELECTION && selectedNameIndex != prevNameIndex) ||
      (currentState == STATE_CLASS_SELECTION && selectedClassIndex != prevClassIndex) ||
      (currentState == STATE_MOOD_SELECTION && selectedMoodIndex != prevMoodIndex)) {
    displayMenu();
    prevState = currentState;
    prevNameIndex = selectedNameIndex;
    prevClassIndex = selectedClassIndex;
    prevMoodIndex = selectedMoodIndex;
  }
  
  updateOLED();
  
  if (currentState == STATE_SUBMISSION) {
    String dateStr = getFormattedDate();
    String message = "Name: " + String(names[selectedNameIndex]) +
                     "\nClass: " + String(classes[selectedClassIndex]) +
                     "\nMood: " + String(moods[selectedMoodIndex]) +
                     "\nDate: " + dateStr;
    sendTelegramMessage(message);
    
    currentState = STATE_CONFIRMATION;
    displayMenu();
    
    // Keep the confirmation state visible for 3 seconds
    unsigned long confirmStart = millis();
    while (millis() - confirmStart < 3000) {
      updateOLED();
      yield();
    }
    
    // Reset to initial state
    selectedNameIndex = 0;
    selectedClassIndex = 0;
    selectedMoodIndex = 0;
    currentState = STATE_NAME_SELECTION;
    prevState = (DeviceState)-1;
  }
}
