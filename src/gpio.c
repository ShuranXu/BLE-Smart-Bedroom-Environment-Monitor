/*
  gpio.c

   Created on: Dec 12, 2018
       Author: Dan Walkes
   Updated by Dave Sluiter Dec 31, 2020. Minor edits with #defines.

   March 17
   Dave Sluiter: Use this file to define functions that set up or control GPIOs.

 */


// *****************************************************************************
// Students:
// We will be creating additional functions that configure and manipulate GPIOs.
// For any new GPIO function you create, place that function in this file.
// *****************************************************************************

#include <stdbool.h>
#include "em_cmu.h"
#include <string.h>
#include "timers.h"
#include "gpio.h"
#include "ble_device_type.h"

// Student Edit: Define these, 0's are placeholder values.
// See the radio board user guide at https://www.silabs.com/documents/login/user-guides/ug279-brd4104a-user-guide.pdf
// and GPIO documentation at https://siliconlabs.github.io/Gecko_SDK_Doc/efm32g/html/group__GPIO.html
// to determine the correct values for these.

#define LED0_port                           gpioPortF
#define LED0_pin                            4
#define LED1_port                           gpioPortF
#define LED1_pin                            5
#define SENLE_port                          gpioPortD
#define SENLE_pin                           15
#define WAIT_PERIOD_MS                      80
#define MS_TO_US                            1000
#define EXTCOMIN_port_D                     gpioPortD
#define EXTCOMIN_pin_13                     13
#define I2C0_SCL_port                       gpioPortD
#define I2C0_SDA_port                       gpioPortD
#define I2C0_SCL_pin                        10
#define I2C0_SDA_pin                        11
#define I2C1_SCL_port                       gpioPortC
#define I2C1_SDA_port                       gpioPortC
#define I2C1_SCL_pin                        10
#define I2C1_SDA_pin                        11


void pwUpSi7021()
{
  GPIO_PinOutSet(SENLE_port,SENLE_pin);
  //wait for 80 ms to power up the sensor
  timeWaitUs_irq((WAIT_PERIOD_MS * MS_TO_US));
}

void pwDownSi7021()
{
  GPIO_PinOutClear(SENLE_port,SENLE_pin);
}

// Set GPIO drive strengths and modes of operation
#if DEVICE_IS_BLE_SERVER
void gpioInit()
{
  //enable GPIO and clock
  CMU_ClockEnable(cmuClock_GPIO, true);
  //configure LED0_port drive strength to weak
  GPIO_DriveStrengthSet(LED0_port, gpioDriveStrengthWeakAlternateWeak);
  //configure LED0 as output
  GPIO_PinModeSet(LED0_port, LED0_pin, gpioModePushPull, false);
  //configure LED1_port drive strength to weak
  GPIO_DriveStrengthSet(LED1_port, gpioDriveStrengthWeakAlternateWeak);
  //configure LED1 as output
  GPIO_PinModeSet(LED1_port, LED1_pin, gpioModePushPull, false);
  //configure D13 (EXTCOMIN) drive strength to weak
  GPIO_DriveStrengthSet(EXTCOMIN_port_D, gpioDriveStrengthWeakAlternateWeak);
  //configure D13 (EXTCOMIN) as output
  GPIO_PinModeSet(EXTCOMIN_port_D, EXTCOMIN_pin_13, gpioModePushPull, false);
  //set the drive strength to weak
  GPIO_DriveStrengthSet(SENLE_port, gpioDriveStrengthWeakAlternateWeak);
  //configure PD15 (SENSOR ENABLE) as output
  GPIO_PinModeSet(SENLE_port, SENLE_pin, gpioModePushPull, false);
  //configure push button 0 as an input
  GPIO_PinModeSet(EXTCOMIN_PB0_port, EXTCOMIN_PB0_pin, gpioModeInput, 1);
  //configure PD10 as pull-up for I2C0 SCL line
  GPIO_PinModeSet(I2C0_SCL_port, I2C0_SCL_pin, gpioModeWiredAndPullUp, 0);
  //configure PD11 as pull-up for I2C0 SDA line
  GPIO_PinModeSet(I2C0_SDA_port, I2C0_SDA_pin, gpioModeWiredAndPullUp, 0);
  //enable GPIO_EVEN_IRQn interrupt vector in NVIC
  NVIC_EnableIRQ(GPIO_EVEN_IRQn);
  //configure PB0 with glitch input filtering enabled
  GPIO_ExtIntConfig(EXTCOMIN_PB0_port, //port
                    EXTCOMIN_PB0_pin, //pin
                    EXTCOMIN_PB0_pin, //intNo
                    true, //risingEdge
                    true, //fallingEdge
                    true);//enable

} // gpioInit()
#else
void gpioInit()
{
  //enable GPIO and clock
  CMU_ClockEnable(cmuClock_GPIO, true);
  //set the EXTCOMIN_port_D drive strength to weak
  GPIO_DriveStrengthSet(EXTCOMIN_port_D, gpioDriveStrengthWeakAlternateWeak);
  //configure D13 (EXTCOMIN) as output
  GPIO_PinModeSet(EXTCOMIN_port_D, EXTCOMIN_pin_13, gpioModePushPull, false);
  //set the SENLE_port drive strength to weak
  GPIO_DriveStrengthSet(SENLE_port, gpioDriveStrengthWeakAlternateWeak);
  //configure PD15 (SENSOR ENABLE) as output
  GPIO_PinModeSet(SENLE_port, SENLE_pin, gpioModePushPull, false);
  //configure push button 0 as an input
  GPIO_PinModeSet(EXTCOMIN_PB0_port, EXTCOMIN_PB0_pin, gpioModeInput, 1);
  //configure push button 1 as an input
  GPIO_PinModeSet(EXTCOMIN_PB1_port, EXTCOMIN_PB1_pin, gpioModeInput, 1);

  //enable GPIO_EVEN_IRQn interrupt vector in NVIC
  NVIC_EnableIRQ(GPIO_EVEN_IRQn);
  //enable GPIO_ODD_IRQn interrupt vector in NVIC
  NVIC_EnableIRQ(GPIO_ODD_IRQn);
  //configure PB0 with glitch input filtering enabled
  GPIO_ExtIntConfig(EXTCOMIN_PB0_port, //port
                    EXTCOMIN_PB0_pin, //pin
                    EXTCOMIN_PB0_pin, //intNo
                    false, //risingEdge
                    true, //fallingEdge
                    true);//enable

  //configure PB1 with glitch input filtering enabled
  GPIO_ExtIntConfig(EXTCOMIN_PB1_port, //port
                    EXTCOMIN_PB1_pin, //pin
                    EXTCOMIN_PB1_pin, //intNo
                    false, //risingEdge
                    true, //fallingEdge
                    true);//enable

} // gpioInit()
#endif

void gpioSetDisplayExtcomin(bool last_extcomin_state_high)
{
  if(last_extcomin_state_high){
      GPIO_PinOutSet(EXTCOMIN_port_D, EXTCOMIN_pin_13);
  }
  else{
      GPIO_PinOutClear(EXTCOMIN_port_D, EXTCOMIN_pin_13);
  }
}

void gpioLed0SetOn()
{
  GPIO_PinOutSet(LED0_port,LED0_pin);
}


void gpioLed0SetOff()
{
  GPIO_PinOutClear(LED0_port,LED0_pin);
}


void gpioLed1SetOn()
{
  GPIO_PinOutSet(LED1_port,LED1_pin);
}


void gpioLed1SetOff()
{
  GPIO_PinOutClear(LED1_port,LED1_pin);
}



