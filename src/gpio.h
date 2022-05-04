/*
   gpio.h

    Created on: Dec 12, 2018
        Author: Dan Walkes
    Updated by Dave Sluiter Sept 7, 2020. moved #defines from .c to .h file.
    Updated by Dave Sluiter Dec 31, 2020. Minor edits with #defines.
 */

#ifndef SRC_GPIO_H_
#define SRC_GPIO_H_

#include "em_gpio.h"

#define EXTCOMIN_PB0_port                   gpioPortF
#define EXTCOMIN_PB0_pin                    6
#define EXTCOMIN_PB1_port                   gpioPortF
#define EXTCOMIN_PB1_pin                    7


// Function prototypes
void gpioInit();
void gpioLed0SetOn();
void gpioLed0SetOff();
void gpioLed1SetOn();
void gpioLed1SetOff();

void pwUpSi7021();
void pwDownSi7021();
void gpioSetDisplayExtcomin(bool last_extcomin_state_high);



#endif /* SRC_GPIO_H_ */
