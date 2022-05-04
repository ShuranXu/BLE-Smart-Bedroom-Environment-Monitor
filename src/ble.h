/**
 * @file ble.h
 * @author Shuran Xu (shxu6388@colorado.edu)
 * @brief This header file contains macros for type conversion,
 * custom data structure for storing BLE state data, and public APIs to
 * provide handling of Bluetooth events.
 * @version 0.1
 * @date 2022-02-15
 *
 * @copyright Copyright (c) 2022
 *
 */

#ifndef __BLE_H__
#define __BLE_H__

#include "sl_bt_api.h"
#include <stdbool.h>

#define  UINT8_TO_BITSTREAM(p, n)     { *(p)++ = (uint8_t)(n); }
#define  UINT32_TO_BITSTREAM(p, n)    { *(p)++ = (uint8_t)(0); *(p)++ = (uint8_t)(n); *(p)++ = (uint8_t)((n) >> 8); \
                                        *(p)++ = (uint8_t)((n) >> 16); *(p)++ = (uint8_t)((n) >> 24); }

#define UINT32_TO_FLOAT(m, e)         (((uint32_t)(m) & 0x00FFFFFFU) | (uint32_t)((int32_t)(e) << 24))


// BLE Data Structure, save all of our private BT data in here.
// Modern C (circa 2021 does it this way)
// typedef ble_data_struct_t is referred to as an anonymous struct definition
typedef struct {

// values that are common to servers and clients
bool bonded;
bd_addr myAddress;
uint8_t addressType;
uint8_t connectionHandle;
char server_addr[18];
char client_addr[18];

// values unique for server
uint8_t advertisingSetHandle;
bool indication_inflight;
bool connOn;
bool temp_indicate_enabled;
bool sound_indicate_enabled;
bool light_indicate_enabled;
bool sleep_hours_enabled;

// values unique for client
char display_bt_addr2[18];
bd_addr server_address;
uint8_t server_address_type;
uint32_t thermometer_service_handle;
uint16_t thermometer_characteristic_handle;
uint32_t light_sensor_service_handle;
uint16_t light_sensor_characteristic_handle;
uint32_t sound_sensor_service_handle;
uint16_t sound_sensor_characteristic_handle;
uint32_t sleep_hours_service_handle;
uint16_t sleep_hours_characteristic_handle;

} conn_properties_t;


//Pending data structure
typedef struct{
  uint8_t connection;
  uint16_t characteristic;
  size_t value_len;
  uint8_t value[5];
}pending_indication_t;


/**
 * @brief Bluetooth stack event handler. This overrides the dummy
 * weak implementation. The implementation of this function is designed
 * to support exclusively either the master device or the slave device,
 * depending on the value of DEVICE_IS_BLE_SERVER.
 * @param evt: Event coming from the Bluetooth stack.
 */
void handle_ble_event(sl_bt_msg_t *evt);

/**
 * @brief This function returns a pointer to the ELB private data
 * @return the address of the global ELB private data
 */
conn_properties_t *getBleDataPtr();

#if DEVICE_IS_BLE_SERVER == 0

bool user_input_status();
uint32_t *get_CAL_temperature();
uint32_t *get_CAL_lux_level();
uint32_t *get_CAL_sound_level();
uint32_t *get_sleep_hrs();
uint8_t get_sleep_time();
#else

/**
 * Update the LCD display with all optimal sensor values sent from
 * the client device.
 */
void LCD_display_optimal_values();
#endif



#endif //__BLE_H__
