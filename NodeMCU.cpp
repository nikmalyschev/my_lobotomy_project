#include <ESP8266WiFi.h>
#include <UniversalTelegramBot.h>
#include <WiFiClientSecure.h>
#include <EEPROM.h>

#define EEPROM_SIZE 64  // Память для хранения Wi-Fi

// ======== Telegram настройки ========
const String botToken = "Т7851986305:AAEQ1RNO1BzjWhgFlPccJr0qM405nlUZMjE";  
const String allowedChatIDs[] = {"798188741", "1395734828"};  // Добавь нужные chat ID

WiFiClientSecure client;
UniversalTelegramBot bot(botToken, client);

String ssid = "";  
String password = "";

unsigned long lastSendTime = 0;  // Для автоотправки сообщений
bool newData = false;
String sensorData = "";  // Сюда будем сохранять полученные данные

// ======== Функция загрузки Wi-Fi из EEPROM ========
void loadWiFiFromEEPROM() {
    EEPROM.begin(EEPROM_SIZE);
    ssid = "";
    password = "";
    for (int i = 0; i < 32; i++) {
        char c = EEPROM.read(i);
        if (c != 0) ssid += c;
    }
    for (int i = 32; i < 64; i++) {
        char c = EEPROM.read(i);
        if (c != 0) password += c;
    }
    EEPROM.end();
}

// ======== Функция сохранения Wi-Fi в EEPROM ========
void saveWiFiToEEPROM(String newSSID, String newPass) {
    EEPROM.begin(EEPROM_SIZE);
    for (int i = 0; i < 32; i++) EEPROM.write(i, i < newSSID.length() ? newSSID[i] : 0);
    for (int i = 32; i < 64; i++) EEPROM.write(i, i - 32 < newPass.length() ? newPass[i - 32] : 0);
    EEPROM.commit();
    EEPROM.end();
}

// ======== Подключение к Wi-Fi ========
void connectToWiFi() {
    WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    Serial.print("Подключение к WiFi...");
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(1000);
        Serial.print(".");
        attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n✅ WiFi подключён!");
        client.setInsecure();
    } else {
        Serial.println("\n❌ Ошибка подключения!");
    }
}

// ======== Обработка команд из Telegram ========
void handleNewMessages() {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
        for (int i = 0; i < numNewMessages; i++) {
            String msgText = bot.messages[i].text;
            String senderID = bot.messages[i].chat_id;

            // Проверяем, есть ли пользователь в списке разрешённых
            bool authorized = false;
            for (String id : allowedChatIDs) {
                if (id == senderID) {
                    authorized = true;
                    break;
                }
            }
            if (!authorized) continue;  // Игнорируем сообщения от неразрешённых пользователей

            if (msgText == "/status") {
                String message = sensorData.length() > 0 ? "🌡 " + sensorData : "❌ Данные ещё не получены.";
                bot.sendMessage(senderID, message, "");
            } else if (msgText.startsWith("/setwifi ")) {
                int spaceIndex = msgText.indexOf(" ", 9);
                if (spaceIndex != -1) {
                    String newSSID = msgText.substring(9, spaceIndex);
                    String newPass = msgText.substring(spaceIndex + 1);
                    saveWiFiToEEPROM(newSSID, newPass);
                    bot.sendMessage(senderID, "✅ Wi-Fi обновлён! Перезапускаюсь...", "");
                    delay(1000);
                    ESP.restart();
                } else {
                    bot.sendMessage(senderID, "⚠️ Использование: `/setwifi SSID ПАРОЛЬ`", "");
                }
            } else {
                bot.sendMessage(senderID, "🛠 Доступные команды:\n/status - запрос данных\n/setwifi SSID ПАРОЛЬ - смена Wi-Fi", "");
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
    if (Serial.available()) {
        sensorData = Serial.readStringUntil('\n');  
        sensorData.trim();  
        newData = true;
    }

    if (newData && millis() - lastSendTime > 60000) {  // Раз в минуту
        if (sensorData.length() > 0) {
            String message = "🌡 Температура: " + sensorData.substring(0, sensorData.indexOf(",")) + "°C\n" +
                             "💧 Влажность: " + sensorData.substring(sensorData.indexOf(",") + 1) + "%";
            for (String id : allowedChatIDs) {
                bot.sendMessage(id, message, "");
            }
            Serial.println("✅ Сообщение отправлено в Telegram!");
        }
        lastSendTime = millis();
        newData = false;
    }

    handleNewMessages();
}
