/*
 * Title:  suffrage
 * Author:  Richard Thai and Garrick Merrill
 * Created:  1/12/2010
 * Modified:  2/22/2011
 *
 * Description:  Header file containing variables, constants, and data structures
 * needed for suffrage sketch.
 */

#ifndef SKETCH_H_GUARD
#define SKETCH_H_GUARD

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "SFBErrors.h"

#define INVALID 0xffffffff
#define TIE 0xfffffffe
#define OFF 0xffffffff
#define MINORITY 0 // Red LED
#define MAJORITY 1 // Green LED
#define PROCESSING 2 // Blue LED
const u32 PRIME_THRESHOLD = 1000; // Accept nothing higher than the 1000th prime
const u32 PRIME_ARR_THRESHOLD = 7920; // 1000th prime is 7919
const u16 IDLE = 5000; // limit for absence of ping until IXM node is considered idle
const u16 pingAll_PERIOD = 1000; // interval for heartbeat
const u16 printTable_PERIOD = 500; // interval for refreshing the table
const u16 FAULT_STATUS_PERIOD = 500; // interval for LED flash initialization
const u32 VOTE_COUNT_MIN = 2; // minimum number of voters for majority vote calculation
const u32 ID_HOST = getBootBlockBoardId(); // host ID
const u32 LED_PIN[3] = // LED PINS to cycle through
      { BODY_RGB_RED_PIN, BODY_RGB_GREEN_PIN, BODY_RGB_BLUE_PIN }; // LED Array for easy reference

bool FAULTY = false; // Determines if the calculation should be erroneous
u32 HOST_CALC = 0; // n_th prime to be calculated
u32 HOST_CALC_VER = 0; // n_th prime calculation version
u32 MAJORITY_RSLT = 0; // n_th prime running majority result
u32 NODE_COUNT = 1; // count of IXM nodes, always includes host IXM
u32 CANDIDATE_COUNT = 0; // count of candidates to vote for
u32 TERMINAL_FACE = INVALID; // terminal face for table printout and clean UI
u32 VOTE_COUNT = 0; // count of IXM votes so far

char sieve[PRIME_ARR_THRESHOLD] =
  { 0 }; // array used for storing prime calculations
char ACTIVE_NODE_ARR[32] =
  { 'I' }; // list of active nodular IXM's
u16 PC_NODE_ARR[32] =
  { 0 }; // ping count for nodes
u32 ID_NODE_ARR[32] =
  { 0 }; // list of nodular IXM ID's
u32 CANDIDATE_ARR[32] =
  { 0 }; // list of the possible values to vote for
u32 VOTE_NODE_ARR[32] =
  { 0 }; // n_th prime calculation result for respective nodes
u32 TS_HOST_ARR[32] =
  { 0 }; // last-received time-stamp of nodes from host times
u32 TS_NODE_ARR[32] =
  { 0 }; // last-received time-stamp of nodes from respective node packets
u32 CANDIDATE_VOTES_ARR[32] =
  { 0 }; // vote-count for the respective results

/*
 * Summary:     Distinguishing keys for IXM node and packet
 * Contains:    u32 ID (board key), u32 TIME (packet key)
 */
struct KEY
{
  u32 ID; // Identifies IXM node (sender)
  u32 TIME; // Identifies packet version
};

/*
 * Summary:     (c)alculation packet structure
 * Contains:    u32 calculation
 */
struct C_PKT
{
  u32 calc;
};

/*
 * Summary:     (r)esult packet structure
 * Contains:    KEY, u32 n_th prime calculation, u32 calculation version,
 *              n_th prime result
 */
struct R_PKT
{
  struct KEY key;
  u32 calc; // denotes the n_th prime calculation
  u32 calc_ver; // denotes the calculation version
  u32 rslt; // denotes the n_th prime result
};

#endif
