#ifndef __TIMERS_H__
#define __TIMERS_H__

#include "sl_power_manager.h"

/**
 * initLETIMER0( ) initializes and configures LETIMER0 according to the
 * desired energy mode, timer period and on-time.
 * @param period: The expected timer period, in MS.
 * @param on_time: The time for LETIMER to turn on LED
 * @param energy_mode: The current energy mode the system is in
 * @reference: The implementation body takes void initLETIMER(void)
 * from the demo project 'pulse_width_modulation' provided by SiliconLabs:
 * https://github.com/SiliconLabs/peripheral_examples/blob/master/series1/letimer/pulse_width_modulation/src/main_s1.c
 */
void initLETIMER0(uint32_t period, uint32_t on_time,
                  sl_power_manager_em_t energy_mode);

/**
 * @brief This function blocks the processor certain amount of time specified
 * by us_wait. Polling of the LETIMER0 is performed to achieve the waiting.
 * Input value check is performed to prevent undefined behaviour from happening.
 * @param us_wait: the target time to wait
 */
void timerWaitUs_polled(uint32_t us_wait);

/**
 * @brief This function waits for the specified time by enabling COMP1
 * interrupt for LETIMER0 with the associated COMP1 value. This function
 * does not block the CPU.
 * @param us_wait: the specified amount of time to wait.
 */
void timeWaitUs_irq(uint32_t us_wait);

/**
 * @brief This function enables or disables COMP1 interrupt based on
 * the given flag.
 * @param set: the flag indicating if the COMP1 interrupt should be enabled.
 */
void set_LETIMER0COMP1_irq(int set);

/**
 * @brief This function returns the top value configured for LETIMER0.
 * @return the top value.
 */
uint32_t getLETIMER0TOP();

#endif // __TIMERS_H__
