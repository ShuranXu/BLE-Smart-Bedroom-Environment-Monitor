/**
 * @file timers.c
 * @author Shuran Xu (shxu6388@colorado.edu)
 * @brief This file includes the implementation of the
 * initialization of LETIMER0 based on the given the
 * energy mode and the LETIMER0 period. Additionally,
 * processor waiting function via polling is also implemented
 * to provide blocking methods for various uses.
 * @version 0.1
 * @date 2022-02-01
 *
 * @copyright Copyright (c) 2022
 *
 */

#include "em_letimer.h"
#include "em_cmu.h"
#include "timers.h"
#include "oscillators.h"

#define INCLUDE_LOG_DEBUG 1
#include "src/log.h"


#define PRESCALER_VALUE     4
#define LFXO_FREQ           32768
#define ULFRCO_FREQ         1000
#define LETIMER0_PERIOD_MS   3000


static uint32_t ACTUAL_CLK_FREQ = 0;
static uint32_t LE_TOP_VALUE = 0;

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
                  sl_power_manager_em_t energy_mode)
{

  //enable clock to the LE interface
  CMU_ClockEnable(cmuClock_HFLE, true);
  //configure the oscillator for LETIMER0 based on the energy mode
  init_osc_for_letimer0(energy_mode);

  //determine the clock frequency based on the energy mode
  if(energy_mode < SL_POWER_MANAGER_EM3){
      //update ACTUAL_CLK_FREQ with LFXO_FREQ
      ACTUAL_CLK_FREQ = LFXO_FREQ / PRESCALER_VALUE;
      //set the prescaler to 4 for the clock
      CMU_ClockDivSet(cmuClock_LETIMER0, PRESCALER_VALUE);
  }
  else{
      ACTUAL_CLK_FREQ = ULFRCO_FREQ;
  }

  //enable the clock
  CMU_ClockEnable(cmuClock_LETIMER0, true);
  //set the top value according to period
  uint32_t top_value = (period * ACTUAL_CLK_FREQ) / 1000;

  //set letimer0 run in repeatFree mode
  //set comp0 as the top value each time letimer0 wraps around
  LETIMER_Init_TypeDef letimerInit = LETIMER_INIT_DEFAULT;
  letimerInit.enable = false;
  letimerInit.comp0Top = true;
  letimerInit.repMode = letimerRepeatFree;
  letimerInit.topValue = top_value;

  //set the repeat mode
  LETIMER_RepeatSet(LETIMER0, 0, 1);

  //set the comp0 register with top_value
  LETIMER_CompareSet(LETIMER0, 0, top_value);

  if(on_time){
      //set the comp1 register with LETIMER_ON_TIME_MS
      uint32_t ontime_value = (on_time * ACTUAL_CLK_FREQ) / 1000;
      LETIMER_CompareSet(LETIMER0, 1, ontime_value);
      //enable LETIMER0 interrupt for COMP1 and UF
      LETIMER0->IEN |= LETIMER_IEN_COMP1 | LETIMER_IEN_UF;
  }
  else{
      //enable LETIMER0 interrupt for COMP1 and UF
      LETIMER0->IEN |= LETIMER_IEN_UF;
  }

  //initialize and enable LETIMER0
  LETIMER_Init(LETIMER0, &letimerInit);

  //enable LETIMER0 IRQ in NVIC
  NVIC_ClearPendingIRQ(LETIMER0_IRQn);
  NVIC_EnableIRQ(LETIMER0_IRQn);

  //enable LETIMER0
  LETIMER_Enable(LETIMER0,true);

} //initLETIMER0


/**
 * @brief This function blocks the processor certain amount of time specified
 * by us_wait. Polling of the LETIMER0 is performed to achieve the waiting.
 * Input value check is performed to prevent undefined behaviour from happening.
 * @param us_wait: the target time to wait
 */
void timerWaitUs_polled(uint32_t us_wait)
{
  // convert wait period from us to ms
  uint32_t ms_wait = us_wait / 1000;

  if(ms_wait > LETIMER0_PERIOD_MS ){
      LOG_ERROR("Error: invalid wait value detected, the wait value will set to %u ms\r\n", \
                LETIMER0_PERIOD_MS);
      ms_wait = LETIMER0_PERIOD_MS;
  }

  const uint32_t TICK_CNT_THRSHOLD = (ms_wait * ACTUAL_CLK_FREQ) / 1000;
  LE_TOP_VALUE = (LETIMER0_PERIOD_MS * ACTUAL_CLK_FREQ) / 1000;
  uint32_t start_tick_cnt;
  uint32_t curr_tick_cnt;
  uint32_t elasped_tick_cnt = 0;

  //time the current count tick
  start_tick_cnt = LETIMER_CounterGet(LETIMER0);

  //start the loop to count the tick
  do{
      curr_tick_cnt = LETIMER_CounterGet(LETIMER0);
      if(curr_tick_cnt <= start_tick_cnt){
          elasped_tick_cnt = start_tick_cnt - curr_tick_cnt;
      }
      else{
          elasped_tick_cnt = LE_TOP_VALUE - curr_tick_cnt + start_tick_cnt;
      }
  }while(elasped_tick_cnt < TICK_CNT_THRSHOLD);

}

/**
 * @brief This function enables or disables COMP1 interrupt based on
 * the given flag.
 * @param set: the flag indicating if the COMP1 interrupt should be enabled.
 */
void set_LETIMER0COMP1_irq(int set)
{
  if(set){
     //set LETIMER0 interrupt for COMP1
     LETIMER0->IEN |= LETIMER_IEN_COMP1;
  }
  else{
      //clear LETIMER0 interrupt for COMP1
      LETIMER0->IEN &= ~LETIMER_IEN_COMP1;
  }
}

/**
 * @brief This function waits for the specified time by enabling COMP1
 * interrupt for LETIMER0 with the associated COMP1 value. This function
 * does not block the CPU.
 * @param us_wait: the specified amount of time to wait.
 */
void timeWaitUs_irq(uint32_t us_wait)
{
  // convert wait period from us to ms
   uint32_t ms_wait = us_wait / 1000;

   if(ms_wait > LETIMER0_PERIOD_MS ){
       LOG_ERROR("Error: invalid wait value detected, the wait value will set to %u ms\r\n", \
                 LETIMER0_PERIOD_MS);
       ms_wait = LETIMER0_PERIOD_MS;
   }

   uint32_t req_tickmnt = (ms_wait * ACTUAL_CLK_FREQ) / 1000;
   uint32_t curr_tick_cnt = LETIMER_CounterGet(LETIMER0);
   uint32_t comp1_val;
   if(curr_tick_cnt >= req_tickmnt){
       comp1_val = curr_tick_cnt - req_tickmnt;
   }
   else{
       comp1_val = LE_TOP_VALUE - (req_tickmnt - curr_tick_cnt);
   }

   LETIMER_CompareSet(LETIMER0, 1, comp1_val);
   set_LETIMER0COMP1_irq(1);
}

/**
 * @brief This function returns the top value configured for LETIMER0.
 * @return the top value.
 */
uint32_t getLETIMER0TOP()
{
  return LE_TOP_VALUE;
}
