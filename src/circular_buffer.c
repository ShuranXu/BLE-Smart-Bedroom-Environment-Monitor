/***********************************************************************
 * @file      circular_buffer.c
 * @version   0.1
 * @brief     Function implementation file.
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

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "circular_buffer.h"
#include "ble.h"

//for debugging only
#define INCLUDE_LOG_DEBUG 1
#include "src/log.h"

// Declare memory for the queue/buffer/fifo, and the write and read pointers
static pending_ind_queue_struct_t   my_queue[QUEUE_DEPTH]; // the queue
static uint32_t                     wptr = 0;              // write pointer
static uint32_t                     rptr = 0;              // read pointer
// Decide how will you handle the full condition. 


// ---------------------------------------------------------------------
// Private function used only by this .c file. 
// compute next ptr value
// Isolation of functionality: This defines "how" a pointer advances. 
// ---------------------------------------------------------------------
static uint32_t nextPtr(uint32_t ptr) {

  if(ptr == (QUEUE_DEPTH - 1))
    return 0;
  else
    return ptr + 1;

} // nextPtr()


// ---------------------------------------------------------------------
// Private function used only by this .c file. 
// check if the circular is empty
// Isolation of functionality: This defines the empty state of the buffer. 
// ---------------------------------------------------------------------
static bool is_queue_empty() {
  return (wptr == rptr);
}

// ---------------------------------------------------------------------
// Private function used only by this .c file. 
// check if the circular is full
// Isolation of functionality: This defines the full state of the buffer. 
// ---------------------------------------------------------------------
static bool is_queue_full() {

  bool res;
  // determine the buffer full condition
  if(wptr > rptr) {
    res = (wptr - rptr) == (QUEUE_DEPTH -1);
  }
  else{
    res = (rptr == (wptr + 1));
  }

  return res;
}

// ---------------------------------------------------------------------
// Public function
// This function writes an entry to the queue.
// Returns false if successful or true if writing to a full fifo.
// ---------------------------------------------------------------------
bool write_queue(uint8_t connection, uint16_t characteristic, \
                      size_t value_len, uint8_t *value)
{
  // check if the buffer is already full
  if(is_queue_full())
    return true;

  // update my_queue
  my_queue[wptr].connection = connection;
  my_queue[wptr].characteristic = characteristic;
  my_queue[wptr].value_len = value_len;
  memcpy(my_queue[wptr].value, value, value_len);

  // advance wptr
  wptr = nextPtr(wptr);

  return false;

} // write_queue()


// ---------------------------------------------------------------------
// Public function
// This function reads an entry from the queue.
// Returns false if successful or true if reading from an empty fifo. 
// ---------------------------------------------------------------------
bool read_queue(uint8_t *connection, uint16_t *characteristic, \
                     size_t *value_len, uint8_t *value)
{
  // check if the buffer is empty
  if(is_queue_empty())
    return true;

  // check if the parameters are null
  if(!connection || !characteristic || !value_len || !value)
    return true;

  // update a and b
  *connection = my_queue[rptr].connection;
  *characteristic = my_queue[rptr].characteristic;
  *value_len = my_queue[rptr].value_len;
  memcpy(value, my_queue[rptr].value, *value_len);

  // advance rptr
  rptr = nextPtr(rptr);

  return false;
} // read_queue()

// ---------------------------------------------------------------------
// Public function
// This function returns the wptr, rptr, full and empty values.
// ---------------------------------------------------------------------
void get_queue_status (uint32_t *_wptr, uint32_t *_rptr, bool *_full, bool *_empty) {

  if(_wptr)
    *_wptr = wptr;
  if(_rptr)
    *_rptr = rptr;
  if(_full)
    *_full = is_queue_full();
  if(_empty)
    *_empty = is_queue_empty();
  
} // get_queue_status() 



// ---------------------------------------------------------------------
// Public function
// Function that computes the number of entries in the queue
// ---------------------------------------------------------------------
uint32_t get_queue_depth() {

  if(is_queue_empty())
    return 0;
  else if(is_queue_full())
    return QUEUE_DEPTH - 1;
  else if(wptr > rptr)
    return (wptr - rptr);
  else  
    return (QUEUE_DEPTH - rptr + wptr);

} // get_queue_depth() 


// ---------------------------------------------------------------------
// Public function
// Function that clears the circular buffer, including wptr and rptr. The
// the function is intended to be used only in the unit test suite.  
// ---------------------------------------------------------------------
void clear_queue(){

  wptr = 0;
  rptr = 0;
  for(int i=0;i<QUEUE_DEPTH;i++){
    my_queue[i].connection = 0;
    my_queue[i].characteristic = 0;
    my_queue[i].value_len = 0;
    memset(my_queue[i].value, 0, MAX_INDICATION_VALUE_LEN);

  }
} // clear_queue

// ---------------------------------------------------------------------
// Public function
// Function that removes the elements with the specified characteristic
// from the circular buffer.
// ---------------------------------------------------------------------
void remove_queue_by_characteristic(uint16_t characteristic)
{
  //get the current queue depth
  int i=0;
  int curr_depth = get_queue_depth();
  for(;i<curr_depth;i++){

      pending_indication_t pending_indicate;
      if(!read_queue(&pending_indicate.connection, &pending_indicate.characteristic, \
                           &pending_indicate.value_len, &pending_indicate.value[0])){
          if(pending_indicate.characteristic != characteristic){
              //write the element back to the queue
              if(write_queue(pending_indicate.connection, pending_indicate.characteristic, \
                             pending_indicate.value_len, &pending_indicate.value[0])){
                  LOG_ERROR("Failed to enqueue the inspected indication back to the queue\r\n");
              }
          }
      }
  }
}
