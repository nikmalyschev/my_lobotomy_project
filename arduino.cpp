#include <Wire.h>               
#include <LiquidCrystal_I2C.h>  
#include <DHT.h>  

#define DHTPIN 2       // Подключен к D2
#define DHTTYPE DHT11  

DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Адрес дисплея (может быть 0x3F)

void setup() {
    Serial.begin(115200);
    dht.begin();
    lcd.begin();
    lcd.backlight();
}

void loop() {
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (isnan(h) || isnan(t)) {
        Serial.println("Ошибка считывания DHT11");
        return;
    }

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("T: ");
    lcd.print(t);
    lcd.print("C");

    lcd.setCursor(0, 1);
    lcd.print("H: ");
    lcd.print(h);
    lcd.print("%");

    Serial.print(t);
    Serial.print(",");
    Serial.println(h);

    delay(2000);
}
