#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>

#include "device.h"
#include "settings.h"
#include "board.h"
#include "movements.h"
#include "ack.h"

//Struttura dei pids
struct
{
    pid_t ack_manager;
    pid_t devices[DEVICE_COUNT];
} pids;

//Killa i figli, li aspetta per terminare, dealloca la memoria e si chiude
void die(int code)
{
    for (int i = 0; i < DEVICE_COUNT; i++)
        if (kill(pids.devices[i], SIGTERM) == -1)
            perror("Server error: Kill di un figlio");
    if (kill(pids.ack_manager, SIGTERM) == -1)
        perror("Server error: Kill dell'ack manager");

    //Attende la terminazione dei figli
    while (wait(NULL) != -1)
        ;

    // Chiusura memoria allocata e intercommunication process
    if (teardown_board() == -1)
        perror("Server error: Chiudere la board");
    teardown_steps();
    if (teardown_move_semaphores() == -1)
        perror("Server error: Chiudere i semafori dei movimenti");
    if (teardown_ack_table() == -1)
        perror("Server error: Chiudere la tabella ack");

    exit(code);
}

static void fatal(char *msg)
{
    perror(msg);
    die(1);
}

//Gestore degli errori
static void sighandler(int sig)
{
    die(0);
}

//Gestore dei segnali e chiusura del server error
int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("Usare: server <msg_queue_key> <movements_file_path>\n");
        return 1;
    }

    // Blocco di ogni segnale tranne SIGTERM e gestore di segnali
    sigset_t sig_set;
    if (sigfillset(&sig_set) == -1)
        fatal("Server error: Riempiendo il set di segnali");
    if (sigdelset(&sig_set, SIGTERM) == -1)
        fatal("Server error: Rimuovendo SIGTERM dal set di segnali");
    if (sigprocmask(SIG_SETMASK, &sig_set, NULL) == -1)
        fatal("Server error: Impostando il SIG_SETMASK nel set di segnali");
    if (signal(SIGTERM, sighandler) == SIG_ERR)
        fatal("Server error: Impostando il gestore di segnali");

    //Inizializzazione delle componenti del server
    if (initialize_board() == -1)
        fatal("Server error: Inizializzazione della board");
    if (initialize_steps(argv[2]) == -1)
        fatal("Server error: Inizializzazione dei movimenti");
    if (initialize_move_semaphores() == -1)
        fatal("Server error: Inizializzazione dei semafori dei movimenti");
    if (initialize_ack_table() == -1)
        fatal("Server error: Inizializzazione della tabella degli ack");

    //Creazione ack manager
    if (!(pids.ack_manager = fork()))
        ack_manager_loop(atoi(argv[1]));

    //Creazione device
    for (int dev_i = 0; dev_i < DEVICE_COUNT; dev_i++)
        if (!(pids.devices[dev_i] = fork()))
            device_loop(dev_i);

    //Esecuzione movimenti dei device
    for (int step_i = 0; step_i < movements_count; step_i++)
    {
        printf("## movimento %d: posizione del device ###########\n", step_i);
        if (perform_step() == -1)
            fatal("Server error: Esecuzione movimento");
        printf("#######################################\n\n");

        //Invia all'ack manager il SIGUSR1
        if (kill(pids.ack_manager, SIGUSR1) == -1)
            fatal("Server error:  Gestore ACK del movimento attuale");

        sleep(2);
    }
    //Se finisce le righe di posizioni si chiude
    die(0);
}