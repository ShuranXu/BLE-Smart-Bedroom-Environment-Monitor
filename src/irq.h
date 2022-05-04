/**
 * @file irq.h
 * @author Shuran Xu (shxu6388@colorado.edu)
 * @brief This header file contains custom interrupt handler implementation
 * for peripherals.
 * @version 0.1
 * @date 2022-02-01
 *
 * @copyright Copyright (c) 2022
 *
 */

#ifndef __IRQ_H__
#define __IRQ_H__

#include <stdint.h>

/**
 * This function overwrites the weak version of LETIMER0 IRQ handler.
 * The handler toggles LED0 pin based on the current count value.
 */
void LETIMER0_IRQHandler(void);

/**
 * @brief This function overwrites the weak version of I2C0_IRQHandler
 *  IRQ handler. The function currently handles the transfer-triggered
 *  interrupts and record transfer status in case transfer failure
 *  happens.
 */
void I2C0_IRQHandler(void);


/**
 * @brief This function overwrites the weak version of GPIO_EVEN_IRQHandler
 *  IRQ handler.  It handles interrupts triggered for GPIO pin PB0.
 * Event mask bit evtGPIO_PB0 will be set when PB0 is pressed or
 * released.
 */
void GPIO_EVEN_IRQHandler(void);

/**
 * @brief This function computes the timestamp value since the program start.
 * @return the accumualted timestamp value
 */
uint32_t letimerMilliseconds();


/**
 * @brief This function overwrites the weak version of ADC0_IRQHandler
 *  IRQ handler.  It handles interrupts triggered for ADC0.
 * Event mask bit evtADC0_TRANDONE will be set when PB0 is pressed or
 * released.
 */
void ADC0_IRQHandler(void);


/**
 * @brief This function returns a pointer to adc0_data
 * @return the address of the global ELB private data
 */
uint32_t *getADC0Data();


/**
 * @brief This function dumps all failed transfer status for any I2C transfers.
 * The stat_cnts is reset upon the dumping.
 */
void dump_I2C_transfer_status();

#endif // __IRQ_H__
