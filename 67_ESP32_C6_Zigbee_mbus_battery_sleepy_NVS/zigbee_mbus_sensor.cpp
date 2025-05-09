//66_zigbee_mbus_sensor.cpp 
#include "esp_zigbee_type.h"
#include "esp_log.h"
#include "zigbee_mbus_sensor.h"
#include "esp_zigbee_core.h"
#include "zcl/esp_zigbee_zcl_command.h"
#include "zcl/esp_zigbee_zcl_power_config.h"
#include "esp_zigbee_cluster.h"
#include <cstring>
#include <cmath> 
#include "esp_log_level.h"

#include "nvs_flash.h"
#include "nvs.h"

#include "zigbee_mbus_sensor.h"
#include "Zigbee.h"

#define ESP_ZB_ZCL_METERING_FORMATTING_SET

#define ZB_MBUS_SENSOR_ENDPOINT_NUMBER 11

//--------------------------------------------------------O-T-A--------------------------------------------------------------------------------------------------------

/* Zigbee OTA configuration */
#define OTA_UPGRADE_RUNNING_FILE_VERSION    0x01010100  // Increment this value when the running image is updated
#define OTA_UPGRADE_DOWNLOADED_FILE_VERSION 0x01010101  // Increment this value when the downloaded image is updated
#define OTA_UPGRADE_HW_VERSION              0x0101      // The hardware version, this can be used to differentiate between different hardware versions

//----------------------------------------------------------------------------------------------------------------------------------------------------------------


ZigbeeMBusSensor zbMBusSensor(ZB_MBUS_SENSOR_ENDPOINT_NUMBER);

static const char* TAG = "*";
// Определение класса ZigbeeMBusSensor и его методов в .cpp файле
// (деструктор и конструктор)

// Преобразование float → int16_t с заданным масштабом и округлением
static int16_t float_to_int16_scaled(float value, float scale) {
    if (value >= 0.0f) {
        return static_cast<int16_t>(value * scale + 0.5f);
    } else {
        return static_cast<int16_t>(value * scale - 0.5f);
    }
}

//Деструктор
ZigbeeMBusSensor::~ZigbeeMBusSensor() {
}

ZigbeeMBusSensor::ZigbeeMBusSensor(uint8_t endpoint) : ZigbeeEP(endpoint) {
    // Конфигурация Basic Cluster
    static esp_zb_basic_cluster_cfg_t mbus_basic_cfg = {
        .zcl_version = 3,
        .power_source = ESP_ZB_ZCL_BASIC_POWER_SOURCE_DEFAULT_VALUE
    };
    static esp_zb_attribute_list_t *mbus_basic_cluster = esp_zb_basic_cluster_create(&mbus_basic_cfg);
    
    // Конфигурация Identify Cluster
    static esp_zb_identify_cluster_cfg_t mbus_identify_cfg = {
        .identify_time = ESP_ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE,
    };
    static esp_zb_attribute_list_t *mbus_identify_cluster = esp_zb_identify_cluster_create(&mbus_identify_cfg);

    // Конфигурация VolumeFlow
    static esp_zb_flow_meas_cluster_cfg_t mbus_flow_volume_cfg = {
        .measured_value = 0,
        .min_value      = 0,
        .max_value      = 32767
    };
    static esp_zb_attribute_list_t *mbus_flow_volume_cluster = esp_zb_flow_meas_cluster_create(&mbus_flow_volume_cfg);

    // Конфигурация FlowTemp
    static esp_zb_temperature_meas_cluster_cfg_t mbus_flow_temp_cfg = {
        .measured_value = 0,       // начальное значение
        .min_value      = -4000,   // –40.00 °C
        .max_value      = 12500,   // 125.00 °C
    };
    static esp_zb_attribute_list_t *mbus_flow_temp_cluster = esp_zb_temperature_meas_cluster_create(&mbus_flow_temp_cfg);

    // Конфигурация ReturnTemp
/*  static esp_zb_temperature_meas_cluster_cfg_t mbus_return_temp_cfg = {};
    static esp_zb_attribute_list_t *mbus_return_temp_cluster = esp_zb_temperature_meas_cluster_create(&mbus_return_temp_cfg);
*/
    // Конфигурация Power Cluster
    static esp_zb_power_config_cluster_cfg_t mbus_power_cfg = {};
    static esp_zb_attribute_list_t *mbus_power_cluster = esp_zb_power_config_cluster_create(&mbus_power_cfg);

    // Добавление кластеров в список
    esp_zb_metering_cluster_cfg_t mbus_metering_cfg = {
        .current_summation_delivered = {0},
        .status = 0,
        .uint_of_measure = 0x00, // kWh
        .summation_formatting = 0x33,
        .metering_device_type = ESP_ZB_ZCL_METERING_HEAT_METERING //  /*ESP_ZB_ZCL_METERING_HEAT_METERING esp_zb_zcl_metering_device_type_t;*/
    };
    static esp_zb_attribute_list_t *mbus_metering_cluster = esp_zb_metering_cluster_create(&mbus_metering_cfg);

    static esp_zb_cluster_list_t *mbus_cluster_list = esp_zb_zcl_cluster_list_create();
    esp_zb_cluster_list_add_basic_cluster(mbus_cluster_list, mbus_basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_identify_cluster(mbus_cluster_list, mbus_identify_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_metering_cluster(mbus_cluster_list, mbus_metering_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

//    esp_zb_cluster_list_add_flow_meas_cluster(mbus_cluster_list, mbus_flow_volume_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
//    esp_zb_cluster_list_add_temperature_meas_cluster(mbus_cluster_list, mbus_flow_temp_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
//    esp_zb_cluster_list_add_temperature_meas_cluster(mbus_cluster_list, mbus_return_temp_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    esp_zb_cluster_list_add_power_config_cluster(mbus_cluster_list, mbus_power_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    static esp_zb_endpoint_config_t mbus_ep_config = {
        .endpoint = endpoint,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID, //    HA profile ID = 0x0104U, /*!< @ref esp_zb_af_profile_id_t */
        .app_device_id = ESP_ZB_HA_METER_INTERFACE_DEVICE_ID, // 0x0053  /!< @ref esp_zb_ha_standard_devices_t /
        .app_device_version = 1
    };
       
    setEpConfig(mbus_ep_config, mbus_cluster_list);
}

void initZigbeeMBusSensor() {
    zbMBusSensor.setManufacturerAndModel("NAGL DIY", "ZigbeeMBusSensor");

    Zigbee.addEndpoint(&zbMBusSensor);

    zbMBusSensor.setPowerSource(ZB_POWER_SOURCE_BATTERY, 100);

    // Add OTA client to the M-Bus Sensor
    zbMBusSensor.addOTAClient(OTA_UPGRADE_RUNNING_FILE_VERSION, OTA_UPGRADE_DOWNLOADED_FILE_VERSION, OTA_UPGRADE_HW_VERSION);
  
    // Start Zigbee OTA client query, first request is within a minute and the next requests are sent every hour automatically
    zbMBusSensor.requestOTAUpdate();
}

void ZigbeeMBusSensor::updateSummationDeliveredWithFormatting(double value, uint8_t decimal_places) {
    uint8_t endpoint = ZB_MBUS_SENSOR_ENDPOINT_NUMBER;

    // Масштабируем значение
    uint64_t scale = 1;
    for (uint8_t i = 0; i < decimal_places; i++) {
        scale *= 10;
    }   
    uint64_t scaled_value = (uint64_t)(value * scale + 0.5);

    // Кодируем в esp_zb_uint48_t
    esp_zb_uint48_t encoded_val;
    encoded_val.low  = (uint32_t)(scaled_value & 0xFFFFFFFFULL);
    encoded_val.high = (uint16_t)(scaled_value >> 32);

    // Установка current_summation_delivered
    esp_err_t status = esp_zb_zcl_set_attribute_val(
        endpoint,
        ESP_ZB_ZCL_CLUSTER_ID_METERING,
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ESP_ZB_ZCL_ATTR_METERING_CURRENT_SUMMATION_DELIVERED_ID,
        &encoded_val,
        false
    );

    // Вычисляем digits_left
    uint8_t digits_left = 0;
    uint64_t int_part = (uint64_t)value;
    while (int_part > 0) {
        digits_left++;
        int_part /= 10;
    }
    if (digits_left == 0) digits_left = 1;

    // Форматирование summation_formatting
    uint8_t formatting = ((digits_left - 1) << 4) | (decimal_places & 0x0F);
    status = esp_zb_zcl_set_attribute_val(
        endpoint,
        ESP_ZB_ZCL_CLUSTER_ID_METERING,
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ESP_ZB_ZCL_ATTR_METERING_SUMMATION_FORMATTING_ID,
        &formatting,
        false
    );

     if (status != ESP_OK) {
        ESP_LOGE("ZIGBEE", "Ошибка при установке current_summation_delivered");
    } else {
        ESP_LOGI("ZIGBEE", "Передано: %.3f -> закодировано как %llu", value, scaled_value);
    }

    if (status != ESP_OK) {
        ESP_LOGE("ZIGBEE", "Ошибка при установке summation_formatting: %d", status);
    } else {
        ESP_LOGI("ZIGBEE", "summation_formatting установлен в 0x%02X (left=%d, right=%d)",
                 formatting, digits_left, decimal_places);
    }
}

void ZigbeeMBusSensor::updateFlowMeasuredValue(float flow_volume) {
    uint8_t endpoint = ZB_MBUS_SENSOR_ENDPOINT_NUMBER;

    // 1. Масштабируем в сотые доли (0.01 м³/ч)
    int16_t scaled_flow = float_to_int16_scaled(flow_volume, 100.0f);

    // 2. Передача атрибута MeasuredValue (ID = 0x0000)
    esp_err_t status = esp_zb_zcl_set_attribute_val(
        endpoint,
        ESP_ZB_ZCL_CLUSTER_ID_FLOW_MEASUREMENT,       // 0x0404
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ESP_ZB_ZCL_ATTR_FLOW_MEASUREMENT_VALUE_ID,    // правильный макрос :contentReference[oaicite:1]{index=1}
        &scaled_flow,
        false
    );

    if (status != ESP_OK) {
        ESP_LOGE("ZIGBEE", "Ошибка установки Flow MeasuredValue: %d", status);
    } else {
        ESP_LOGI("ZIGBEE", "Flow MeasuredValue установлен: %.2f m³/ч (scaled=%d)", flow_volume, scaled_flow);
    }
}
// Обновление температуры протока
void ZigbeeMBusSensor::updateFlowTemperature(float temperature_celsius) {
    uint8_t endpoint = ZB_MBUS_SENSOR_ENDPOINT_NUMBER;


    // Zigbee Temperature Measurement — сотые доли градуса (0.01 °C)
    int16_t scaled = float_to_int16_scaled(temperature_celsius, 100.0f);

    esp_err_t status = esp_zb_zcl_set_attribute_val(
        endpoint,
        ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT,
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID,
        &scaled,
        false
    );

    if (status != ESP_OK) {
        ESP_LOGE("ZIGBEE", "Ошибка установки Flow Temp: %d", status);
    } else {
        ESP_LOGI("ZIGBEE", "Flow Temp установлен: %.2f °C", temperature_celsius);
    }
}
/*
// Обновление температуры обратной линии
void ZigbeeMBusSensor::updateReturnTemperature(double temperature_celsius) {
    uint8_t endpoint = ZB_MBUS_SENSOR_ENDPOINT_NUMBER;

    int16_t scaled = static_cast<int16_t>(temperature_celsius * 100 + (temperature_celsius >= 0 ? 0.5 : -0.5));

    esp_err_t status = esp_zb_zcl_set_attribute_val(
        endpoint,
        ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT,
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID,
        &scaled,
        false
    );

    if (status != ESP_OK) {
        ESP_LOGE("ZIGBEE", "Ошибка при установке Return Temperature: %d", status);
    } else {
        ESP_LOGI("ZIGBEE", "Return Temperature установлен: %.2f °C", temperature_celsius);
    }
}

*/
void ZigbeeMBusSensor::updateMainVoltage(float voltage_volts) {
    uint8_t endpoint = ZB_MBUS_SENSOR_ENDPOINT_NUMBER;

    // Конвертация в единицы 100 mV (1 = 0.1 V)
    uint16_t voltage_100mv = static_cast<uint16_t>(roundf(voltage_volts * 100.0f));

    esp_err_t status = esp_zb_zcl_set_attribute_val(
        endpoint,
        ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG,             // Power Config Cluster
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ESP_ZB_ZCL_ATTR_POWER_CONFIG_MAINS_VOLTAGE_ID,  // MainsVoltage (ID 0x0000)
        &voltage_100mv,
        false                                            // без reportable-change
    );

    if (status != ESP_OK) {
        ESP_LOGE("ZIGBEE", "Ошибка установки MainsVoltage: %d", status);
    } else {
        ESP_LOGI("ZIGBEE", "MainsVoltage установлен: %.1f V (scaled=%u)", voltage_volts, voltage_100mv);
    }
}