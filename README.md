# Mental_Health_Recoder
Student Mood Selection System

This project is a student mood selection system using an ESP32 with an OLED display, rotary encoder, and Telegram integration. The system allows students to select their name, class, and current mood, which is then sent to a Telegram bot along with the date and time.

Features

Rotary Encoder Navigation: Select between different options (Name, Class, Mood).

OLED Display: Shows the current selection.

WiFi Connectivity: ESP32 connects to the internet.

Telegram Bot Integration: Sends student mood data to a Telegram chat.

Hardware Requirements

ESP32

0.96-inch OLED Display (SSD1306)

Rotary Encoder with Push Button

WiFi Connection

Software Requirements

Arduino IDE

ESP32 Board Package

Adafruit SSD1306 Library

Adafruit GFX Library

WiFi Library

NTP Client Library

Universal Telegram Bot Library

Installation & Setup

Clone the Repository

git clone https://github.com/yourusername/your-repo-name.git
cd your-repo-name

Open the Arduino IDE and install the necessary libraries.

Edit the WiFi and Telegram credentials in the code:

#define WIFI_SSID "YourWiFiName"
#define WIFI_PASSWORD "YourWiFiPassword"
#define TELEGRAM_BOT_TOKEN "YourTelegramBotToken"

Upload the code to your ESP32.

Usage

Use the rotary encoder to navigate between Name, Class, and Mood.

Press the rotary button to confirm each selection.

Once all selections are made, the system sends the data to Telegram.

The OLED display updates accordingly.

Contributing

Feel free to contribute by submitting issues or pull requests.

License

This project is licensed under the MIT License.

Contact

For any inquiries, contact: [your email or GitHub profile]

