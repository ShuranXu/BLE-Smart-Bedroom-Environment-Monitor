/**
 * @file ble.c
 * @author Shuran Xu (shxu6388@colorado.edu)
 * @brief This source file provides the implementation of the overwritten
 * bluetooth stack event handler and the getter function of ble_data.
 * @version 0.1
 * @date 2022-02-15
 * @reference The example project soc_thermometer is studied and the handling of
 * event sl_bt_evt_system_boot_id is taken as reference to implement the handler
 * for sl_bt_evt_system_boot_id in handle_ble_event( ).
 *
 * @copyright Copyright (c) 2022
 *
 */

#include "ble.h"
#include "sl_bluetooth.h"
#include "sl_status.h"
#include "gatt_db.h"
#include "lcd.h"
#include "gpio.h"
#include "scheduler.h"
#include "ble_device_type.h"
#include "circular_buffer.h"
#include "scheduler.h"
#include "adc.h"
#include "app.h"

//for debugging only
#define INCLUDE_LOG_DEBUG 1
#include "src/log.h"


//#define DEBUG_MODE

//BLE private data
static conn_properties_t  ble_data = {
    .bonded = false,
    .advertisingSetHandle = 0,
    .indication_inflight = false,
    .temp_indicate_enabled = false,
    .sound_indicate_enabled = false,
    .light_indicate_enabled = false,
    .sleep_hours_enabled = false,
    .myAddress = {},
    .addressType = 0,
    .connOn = 0,
};

/**
 * @brief This function returns a pointer to the ELB private data
 * @return the address of the global ELB private data
 */
conn_properties_t *getBleDataPtr()
{
  return (&ble_data);
} //getBleDataPtr


#if DEVICE_IS_BLE_SERVER

/**
 * This following variables stores the values sent from the client.
 */
static uint32_t optimal_temp_value = 0;
static uint32_t optimal_light_value = 0;
static uint32_t optimal_sound_value = 0;

/**
 * Update the LCD display with all optimal sensor values sent from
 * the client device.
 */
void LCD_display_optimal_values()
{
   displayPrintf(DISPLAY_ROW_TEMPVALUE, "Temp:%d C", optimal_temp_value);
   displayPrintf(DISPLAY_ROW_8, " Light:%d lux", optimal_light_value);
   displayPrintf(DISPLAY_ROW_9, "Sound:%d dB", optimal_sound_value);
}


/**
 * Reset all fields of ble_data. This function shall be
 * used whenever the connection is closed and the device
 * is booted.
 */
static void reset_bleDataInternals()
{
  //reset ble_data.bonded
  ble_data.bonded = false;
  // update the connOn
  ble_data.connOn = false;
  //reset indication_inflight
  ble_data.indication_inflight = false;
  // reset the connection handle
  ble_data.connectionHandle = 0;
  //reset light_indicate_enabled
  ble_data.light_indicate_enabled = false;
  //reset temp_indicate_enabled
  ble_data.temp_indicate_enabled = false;
  //reset sleep_hours_enabled
  ble_data.sleep_hours_enabled = false;
  //reset sound_indicate_enabled
  ble_data.sound_indicate_enabled = false;
  //reset advertisingSetHandle
  ble_data.advertisingSetHandle = 0;
}

/**
 * Send the indication to the client if there is no ongoing indication.Otherwise
 * the indication will be enqueued.
 * @param bleDataPtr: the pointer of ble_data
 * @param characteristic_handle: the handle of the target characteristic
 * @return: 1 if error otherwise 0.
 */
static int send_indication(conn_properties_t *bleDataPtr, uint32_t characteristic_handle)
{
  sl_status_t sc;
  uint8_t sensor_indication_value = 1;
  if(!bleDataPtr->indication_inflight){
      sc = sl_bt_gatt_server_send_indication(
          ble_data.connectionHandle,
          characteristic_handle,
          1,
          (const uint8_t *)&sensor_indication_value);

      if(sc != SL_STATUS_OK){
          //output the error message
          LOG_ERROR("Failed to send the indication, error code = 0x%x\r\n", sc);
          return 1;
      }

      //set ble_data.indication_inflight
      bleDataPtr->indication_inflight = true;
  }
  else{
      LOG_INFO("indication_inflight = 1 !\r\n");
      //queue the indication so that it can be sent later
     if(write_queue(ble_data.connectionHandle, characteristic_handle, \
     1, (uint8_t *)&sensor_indication_value)){
         LOG_ERROR("Failed to enqueue the indication\r\n");
         return 1;
     }
  }
  return 0;
}

/**
 * Handle the pending indications stored in the circular buffer.
 * @param bleDataPtr: the pointer of ble_data
 */
static void handle_pending_indications(conn_properties_t *bleDataPtr)
{
    //retrieve the pending indication entity if any
    if(get_queue_depth() > 0){

        pending_indication_t pending_indicate;

        if(!read_queue(&pending_indicate.connection, &pending_indicate.characteristic, \
                             &pending_indicate.value_len, &pending_indicate.value[0])){

            send_indication(bleDataPtr, pending_indicate.characteristic);
        }
    }
}


/**
 * @brief Bluetooth stack event handler. This overrides the dummy
 * weak implementation. The following implementation is designed
 * for the slave device.
 * @param evt: Event coming from the Bluetooth stack.
 */
void handle_ble_event(sl_bt_msg_t *evt)
{
  int i;
  int index = 0;
  sl_status_t sc;

  switch(SL_BT_MSG_ID(evt->header)) {

    // ******************************************************
    // Events common to both Servers and Clients
    // ******************************************************
    // --------------------------------------------------------
    // This event indicates the device has started and the radio is ready.
    // Do not call any stack API commands before receiving this boot event!
    // Including starting BT stack soft timers!
    // --------------------------------------------------------
    case sl_bt_evt_system_boot_id:

#ifdef DEBUG_MODE
      // handle boot event
      LOG_INFO("Bluetooth stack booted: v%d.%d.%d-b%d\r\n",
                        evt->data.evt_system_boot.major,
                        evt->data.evt_system_boot.minor,
                        evt->data.evt_system_boot.patch,
                        evt->data.evt_system_boot.build);
#endif

      reset_bleDataInternals();
      // Delete all bondings
      sl_bt_sm_delete_bondings();
      // Configure security requirements and I/O capabilities of the system
      sl_bt_sm_configure(0x2F, sl_bt_sm_io_capability_displayyesno);
      // Set the device should accept new bondings
      sl_bt_sm_set_bondable_mode(true);

      // Initialize the LCD display
      displayInit();

      // Extract unique Identity BT Address.
      sc = sl_bt_system_get_identity_address(&ble_data.myAddress, &ble_data.addressType);
      if(sc != SL_STATUS_OK){
            LOG_ERROR("Failed to extract Identity address\r\n");
            return;
       }
#ifdef DEBUG_MODE
       LOG_INFO("Server identity addr:\r\n");
       for(i=0;i<6;i++){
           LOG_INFO("0x%x\r\n", ble_data.myAddress.addr[i]);
       }
#endif

      // Create an advertising set.
      sc = sl_bt_advertiser_create_set(&ble_data.advertisingSetHandle);
      if(sc != SL_STATUS_OK){
          LOG_ERROR("Failed to create an advertising set\r\n");
          return;
      }

      // Set advertising interval to 250ms.
      sc = sl_bt_advertiser_set_timing(
          ble_data.advertisingSetHandle, // advertising set handle
          400, // min. adv. interval (Value in units of 0.625 ms)
          400, // max. adv. interval (Value in units of 0.625 ms)
          0,   // adv. duration
          0);  // max. num. adv. events

      if(sc != SL_STATUS_OK){
            LOG_ERROR("Failed to set the advertiser timing parameters\r\n");
            return;
        }

      // Start general advertising and enable connections.
      sc = sl_bt_advertiser_start(
          ble_data.advertisingSetHandle,
          sl_bt_advertiser_general_discoverable,
          sl_bt_advertiser_connectable_scannable);

      if(sc != SL_STATUS_OK){
          LOG_ERROR("Failed to start advertising\r\n");
          return;
      }

      // obtain and display the device identity address
      index = 0;
      for(i=0;i<6;i++){
          sprintf(&ble_data.server_addr[index], "%X", ble_data.myAddress.addr[i]);
          index += 2;
          if(i < 5)
            ble_data.server_addr[index] = ':';
          index++;
       }

      setup_display(BLE_DEVICE_TYPE_STRING, (uint8_t *)ble_data.server_addr, "Project", "Advertising");
      break;

    case sl_bt_evt_connection_opened_id:

      // display Connected as the current connection state
      displayPrintf(DISPLAY_ROW_CONNECTION, "Connected");
      // obtain and display the device identity address
      index = 0;
      for(i=0;i<6;i++){
          sprintf(&ble_data.client_addr[index], "%X", evt->data.evt_connection_opened.address.addr[i]);
          index += 2;
          if(i < 5)
            ble_data.client_addr[index] = ':';
          index++;
       }

      // display the client address
      displayPrintf(DISPLAY_ROW_BTADDR2, ble_data.client_addr);
      // update the connOn
      ble_data.connOn = true;
      //reset indication_inflight
      ble_data.indication_inflight = false;
      // update the connection handle
      ble_data.connectionHandle = evt->data.evt_connection_opened.connection;

      // set the connection timing parameters
      sc = sl_bt_connection_set_parameters(
          ble_data.connectionHandle, //connection handle
          60, // min interval , ms
          60, // max interval, ms
          4, // slave latency, ms
          76, //supervision timeout, ms
          0, //min_ce_length
          0xffff); // max_ce_length

      if(sc != SL_STATUS_OK){
            LOG_ERROR("Failed to configure conn parameters, error = 0x%x\r\n", sc);
            return;
       }
      break;

    case sl_bt_evt_connection_closed_id:

      reset_bleDataInternals();
      // Delete all bondings
      sl_bt_sm_delete_bondings();
      // display advertising as the current connection state
      displayPrintf(DISPLAY_ROW_CONNECTION, "Advertising");
      // Restart advertising after client has disconnected.
      sc = sl_bt_advertiser_start(
          ble_data.advertisingSetHandle,
          sl_bt_advertiser_general_discoverable,
          sl_bt_advertiser_connectable_scannable);

      if(sc != SL_STATUS_OK){
          LOG_ERROR("Failed to start advertising\r\n");
          return;
      }

      break;

    // handle external signal event
    case sl_bt_evt_system_external_signal_id:

       if(evt->data.evt_system_external_signal.extsignals == evtGPIO_PB0) {
           unsigned int pad_value = 1 - GPIO_PinInGet(EXTCOMIN_PB0_port, EXTCOMIN_PB0_pin);

          if(pad_value){
              displayPrintf(DISPLAY_ROW_9, "Button Pressed");
          }
          else{
              displayPrintf(DISPLAY_ROW_9, "Button Released");
          }
          if(!ble_data.bonded){
              // accept or reject the reported passkey confirm value based on PB0 value
              sl_bt_sm_passkey_confirm(ble_data.connectionHandle, pad_value);
          }
       }
      break;

    // Indicates either that a local Client Characteristic Configuration descriptor was changed by the remote GATT client,
    //or that a confirmation from the remote GATT client was received upon a successful reception of the indication.
    case sl_bt_evt_gatt_server_characteristic_status_id:

      if(evt->data.evt_gatt_server_characteristic_status.status_flags == gatt_server_confirmation){
          //reset ble_data.indication_inflight
          ble_data.indication_inflight = false;

          LOG_INFO("Confirmation received\r\n");

          handle_pending_indications(&ble_data);
      }

      if(evt->data.evt_gatt_server_characteristic_status.status_flags == gatt_server_client_config){

        switch(evt->data.evt_gatt_server_characteristic_status.characteristic){
            case gattdb_temperature_measurement:{
              ble_data.temp_indicate_enabled = true;
              break;
            }
            case gattdb_sound_measurement:{
              ble_data.sound_indicate_enabled = true;
              break;
            }
            case gattdb_light_measurement:{
              ble_data.light_indicate_enabled = true;
              break;
            }
            case gattdb_sleep_hours:{
              ble_data.sleep_hours_enabled = true;
              break;
            }
            default:break;
        }
      }

      break;


    //Indicates that the value of an attribute in the local GATT database
    // was changed by a remote GATT client.
    case  sl_bt_evt_gatt_server_attribute_value_id:

      // Update the GATT light measurement characteristic
      if(evt->data.evt_gatt_server_attribute_value.attribute == gattdb_light_measurement){

            size_t value_len = 0;
            sl_status_t sc = sl_bt_gatt_server_read_attribute_value(gattdb_light_measurement,
                                                                    0,
                                                                    sizeof(optimal_light_value),
                                                                    &value_len,
                                                                    (uint8_t *)&optimal_light_value
                                                                    );

            if(sc != SL_STATUS_OK){
                LOG_ERROR("Failed to read the GATT database for the light sensor, rc = 0x%x\r\n",sc);
            }

            if(value_len != sizeof(optimal_light_value)){
                LOG_WARN("Warning: received light value length is not 4  but %d!\r\n",value_len);
            }

            LOG_INFO("optimal light value = %d\r\n",optimal_light_value);

            //send the indication to the client
            LOG_INFO("Send the indication for the light update\r\n");
            if(send_indication(&ble_data, gattdb_light_measurement)){
                LOG_ERROR("Failed to send the indication for the light characteristic\r\n");
            }
      }

       // Update the GATT temperature measurement characteristic
       if(evt->data.evt_gatt_server_attribute_value.attribute == gattdb_temperature_measurement){

            size_t value_len = 0;
            sl_status_t sc = sl_bt_gatt_server_read_attribute_value(gattdb_temperature_measurement,
                                                                    0,
                                                                    sizeof(optimal_temp_value),
                                                                    &value_len,
                                                                    (uint8_t *)&optimal_temp_value
                                                                    );

            if(sc != SL_STATUS_OK){
                LOG_ERROR("Failed to read the GATT database for the temperature sensor, rc = 0x%x\r\n",sc);
            }

            if(value_len != sizeof(optimal_temp_value)){
                LOG_WARN("Warning: received temp value length is not 4  but %d!\r\n",value_len);
            }

            LOG_INFO("optimal temp value = %d\r\n",optimal_temp_value);

            //send the indication to the client
            LOG_INFO("Send the indication for the temperature update\r\n");
            if(send_indication(&ble_data, gattdb_temperature_measurement)){
                LOG_ERROR("Failed to send the indication for the temperature characteristic\r\n");
            }
      }

        // Update the GATT sound measurement characteristic
        if(evt->data.evt_gatt_server_attribute_value.attribute == gattdb_sound_measurement){

            size_t value_len = 0;
            sl_status_t sc = sl_bt_gatt_server_read_attribute_value(gattdb_sound_measurement,
                                                                    0,
                                                                    sizeof(optimal_sound_value),
                                                                    &value_len,
                                                                    (uint8_t *)&optimal_sound_value
                                                                    );

            if(sc != SL_STATUS_OK){
                LOG_ERROR("Failed to read the GATT database for the sound sensor, rc = 0x%x\r\n",sc);
            }

            if(value_len != sizeof(optimal_sound_value)){
                LOG_WARN("Warning: received sound value length is not 4  but %d!\r\n",value_len);
            }


            LOG_INFO("optimal sound value = %d\r\n",optimal_sound_value);

            //send the indication to the client
            LOG_INFO("Send the indication for the sound update\r\n");
            if(send_indication(&ble_data, gattdb_sound_measurement)){
                LOG_ERROR("Failed to send the indication for the sound characteristic\r\n");
            }
      }

       // Update the GATT sleep_hours characteristic
       if(evt->data.evt_gatt_server_attribute_value.attribute == gattdb_sleep_hours){

          //update the sleep hours on the server side
          uint32_t *sleep_hours = getSleepHours();
          size_t value_len = 0;
          sl_status_t sc = sl_bt_gatt_server_read_attribute_value(gattdb_sleep_hours,
                                                                  0,
                                                                  sizeof(*sleep_hours),
                                                                  &value_len,
                                                                  (uint8_t *)sleep_hours
                                                                  );

          if(sc != SL_STATUS_OK){
              LOG_ERROR("Failed to read the GATT database for the sleep_hours, rc = 0x%x\r\n",sc);
          }

          if(value_len != sizeof(sleep_hours)){
              LOG_WARN("Warning: received sleep_hours value length is not 4  but %d!\r\n",value_len);
          }


          //send the indication to the client
          LOG_INFO("Send the indication for the sleep hours update\r\n");
          if(send_indication(&ble_data, gattdb_sleep_hours)){
              LOG_ERROR("Failed to send the indication for the sleep_hours characteristic\r\n");
          }
      }

      break;

    case sl_bt_evt_sm_confirm_bonding_id:
      //Accept the bonding request.
      sl_bt_sm_bonding_confirm(ble_data.connectionHandle, 1);
      break;

    case sl_bt_evt_sm_confirm_passkey_id:
      //display the passkey on LCD
      displayPrintf(DISPLAY_ROW_PASSKEY, "Passkey %d", evt->data.evt_sm_confirm_passkey.passkey);
      //display the required user action
      displayPrintf(DISPLAY_ROW_ACTION, "Confirm with PB0");
      break;

    case sl_bt_evt_sm_bonded_id:
      //set ble_data.bonded
      ble_data.bonded = true;
      //clear the DISPLAY_ROW_PASSKEY row on LCD
      displayPrintf(DISPLAY_ROW_PASSKEY, "");
      //clear the DISPLAY_ROW_ACTION row on LCD
      displayPrintf(DISPLAY_ROW_ACTION, "");
      //update CONNECTION ROW on LCD
      displayPrintf(DISPLAY_ROW_CONNECTION, "Bonded");
      break;

    case sl_bt_evt_sm_bonding_failed_id:
      //reset ble_data.bonded
      ble_data.bonded = false;
      LOG_ERROR("Device bonding process failed\r\n");
      break;

    case sl_bt_evt_system_soft_timer_id:

      displayUpdate();

      break;

    default:
      break;
    } // end - switch

} // handle_ble_event()

#else

// connection parameters used for the client to setup the connection
#define CONN_INTERVAL_MIN             60   //75ms
#define CONN_INTERVAL_MAX             60   //75ms
#define CONN_RESPONDER_LATENCY        4    //4 slave latency
#define CONN_TIMEOUT                  85  //850ms
#define CONN_MIN_CE_LENGTH            0
#define CONN_MAX_CE_LENGTH            4

#define SCAN_INTERVAL                 80   //50ms
#define SCAN_WINDOW                   40   //25ms
#define SCAN_PASSIVE                  0

/**
 * Tiny state machine that handles the user inputs
 */
typedef enum{
    state_SLEEP_TIME_INPUT=1,//!< state_SLEEP_TIME_INPUT
    state_SLEEP_TIME_CONFIRM,//!< state_SLEEP_TIME_CONFIRM
    state_SLEEP_HRS_INPUT,   //!< state_SLEEP_HRS_INPUT
}sleep_user_input_t;

//Global state machine instance handling user interfacing
static sleep_user_input_t sleep_input_state = state_SLEEP_TIME_INPUT;

/**
 * The following are local variables used to track and record the user inputs.
 */
static uint8_t sleep_time = 0;
static uint32_t sleep_hrs  = 0;
//0 for AM, 1 for PM
static uint8_t AM_or_PM = 0;

//Flags for tracking switch presses
static bool PB0_time_confirmed = false;
static bool PB1_hour_confirmed = false;
static bool sleep_time_confirm = false;
static bool sleep_hour_confirm = false;
static bool user_input_confirm = false;


//Lookup tables for environmental factors
static uint32_t CAL_sound_level []  = {50};
static uint32_t CAL_lux_level   []  = {10};
static uint32_t CAL_temperature []  = {20};


/**
 * The following are getters for lookup table and flags
 */
uint8_t get_sleep_time()
{
  return sleep_time;
}
uint32_t *get_sleep_hrs()
{
  return &sleep_hrs;
}
uint32_t *get_CAL_sound_level()
{
  return &CAL_sound_level[0];
}
uint32_t *get_CAL_lux_level()
{
  return &CAL_lux_level[0];
}
uint32_t *get_CAL_temperature()
{
  return &CAL_temperature[0];
}
bool user_input_status()
{
  return user_input_confirm;
}


/**
 * @brief Bluetooth stack event handler. This overrides the dummy
 * weak implementation. The following implementation is designed
 * for the master device. All the tasks to be done here are saving
 * event data and set/clear flags according to the received event.
 * @param evt: Event coming from the Bluetooth stack.
 */
void handle_ble_event(sl_bt_msg_t *evt)
{
  switch(SL_BT_MSG_ID(evt->header)) {

    // ******************************************************
    // Events common to both Servers and Clients
    // ******************************************************
    // --------------------------------------------------------
    // This event indicates the device has started and the radio is ready.
    // Do not call any stack API commands before receiving this boot event!
    // Including starting BT stack soft timers!
    // --------------------------------------------------------
    case sl_bt_evt_system_boot_id:
      // handle boot event
//      LOG_INFO("Bluetooth stack booted: v%d.%d.%d-b%d\r\n",
//                        evt->data.evt_system_boot.major,
//                        evt->data.evt_system_boot.minor,
//                        evt->data.evt_system_boot.patch,
//                        evt->data.evt_system_boot.build);

      // Delete all bondings
      sl_bt_sm_delete_bondings();
      // Configure security requirements and I/O capabilities of the system
      sl_bt_sm_configure(0x2F, sl_bt_sm_io_capability_displayyesno);
      // Set the device should accept new bondings
      sl_bt_sm_set_bondable_mode(true);

      // Initialize the LCD display
      displayInit();

      int index = 0;
      int i, sc;

      // Extract unique Identity BT Address.
      sc = sl_bt_system_get_identity_address(&ble_data.myAddress, \
                                            &ble_data.addressType);
      if(sc != SL_STATUS_OK){
           LOG_ERROR("Failed to extract Identity address\r\n");
           return;
      }

      // Set the unique Identity BT Address.
      for(i=0;i<6;i++){
         sprintf(&ble_data.client_addr[index], "%X", \
                 ble_data.myAddress.addr[i]);
         index += 2;
         if(i < 5)
           ble_data.client_addr[index] = ':';
         index++;
      }

      // display the client device identity information to the LCD
      setup_display(BLE_DEVICE_TYPE_STRING, (uint8_t *)ble_data.client_addr, \
                   "Project", "Discovering");

      // Set passive scanning on 1Mb PHY
      sc = sl_bt_scanner_set_mode(sl_bt_gap_1m_phy, SCAN_PASSIVE);
      if(sc != SL_STATUS_OK){
          LOG_ERROR("Failed to set the mode for scanner\r\n");
          break;
      }

      // Set scan interval and scan window
      sc = sl_bt_scanner_set_timing(sl_bt_gap_1m_phy, SCAN_INTERVAL, SCAN_WINDOW);
      if(sc != SL_STATUS_OK){
          LOG_ERROR("Failed to set the timing parameters for scanner\r\n");
          break;
      }

      // Set the default connection parameters for subsequent connections
      sc = sl_bt_connection_set_default_parameters(CONN_INTERVAL_MIN,
                                                CONN_INTERVAL_MAX,
                                                CONN_RESPONDER_LATENCY,
                                                CONN_TIMEOUT,
                                                CONN_MIN_CE_LENGTH,
                                                CONN_MAX_CE_LENGTH);
      if(sc != SL_STATUS_OK){
         LOG_ERROR("Failed to set the default connection parameters\r\n");
         break;
      }

      // Start scanning - looking for thermometer devices
      sc = sl_bt_scanner_start(sl_bt_gap_1m_phy, sl_bt_scanner_discover_generic);
      if(sc != SL_STATUS_OK){
         LOG_ERROR("Failed to start the scanner, error = 0x%x\r\n", sc);
         break;
      }
      break;

   case sl_bt_evt_connection_opened_id:
      // update the connection handle
      ble_data.connectionHandle = evt->data.evt_connection_opened.connection;
      // initiate the pairing process
      sl_bt_sm_increase_security(ble_data.connectionHandle);
      break;

    case sl_bt_evt_connection_closed_id:
      // reset the connection handle
      ble_data.connectionHandle = 0;
      //reset ble_data.bonded
      ble_data.bonded = false;
      // Delete all bondings
      sl_bt_sm_delete_bondings();
      // clear LCD displays
      displayPrintf(DISPLAY_ROW_8, "");
      displayPrintf(DISPLAY_ROW_9, "");
      displayPrintf(DISPLAY_ROW_10, "");
      displayPrintf(DISPLAY_ROW_11, "");
      break;


    // This event is generated when a new service is discovered
    case sl_bt_evt_gatt_service_id:

      // temp
      if(memcmp(evt->data.evt_gatt_service.uuid.data, get_thermo_service_uuid(), \
                sizeof(*get_thermo_service_uuid())) == 0)
          ble_data.thermometer_service_handle = evt->data.evt_gatt_service.service;
      // light
      if(memcmp(evt->data.evt_gatt_service.uuid.data, get_light_service_uuid(), \
                sizeof(*get_light_service_uuid())) == 0)
          ble_data.light_sensor_service_handle = evt->data.evt_gatt_service.service;
      // Sound
      if(memcmp(evt->data.evt_gatt_service.uuid.data, get_sound_service_uuid(), \
                sizeof(*get_sound_service_uuid())) == 0)
          ble_data.sound_sensor_service_handle = evt->data.evt_gatt_service.service;
      //Sleep hour
      if(memcmp(evt->data.evt_gatt_service.uuid.data, get_sleep_hours_service_uuid(), \
                sizeof(*get_sleep_hours_service_uuid())) == 0)
          ble_data.sleep_hours_service_handle = evt->data.evt_gatt_service.service;

      break;

    // This event is generated when a new characteristic is discovered
    case sl_bt_evt_gatt_characteristic_id:
      // temp
      if(memcmp(evt->data.evt_gatt_characteristic.uuid.data, get_thermo_char_uuid(), \
                sizeof(*get_thermo_char_uuid())) == 0){
          ble_data.thermometer_characteristic_handle = evt->data.evt_gatt_characteristic.characteristic;
          LOG_INFO("thermometer_characteristic_handle is assigned\r\n");
      }

      //light
      if(memcmp(evt->data.evt_gatt_characteristic.uuid.data, get_light_char_uuid(), \
                sizeof(*get_light_char_uuid())) == 0){
          ble_data.light_sensor_characteristic_handle = evt->data.evt_gatt_characteristic.characteristic;
          LOG_INFO("light_sensor_characteristic_handle is assigned\r\n");
      }

      //sound
      if(memcmp(evt->data.evt_gatt_characteristic.uuid.data, get_sound_char_uuid(), \
                sizeof(*get_sound_char_uuid())) == 0){
          ble_data.sound_sensor_characteristic_handle = evt->data.evt_gatt_characteristic.characteristic;
          LOG_INFO("sound_sensor_characteristic_handle is assigned\r\n");
      }

      //Sleep hours
      if(memcmp(evt->data.evt_gatt_characteristic.uuid.data, get_sleep_hours_char_uuid(), \
                sizeof(*get_sleep_hours_char_uuid())) == 0){
          ble_data.sleep_hours_characteristic_handle = evt->data.evt_gatt_characteristic.characteristic;
          LOG_INFO("sleep_hours_characteristic_handle is assigned\r\n");
      }

      break;


    // This event is generated when a characteristic value was received e.g. an indication
    case sl_bt_evt_gatt_characteristic_value_id:
      break;

    case sl_bt_evt_system_external_signal_id:

      /**
       * Update Sleep Time & Sleep Hours
       */
      if(evt->data.evt_system_external_signal.extsignals == evtLETIMER0_UF){

          if(ble_handle_sleep_values()){

              if(sleep_hrs){
                   //decrement by 1
                  sleep_hrs -= 1;
                  //update the LCD display row 10
                  displayPrintf(DISPLAY_ROW_10, "Sleep Hours: %d", sleep_hrs);
               }
               else{
                   //reset sleep_time
                   sleep_time = 0;
                   //reset user_input_confirm
                   user_input_confirm = false;
                   //reset the scheduler signal
                   reset_ble_sleep_handling();
                   //reset the LCD display row 8
                   displayPrintf(DISPLAY_ROW_8, "Sleep Time: ");
                   //reset the LCD display row 10
                   displayPrintf(DISPLAY_ROW_10, "PB0 to toggle time");
               }
          }
      }

      /* Button-0 pressed
       * ***************
       * Button-0 press is used for 3 things
       * 1. Confirm the passkey
       * 2. Selecting sleep time
       * 3. Selecting hours of sleep
       */
      if(evt->data.evt_system_external_signal.extsignals == evtGPIO_PB0)
      {
        unsigned int pad_value = 1 - GPIO_PinInGet(EXTCOMIN_PB0_port, EXTCOMIN_PB0_pin);
        if(!ble_data.bonded)
            sl_bt_sm_passkey_confirm(ble_data.connectionHandle, pad_value);


        if(ble_data.bonded){

            switch(sleep_input_state){
              case state_SLEEP_TIME_INPUT:{
                  //Incrementing hours
                  sleep_time ++;

                  if(sleep_time > 12)
                    sleep_time = 1;

                  //Displaying sleep time
                  displayPrintf(DISPLAY_ROW_10, "");
                  displayPrintf(DISPLAY_ROW_8, "Sleep Time: %d",sleep_time);
                  displayPrintf(DISPLAY_ROW_11, "Press PB1 to confirm");

                  break;
              }
              case state_SLEEP_TIME_CONFIRM:{

                  if(AM_or_PM)
                  {
                    displayPrintf(DISPLAY_ROW_10, "%u PM confirmed",sleep_time);
                  }
                else
                  {
                    displayPrintf(DISPLAY_ROW_10, "%u AM confirmed",sleep_time);
                  }

                  //reset row 9 and row 11
                  displayPrintf(DISPLAY_ROW_9, "");
                  displayPrintf(DISPLAY_ROW_11, "");
                  sleep_input_state = state_SLEEP_HRS_INPUT;
                  break;
              }
              case state_SLEEP_HRS_INPUT:{

                  displayPrintf(DISPLAY_ROW_8, "");
                  displayPrintf(DISPLAY_ROW_9, "");
                  sleep_hrs++;
                  if(sleep_hrs > 24)
                    sleep_hrs = 1;

                  displayPrintf(DISPLAY_ROW_8, "Sleep Hours: %u hrs",sleep_hrs);
                  displayPrintf(DISPLAY_ROW_11, "Press PB1 to confirm");
                  break;
              }
              default:break;
            }
        }
      }

      /* Button-1 pressed
       * ***************
       * Button-1 press is used for 3 things
       * 1. Toggling AM/PM
       * 2. Confirming Sleep time
       * 3. Confirming Sleep Hours
       */
      if(evt->data.evt_system_external_signal.extsignals == evtGPIO_PB1 )
      {

          if(ble_data.bonded){

              switch(sleep_input_state){
                case state_SLEEP_TIME_INPUT:
                case state_SLEEP_TIME_CONFIRM:{

                  sleep_input_state = state_SLEEP_TIME_CONFIRM;
                  displayPrintf(DISPLAY_ROW_10, "Press PB1 for AM/PM");
                  displayPrintf(DISPLAY_ROW_11, "Press PB0 to confirm");

                  if(AM_or_PM == 0){
                      AM_or_PM = 1;
                      displayPrintf(DISPLAY_ROW_9, "Reference Time: PM");
                  }
                  else if(AM_or_PM == 1){
                      AM_or_PM = 0;
                      displayPrintf(DISPLAY_ROW_9, "Reference Time: AM");
                  }
                  break;
                }
                case state_SLEEP_HRS_INPUT:{
                  displayPrintf(DISPLAY_ROW_8, "");
                  displayPrintf(DISPLAY_ROW_9, "");
                  displayPrintf(DISPLAY_ROW_11, "");

                  displayPrintf(DISPLAY_ROW_10, "%u hrs confirmed",sleep_hrs);
                  // set the user input completion flag
                  user_input_confirm = true;
                  //reset sleep_input_state
                  sleep_input_state = state_SLEEP_TIME_INPUT;
                  break;
              }

               default:break;
              }
          }
      }

      break;


    // This event is generated to indicate the completion of a command.
    case sl_bt_evt_gatt_procedure_completed_id:
      if(evt->data.evt_gatt_procedure_completed.result == SL_STATUS_BT_ATT_INSUFFICIENT_ENCRYPTION){
           sl_bt_sm_increase_security(ble_data.connectionHandle);
       }
      break;

    // This event is generated to indicate a request for passkey display and
    // confirmation by the user.
    case sl_bt_evt_sm_confirm_passkey_id:
        //display the passkey on LCD
        displayPrintf(DISPLAY_ROW_PASSKEY, "Passkey %d", evt->data.evt_sm_confirm_passkey.passkey);
        //display the required user action
        displayPrintf(DISPLAY_ROW_ACTION, "Confirm with PB0");
        break;

    case sl_bt_evt_sm_bonded_id:
      //set ble_data.bonded
      ble_data.bonded = true;
      //clear the DISPLAY_ROW_PASSKEY row on LCD
      displayPrintf(DISPLAY_ROW_PASSKEY, "");
      //clear the DISPLAY_ROW_ACTION row on LCD
      displayPrintf(DISPLAY_ROW_ACTION, "");
      //update CONNECTION ROW on LCD
      displayPrintf(DISPLAY_ROW_CONNECTION, "Bonded");

      PB0_time_confirmed = false;
      PB1_hour_confirmed = false;
      sleep_time_confirm = false;
      sleep_hour_confirm = false;
      user_input_confirm = false;

      displayPrintf(DISPLAY_ROW_10, "PB0 to toggle time");

      break;

    case sl_bt_evt_sm_bonding_failed_id:
      //reset ble_data.bonded
      ble_data.bonded = false;
      LOG_ERROR("Device bonding process failed\r\n");
      break;

    case sl_bt_evt_system_soft_timer_id:
      // Prevent charge buildup within the Liquid Crystal Cells.
      displayUpdate();
      break;

    default:
      break;
    } // end - switch

} // handle_ble_event()

#endif


