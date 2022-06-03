#include <sys/shm.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>

#include "ack.h"
#include "movements.h"

static int ack_table_shm_id;
static ack *ack_table_ptr;

static int ack_table_sem_id;

//Inizializzazione della tabella degli ACK
int initialize_ack_table()
{
    //Inizializzazione shared memory segment della tabella degli ACK
    ack_table_shm_id = shmget(IPC_PRIVATE, ACK_TABLE_BYTES, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    if (ack_table_shm_id == -1)
        return -1;

    //Attach del shared memory segment della tabella degli ACK
    ack_table_ptr = shmat(ack_table_shm_id, NULL, 0);
    if (ack_table_ptr == (ack *)-1)
        return -1;

    //Setta a 0 ogni byte della memoria allocata alla tabella degli ACK
    memset(ack_table_ptr, 0, ACK_TABLE_BYTES);

    //Inizializzazione del semaforo della tabella degli ACK
    ack_table_sem_id = semget(IPC_PRIVATE, 1, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    if (ack_table_sem_id == -1)
        return -1;

    //Setting del semaforo della tabella degli ACK
    if (semctl(ack_table_sem_id, 0, SETVAL, 1) == -1)
        return -1;

    return 0;
}

//Chiusura della tabella degli ACK
int teardown_ack_table()
{
    //Se la tabella degli ACK non è nulla e non è già chiusa
    if (ack_table_ptr != NULL && ack_table_ptr != (ack *)-1)
    {
        //Detach dello shared memory segment dalla tabella degli ACK
        if (shmdt(ack_table_ptr) == -1)
            return -1;
        //Eliminazione dello shared memory segment
        if (shmctl(ack_table_shm_id, IPC_RMID, NULL) == -1)
            return -1;
    }

    //Se il semaforo della tabella degli ACK non è nulla e non è già vuota
    if (ack_table_sem_id != 0 && ack_table_sem_id != -1)
    {
        //Eliminazione dello shared memory segment
        if (semctl(ack_table_sem_id, 0, IPC_RMID) == -1)
            return -1;
    }

    return 0;
}

static int queue_id;

//Inizializzazione della coda
int init_feedback_queue(int key)
{
    //Inizializzazione della coda
    queue_id = msgget(key, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    if (queue_id == -1)
        return -1;

    return 0;
}

//Chiusura della coda
int teardown_feedback_queue()
{
    //Se la coda non è nulla e non è già chiusa
    if (queue_id != 0 && queue_id != -1)
        //Cancellazione della coda
        if (msgctl(queue_id, IPC_RMID, NULL) == -1)
            return -1;

    return 0;
}

//Funzione per il blocco della tabella degli ACKS
int lock_ack_table()
{
    struct sembuf op = {.sem_num = 0, .sem_op = -1};
    while (true)
    {
        int res = semop(ack_table_sem_id, &op, 1);
        if (res != -1)
            return 0;
        else if (errno == EINTR)
            continue;
        else
            return -1;
    }
}

//Funzione per lo sblocco della tabella degli ACK
int unlock_ack_table()
{
    struct sembuf op = {.sem_num = 0, .sem_op = +1};
    while (true)
    {
        int res = semop(ack_table_sem_id, &op, 1);
        if (res != -1)
            return 0;
        else if (errno == EINTR)
            continue;
        else
            return -1;
    }
}

//Le variabili globali sono inizializzate a 0
// Usiamo questa variabile per controllare se la riga è vuota
ack test_ack;

static bool is_row_free(int row_i)
{
    return memcmp(ack_table_ptr + row_i, &test_ack, sizeof(ack)) == 0;
}

//Aggiunta ack
int add_ack(msg *msg_ptr)
{
    ack new_ack = {
        .pid_sender = msg_ptr->pid_sender,
        .pid_receiver = msg_ptr->pid_receiver,
        .message_id = msg_ptr->id,
        .timestamp = time(0),
        .zombie_at = -1,
    };

    //Inserisce l'ack nella tabellas
    int row_i;
    for (row_i = 0; row_i < ACK_TABLE_ROWS; row_i++)
    {
        if (is_row_free(row_i))
        {
            ack_table_ptr[row_i] = new_ack;
            break;
        }
    }

    // Non ho trovato una riga vuota
    if (row_i == ACK_TABLE_ROWS)
        return -1;

    return 0;
}

//Funzione ha già ricevuto il messaggio
bool has_dev_received_msg(pid_t dev_pid, int msg_id)
{
    for (int row_i = 0; row_i < ACK_TABLE_ROWS; row_i++)
    {
        ack *ack = ack_table_ptr + row_i;
        if (ack->pid_receiver == dev_pid && ack->message_id == msg_id)
            return true;
    }

    return false;
}

//Comparatore degli id dei messaggi
static int comparator(const void *a, const void *b)
{
    return ((ack *)b)->message_id - ((ack *)a)->message_id;
}

//Funzione display tabella degli ack
void display_ack_table()
{
    printf("=== ACK TABELLA ===============\n");
    for (int row_i = 0; row_i < ACK_TABLE_ROWS; row_i++)
    {
        printf("%d ", row_i);
        if (is_row_free(row_i))
        {
            printf("VUOTO\n");
        }
        else
        {
            ack *ack = ack_table_ptr + row_i;
            printf("%d %d %s\n", ack->pid_receiver, ack->message_id, ack->zombie_at != -1 ? "ZOMBIE" : "");
        }
    }
    printf("===============================\n\n");
}

//Gestione degli errori
static void fatal(char *msg)
{
    perror(msg);
    kill(getppid(), SIGTERM);
    exit(1);
}

//Gestione del gestore dei segnali
void sighandler(int sig)
{
    if (sig == SIGUSR1)
        current_movement++;
    else if (sig == SIGTERM)
    {
        if (teardown_feedback_queue() == -1)
            perror("Ack manager error: Terminando la feedback queue");
        exit(0);
    }
}

_Noreturn void ack_manager_loop(int msg_queue_key)
{
    if (signal(SIGTERM, sighandler) == SIG_ERR)
        fatal("Ack manager error: Settando il SIGTERM nel gestore dei segnali");

    //Sbloccando SIGUSR1 e il gestore di segnali
    sigset_t sigset;
    if (sigemptyset(&sigset) == -1)
        fatal("Ack manager error: Riempiendo il set di segnali");
    if (sigaddset(&sigset, SIGUSR1) == -1)
        fatal("Ack manager error: Rimuovendo SIGUSR1 dal set di segnali");
    if (sigprocmask(SIG_UNBLOCK, &sigset, NULL) == -1)
        fatal("Ack manager error: Impostando il SIG_UNBLOCK nel set di segnali");
    if (signal(SIGUSR1, sighandler) == SIG_ERR)
        fatal("Ack manager error: Impostando SIGUSR1 nel gestore dei segnali");
    if (init_feedback_queue(msg_queue_key) == -1)
        fatal("Ack manager error: Inizializzazione della feedback queue");

    while (true)
    {
        unsigned int remaining = DEVICE_COUNT;
        while (remaining > 0)
            remaining = sleep(remaining);

        // Blocco il semaforo
        if (lock_ack_table() == -1)
            fatal("Ack manager error: Acquisendo il mutex della tabella degli ACK");

        // Sorting in ordine discendente in base all'id
        //Unisce gli ack con lo stesso id vicini
        qsort(ack_table_ptr, ACK_TABLE_ROWS, sizeof(ack), comparator);

        int message_id = -1, streak = 0;

        for (int row_i = 0; row_i < ACK_TABLE_ROWS && !is_row_free(row_i); row_i++)
        {
            // Elimino gli zombie se sono troppo vecchi
            if (ack_table_ptr[row_i].zombie_at != -1 && current_movement - ack_table_ptr[row_i].zombie_at >= 2)
            {
                memset(ack_table_ptr, 0, sizeof(ack) * DEVICE_COUNT);
                row_i += DEVICE_COUNT - 1;
                continue;
            }

            if (ack_table_ptr[row_i].message_id == message_id)
            {
                streak++;
                if (streak == DEVICE_COUNT)
                {
                    //Mando messaggi al client
                    feedback feedback_msg = {
                        .message_id = message_id,
                    };
                    memcpy(feedback_msg.acks, ack_table_ptr + row_i - DEVICE_COUNT + 1, sizeof(ack[DEVICE_COUNT]));
                    if (msgsnd(queue_id, &feedback_msg, sizeof(feedback) - sizeof(long), 0) == -1)
                        perror("Ack manager error: Invio del feedback al client");

                    // Setto gli ack di questo messaggio a zombie
                    for (int go_back = 0; go_back < DEVICE_COUNT; go_back++)
                        ack_table_ptr[row_i - go_back].zombie_at = current_movement;
                }
            }
            else
            {
                message_id = ack_table_ptr[row_i].message_id;
                streak = 1;
            }
        }

        // Sblocco il semaforo
        if (unlock_ack_table() == -1)
            fatal("Ack manager error: Rilasciando il mutex della tabella degli ACK");
    }
}
