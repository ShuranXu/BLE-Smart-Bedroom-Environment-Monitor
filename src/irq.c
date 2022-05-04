/**
 * @file irq.c
 * @author Shuran Xu (shxu6388@colorado.edu)
 * @brief This file contains interrupt handler implementations. Currently
 * only the ISR of LETIMER0 is supported for periodic event handling.
 * @version 0.1
 * @date 2022-02-01
 *
 * @copyright Copyright (c) 2022
 *
 */

#include "irq.h"
#include "em_letimer.h"
#include "em_gpio.h"
#include "scheduler.h"
#include "app.h"
#include "em_i2c.h"
#include "timers.h"
#include "em_adc.h"

#define INCLUDE_LOG_DEBUG 1
#include "src/log.h"

static uint32_t uf_counter = 0;
static uint32_t adc0_data;

#define EXTCOMIN_PB0_pin    6
#define EXTCOMIN_PB1_pin    7


/**
 * @brief This function returns a pointer to adc0_data
 * @return the address of the global ELB private data
 */
uint32_t *getADC0Data()
{
  return &adc0_data;
}

/**
 * Define high-speed event logger. The array indices correspond to
 * one particular I2C transfer status as shown below:
 * i2cTransferNack       = -1,   < NACK received during transfer.
 * i2cTransferBusErr     = -2,   < Bus error during transfer (misplaced START/STOP).
 * i2cTransferArbLost    = -3,   < Arbitration lost during transfer.
 * i2cTransferUsageFault = -4,   < Usage fault.
 * i2cTransferSwFault    = -5    < SW fault.
 */
static uint32_t stat_cnts[5] = {0,0,0,0,0};

/**
 * @brief This function dumps all failed transfer status for any I2C transfers.
 * The stat_cnts is reset upon the dumping.
 */
void dump_I2C_transfer_status()
{
  LOG_INFO("#i2cTransferNack = %d\r\n", stat_cnts[0]);
  LOG_INFO("#i2cTransferBusErr = %d\r\n", stat_cnts[1]);
  LOG_INFO("#i2cTransferArbLost = %d\r\n", stat_cnts[2]);
  LOG_INFO("#i2cTransferUsageFault = %d\r\n", stat_cnts[3]);
  LOG_INFO("#i2cTransferSwFault = %d\r\n", stat_cnts[4]);

  int i;
  for(i=0;i<5;i++){
      stat_cnts[i] = 0;
  }
}

/**
 * This function overwrites the weak version of LETIMER0 IRQ handler.
 * The handler currently handles UF and COMP1 interrupts.
 */
void LETIMER0_IRQHandler(void)
{
  //find the source of interrupts
  uint32_t flags = LETIMER_IntGetEnabled(LETIMER0);

  //set the underflow event if UF flag is set
  if(flags & LETIMER_IF_UF){
      uf_counter++;
      schedulerSetEventLE0_UF();
  }

  //set the COMP1 event if COMP1 flag is set
  if(flags & LETIMER_IF_COMP1){
      schedulerSetEventLE0_COMP1();
  }

  //clear flags
  LETIMER_IntClear(LETIMER0, flags);

} //LETIMER0_IRQHandler


/**
 * @brief This function overwrites the weak version of I2C0_IRQHandler
 *  IRQ handler. The function currently handles the transfer-triggered
 *  interrupts and record transfer status in case transfer failure
 *  happens.
 */
void I2C0_IRQHandler(void)
{
  uint32_t flags = I2C_IntGetEnabled(I2C0);
  I2C_TransferReturn_TypeDef transerStatus;
  //get the transfer status
  transerStatus = I2C_Transfer(I2C0);

  switch(transerStatus){
    case i2cTransferDone:
      schedulerSetEventI2C0TranDone();
      break;
    case i2cTransferNack:
      schedulerSetEventI2C0TranNACK();
      break;
    default:
      break;
  }

  I2C_IntClear(I2C0,flags);
} //I2C0_IRQHandler


/**
 * @brief This function overwrites the weak version of GPIO_EVEN_IRQHandler
 *  IRQ handler.  It handles interrupts triggered for GPIO pin PB0.
 * Event mask bit evtGPIO_PB0 will be set when PB0 is pressed or
 * released.
 */
void GPIO_EVEN_IRQHandler(void)
{
  /* Clear flag for Push Button 0 interrupt.
   *
   * Note: All the ports share a total of 16 interrupts
   * - one per pin number - i.e. pin 9 of port A and D share one interrupt,
   * so to clear interrupts produced by either one of them we have to
   * clear bit 9.
   */
  GPIO_IntClear(1 << EXTCOMIN_PB0_pin);
  //set event for PB0
  schedulerSetEventGPIO_PB0();

} //GPIO_EVEN_IRQHandler


/**
 * @brief This function overwrites the weak version of GPIO_ODD_IRQHandler
 *  IRQ handler.  It handles interrupts triggered for GPIO pin PB1.
 * Event mask bit evtGPIO_PB1 will be set when PB1 is pressed or
 * released.
 */
void GPIO_ODD_IRQHandler(void)
{
  /* Clear flag for Push Button 1 interrupt.
   *
   * Note: All the ports share a total of 16 interrupts
   * - one per pin number - i.e. pin 9 of port A and D share one interrupt,
   * so to clear interrupts produced by either one of them we have to
   * clear bit 9.
   */
  GPIO_IntClear(1 << EXTCOMIN_PB1_pin);
  //set event for PB1
  schedulerSetEventGPIO_PB1();

} //GPIO_EVEN_IRQHandler


/**
 * @brief This function overwrites the weak version of ADC0_IRQHandler
 *  IRQ handler.  It handles interrupts triggered for ADC0.
 * Event mask bit evtADC0_TRANDONE will be set when PB0 is pressed or
 * released.
 */
void ADC0_IRQHandler(void)
{
  uint32_t flags = ADC_IntGetEnabled(ADC0);
  // Get the single conversion result
  adc0_data = ADC_DataSingleGet(ADC0);
  // Set the scheduler event
  schedulerSetEventADC0_TRANDONE();
  // Clear the flags
  ADC_IntClear(ADC0, flags);

} //ADC0_IRQHandler



/**
 * @brief This function computes the timestamp value since the program start.
 * @return the accumulated timestamp value
 */
uint32_t letimerMilliseconds()
{
  uint32_t elapsed_partial_ticks = getLETIMER0TOP() - LETIMER_CounterGet(LETIMER0);
  return ((uf_counter * LETIMER_PERIOD_MS) + elapsed_partial_ticks);
}
