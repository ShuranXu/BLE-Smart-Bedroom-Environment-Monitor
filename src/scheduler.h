/**
 * @file scheduler.h
 * @author Shuran Xu (shxu6388@colorado.edu)
 * @brief This header file contains public APIs pertaining event settings to
 * be scheduler at LETIMER0 interrupt, based on the interrupt flags triggered.
 * In addition, custom enum type is defined as event types.
 *
 * Updates: I2C event handling functions are added, the temperature state
 * machine is modified to access BT event message rather than the custom-defined
 * event enum type. getNextEvent() is removed as it is no longer used.
 * @version 0.1
 * @date 2022-02-01
 *
 * @copyright Copyright (c) 2022
 *
 */

#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

#include <stdint.h>
#include "sl_bt_api.h"

/**
 * Enum type defining the external signal events triggered by BLE stack
 */
typedef enum {
  evtI2C0_TRANDONE = 1,//!< evtI2C0_TRANDONE
  evtI2C0_TRANNACK,    //!< evtI2C0_TRANNACK
  evtADC0_TRANDONE,    //!< evtADC0_TRANDONE
  evtLETIMER0_UF,      //!< evtLETIMER0_UF
  evtLETIMER0_COMP1,   //!< evtLETIMER0_COMP1
  evtGPIO_PB0,         //!< evtGPIO_PB0
  evtGPIO_PB1          //!< evtGPIO_PB1

}evt_t;


/**
 * Activate all services by enabling the temperature service as a starter
 */
void activate_services();

/**
 * @brief This function sets the event bit associated
 * with LETIMER0 UF interrupts.
 */
void schedulerSetEventLE0_UF();

/**
 * @brief This function sets the event bit associated
 * with LETIMER0 COMP1 interrupts.
 */
void schedulerSetEventLE0_COMP1();

/**
 * @brief This function sets the event bit associated
 * with I2C0 Transfer Complete event.
 */
void schedulerSetEventI2C0TranDone();

/**
 * @brief This function sets the event bit associated
 * with I2C0 Transfer NACK event.
 */
void schedulerSetEventI2C0TranNACK();


/**
 * @brief This function sets the event bit associated
 * with ADC0 Transfer Done event.
 */
void schedulerSetEventADC0_TRANDONE();

/**
 * @brief This function sets the event bit associated
 * with GPIO pin PB0 interrupts.
 */
void schedulerSetEventGPIO_PB0();

/**
 * @brief This function sets the event bit associated
 * with GPIO pin PB1 interrupts.
 */
void schedulerSetEventGPIO_PB1();

/**
 * @brief The finite state machine is designed to
 * manipulate the Si7021sensor. The supported service
 * is the temperature measurement.
 * @param evt: the pointer of the BT event message.
 */
void temperature_state_machine(sl_bt_msg_t *evt);

/**
 * @brief The finite state machine is designed for the ISL29125 sensor.
 * The supported service is the sensor configuration and the light density
 * measurement.
 * @param evt: the pointer of the BT event message.
 */
void light_state_machine(sl_bt_msg_t *evt);

/**
 * @brief The finite state machine is designed to
 * manipulate the SEN-14262 sensor. The supported service
 * is the temperature measurement.
 * @param evt: the pointer of the BT event message.
 */
void sound_state_machine(sl_bt_msg_t *evt);

/**
 * @brief The finite state machine is designed for the master device to
 * discover primary services on the slave device, and listen for
 * the sensor temperature values from the slave device.
 * @param evt: the pointer of the BT event message.
 */
void discovery_state_machine(sl_bt_msg_t *evt);


/**
 * @brief Getter function for the thermometer service UUID
 * @return the address of thermo_service_uuid
 */
uint8_t *get_thermo_service_uuid();

/**
 * @brief Getter function for the thermometer characteristic UUID
 * @return the address of thermo_char_uuid
 */
uint8_t *get_thermo_char_uuid();
/**
 * Obtain the UUID of the light service
 * @return the address of the light_sensor_service_UUID
 */
uint8_t *get_light_service_uuid();
/**
 * Obtain the UUID of the light characteristic
 * @return the address of the light_sensor_char_UUID
 */
uint8_t *get_light_char_uuid();
/**
 * Obtain the UUID of the sound service
 * @return the address of the sound_sensor_service_UUID
 */
uint8_t *get_sound_service_uuid();
/**
 * Obtain the UUID of the sound characteristic
 * @return the address of the sound_sensor_char_UUID
 */
uint8_t *get_sound_char_uuid();
/**
 * Obtain the UUID of the sleep hours service
 * @return the address of the sleep_hours_service_UUID
 */
uint8_t *get_sleep_hours_service_uuid();
/**
 * Obtain the UUID of the sleep hours characteristic
 * @return the address of the sleep_hours_char_UUID
 */
uint8_t *get_sleep_hours_char_uuid();
/**
 * Get the flag that triggers the BLE stack to handle the sleep hours
 */
bool ble_handle_sleep_values();
/**
 * Reset the flag that triggers the BLE stack to handle the sleep hours
 */
void reset_ble_sleep_handling();

#endif // __SCHEDULER_H__

