/**
 * @file oscillators.c
 * @author Shuran Xu (shxu6388@colorado.edu)
 * @brief This files implements public APIs related to
 * the on-chip oscillator initialization and manipulations. Currently
 * only the initialization of oscillators related to LETIMER0 is
 * implemented.
 * @version 0.1
 * @date 2022-02-01
 *
 * @copyright Copyright (c) 2022
 *
 */

#include "oscillators.h"
#include "em_cmu.h"

/**
 * init_osc_for_letimer0( ) initializes and selects the oscillator as the
 * LFA clock source for LETIMER0 peripheral, based on the given energy mode.
 * @param energy_mode: The target energy mode.
 * @reference: The initialization of LFXO is implemented based on the code
 * example from an0004.1-efm32-cmu.pdf at P12, section 2.3.2. LFXO.
 */
void init_osc_for_letimer0(sl_power_manager_em_t energy_mode)
{
  switch(energy_mode)
  {
    case SL_POWER_MANAGER_EM0:
    case SL_POWER_MANAGER_EM1:
    case SL_POWER_MANAGER_EM2:
      {
        //init LFXO with the default parameter
        //LFXO operates at 32768 Hz
        CMU_LFXOInit_TypeDef lfxo_init = CMU_LFXOINIT_DEFAULT;
        CMU_LFXOInit(&lfxo_init);
        //enable LFXO and set LFXO for LFACLK
        CMU_ClockSelectSet(cmuClock_LFA, cmuSelect_LFXO);
        break;
      }
    case SL_POWER_MANAGER_EM3:
      {
        //no need to initialize ULFRCO since it is always
        //available in the low-frequency clock domains.
        //set ULFRCO for LFACLK. But we need to enable ULFRCO.
        CMU_OscillatorEnable(cmuOsc_ULFRCO, true, true);
        CMU_ClockSelectSet(cmuClock_LFA, cmuSelect_ULFRCO);
        break;
      }
    default:
      break;
  }
}
