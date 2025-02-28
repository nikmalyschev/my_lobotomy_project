#include <ESP8266WiFi.h>
#include <UniversalTelegramBot.h>
#include <WiFiClientSecure.h>
#include <EEPROM.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>

#define EEPROM_SIZE 64  // Память для хранения настроек Wi-Fi
#define MAX_CHATS 10    // Максимальное число сохраняемых chat ID

// ======== Telegram настройки ========
const String botToken = "7851986305:AAEQ1RNO1BzjWhgFlPccJr0qM405nlUZMjE";  

WiFiClientSecure client;
UniversalTelegramBot bot(botToken, client);

String ssid = "";  
String password = "";

unsigned long lastSendTime = 0;  // Для автоотправки сообщений
bool newData = false;
String sensorData = "";  // Сюда сохраняются данные с датчика

// Массив для хранения известных chat ID
String knownChats[MAX_CHATS];
int knownChatsCount = 0;

// ======== Функция загрузки Wi-Fi из EEPROM ========
void loadWiFiFromEEPROM() {
    EEPROM.begin(EEPROM_SIZE);
    ssid = "";
    password = "";
    // Читаем первые 32 байта для SSID
    for (int i = 0; i < 32; i++) {
        char c = EEPROM.read(i);
        if (c != 0) ssid += c;
    }
    // Читаем оставшиеся 32 байта для пароля
    for (int i = 32; i < 64; i++) {
        char c = EEPROM.read(i);
        if (c != 0) password += c;
    }
    EEPROM.end();
}

// ======== Функция сохранения Wi-Fi в EEPROM ========
void saveWiFiToEEPROM(String newSSID, String newPass) {
    EEPROM.begin(EEPROM_SIZE);
    for (int i = 0; i < 32; i++) {
        EEPROM.write(i, i < newSSID.length() ? newSSID[i] : 0);
    }
    for (int i = 32; i < 64; i++) {
        EEPROM.write(i, i - 32 < newPass.length() ? newPass[i - 32] : 0);
    }
    EEPROM.commit();
    EEPROM.end();
}

// ======== Подключение к Wi-Fi ========
void connectToWiFi() {
    WiFi.mode(WIFI_STA);
    
    // Если настройки отсутствуют или некорректны, запускаем точку доступа для настройки
    if (ssid.length() < 2 || password.length() < 8) {  
        Serial.println("❌ Данные Wi-Fi отсутствуют! Запуск точки доступа...");
        
        WiFiManager wifiManager;
        wifiManager.autoConnect("ESP_SETUP");  // SSID точки доступа для настройки Wi-Fi

        ssid = WiFi.SSID();
        password = WiFi.psk();
        saveWiFiToEEPROM(ssid, password);
        
        Serial.println("✅ Wi-Fi сохранён, перезапуск...");
        delay(1000);
        ESP.restart();
    } else {
        WiFi.begin(ssid.c_str(), password.c_str());
        Serial.print("Подключение к Wi-Fi...");
        
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20) {
            delay(1000);
            Serial.print(".");
            attempts++;
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\n✅ WiFi подключён!");
            client.setInsecure(); // Отключаем проверку сертификатов
        } else {
            Serial.println("\n❌ Ошибка подключения!");
        }
    }
}

// ======== Обработка команд из Telegram ========
void handleNewMessages() {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
        for (int i = 0; i < numNewMessages; i++) {
            String msgText = bot.messages[i].text;
            String senderID = bot.messages[i].chat_id;
            
            // Добавляем chat ID в список известных, если его там ещё нет
            bool exists = false;
            for (int j = 0; j < knownChatsCount; j++) {
                if (knownChats[j] == senderID) {
                    exists = true;
                    break;
                }
            }
            if (!exists && knownChatsCount < MAX_CHATS) {
                knownChats[knownChatsCount++] = senderID;
            }

            // Обработка команды /status
            if (msgText == "/status") {
                String message = sensorData.length() > 0 ? "🌡 " + sensorData : "❌ Данные ещё не получены.";
                bot.sendMessage(senderID, message, "");
            } else {
                // Вывод справки по доступным командам
                bot.sendMessage(senderID, "🛠 Доступные команды:\n/status - запрос данных", "");
            }
        }
        numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
}

// ======== Настройка ESP ========
void setup() {
    Serial.begin(115200);
    Serial.swap();  // Используем D7 (RX) и D8 (TX) для связи с Arduino

    loadWiFiFromEEPROM();  // Загружаем Wi-Fi из памяти
    connectToWiFi();
}

// ======== Главный цикл ========
void loop() {
    // Чтение данных с последовательного порта
    if (Serial.available()) {
        sensorData = Serial.readStringUntil('\n');  
        sensorData.trim();
        newData = true;
    }

    // Автоматическая отправка данных раз в минуту всем зарегистрированным чатам
    if (newData && millis() - lastSendTime > 60000) {
        if (sensorData.length() > 0) {
            String message = "🌡 Температура: " + sensorData.substring(0, sensorData.indexOf(",")) + "°C\n" +
                             "💧 Влажность: " + sensorData.substring(sensorData.indexOf(",") + 1) + "%";
            for (int i = 0; i < knownChatsCount; i++) {
                bot.sendMessage(knownChats[i], message, "");
            }
            Serial.println("✅ Сообщение отправлено в Telegram!");
        }
        lastSendTime = millis();
        newData = false;
    }

    handleNewMessages();
}
