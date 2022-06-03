#pragma once

#include "settings.h"
#include "board.h"

// Un movimento è un array di DEVICE_COUNT posizioni
typedef pos_t movement[DEVICE_COUNT];

//Contatore dei movimenti
extern long movements_count;

//Puntatore a un blocco di memoria contenente tutti i movimenti presi dai device
extern movement *movements_mem_ptr;

//Inizializzazione dei movimenti
int initialize_steps(char *path);

//Chiusura dei movimenti
void teardown_steps();

//Inizializzazione dei semafori per movimenti
int initialize_move_semaphores();

//Chiusura dei semafori per movimenti
int teardown_move_semaphores();

//Variabile movimento corrente
extern int current_movement;

//Turno di attesa
int wait_turn(int device_i);

//Passa il turno al successivo
int pass_turn(int device_i);

//Effettua movimento
int perform_step();
