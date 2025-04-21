// 57_zigbee_mbus_sensor.cpp 
#include "esp_zigbee_type.h"
#include "esp_log.h"
#include "zigbee_mbus_sensor.h"
#include "esp_zigbee_core.h"
#include "zcl/esp_zigbee_zcl_command.h"
#include "esp_zigbee_cluster.h"
#include <cstring>
#include <cmath> 
#include "esp_log_level.h"

#include "zigbee_mbus_sensor.h"
#include "Zigbee.h"
#define ESP_ZB_ZCL_METERING_FORMATTING_SET

#define ZB_MBUS_SENSOR_ENDPOINT_NUMBER 11

ZigbeeMBusSensor zbMBusSensor(ZB_MBUS_SENSOR_ENDPOINT_NUMBER);

void initZigbeeMBusSensor() {
    zbMBusSensor.setManufacturerAndModel("NAGL DIY", "ZigbeeMBusSensor");
    Zigbee.addEndpoint(&zbMBusSensor);
}

static const char* TAG = "*";
// Определение класса ZigbeeMBusSensor и его методов в .cpp файле
// (деструктор и конструктор)

//Деструктор
ZigbeeMBusSensor::~ZigbeeMBusSensor() {
}

ZigbeeMBusSensor::ZigbeeMBusSensor(uint8_t endpoint) : ZigbeeEP(endpoint) {
    static esp_zb_basic_cluster_cfg_t mbus_basic_cfg = {
        .zcl_version = 3,
        .power_source = ESP_ZB_ZCL_BASIC_POWER_SOURCE_DEFAULT_VALUE
    };
    static esp_zb_attribute_list_t *mbus_basic_cluster = esp_zb_basic_cluster_create(&mbus_basic_cfg);

    static esp_zb_identify_cluster_cfg_t mbus_identify_cfg = {
        .identify_time = ESP_ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE,
    };
    static esp_zb_attribute_list_t *mbus_identify_cluster = esp_zb_identify_cluster_create(&mbus_identify_cfg);

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


    static esp_zb_endpoint_config_t mbus_ep_config = {
        .endpoint = endpoint,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID, //    HA profile ID = 0x0104U, /*!< @ref esp_zb_af_profile_id_t */
        //.app_profile_id = ESP_ZB_AF_SE_PROFILE_ID, //   SE profile ID = 0x0109U /*!< @ref esp_zb_af_profile_id_t */
        .app_device_id = ESP_ZB_HA_METER_INTERFACE_DEVICE_ID, // 0x0053  /!< @ref esp_zb_ha_standard_devices_t /
        .app_device_version = 1
    };
    
    esp_zb_cluster_list_add_metering_cluster(mbus_cluster_list, mbus_metering_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    
    setEpConfig(mbus_ep_config, mbus_cluster_list);
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

    if (status != ESP_OK) {
        ESP_LOGE("ZIGBEE", "Ошибка при установке current_summation_delivered");
    } else {
        ESP_LOGI("ZIGBEE", "Передано: %.3f -> закодировано как %llu", value, scaled_value);
    }

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
        ESP_LOGE("ZIGBEE", "Ошибка при установке summation_formatting: %d", status);
    } else {
        ESP_LOGI("ZIGBEE", "summation_formatting установлен в 0x%02X (left=%d, right=%d)",
                 formatting, digits_left, decimal_places);
    }
}
