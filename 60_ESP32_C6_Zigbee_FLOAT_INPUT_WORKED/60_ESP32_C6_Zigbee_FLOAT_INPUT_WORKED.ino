//57_ESP32_C6_Zigbee.ino

#ifndef ZIGBEE_MODE_ED
#error "Zigbee end device mode is not selected in Tools->Zigbee mode"
#endif

#include "Zigbee.h"
#include "zigbee_mbus_sensor.h"

extern ZigbeeMBusSensor zbMBusSensor;

void decodeMBusData() {
  float heat_sum_kwh = 12;
}

void checkSerialReboot() {  //Перезагрузка ESP32 через Serial Monitor + вкл./выкл. DC-DC
  while (Serial.available() > 0) {
    char input = Serial.read();
    if (input == 'r' || input == 'R') {
      Serial.println("\n🔃 Инициирована перезагрузка ESP32!");
      delay(1000);    // Даем время для отправки сообщения
      ESP.restart();  // Основная функция перезагрузки
    }
  }
}

void setup() {
  Serial.begin(115200);  
  
  initZigbeeMBusSensor(); 

  Serial.println("Starting Zigbee...");
  // When all EPs are registered, start Zigbee in End Device mode
  if (!Zigbee.begin()) {
    Serial.println("Zigbee failed to start!");
    Serial.println("Rebooting...");
    ESP.restart();
  } else {
    Serial.println("Zigbee started successfully!");
  }
  Serial.println("Connecting to network");
  while (!Zigbee.connected()) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();
}

void loop() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    Serial.print("Input received: ");
    Serial.println(input);

    if (input.length() > 0) {
      int sep = input.indexOf(';');
      float val = 0;
      uint8_t dp = 3; // по умолчанию 3 знака после запятой

      if (sep != -1) {
        val = input.substring(0, sep).toFloat();
        dp = input.substring(sep + 1).toInt();
      } else {
        val = input.toFloat();
      }

      if (val >= 0.0f && val <= 4294967.0f && dp <= 6) {
        zbMBusSensor.updateSummationDeliveredWithFormatting(val, dp);
        Serial.print("Updated current_summation_delivered to: ");
        Serial.println(val, dp);
      } else {
        Serial.println("Invalid input (must be a positive float within range and decimal_places <= 6)");
      }
    }
  }

  checkSerialReboot();  // Ручная перезагрузка платы
}
