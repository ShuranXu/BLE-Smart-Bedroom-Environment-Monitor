/**
 * @file oscillators.h
 * @author Shuran Xu (shxu6388@colorado.edu)
 * @brief This header files includes public APIs related to
 * the on-chip oscillator initialization and manipulations.
 * @version 0.1
 * @date 2022-02-01
 *
 * @copyright Copyright (c) 2022
 *
 */


#ifndef __OSCILLATORS_H__
#define __OSCILLATORS_H__

#include "sl_power_manager.h"

void init_osc_for_letimer0(sl_power_manager_em_t energy_mode);

#endif // __OSCILLATORS_H__
