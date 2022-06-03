#include "movements.h"

#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/sem.h>

#define STEP_ROW_BYTES (DEVICE_COUNT * 3 + (DEVICE_COUNT - 1))
#define STEP_ROW_EOL "\n"

//Contatore dei movimenti
long movements_count;

//Puntatore a un blocco di memoria contenente tutti i movimenti presi dai device
movement *movements_mem_ptr;

//Inizializzazione dei movimenti
int initialize_steps(char *path)
{
    //Apro il file delle posizioni
    int fd = open(path, S_IRUSR);
    if (fd == -1)
        return -1;

    //Informazioni del file
    struct stat f_stat;
    if (fstat(fd, &f_stat) == -1)
        return -1;

    //Conteggio righe del file
    movements_count = f_stat.st_size / (long)(STEP_ROW_BYTES + sizeof(STEP_ROW_EOL) - 1);

    //Alloco memoria per ogni riga di movimenti
    movements_mem_ptr = malloc(sizeof(movement) * movements_count);
    if (movements_mem_ptr == NULL)
        return -1;

    //Inizializzo una riga
    char buf[STEP_ROW_BYTES];
    for (int step_i = 0; step_i < movements_count; step_i++)
    {
        //Leggo la riga
        if (read(fd, buf, STEP_ROW_BYTES) < STEP_ROW_BYTES)
            return -1;

        //Setto l'offset per la prossima lettura
        if (lseek(fd, 1, SEEK_CUR) == -1)
            return -1;

        //Setto la posizione del singolo movimento per ogni device
        for (int device_i = 0; device_i < DEVICE_COUNT; device_i++)
        {
            movements_mem_ptr[step_i][device_i].x = buf[device_i * 4] - '0';
            movements_mem_ptr[step_i][device_i].y = buf[device_i * 4 + 2] - '0';
        }
    }
    //Chiusura del file delle posizioni
    if (close(fd) == -1)
        return -1;

    return 0;
}

//Chiusura dei movimenti
void teardown_steps()
{
    //Se esiste il blocco di memoria, cancellalo
    if (movements_mem_ptr != NULL)
        free(movements_mem_ptr);
}

//Semaforo dei movimenti
static int steps_sem_id;

//Inizializzazione dei semafori per movimenti
int initialize_move_semaphores()
{
    //Creazione del semaforo (6 semafori)
    steps_sem_id = semget(IPC_PRIVATE, DEVICE_COUNT + 1, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    if (steps_sem_id == -1)
        return -1;

    //Array di char per settare i semafori a 0
    unsigned char *initializer[DEVICE_COUNT + 1] = {0};

    //Set dei semafori a 0
    if (semctl(steps_sem_id, 0, SETALL, initializer) == -1)
        return -1;

    return 0;
}

//Chiusura dei semafori per movimenti
int teardown_move_semaphores()
{
    //Se il semaforo non è nullo o vuoto
    if (steps_sem_id != 0 && steps_sem_id != -1)

        //Cancellazione del semaforo
        if (semctl(steps_sem_id, 0, IPC_RMID) == -1)
            return -1;

    return 0;
}

//Variabile movimento corrente
int current_movement = 0;

//Turno di attesa
int wait_turn(int device_i)
{
    //Creazione dell'operazione del semaforo
    struct sembuf op = {.sem_num = device_i, .sem_op = -1};
    //Set del semaforo nel semaforo dei movimenti
    if (semop(steps_sem_id, &op, 1) == -1)
        return -1;
    return 0;
}

//Passa il turno al successivo
int pass_turn(int device_i)
{
    //Creazione dell'operazione del semaforo
    struct sembuf op = {.sem_num = device_i + 1, .sem_op = +1};
    //Set del semaforo nel semaforo dei movimenti
    if (semop(steps_sem_id, &op, 1) == -1)
        return -1;
    return 0;
}

//Effettua movimento
int perform_step()
{
    for (int i = 0; i < 2; i++)
    {
        //Creazione dell'operazione del semaforo
        struct sembuf op = {.sem_num = 0, .sem_op = +1};
        //Set del semaforo nel semaforo dei movimenti
        if (semop(steps_sem_id, &op, 1) == -1)
            return -1;

        op.sem_num = DEVICE_COUNT;
        op.sem_op = -1;
        //Set del semaforo nel semaforo dei movimenti
        if (semop(steps_sem_id, &op, 1) == -1)
            return -1;
    }

    current_movement++;
    return 0;
}
