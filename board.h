#pragma once

#include <unistd.h>

#include "settings.h"

//Alto sinistra è (0,0)
//Basso destra è (BOARD_COLS-1, BOARD_ROWS-1)

//Dimensione della board
#define BOARD_ROWS 10
#define BOARD_COLUMNS 10

//Struttura della posizione per identificare una cella
typedef struct
{
    int x;
    int y;
} pos_t;

//Inizializzazione della board
int initialize_board();

//Chiusura della board
int teardown_board();

//Getter e setter della posizione(x,y)
pid_t board_get(pos_t position);
void board_set(pos_t position, pid_t value);

//Visualizza la board
void display_board();
