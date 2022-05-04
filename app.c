/***************************************************************************//**
 * @file
 * @brief Core application logic.
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 * Date:        08-07-2021
 * Author:      Dave Sluiter
 * Description: This code was created by the Silicon Labs application wizard
 *              and started as "Bluetooth - SoC Empty".
 *              It is to be used only for ECEN 5823 "IoT Embedded Firmware".
 *              The MSLA referenced above is in effect.
 *
 ******************************************************************************/


// *************************************************
// Students: It is OK to modify this file.
//           Make edits appropriate for each
//           assignment.
// *************************************************

#include "em_common.h"
#include "em_i2c.h"
#include "app_assert.h" // got messages that sl_app_assert() is deprecated and should switch to all_assert()

#include "sl_bluetooth.h"
#include "gatt_db.h"

#include "sl_status.h" // for sl_status_print()

#include "src/ble_device_type.h"
#include "src/lcd.h"
#include "src/circular_buffer.h"



// See: https://docs.silabs.com/gecko-platform/latest/service/power_manager/overview
#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)

// -----------------------------------------------------------------------------
// defines for power manager callbacks
// -----------------------------------------------------------------------------
// Return values for app_is_ok_to_sleep():
//   Return false to keep sl_power_manager_sleep() from sleeping the MCU.
//   Return true to allow system to sleep when you expect/want an IRQ to wake
//   up the MCU from the call to sl_power_manager_sleep() in the main while (1)
//   loop.

#if (LOWEST_ENERGY_MODE < SL_POWER_MANAGER_EM1)
#define APP_IS_OK_TO_SLEEP      (false)
#else
#define APP_IS_OK_TO_SLEEP      (true)
#endif

// Return values for app_sleep_on_isr_exit():
//   SL_POWER_MANAGER_IGNORE; // The module did not trigger an ISR and it doesn't want to contribute to the decision
//   SL_POWER_MANAGER_SLEEP;  // The module was the one that caused the system wakeup and the system SHOULD go back to sleep
//   SL_POWER_MANAGER_WAKEUP; // The module was the one that caused the system wakeup and the system MUST NOT go back to sleep
//
// Notes:
//       SL_POWER_MANAGER_IGNORE, we see calls to app_process_action() on each IRQ. This is the
//       expected "normal" behavior.
//
//       SL_POWER_MANAGER_SLEEP, the function app_process_action()
//       in the main while(1) loop will not be called! It would seem that sl_power_manager_sleep()
//       does not return in this case.
//
//       SL_POWER_MANAGER_WAKEUP, doesn't seem to allow ISRs to run. Main while loop is
//       running continuously, flooding the VCOM port with printf text with LETIMER0 IRQs
//       disabled somehow, LED0 is not flashing.

#define APP_SLEEP_ON_ISR_EXIT   (SL_POWER_MANAGER_IGNORE)
//#define APP_SLEEP_ON_ISR_EXIT   (SL_POWER_MANAGER_SLEEP)
//#define APP_SLEEP_ON_ISR_EXIT   (SL_POWER_MANAGER_WAKEUP)

#endif // defined(SL_CATALOG_POWER_MANAGER_PRESENT)


#include "app.h"



// Students: Here is an example of how to correctly include logging functions in
//           each .c file.
//           Apply this technique to your other .c files.
//           Do not #include "src/log.h" in any .h file! This logging scheme is
//           designed to be included at the top of each .c file that you want
//           to call one of the LOG_***() functions from.

// Include logging specifically for this .c file
//#define INCLUDE_LOG_DEBUG 1
#include "src/log.h"

#include "src/timers.h"
#include "src/gpio.h"
#include "src/scheduler.h"
#include "src/i2c.h"
#include "src/ble.h"
#include "src/adc.h"

/*****************************************************************************
 * Application Power Manager callbacks
 *****************************************************************************/
#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)



bool app_is_ok_to_sleep(void)
{
  return APP_IS_OK_TO_SLEEP;

} // app_is_ok_to_sleep()



sl_power_manager_on_isr_exit_t app_sleep_on_isr_exit(void)
{
  return APP_SLEEP_ON_ISR_EXIT;

} // app_sleep_on_isr_exit()



#endif // defined(SL_CATALOG_POWER_MANAGER_PRESENT)


//#define TEST_WAITTIME_US


/**************************************************************************//**
 * Application Init.
 *****************************************************************************/
SL_WEAK void app_init(void)
{
  // Put your application 1-time initialization code here.
  // This is called once during start-up.
  // Don't call any Bluetooth API functions until after the boot event.

  //initialize letimer0
  initLETIMER0(LETIMER_PERIOD_MS, LETIMER_ON_TIME_MS, LOWEST_ENERGY_MODE);
  //initialize GPIO pins
  gpioInit();
  //power up the temperature sensor
  pwUpSi7021();
  //initialize ADC0
  initADC0();
  //initialize I2C0
  initI2C0_for_ISL29125();
  //reset the sensor
  reset_ISL29125();
  //configure the sensor
  configure_ISL29125();
  //reset I2C0
  I2C_Reset(I2C0);
  //clear the circular buffer
  clear_queue();

   //Add power requirement to run if the energy mode is EM1 or EM2
#if defined(LOWEST_ENERGY_MODE)
  if((LOWEST_ENERGY_MODE == SL_POWER_MANAGER_EM1) || \
      (LOWEST_ENERGY_MODE == SL_POWER_MANAGER_EM2))
    sl_power_manager_add_em_requirement(LOWEST_ENERGY_MODE);
#endif

}

uint32_t start_tick_cnt;
uint32_t stop_tick_cnt;
static uint32_t sleep_hours = 0;

/**
 * @brief This function returns a pointer to the user sleep hours
 * @return the address of sleep_hours
 */
uint32_t *getSleepHours()
{
  return &sleep_hours;
}
/**************************************************************************//**
 * Application Process Action.
 *****************************************************************************/
SL_WEAK void app_process_action(void)
{
  // Put your application code here.
  // This is called repeatedly from the main while(1) loop
  // Notice: This function is not passed or has access to Bluetooth stack events.
  //         We will create/use a scheme that is far more energy efficient in
  //         later assignments.

}

/**************************************************************************//**
 * Bluetooth stack event handler.
 * This overrides the dummy weak implementation.
 *
 * @param[in] evt Event coming from the Bluetooth stack.
 *
 * The code here will process events from the Bluetooth stack. This is the only
 * opportunity we will get to act on an event.
 *****************************************************************************/
void sl_bt_on_event(sl_bt_msg_t *evt)
{
  
  // Just a trick to hide a compiler warning about unused input parameter evt.
  (void) evt;

  // Some events require responses from our application code,
  // and don’t necessarily advance our state machines.
   handle_ble_event(evt); // put this code in ble.c/.h

#if DEVICE_IS_BLE_SERVER

  // sequence through states driven by events
   if(SL_BT_MSG_ID(evt->header) != sl_bt_evt_system_external_signal_id){
       return;
   }

   uint32_t event = evt->data.evt_system_external_signal.extsignals;
   //BLE private data
   conn_properties_t *bleDataPtr = getBleDataPtr();

   if((event == evtLETIMER0_UF) && bleDataPtr->connOn && \
       bleDataPtr->bonded){

       if(!sleep_hours){
           //turn off LED0
           gpioLed0SetOff();
           //initiate all sensors to read data
           activate_services();
       }
       else{
           LCD_display_optimal_values();
           sleep_hours--;
           //turn on LED0
           gpioLed0SetOn();
           return;
       }
   }
   else if(!bleDataPtr->connOn || !bleDataPtr->bonded){
       //display the required user action
       displayPrintf(DISPLAY_ROW_ACTION, "Pairing Required");
       //clear all displays
       displayPrintf(DISPLAY_ROW_TEMPVALUE, "");
       displayPrintf(DISPLAY_ROW_8, "");
       displayPrintf(DISPLAY_ROW_9, "");
       return;
   }

   temperature_state_machine(evt);
   light_state_machine(evt);
   sound_state_machine(evt);


#else

   // sequence through service and characteristic discovery
   discovery_state_machine(evt);

#endif
   
} // sl_bt_on_event()

