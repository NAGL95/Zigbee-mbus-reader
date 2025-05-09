//66_ESP32_C6_Zigbee.ino
#include <Arduino.h>
#include <ESP.h>
#include <MBusinoLib.h>
#include <ArduinoJson.h>
#include <Battery18650Stats.h>

#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <driver/adc.h> // Добавьте этот заголовочный файл


#include "Zigbee.h"
#include "zigbee_mbus_sensor.h"

#ifndef ZIGBEE_MODE_ED
#error "Zigbee end device mode is not selected in Tools->Zigbee mode"
#endif

extern ZigbeeMBusSensor zbMBusSensor;

#define uS_TO_S_FACTOR 1000000ULL /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  43200         /* Sleep for 55s will + 5s delay for establishing connection => data reported every 1 minute */

#define ADC_PIN 0 
#define CONVERSION_FACTOR 2.05
#define READS 10 
Battery18650Stats battery(ADC_PIN, CONVERSION_FACTOR, READS);

//--------------------- Настройки UART-----------------------------------------------------------------------
#define RXD 20             // Настраиваем пин RX для платы RS485 HW-0519
#define TXD 21             // Настраиваем пин TX для платы RS485 HW-0519
#define MBUS_ADDRES 0x88    // Адрес M-Bus устройства
#define START_ADDRESS 0x0C  // Устанавливаем бит с которого

unsigned long timerMbus = 0;
const unsigned long requestInterval = 10000;

//--------------------- Установка пина управления питанием HW-0519--------------------------------
#define DC_DC_PIN 1                                                    //Управлением оптопарой (оптопара работает в качестве ключа линиии GND)
//--------------------- Установка пина управления DC-DC модулем MT3608-----------------------------
#define HW_0519_PIN 2                                                        //Управлением оптопарой (оптопара работает в качестве ключа линиии GND)
// ------------------------------------- Определение массива пинов --------------------------------

const int pinsInitial[] = {LED_BUILTIN, 15, 16, 17};
const int pinsCountInitial = sizeof(pinsInitial) / sizeof(pinsInitial[0]);

const int pinsPheripirals[] = {DC_DC_PIN, HW_0519_PIN};
const int pinsCountPheripirals = sizeof(pinsPheripirals) / sizeof(pinsPheripirals[0]);

// ------------------------------------- Подготовка к сну -----------------------------------------

// ------ Функция декодирования M-BUS телеграмм ------ 
void decodeMBusData() {

  byte buffer[512] = { 0 };
  int index = 0;
  unsigned long timeout = millis() + 5000;  // Таймаут 5 секунд
  int packet_size = buffer[1] + 6;
  // 1. Чтение данных с таймаутом
  while (millis() < timeout && index < sizeof(buffer)) {
    if (Serial1.available()) {
      buffer[index++] = Serial1.read();
      delayMicroseconds(500);
      timeout = millis() + 5000;  // Обновляем таймаут при получении данных
    }
  }

 /*// 2. Вывод сырых данных для отладки
  Serial.printf("\n📨 Получено %d байт:\n", index);
  for (int i = 0; i < index; i++) {
    Serial.printf("%02X ", buffer[i]);
    if ((i + 1) % 16 == 0) Serial.println();
  }
  Serial.println("\n");

  // 3. Проверка минимальной длины телеграммы
  if (index < 10) {
    Serial.println("❌ Ошибка: Слишком короткий пакет");
    return;
  }*/

  // 4. Декодирование данных
  MBusinoLib decoder(254);
  DynamicJsonDocument jsonDoc(2048);
  JsonArray root = jsonDoc.to<JsonArray>();
  const int headerSize = 9;
  int dataLen = index - headerSize - 2; // minus checksum and end byte
  if (dataLen <= 0) {
    Serial.println("❌ Недостаточно данных после заголовка.");
  return;
  }

//  Serial.printf("📦 Декодируем с позиции %d, длина %d\n", headerSize, dataLen);
  uint8_t fields = decoder.decode(&buffer[headerSize], dataLen, root);

  if (fields == 0) {
    Serial.println("❌ Ошибка декодирования M-Bus!");
    return;
  }

  // 5. Вывод декодированных данных телеграммы
//  Serial.println("✅ Успешно декодированные поля:");
//  serializeJsonPretty(root, Serial);
//  Serial.println("\n");
  
  // 6.Маппинг полей с автоматическим определением
  DynamicJsonDocument doc(1024);

  for (uint8_t k = 0; k < fields; k++) {
    JsonObject field = root[k];
    uint8_t storage = field["storage"] | 0;
    const char* name = field["name"].as<const char*>();
    double value = field["value_scaled"].as<double>();
    uint8_t code = field["code"].as<uint8_t>();
    uint8_t vif = strtoul(field["vif"].as<String>().c_str() + 2, NULL, 16);  // Конвертация hex строки в число
    uint8_t subUnit = field.containsKey("subUnit") ? field["subUnit"].as<uint8_t>() : 0;
    const char* unit = field["units"].as<const char*>();

    // 6.1.Получаем смещение байта в телеграмме
    int byteIndex = field.containsKey("byte_position") ? field["byte_position"].as<int>() : -1;
//    Serial.printf("🛠 Проверка: name=%s, vif=0x%02X, byteIndex=%d, value=%.2f\n",
//                  name, vif, byteIndex, value);

    //  6.2.Фиксация mbus_heat_sum (Heat SUM) и перевод из kWh в Gcal 
    if (strcmp(name, "energy") == 0 &&  value > 100000) {
      
      Serial.println("🔥 Найдено значение mbus_heat_sum_Wh!");
      float heat_sum_Wh = value;
      float heat_sum_Gcal = heat_sum_Wh*0.00000085985;
      doc["mbus_heat_sum_Gcal"] = heat_sum_Gcal;  //
      Serial.printf("  ✅ Heat: %.3f\n", heat_sum_Gcal);
      uint8_t dp = 3;
      zbMBusSensor.updateSummationDeliveredWithFormatting(heat_sum_Gcal, dp);
      //Serial.printf("[AUTO] Обновлено из decodeMBusData: %.3f\n", heat_sum_Gcal);

      /*float heat_sum_Gcal = heat_sum_Wh * 0.00000000086;
      doc["mbus_heat_sum_Gcal"] = heat_sum_Gcal;
      Serial.printf("  ✅ Heat: %.3f\n", heat_sum_Gcal);*/

    } else if (strcmp(name, "energy") == 0 && vif && value <= 100000) {
      continue;
    }

    //  6.3.Фиксация mbus_volume_sum (Volume)                     // Суммарный объём горячей воды м3
    if (strcmp(name, "volume") == 0 && vif == 0x14) {
      doc["mbus_volume_sum"] = value;
      float volume_sum = value;
      Serial.printf("  ✅ Volume: %.3f\n", volume_sum);
//      zbMBusSensor.updateFlowMeasuredValue(volume_sum);
    } 
    else if ((vif == 0x14) && value <= 0) {
      continue;
    }

    //  6.4.Фиксация mbus_power (Power)                           // Потребляемое тепло Ватт
    if (strcmp(name, "power") == 0 && vif == 0x2C) {
      doc["mbus_power"] = value;
      float power = value;
      Serial.printf("  ✅ Power: %.3f\n", power);
    }

    //  6.5.Фиксация mbus_volume_flow
    if (strcmp(name, "volume_flow") == 0 && vif == 0x3b) {  // Текущий(мгновенный) объем
      doc["mbus_volume_flow"] = value;
      //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!// TO-DO
    } else if ((vif == 0x3b) && value <= 0) {
      continue;
    }

    //  6.6.Фиксация mbus_flow_temerature
    if (strcmp(name, "flow_temperature") == 0) {  // Температура потока
      doc["mbus_temp_flow"] = value;
      float tFlow = value;
      Serial.printf("  ✅ FlowTemp: %.3f\n", tFlow);
//      zbMBusSensor.updateFlowTemperature(tFlow);
    } else if (strcmp(name, "flow_temperature") && value <= 0) {
      continue;
    }

    //  6.7.Фиксация mbus_return_temerature
    if (strcmp(name, "return_temperature") == 0) {  // Температура обратки
      doc["mbus_temp_return"] = value;
      float tReturn = value;
      Serial.printf("  ✅ ReturnTemp: %.3f\n", tReturn);
//      zbMBusSensor.updateReturnTemperature(tReturn);
    } else if (strcmp(name, "return_temperature") == 0 && value <= 0) {
      continue;
    }


  }

  // 7. Отправка данных

  //char jsonPayload[1024];
  //serializeJson(doc, jsonPayload);

  //Serial.println("Декодированные данные:");
  //serializeJsonPretty(doc, Serial);
  //Serial.println("\n-------------------------------------");

  Serial1.flush();
}

// ---------- Отправка M-Bus запроса -----------------
void sendREQ_UD2(uint8_t addres) {
  byte frame[] = { 0x10, 0x5B, addres, (byte)(0x5B + addres), 0x16 };  //{0x10, 0x5B, addres, (byte)(0x5B + addres), 0x16};
  Serial1.write(frame, sizeof(frame));
  Serial.println("Отправлен запрос REQ_UD2");
}

// ---------- Ручная перезагрузка -------------------- 
void checkSerialReboot() {  //Перезагрузка ESP32 через Serial Monitor + вкл./выкл. DC-DC
  while (Serial.available() > 0) {
    char input = Serial.read();
    if (input == 'r' || input == 'R') {
      Serial.println("\n🔃 Инициирована перезагрузка ESP32!");
      delay(1000);    // Даем время для отправки сообщения
      ESP.restart();  // Основная функция перезагрузки
    }
    
    if (input == 'dc_pin_on') {
        digitalWrite(DC_DC_PIN, HIGH); // Просто устанавливаем HIGH
        Serial.println("DC pin ON");
        }
    
    if (input == 'dc_pin_off') {
        digitalWrite(DC_DC_PIN, LOW); // Просто устанавливаем LOW
        Serial.println("DC pin OFF");
      }
  }
}

void prepareForSleep(){
  delay(20000);

  for (int p=0; p < pinsCountPheripirals; p++) {
    pinMode(pinsPheripirals[p], OUTPUT);
    digitalWrite(pinsPheripirals[p], LOW);
  }

  Serial1.end();
  Serial.end();
  
  esp_deep_sleep_start();
}

// ---------- Основные функции -----------------------
void setup() {
  //Запуск Serial для вывода сообщений
  Serial.begin(115200);

  for (int i = 0; i < pinsCountInitial; i++) {
    pinMode(pinsInitial[i], OUTPUT);
    digitalWrite(pinsInitial[i], LOW);
  }
  
  for (int p = 0; p < pinsCountPheripirals; p++) {
    pinMode(pinsPheripirals[p], OUTPUT);
    digitalWrite(pinsPheripirals[p], HIGH);
  }

  // Configure the wake up source and set to wake up every 5 seconds
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);

  //Запуск Serial1 для чтения MBus
  Serial1.begin(2400, SERIAL_8E1, RXD, TXD);
  
  //Запуск Zigbee
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

  Serial.print("Volts: ");
  Serial.println(battery.getBatteryVolts());

  Serial.print("Charge level: ");
  Serial.println(battery.getBatteryChargeLevel());

  Serial.print("Charge level (using the reference table): "); 
  Serial.println(battery.getBatteryChargeLevel(true));
  
  zbMBusSensor.updateMainVoltage(battery.getBatteryVolts());
}

void loop() {
  checkSerialReboot();
  // Отправка запроса к M-Bus
  if (millis() - timerMbus > requestInterval) {
    timerMbus = millis();
    sendREQ_UD2(MBUS_ADDRES);
    delay(2000);
    decodeMBusData();   

    prepareForSleep();  
  }
}