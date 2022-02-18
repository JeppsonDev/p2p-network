/**
 * node_states.h
 *
 * The interface for the node states implementation of a state machine
 *
 * @author Casper Hägglund, Jesper Byström
 * @date 2021-10-27
 *
 */


#ifndef OU3_NODE_STATES_H
#define OU3_NODE_STATES_H
#include "node.h"
#include <stdio.h>
#include <poll.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#define maxListeners 5
#define BUFF_SIZE 1024
#define UDP 100
#define TCP 101

/**
 * 
 * Enum representing the states for the state machine
 *
 */
typedef enum {
    Q1,
    Q2,
    Q3,
    Q4,
    Q5,
    Q6,
    Q7,
    Q8,
    Q9,
    Q10,
    Q11,
    Q12,
    Q13,
    Q14,
    Q15,
    Q16,
    Q17,
    Q18,
    EXIT
} states;

/**
 * 
 * Callback function for state in state machine
 *
 */
typedef states (*eventHandler)(node*);

/**
 * 
 * The struct representing the event state
 *
 */
typedef struct {
    eventHandler handler;
} state;

state* node_states_get_state_machine();

#endif
