#include <stdio.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/stat.h>

#include "board.h"

#define BOARD_BYTES sizeof(pid_t) * BOARD_ROWS *BOARD_COLUMNS

static int board_shm_id;
static pid_t *board_ptr;

//Inizializzazione della board
int initialize_board()
{
    //Inizializzazione shared memory segment della boards
    board_shm_id = shmget(IPC_PRIVATE, BOARD_BYTES, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    if (board_shm_id == -1)
        return -1;

    //Collegamento al shared memory segment della board
    board_ptr = shmat(board_shm_id, NULL, 0);
    if (board_ptr == (pid_t *)-1)
        return -1;

    //Setta a 0 ogni byte della memoria allocata alla board
    memset(board_ptr, 0, BOARD_BYTES);

    return 0;
}

//Chiusura della boards
int teardown_board()
{
    if (board_ptr != NULL && board_ptr != (pid_t *)-1)
    {
        //Scollegamento al shared memory segment della board
        if (shmdt(board_ptr) == -1)
            return -1;

        //Cancellando lo shared memory segment
        if (shmctl(board_shm_id, IPC_RMID, NULL) == -1)
            return -1;
    }

    return 0;
}

//Getter posizione della cella
pid_t board_get(pos_t position)
{
    return board_ptr[position.y * BOARD_COLUMNS + position.x];
}

//Setter posizione della cella
void board_set(pos_t position, pid_t value)
{
    board_ptr[position.y * BOARD_COLUMNS + position.x] = value;
}

//Visualizza la board
void display_board()
{
    printf("┌");
    for (int i = 0; i < BOARD_ROWS; i++)
        printf("-");
    printf("┐\n");

    for (int i = 0; i < BOARD_ROWS; i++)
    {
        printf("|");

        for (int j = 0; j < BOARD_COLUMNS; j++)
        {
            pos_t position = {i, j};
            pid_t pid = board_get(position);
            char *string = pid == 0 ? " " : "#";
            printf("%s", string);
        }

        printf("|\n");
    }

    printf("└");
    for (int i = 0; i < BOARD_ROWS; i++)
        printf("-");
    printf("┘\n");
}
