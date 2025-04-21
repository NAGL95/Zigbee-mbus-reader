// 57_zigbee_mbus_sensor.h
#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include "ha/esp_zigbee_ha_standard.h"
#include "zcl/esp_zigbee_zcl_common.h"  // Для esp_zb_zcl_cluster_list_t
#include "esp_zigbee_core.h"            // Для esp_zb_endpoint_config_t


class ZigbeeMBusSensor : public ZigbeeEP {
public:
    ZigbeeMBusSensor(uint8_t endpoint);
     ~ZigbeeMBusSensor();
   
    void reportMBusData();
    void registerDevice();
    //void updateSummationDelivered(uint32_t value);
    void updateSummationDeliveredWithFormatting(double value, uint8_t decimal_places);
};
#endif  // CONFIG_ZB_ENABLED

extern ZigbeeMBusSensor zbMBusSensor;

void initZigbeeMBusSensor();