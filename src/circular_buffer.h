/***********************************************************************
 * @file      circular_buffer.h
 * @version   0.1
 * @brief     Function header/interface file.
 *
 * @author    Shuran Xu, shxu6388@colorado.edu
 * @date      Jan 21, 2022
 *
 * @institution University of Colorado Boulder (UCB)
 * @course      ECEN 5823-001: IoT Embedded Firmware
 * @instructor  David Sluiter
 *
 * @assignment Assignment 1.5 - Circular Buffer
 * @due        
 *
 * @resources   
 * 
 */

// Define everything that a caller needs to know



#ifndef __MY_CIRCULAR_BUFFER__
#define __MY_CIRCULAR_BUFFER__


#include <stdint.h>
#include <stdbool.h>

// This is the number of entries in the queue. Please leave
// this value set to 24.
#define QUEUE_DEPTH      (24)
// Student edit: 
//   define this to 1 if your design uses all array entries
//   define this to 0 if your design leaves 1 array entry empty
#define USE_ALL_ENTRIES  (0)

#define MAX_INDICATION_VALUE_LEN    (5)



// Modern C (circa 2021 does it this way)
// typedef <name> is referred to as an anonymous struct definition
// This is the structure of 1 queue/buffer entry
typedef struct {
  uint8_t connection;
  uint16_t characteristic;
  size_t value_len;
  uint8_t value[MAX_INDICATION_VALUE_LEN];
} pending_ind_queue_struct_t;



// function prototypes
bool     write_queue (uint8_t connection, uint16_t characteristic, \
                      size_t value_len, uint8_t *value);
bool     read_queue (uint8_t *connection, uint16_t *characteristic, \
                     size_t *value_len, uint8_t *value);
void     get_queue_status (uint32_t *_wptr, uint32_t *_rptr, bool *_full, bool *_empty);
uint32_t get_queue_depth (void);
void     clear_queue();
void     remove_queue_by_characteristic(uint16_t characteristic);


#endif // __MY_CIRCULAR_BUFFER__

