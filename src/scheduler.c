/**
 * @file scheduler.c
 * @author Shuran Xu (shxu6388@colorado.edu)
 * @brief This file includes the implementation of scheduling-related
 * APIs such as event setting functions and event retrieval.
 * @version 0.2
 * @date 2022-02-26
 *
 * @reference void sl_bt_on_event(sl_bt_msg_t* evt) in  soc_thermometer_client/app.c
 * was studied in detail to construct steps needed to establish the state machine for
 * the client device on A7.
 * @copyright Copyright (c) 2022
 *
 */

#include <math.h>
#include "scheduler.h"
#include "em_core.h"
#include "em_adc.h"
#include "timers.h"
#include "irq.h"
#include "app.h"
#include "gpio.h"
#include "i2c.h"
#include "em_i2c.h"
#include "sl_status.h"
#include "ble.h"
#include "gatt_db.h"
#include "lcd.h"
#include "adc.h"
#include "ble_device_type.h"
#include "circular_buffer.h"

//for debugging only
#define INCLUDE_LOG_DEBUG 1
#include "src/log.h"


/**
 * htm_state_t type variable is used as the state machine variable for
 * the server, and  discover_state_t type variable is used as the state
 * machine variable for the client.
 */
typedef enum {
  state_IDLE = 0,      //!< state_IDLE
  state_TIMEVT_1,      //!< state_TIMEVT_1
  state_I2C_WRITE_COMP,//!< state_I2C_WRITE_COMP
  state_TIMEVT_2,      //!< state_TIMEVT_2
  state_I2C_READ_COMP, //!< state_I2C_READ_COMP
  TOTAL_NUM_STATES     //!< TOTAL_NUM_STATES
}htm_state_t;

typedef enum {
  state_READ_RGB=0,
  state_COMP_LUX,
  state_SLEEP
}light_state_t;

typedef enum {
  state_SINGLE_SCAN = 0,
  state_READ_SOUND_LVL
}sound_state_t;

typedef enum {
  NO_SERVICE=0,
  SOUND_SERVICE,
  LIGHT_SERVICE,
  TEMP_SERVICE
}service_t;


// global flag that activates all services
static service_t sensor_service = NO_SERVICE;

/**
 * Activate all services by enabling the temperature service as a starter
 */
void activate_services()
{
  sensor_service = TEMP_SERVICE;
}

/**
 * @brief This function sets the event bit associated
 * with LETIMER0 UF interrupts.
 */
void schedulerSetEventLE0_UF()
{
  CORE_DECLARE_IRQ_STATE;
  // enter the critical section
  CORE_ENTER_CRITICAL();
  // mask the read-temp event bit
  sl_bt_external_signal(evtLETIMER0_UF);
  // exit the critical section
  CORE_EXIT_CRITICAL();
}

/**
 * @brief This function sets the event bit associated
 * with LETIMER0 COMP1 interrupts.
 */
void schedulerSetEventLE0_COMP1()
{
  CORE_DECLARE_IRQ_STATE;
  // enter the critical section
  CORE_ENTER_CRITICAL();
  // mask the COMP1 bit
  sl_bt_external_signal(evtLETIMER0_COMP1);
  // exit the critical section
  CORE_EXIT_CRITICAL();
}

/**
 * @brief This function sets the event bit associated
 * with ADC0 Transfer Done event.
 */
void schedulerSetEventADC0_TRANDONE()
{
  CORE_DECLARE_IRQ_STATE;
  // enter the critical section
  CORE_ENTER_CRITICAL();
  // mask the COMP1 bit
  sl_bt_external_signal(evtADC0_TRANDONE);
  // exit the critical section
  CORE_EXIT_CRITICAL();
}

/**
 * @brief This function sets the event bit associated
 * with I2C0 Transfer Complete event.
 */
void schedulerSetEventI2C0TranDone()
{
  CORE_DECLARE_IRQ_STATE;
  // enter the critical section
  CORE_ENTER_CRITICAL();
  // mask the I2C0 event bit
  sl_bt_external_signal(evtI2C0_TRANDONE);
  // exit the critical section
  CORE_EXIT_CRITICAL();
}

/**
 * @brief This function sets the event bit associated
 * with I2C0 Transfer NACK event.
 */
void schedulerSetEventI2C0TranNACK()
{
  CORE_DECLARE_IRQ_STATE;
  // enter the critical section
  CORE_ENTER_CRITICAL();
  // mask the I2C0 event bit
  sl_bt_external_signal(evtI2C0_TRANNACK);
  // exit the critical section
  CORE_EXIT_CRITICAL();
}

/**
 * @brief This function sets the event bit associated
 * with GPIO pin PB0 interrupts.
 */
void schedulerSetEventGPIO_PB0()
{
  CORE_DECLARE_IRQ_STATE;
  // enter the critical section
  CORE_ENTER_CRITICAL();
  // mask the PB0 event bit
  sl_bt_external_signal(evtGPIO_PB0);
  // exit the critical section
  CORE_EXIT_CRITICAL();
}

/**
 * @brief This function sets the event bit associated
 * with GPIO pin PB1 interrupts.
 */
void schedulerSetEventGPIO_PB1()
{
  CORE_DECLARE_IRQ_STATE;
  // enter the critical section
  CORE_ENTER_CRITICAL();
  // mask the PB1 event bit
  sl_bt_external_signal(evtGPIO_PB1);
  // exit the critical section
  CORE_EXIT_CRITICAL();
}


// -----------------------------------------------
// Private function, original from Dan Walkes. I fixed a sign extension bug.
// We'll need this for Client A7 assignment to convert health thermometer
// indications back to an integer. Convert IEEE-11073 32-bit float to signed integer.
// -----------------------------------------------
//static int32_t FLOAT_TO_INT32(const uint8_t *value_start_little_endian)
//{
//  uint8_t signByte = 0;
//  int32_t mantissa;
//  // input data format is:
//  // [0] = flags byte
//  // [3][2][1] = mantissa (2's complement)
//  // [4] = exponent (2's complement)
//  // BT value_start_little_endian[0] has the flags byte
//  int8_t exponent = (int8_t)value_start_little_endian[4];
//  // sign extend the mantissa value if the mantissa is negative
//  if (value_start_little_endian[3] & 0x80) { // msb of [3] is the sign of the mantissa
//      signByte = 0xFF;
//  }
//  mantissa = (int32_t) (value_start_little_endian[1] << 0) | \
//  (value_start_little_endian[2] << 8) | \
//  (value_start_little_endian[3] << 16) | \
//  (signByte << 24) ;
//
//  // value = 10^exponent * mantissa, pow() returns a double type
//  return (int32_t) (pow(10, exponent) * mantissa);
//
//} // FLOAT_TO_INT32

void temperature_state_machine(sl_bt_msg_t *evt)
{
  if(sensor_service != TEMP_SERVICE){
      return;
  }

  uint32_t event = 0;
  static htm_state_t next_state = state_IDLE;

  htm_state_t curr_state = next_state;
  event = evt->data.evt_system_external_signal.extsignals;

  switch(curr_state){

    case state_IDLE:{

//        LOG_INFO("Current state = state_IDLE\r\n");
        //delay 1 ms to transition to state_TIMEVT_1
        timeWaitUs_irq(1000);
        next_state = state_TIMEVT_1;

        break;
      }

    case state_TIMEVT_1:{
        next_state = state_TIMEVT_1; //default
//        LOG_INFO("Current state = state_TIMEVT_1\r\n");
        if(event == evtLETIMER0_COMP1) {
            //disable LETIMER0 COMP1 interrupt
            set_LETIMER0COMP1_irq(false);
            //initialize I2C0
            initI2C0_for_I7021();
            //add the power requirement so that EM1 is the minimum power mode
            sl_power_manager_add_em_requirement(SL_POWER_MANAGER_EM1);
            //initiate I2C send command
            I7021_write();
            next_state = state_I2C_WRITE_COMP;
        }
        break;
    }
    case state_I2C_WRITE_COMP:{
      next_state = state_I2C_WRITE_COMP; //default
//      LOG_INFO("Current state = state_I2C_WRITE_COMP\r\n");

      if(event == evtI2C0_TRANDONE){
          //disable I2C interrupt
          NVIC_DisableIRQ(I2C0_IRQn);
          //remove the power requirement
          sl_power_manager_remove_em_requirement(SL_POWER_MANAGER_EM1);
          //wait for I2C to process the command
          timeWaitUs_irq((MAX_TEMP_CONV_TIME_MS * MS_TO_US));
          next_state = state_TIMEVT_2;
      }
      if(event == evtI2C0_TRANNACK){
          //resend the I2C command
          I7021_write();
      }
      break;
    }
    case state_TIMEVT_2:{
      next_state = state_TIMEVT_2; //default
//      LOG_INFO("Current state = state_TIMEVT_2\r\n");
      if(event == evtLETIMER0_COMP1) {
            //disable LETIMER0 COMP1 interrupt
            set_LETIMER0COMP1_irq(false);
            //add the power requirement so that EM1 is the minimum power mode
            sl_power_manager_add_em_requirement(SL_POWER_MANAGER_EM1);
            //initiate I2C send command
            I7021_read();
            next_state = state_I2C_READ_COMP;
        }
        break;
    }
    case state_I2C_READ_COMP:{
      next_state = state_I2C_READ_COMP; //default
//      LOG_INFO("Current state = state_I2C_READ_COMP\r\n");
      if(event == evtI2C0_TRANDONE){
            //disable I2C interrupt
            NVIC_DisableIRQ(I2C0_IRQn);
            //remove the power requirement
            sl_power_manager_remove_em_requirement(SL_POWER_MANAGER_EM1);

            //read the current temperature
            uint32_t temperature_value = get_temperature_data();
            // update the LCD display with temperature data
            displayPrintf(DISPLAY_ROW_TEMPVALUE, "Temp=%d C",temperature_value);

            //reset I2C0
            I2C_Reset(I2C0);
            //set next state to IDLE to jump back the loop
            next_state = state_IDLE;
            //reset service
            sensor_service = LIGHT_SERVICE;
        }

      if(event == evtI2C0_TRANNACK){
          //resend the I2C command
          I7021_read();
      }
        break;
    }
    default:
      break;
  }
}


/**
 * @brief The finite state machine is designed for the ISL29125 sensor.
 * The supported service is the sensor configuration and the light density
 * measurement.
 * @param evt: the pointer of the BT event message.
 */

void light_state_machine(sl_bt_msg_t *evt)
{
  if(sensor_service != LIGHT_SERVICE){
      return;
  }

  uint32_t event = 0;
  static light_state_t next_state = state_READ_RGB;

  light_state_t curr_state = next_state;
  event = evt->data.evt_system_external_signal.extsignals;

  switch(curr_state){

    case state_READ_RGB:{
          next_state = state_COMP_LUX;
          initI2C0_for_ISL29125();
//          LOG_INFO("state = state_READ_RGB\r\n");
          //add the power requirement so that EM1 is the minimum power mode
          sl_power_manager_add_em_requirement(SL_POWER_MANAGER_EM1);
         //read the RGB values from the sensor
         ISL29125_measure_RGB();
        break;
    }
    case state_COMP_LUX:{
        next_state = state_COMP_LUX; //default
//        LOG_INFO("state = state_COMP_LUX\r\n");

        if(event == evtI2C0_TRANDONE){
            //remove the power requirement
            sl_power_manager_remove_em_requirement(SL_POWER_MANAGER_EM1);
            //convert the RGB to XYZ
            ISL29125_transform_RBG_to_XYZ();
            //Calculate the light intensity in units of lux
            uint32_t light_data = calculate_light_density_in_lux();
            // print out the current light density
//            LOG_INFO("Light density = %u lux\r\n", light_data);
            //display the updated light setting
            displayPrintf(DISPLAY_ROW_8, " Light:%d lux", light_data);

            //reset service
            sensor_service = SOUND_SERVICE;
            //reset the state
            next_state = state_READ_RGB;
            //reset I2C0
            I2C_Reset(I2C0);
        }

        else if(event == evtI2C0_TRANNACK){
             //read the RGB values from the sensor again
            LOG_INFO("NACK received, re-run ISL29125_measure_RGB( )\r\n");
            ISL29125_measure_RGB();
          }
        break;
    }

    default:
          break;
  }
}


/**
 * @brief The finite state machine is designed to
 * manipulate the SEN-14262 sensor. The supported service
 * is the temperature measurement.
 * @param evt: the pointer of the BT event message.
 */
void sound_state_machine(sl_bt_msg_t *evt)
{
    if(sensor_service != SOUND_SERVICE){
        return;
    }

    uint32_t event = 0;
    static sound_state_t next_state = state_SINGLE_SCAN;
    sound_state_t curr_state = next_state;
    event = evt->data.evt_system_external_signal.extsignals;

    switch(curr_state){

        case state_SINGLE_SCAN:{
            next_state = state_SINGLE_SCAN; //default
//            LOG_INFO("Current state = state_SINGLE_SCAN\r\n");

             // start ADC to scan
              ADC_Start(ADC0, ADC_CMD_SINGLESTART);
              next_state = state_READ_SOUND_LVL;

            break;
          }
        case state_READ_SOUND_LVL:{
            next_state = state_READ_SOUND_LVL; //default
//            LOG_INFO("Current state = state_READ_SOUND_LVL\r\n");
            if(event == evtADC0_TRANDONE){
              // get and log the ADC data
                uint32_t *adc0_data = getADC0Data();
                // 5000/4096 = 1.221
                uint32_t millivolts = (uint32_t)(*adc0_data * 1.221);
                uint32_t sound_db = ADCmVtodB(millivolts);
//                LOG_INFO("Sound = %d db\r\n", sound_db);
                //display the updated sound setting
                displayPrintf(DISPLAY_ROW_9, "Sound:%d dB", sound_db);

                //reset service
                sensor_service = NO_SERVICE;
                // reset the state
                next_state = state_SINGLE_SCAN;
            }
            break;
        }
        default:
           break;
      }
}


#if DEVICE_IS_BLE_SERVER == 0

/**
 * The states for the client's discovery state machine
 */
typedef enum {
  state_SCAN = 0,                               //!< state_SCAN
  state_CONNECT,                                //!< state_CONNECT
  state_BONDING,                                //!< state_BONDING
  state_DISCOVER_THM_SERVICE,                   //!< state_DISCOVER_THM_SERVICE
  state_DISCOVER_THM_CHARACTERISTIC,            //!< state_DISCOVER_THM_CHARACTERISTIC
  state_ENABLE_THM_INDICATION,                  //!< state_ENABLE_THM_INDICATION
  state_DISCOVER_LIGHT_INTENSITY_SERVICE,       //!< state_DISCOVER_LIGHT_INTENSITY_SERVICE
  state_DISCOVER_LIGHT_INTENSITY_CHARACTERISTIC,//!< state_DISCOVER_LIGHT_INTENSITY_CHARACTERISTIC
  state_ENABLE_LIGHT_INTENSITY_INDICATIONS,     //!< state_ENABLE_LIGHT_INTENSITY_INDICATIONS
  state_DISCOVER_SOUND_SERVICE,                 //!< state_DISCOVER_SOUND_SERVICE
  state_DISCOVER_SOUND_CHRACTERISTIC,           //!< state_DISCOVER_SOUND_CHRACTERISTIC
  state_ENABLE_SOUND_INDICATIONS,               //!< state_ENABLE_SOUND_INDICATIONS
  state_DISCOVER_SLEEP_HOUR_SERVICE,            //!< state_DISCOVER_SLEEP_HOUR_SERVICE
  state_DISCOVER_SLEEP_HOUR_CHARACTERISTIC,     //!< state_DISCOVER_SLEEP_HOUR_CHARACTERISTIC
  state_ENABLE_SLEEP_HOUR_INDICATIONS,          //!< state_ENABLE_SLEEP_HOUR_INDICATIONS
  state_PENDING,                                //!< state_PENDING
  state_RUNNING                                 //!< state_RUNNING
}discover_state_t;

/**
 * The states defined for the inner state machine of the discovery state
 * machine for writing GATT characteristic values to the server
 */
typedef enum {
  state_WRITE_TEMP_VALUE=1,     //!< state_WRITE_TEMP_VALUE
  state_SEND_TEMP_CONFIRM,      //!< state_SEND_TEMP_CONFIRM
  state_WRITE_LIGHT_VALUE,      //!< state_WRITE_LIGHT_VALUE
  state_SEND_LIGHT_CONFIRM,     //!< state_SEND_LIGHT_CONFIRM
  state_WRITE_SOUND_VALUE,      //!< state_WRITE_SOUND_VALUE
  state_SEND_SOUND_CONFIRM,     //!< state_SEND_SOUND_CONFIRM
  state_WRITE_SLEEP_HOURS_VALUE,//!< state_WRITE_SLEEP_HOURS_VALUE
  state_SEND_SLEEP_HOURS_CONFIRM//!< state_SEND_SLEEP_HOURS_CONFIRM
}write_data_state_t;


// Health Thermometer service UUID defined by Bluetooth SIG
static uint8_t thermo_service_uuid[2] = { 0x09, 0x18 };
// Temperature Measurement characteristic UUID defined by Bluetooth SIG
static uint8_t thermo_char_uuid[2] = { 0x1c, 0x2a };
//Light sensor UUIDs
static uint8_t light_sensor_service_UUID[16]     = {0x7f, 0xf3, 0x8f, 0xfb, 0x7f, 0x5a, 0xcd, 0xb3, 0xff, 0x45, 0xfe, 0x0c, 0x41, 0xb8, 0x3b, 0x10};
static uint8_t light_sensor_char_UUID[16]          = {0x12, 0x2b, 0xbb, 0x8f, 0x32, 0x4d, 0x9c, 0xa8, 0x94, 0x40, 0x47, 0xc9, 0xd0, 0x5c, 0x71, 0x85};
//Sound sensor UUIDs
static uint8_t sound_sensor_service_UUID[16]  = {0x51, 0x35, 0x0c, 0x95, 0x5f, 0xa7, 0x8c, 0x93, 0x3d, 0x4a, 0x73, 0x38, 0x26, 0xe8, 0xe5, 0xc3};
static uint8_t sound_sensor_char_UUID[16]       = {0x29, 0x8b, 0x48, 0xbd, 0x6f, 0xb8, 0x00, 0xa6, 0xe8, 0x43, 0x95, 0x50, 0xf8, 0x55, 0xdb, 0x9b};
//Sleep hours
static uint8_t sleep_hours_service_UUID[16]     = {0x58, 0xbb, 0x62, 0xfa, 0x6b, 0x1f, 0x4e, 0xa1, 0x3a, 0x47, 0x7f, 0x23, 0x5d, 0xbe,  0x11, 0xfe};
static uint8_t sleep_hours_char_UUID[16]         =  {0x82, 0x9a, 0x16, 0x45, 0x50, 0xd4, 0xef, 0xbc, 0x2a, 0x4b, 0x8a, 0xf0, 0x4e, 0xa9, 0x99, 0xb9};

// global flag to indicate the current connection is closed
static bool client_conn_closed = false;
// global flag to signal the BLE stack to handle the user sleep inputs
static bool ble_process_sleep_values = false;

/**
 * Get the flag that triggers the BLE stack to handle the sleep hours
 */
bool ble_handle_sleep_values()
{
  return ble_process_sleep_values;
}

/**
 * Reset the flag that triggers the BLE stack to handle the sleep hours
 */
void reset_ble_sleep_handling()
{
  ble_process_sleep_values = false;
}

/**
 * @brief Getter function for the thermometer service UUID
 * @return the address of thermo_service_uuid
 */
uint8_t *get_thermo_service_uuid()
{
  return &thermo_service_uuid[0];
}
/**
 * @brief Getter function for the thermometer characteristic UUID
 * @return the address of thermo_char_uuid
 */
uint8_t *get_thermo_char_uuid()
{
  return &thermo_char_uuid[0];
}

/**
 * Obtain the UUID of the light service
 * @return the address of the light_sensor_service_UUID
 */
uint8_t *get_light_service_uuid()
{
  return &light_sensor_service_UUID[0];
}

/**
 * Obtain the UUID of the light characteristic
 * @return the address of the light_sensor_char_UUID
 */
uint8_t *get_light_char_uuid()
{
  return &light_sensor_char_UUID[0];
}

/**
 * Obtain the UUID of the sound service
 * @return the address of the sound_sensor_service_UUID
 */
uint8_t *get_sound_service_uuid()
{
  return &sound_sensor_service_UUID[0];
}

/**
 * Obtain the UUID of the sound characteristic
 * @return the address of the sound_sensor_char_UUID
 */
uint8_t *get_sound_char_uuid()
{
  return &sound_sensor_char_UUID[0];
}

/**
 * Obtain the UUID of the sleep hours service
 * @return the address of the sleep_hours_service_UUID
 */
uint8_t *get_sleep_hours_service_uuid()
{
  return &sleep_hours_service_UUID[0];
}

/**
 * Obtain the UUID of the sleep hours characteristic
 * @return the address of the sleep_hours_char_UUID
 */
uint8_t *get_sleep_hours_char_uuid()
{
  return &sleep_hours_char_UUID[0];
}

//declare the inner state machine function called in discovery_state_machine().
static void server_update_state_machine(sl_bt_msg_t *evt);

/**
 * @brief The finite state machine is designed for the master device to
 * discover primary services on the slave device, and listen for
 * the sensor temperature values from the slave device.
 * @param evt: the pointer of the BT event message.
 */
void discovery_state_machine(sl_bt_msg_t *evt)
{
    int sc;
    discover_state_t curr_state;
    static discover_state_t next_state = state_SCAN;

    //BLE private data
    conn_properties_t *bleDataPtr = getBleDataPtr();
    uint32_t event = SL_BT_MSG_ID(evt->header);

    curr_state = next_state;

     switch(curr_state){

       case state_SCAN:{
         LOG_INFO("Current state = state_SCAN\r\n");
         next_state = state_SCAN;

         if(client_conn_closed){

             // display Discovering on the LCD
             displayPrintf(DISPLAY_ROW_CONNECTION, "Discovering");
             // Display the DISPLAY_ROW_BTADDR2 row with nothing
             displayPrintf(DISPLAY_ROW_BTADDR2, "");
             //update the LCD temperature display row with nothing
             displayPrintf(DISPLAY_ROW_TEMPVALUE, "");
             //update the LCD Button display row with nothing
             displayPrintf(DISPLAY_ROW_9, "");

            // Start scanning - looking for thermometer devices
            sc = sl_bt_scanner_start(sl_bt_gap_1m_phy, sl_bt_scanner_discover_generic);
            if(sc != SL_STATUS_OK){
                LOG_ERROR("Failed to start the scanner\r\n");
                break;
            }

            // loop back to the SCAN state until the scan report event is received
            next_state = state_SCAN;
            //reset client_conn_closed
            client_conn_closed = false;
            break;
         }

         if(event == sl_bt_evt_scanner_scan_report_id){

             // Parse advertisement packets
             if (evt->data.evt_scanner_scan_report.packet_type == 0) {

                 const bd_addr server_bt_addr = SERVER_BT_ADDRESS;
                 if(memcmp(&(evt->data.evt_scanner_scan_report.address.addr), &(server_bt_addr.addr), \
                           sizeof(server_bt_addr.addr)) == 0){

                     // then stop scanning
                     sc = sl_bt_scanner_stop();
                     if(sc != SL_STATUS_OK){
                          LOG_ERROR("Failed to stop the scanner\r\n");
                          break;
                      }
                     // connect to the slave device
                     sc = sl_bt_connection_open(evt->data.evt_scanner_scan_report.address,
                                                evt->data.evt_scanner_scan_report.address_type,
                                                sl_bt_gap_1m_phy,
                                                NULL);
                     if(sc != SL_STATUS_OK){
                         LOG_ERROR("Failed to connect to the slave\r\n");
                         break;
                     }
                     next_state = state_CONNECT;
                 }
              }
         }
         break;
       }

       case state_CONNECT:{

         LOG_INFO("Current state = state_CONNECT\r\n");
         next_state = state_CONNECT;

         if(event == sl_bt_evt_connection_opened_id){
              int index = 0;
              int i;

              // Set the remote device (i.e. server) device
              bleDataPtr->server_address = SERVER_BT_ADDRESS;
              bleDataPtr->server_address_type = evt->data.evt_connection_opened.address_type;

              for(i=0;i<6;i++){
                  sprintf(&bleDataPtr->display_bt_addr2[index], "%X", \
                          bleDataPtr->server_address.addr[i]);
                  index += 2;
                  if(i < 5)
                    bleDataPtr->display_bt_addr2[index] = ':';
                  index++;
               }
              // display Connected as the current connection state
              displayPrintf(DISPLAY_ROW_CONNECTION, "Connected");
              // Display the actual address of the server device
              displayPrintf(DISPLAY_ROW_BTADDR2, "%s", bleDataPtr->display_bt_addr2);

              next_state = state_BONDING;
         }
         break;
       }

       case state_BONDING:{
         LOG_INFO("Current state = state_BONDING\r\n");
         next_state = state_BONDING;
         if(bleDataPtr->bonded){
             next_state = state_DISCOVER_THM_SERVICE;
         }
         break;
       }

       case state_DISCOVER_THM_SERVICE:{

         // Update LCD display to start device setup
         displayPrintf(DISPLAY_ROW_ACTION, "Initializing Device");

         LOG_INFO("Current state = state_DISCOVER_THM_SERVICES\r\n");

         // Discover Health Thermometer service on the responder device
         sc = sl_bt_gatt_discover_primary_services_by_uuid(bleDataPtr->connectionHandle,
                                                           sizeof(thermo_service_uuid),
                                                           (const uint8_t*)thermo_service_uuid);
         if(sc != SL_STATUS_OK){
              LOG_ERROR("Failed to discover the thermometer services\r\n");
              break;
          }

         next_state = state_DISCOVER_THM_CHARACTERISTIC;
         break;
       }

       case state_DISCOVER_THM_CHARACTERISTIC:{

         LOG_INFO("Current state = state_DISCOVER_THM_CHARACTERISTIC\r\n");
         next_state = state_DISCOVER_THM_CHARACTERISTIC;

         if(event == sl_bt_evt_gatt_procedure_completed_id){
           // Discover thermometer characteristic on the responder device
           sc = sl_bt_gatt_discover_characteristics_by_uuid(evt->data.evt_gatt_procedure_completed.connection,
                                                            bleDataPtr->thermometer_service_handle,
                                                            sizeof(thermo_char_uuid),
                                                            (const uint8_t*)thermo_char_uuid);
           if(sc != SL_STATUS_OK){
               LOG_ERROR("Failed to discover the thermometer characteristic, rc = 0x%x\r\n", sc);
               break;
           }
           next_state = state_ENABLE_THM_INDICATION;
         }

         break;
       }

       case state_ENABLE_THM_INDICATION:{

         LOG_INFO("Current state = state_ENABLE_THM_INDICATION\r\n");
         next_state = state_ENABLE_THM_INDICATION;
         if(event == sl_bt_evt_gatt_procedure_completed_id){
                // enable indications
                sc = sl_bt_gatt_set_characteristic_notification(evt->data.evt_gatt_procedure_completed.connection,
                                                                bleDataPtr->thermometer_characteristic_handle,
                                                                sl_bt_gatt_indication);
                if(sc != SL_STATUS_OK){
                     LOG_ERROR("Failed to enable the thermometer indication, rc = 0x%x\r\n", sc);
                     break;
                 }
                 next_state = state_DISCOVER_LIGHT_INTENSITY_SERVICE;
          }

         break;
       }

       case state_DISCOVER_LIGHT_INTENSITY_SERVICE:{

          LOG_INFO("Current state = state_DISCOVER_LIGHT_INTENSITY_SERVICE\r\n");
          next_state = state_DISCOVER_LIGHT_INTENSITY_SERVICE;

          if(event == sl_bt_evt_gatt_procedure_completed_id){
              sc = sl_bt_gatt_discover_primary_services_by_uuid (
                  evt->data.evt_gatt_procedure_completed.connection,
                  sizeof(light_sensor_service_UUID),
                  (const uint8_t*) light_sensor_service_UUID);

              if (sc != SL_STATUS_OK)
              {
                LOG_ERROR("Failed to discover the light intensity services, rc = 0x%x\r\n", sc);
                break;
              }

              next_state = state_DISCOVER_LIGHT_INTENSITY_CHARACTERISTIC;
          }

          break;
      }

       case state_DISCOVER_LIGHT_INTENSITY_CHARACTERISTIC:{

          LOG_INFO("Current state = state_DISCOVER_LIGHT_INTENSITY_CHARACTERISTIC\r\n");
          if (event == sl_bt_evt_gatt_procedure_completed_id)
          {
              // Discover thermometer characteristic on the responder device
              sc = sl_bt_gatt_discover_characteristics_by_uuid (
                  evt->data.evt_gatt_procedure_completed.connection,
                  bleDataPtr->light_sensor_service_handle,
                  sizeof(light_sensor_char_UUID),
                  (const uint8_t*) light_sensor_char_UUID);

              if (sc != SL_STATUS_OK)
              {
                LOG_ERROR(
                    "Failed to discover the light sensor characteristic, rc = 0x%x\r\n",
                    sc);
                break;
              }
              next_state = state_ENABLE_LIGHT_INTENSITY_INDICATIONS;
            }
          break;
      }

       case state_ENABLE_LIGHT_INTENSITY_INDICATIONS:{

          LOG_INFO("Current state = state_ENABLE_LIGHT_INTENSITY_INDICATIONS\r\n");
          if (event == sl_bt_evt_gatt_procedure_completed_id)
          {
            sc = sl_bt_gatt_set_characteristic_notification (
                evt->data.evt_gatt_procedure_completed.connection,
                bleDataPtr->light_sensor_characteristic_handle,
                sl_bt_gatt_indication);

            if (sc != SL_STATUS_OK)
            {
              LOG_ERROR("Failed to enable the light sensor indication, rc = 0x%x\r\n",sc);
              break;
            }

            next_state = state_DISCOVER_SOUND_SERVICE;
          }
          break;
      }

       case state_DISCOVER_SOUND_SERVICE:{

          LOG_INFO("Current state = state_DISCOVER_SOUND_SERVICE\r\n");
          next_state = state_DISCOVER_SOUND_SERVICE;

          if (event == sl_bt_evt_gatt_procedure_completed_id)
          {
            sc = sl_bt_gatt_discover_primary_services_by_uuid (
                evt->data.evt_gatt_procedure_completed.connection,
                sizeof(sound_sensor_service_UUID),
                (const uint8_t*) sound_sensor_service_UUID);

            if (sc != SL_STATUS_OK)
            {
              LOG_ERROR("Failed to discover the thermometer services\r\n");
              break;
            }
            next_state = state_DISCOVER_SOUND_CHRACTERISTIC;
          }
          break;
      }

       case state_DISCOVER_SOUND_CHRACTERISTIC:{

          LOG_INFO("Current state = state_DISCOVER_SOUND_CHRACTERISTIC\r\n");
          next_state = state_DISCOVER_SOUND_CHRACTERISTIC;

          if (event == sl_bt_evt_gatt_procedure_completed_id)
          {
            // Discover thermometer characteristic on the responder device
            sc = sl_bt_gatt_discover_characteristics_by_uuid (
                evt->data.evt_gatt_procedure_completed.connection,
                bleDataPtr->sound_sensor_service_handle,
                sizeof(sound_sensor_char_UUID),
                (const uint8_t*) sound_sensor_char_UUID);

            if (sc != SL_STATUS_OK)
            {
              LOG_ERROR("Failed to discover the thermometer characteristic, rc = 0x%x\r\n",sc);
              break;
            }

            next_state = state_ENABLE_SOUND_INDICATIONS;
          }
          break;
        }

       case state_ENABLE_SOUND_INDICATIONS:{

          LOG_INFO("Current state = state_ENABLE_SOUND_INDICATIONS\r\n");
          next_state = state_ENABLE_SOUND_INDICATIONS;

          if (event == sl_bt_evt_gatt_procedure_completed_id)
          {
            sc = sl_bt_gatt_set_characteristic_notification (
                evt->data.evt_gatt_procedure_completed.connection,
                bleDataPtr->sound_sensor_characteristic_handle,
                sl_bt_gatt_indication);

            if (sc != SL_STATUS_OK)
            {
              LOG_ERROR("Failed to enable the sound sensor indication, rc = 0x%x\r\n",sc);
              break;
            }

            next_state = state_DISCOVER_SLEEP_HOUR_SERVICE;
          }
          break;
         }

       case state_DISCOVER_SLEEP_HOUR_SERVICE:{

         LOG_INFO("Current state = state_DISCOVER_SLEEP_HOUR_SERVICE\r\n");
         next_state = state_DISCOVER_SLEEP_HOUR_SERVICE;

         if (event == sl_bt_evt_gatt_procedure_completed_id)
         {
           sc = sl_bt_gatt_discover_primary_services_by_uuid (
               evt->data.evt_gatt_procedure_completed.connection,
               sizeof(sleep_hours_service_UUID),
               (const uint8_t*) sleep_hours_service_UUID);

           if (sc != SL_STATUS_OK)
           {
             LOG_ERROR("Failed to discover the sleep hour services\r\n");
             break;
           }
           next_state = state_DISCOVER_SLEEP_HOUR_CHARACTERISTIC;
         }
           break;
       }

       case state_DISCOVER_SLEEP_HOUR_CHARACTERISTIC:{

          LOG_INFO("Current state = state_DISCOVER_SLEEP_HOUR_CHARACTERISTIC\r\n");
          next_state = state_DISCOVER_SLEEP_HOUR_CHARACTERISTIC;

          if (event == sl_bt_evt_gatt_procedure_completed_id)
          {
            // Discover thermometer characteristic on the responder device
            sc = sl_bt_gatt_discover_characteristics_by_uuid (
                evt->data.evt_gatt_procedure_completed.connection,
                bleDataPtr->sleep_hours_service_handle,
                sizeof(sleep_hours_char_UUID),
                (const uint8_t*) sleep_hours_char_UUID);

            if (sc != SL_STATUS_OK)
              {
                LOG_ERROR("Failed to discover the thermometer characteristic, rc = 0x%x\r\n",sc);
                break;
              }

            next_state = state_ENABLE_SLEEP_HOUR_INDICATIONS;
          }

          break;
       }

       case state_ENABLE_SLEEP_HOUR_INDICATIONS:{

         LOG_INFO("Current state = state_ENABLE_SLEEP_HOUR_INDICATIONS\r\n");
         next_state = state_ENABLE_SLEEP_HOUR_INDICATIONS;

         if (event == sl_bt_evt_gatt_procedure_completed_id)
         {
           sc = sl_bt_gatt_set_characteristic_notification (
               evt->data.evt_gatt_procedure_completed.connection,
               bleDataPtr->sleep_hours_characteristic_handle,
               sl_bt_gatt_indication);

           if (sc != SL_STATUS_OK)
           {
             LOG_ERROR("Failed to enable the sleep_hours indication, rc = 0x%x\r\n",sc);
             break;
           }

           next_state = state_PENDING;
         }
         break;
       }

       case state_PENDING:{

           LOG_INFO("Current state = state_PENDING\r\n");
           next_state = state_PENDING;

           if (event == sl_bt_evt_gatt_procedure_completed_id)
           {
               // Update LCD display to indicate device is active
               displayPrintf(DISPLAY_ROW_ACTION, "Device Active");
               next_state = state_RUNNING;
           }
           break;
         }

       case state_RUNNING:{

         LOG_INFO("Current state = state_RUNNING\r\n");
         next_state = state_RUNNING;

         if(user_input_status()){
             //run the inner state machine to write data to the server
             server_update_state_machine(evt);
             //signal the BLE stack to process the sleep value inputs
             ble_process_sleep_values = true;
             return;
         }

         switch(event){

           case sl_bt_evt_connection_closed_id:
             {
              LOG_INFO("Connection closed\r\n");
              client_conn_closed = true;
              next_state = state_SCAN;
              break;
             }
           default:break;
          }
         break;
        }

       default:
          break;
       }
  }

/**
 * The state machine that writes GATT characteristic values to the server device
 * @param evt: the message event pointer
 */
static void server_update_state_machine(sl_bt_msg_t *evt)
{
  int sc;
  static write_data_state_t next_state = state_WRITE_TEMP_VALUE;
  write_data_state_t curr_state;
  //BLE private data
  conn_properties_t *bleDataPtr = getBleDataPtr();

  curr_state = next_state;

   switch(curr_state){

     case state_WRITE_TEMP_VALUE:{

       LOG_INFO("Current state = state_WRITE_TEMP_VALUE\r\n");
       sc = sl_bt_gatt_write_characteristic_value (
           bleDataPtr->connectionHandle,
           bleDataPtr->thermometer_characteristic_handle,
           sizeof(*get_CAL_temperature ()), (uint8_t *)get_CAL_temperature ());

       if (sc != SL_STATUS_OK)
       {
         LOG_ERROR("Failed to write temperature value to GATT server, rc = 0x%x\r\n",sc);
         break;
       }
       next_state = state_SEND_TEMP_CONFIRM;
       break;
     }
     case state_SEND_TEMP_CONFIRM:{

       LOG_INFO("Current state = state_SEND_TEMP_CONFIRM\r\n");
       next_state = state_SEND_TEMP_CONFIRM;
       // This event is generated when exactly an indication was received
       if((SL_BT_MSG_ID(evt->header) == sl_bt_evt_gatt_characteristic_value_id) && \
           (evt->data.evt_gatt_characteristic_value.att_opcode == sl_bt_gatt_handle_value_indication)){

           if(evt->data.evt_gatt_characteristic_value.characteristic == gattdb_temperature_measurement){
                // Send confirmation for the indication
                sc = sl_bt_gatt_send_characteristic_confirmation(bleDataPtr->connectionHandle);
                if(sc != SL_STATUS_OK){
                    LOG_ERROR("Failed to send the confirmation for the temperature measurement service\r\n");
                    break;
                }
                next_state = state_WRITE_LIGHT_VALUE;
            }
       }
       break;
     }
     case state_WRITE_LIGHT_VALUE:{

       LOG_INFO("Current state = state_WRITE_LIGHT_VALUE\r\n");
       //Writing lux value
       sc = sl_bt_gatt_write_characteristic_value (
           bleDataPtr->connectionHandle,
           bleDataPtr->light_sensor_characteristic_handle,
           sizeof(*get_CAL_lux_level ()), (uint8_t *)get_CAL_lux_level ());

       if (sc != SL_STATUS_OK)
         {
           LOG_ERROR("Failed to write sleep hour value to GATT server, rc = 0x%x\r\n",sc);
           break;
         }
       next_state = state_SEND_LIGHT_CONFIRM;
       break;
     }
     case state_SEND_LIGHT_CONFIRM:{

       LOG_INFO("Current state = state_SEND_LIGHT_CONFIRM\r\n");
        next_state = state_SEND_LIGHT_CONFIRM;
        // This event is generated when exactly an indication was received
        if((SL_BT_MSG_ID(evt->header) == sl_bt_evt_gatt_characteristic_value_id) && \
            (evt->data.evt_gatt_characteristic_value.att_opcode == sl_bt_gatt_handle_value_indication)){

            if(evt->data.evt_gatt_characteristic_value.characteristic == gattdb_light_measurement){
                 // Send confirmation for the indication
                 sc = sl_bt_gatt_send_characteristic_confirmation(bleDataPtr->connectionHandle);
                 if(sc != SL_STATUS_OK){
                     LOG_ERROR("Failed to send the confirmation for the light measurement service\r\n");
                     break;
                 }
                 next_state = state_WRITE_SOUND_VALUE;
             }
        }
        break;
     }
     case state_WRITE_SOUND_VALUE:{

       LOG_INFO("Current state = state_WRITE_SOUND_VALUE\r\n");
       //writing sound value
       sc = sl_bt_gatt_write_characteristic_value (
           bleDataPtr->connectionHandle,
           bleDataPtr->sound_sensor_characteristic_handle,
           sizeof(*get_CAL_sound_level ()), (uint8_t *)get_CAL_sound_level ());

       if (sc != SL_STATUS_OK)
         {
           LOG_ERROR("Failed to write sound sensor value to GATT server, rc = 0x%x\r\n",sc);
           break;
         }
       next_state = state_SEND_SOUND_CONFIRM;
       break;
     }
     case state_SEND_SOUND_CONFIRM:{

       LOG_INFO("Current state = state_SEND_SOUND_CONFIRM\r\n");
       next_state = state_SEND_SOUND_CONFIRM;
       // This event is generated when exactly an indication was received
       if((SL_BT_MSG_ID(evt->header) == sl_bt_evt_gatt_characteristic_value_id) && \
           (evt->data.evt_gatt_characteristic_value.att_opcode == sl_bt_gatt_handle_value_indication)){

           if(evt->data.evt_gatt_characteristic_value.characteristic == gattdb_sound_measurement){
                // Send confirmation for the indication
                sc = sl_bt_gatt_send_characteristic_confirmation(bleDataPtr->connectionHandle);
                if(sc != SL_STATUS_OK){
                    LOG_ERROR("Failed to send the confirmation for the sound measurement service\r\n");
                    break;
                }
                next_state = state_WRITE_SLEEP_HOURS_VALUE;
            }
       }
       break;
     }
     case state_WRITE_SLEEP_HOURS_VALUE:{

       LOG_INFO("Current state = state_WRITE_SLEEP_HOURS_VALUE\r\n");

       //Writing sleep hour value
        sc = sl_bt_gatt_write_characteristic_value (
            bleDataPtr->connectionHandle,
            bleDataPtr->sleep_hours_characteristic_handle,
            sizeof(*get_sleep_hrs ()), (uint8_t *)get_sleep_hrs ());

        if (sc != SL_STATUS_OK)
          {
            LOG_ERROR("Failed to write sleep hour value to GATT server, rc = 0x%x\r\n",sc);
            break;
          }
        next_state = state_SEND_SLEEP_HOURS_CONFIRM;
        break;
     }
     case state_SEND_SLEEP_HOURS_CONFIRM:{

       LOG_INFO("Current state = state_SEND_SLEEP_HOURS_CONFIRM\r\n");
       next_state = state_SEND_SLEEP_HOURS_CONFIRM;
       // This event is generated when exactly an indication was received
       if((SL_BT_MSG_ID(evt->header) == sl_bt_evt_gatt_characteristic_value_id) && \
           (evt->data.evt_gatt_characteristic_value.att_opcode == sl_bt_gatt_handle_value_indication)){

            if(evt->data.evt_gatt_characteristic_value.characteristic == gattdb_sleep_hours){
                 // Send confirmation for the indication
                 sc = sl_bt_gatt_send_characteristic_confirmation(bleDataPtr->connectionHandle);
                 if(sc != SL_STATUS_OK){
                     LOG_ERROR("Failed to send the confirmation for the sleep hours service\r\n");
                     break;
                 }

                 //reset next_state
                 next_state = state_WRITE_TEMP_VALUE;
                 //exit the state machine
                 return;
             }
        }
        break;
     }
   }
}

#endif
