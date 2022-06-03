#pragma once

#include <unistd.h>
#include <time.h>
#include <time.h>
#include <stdbool.h>

#include "message.h"
#include "settings.h"

#define ACK_TABLE_ROWS 10
#define ACK_TABLE_BYTES sizeof(ack) * ACK_TABLE_ROWS

typedef struct {
    pid_t pid_sender;
    pid_t pid_receiver;
    int message_id;
    time_t timestamp;
    int zombie_at;
} ack;

//Inizializzo la ACK table
int initialize_ack_table();

//Chiudo la ACK table
int teardown_ack_table();

//Blocco la ACK table
int lock_ack_table();

//Sblocco la ACK table
int unlock_ack_table();

_Noreturn void ack_manager_loop(int msg_queue_key);

int add_ack(msg *msg_ptr);

bool has_dev_received_msg(pid_t dev_pid, int msg_id);

//Struttura della coda dei feedback
typedef struct {
    long message_id;
    ack acks[DEVICE_COUNT];
} feedback;
