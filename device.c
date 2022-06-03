#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <signal.h>

#include "device.h"
#include "movements.h"
#include "message.h"
#include "ack.h"

char fifo_path[64];
int fifo_fd;

//Inizializzazione della fifo del Device
int init_fifo(pid_t pid)
{
    //Set della path del file fifo nella variabile fifo_path
    sprintf(fifo_path, "/tmp/dev_fifo.%d", pid);

    //Creazione del file fifo con permessi di lettura e scrittura
    if (mkfifo(fifo_path, S_IRUSR | S_IWUSR) == -1)
        return -1;

    //Apertura del file fiflo con permessi di lettura
    fifo_fd = open(fifo_path, O_RDONLY | O_NONBLOCK);
    if (fifo_fd == -1)
        return -1;

    return 0;
}

//Chiusura della fifo del Device error
int teardown_fifo()
{
    //Controllo se il file non è già stato chiuso o è nullo
    if (fifo_fd != 0 && fifo_fd != -1)
    {
        //Chiusura del file fifo
        if (close(fifo_fd) == -1)
            return -1;

        //Cancello il file
        if (unlink(fifo_path) == -1)
            return -1;
    }

    return 0;
}

// Process ID del device
pid_t pid;
// Posizione del device nella board
pos_t current_pos;
// Elenco dei messaggi che contiene questo dispositivo
// `message-> next` indicherà il campo all'interno del primo messaggio che questo dispositivo contiene
list_handle_t messages = null_list_handle;

void print_status()
{
    char msg_ids[256] = "";
    list_handle_t *iter;
    list_for_each(iter, &messages)
    {
        char msg_id[8];
        sprintf(msg_id, "%d, ", list_entry(iter, msg, list_handle)->id);
        strcat(msg_ids, msg_id);
    }

    // Rimozione del `, ` se la lista dei messaggi non è vuota
    if (msg_ids[0] != '\0')
    {
        msg_ids[strlen(msg_ids) - 2] = '\0';
    }

    printf("%d %d %d msgs: %s\n", pid, current_pos.x, current_pos.y, msg_ids);
}

//Gestore dei segnali
static void sighandler(int sig)
{
    if (teardown_fifo() == -1)
        perror("Device error: Chiudendo la FIFO");

    list_handle_t *iter;
    list_for_each(iter, &messages)
    {
        msg *message = list_entry(iter, msg, list_handle);
        free(message);
    }

    exit(0);
}

//Gestione degli errori
static void fatal(char *msg)
{
    perror(msg);
    kill(getppid(), SIGTERM);
    exit(1);
}

_Noreturn void device_loop(int device_i)
{
    if (signal(SIGTERM, sighandler) == SIG_ERR)
        fatal("Device error: Impostando il gestore dei segnali");

    //Getter del pid del device
    pid = getpid();

    //Inizializzazione fifo
    if (init_fifo(pid) == -1)
        fatal("Device error: Creando la FIFO");

    while (true)
    {
        // Prima scansione:
        //  1) Invio messaggi
        //  2) Ricezione messaggi
        //  3) Movimento
        if (wait_turn(device_i) == -1)
            fatal("Device error: Turno di attesa, primo giro");

        // 1) Invio messaggi
        for (int x = 0; x < BOARD_COLUMNS; x++)
        {
            for (int y = 0; y < BOARD_ROWS; y++)
            {
                pos_t position = {
                    .x = x,
                    .y = y,
                };

                pid_t target_pid;
                //Se non ci sono Device error in questa cella, saltala
                if ((target_pid = board_get(position)) == 0 || target_pid == pid)
                    continue;

                //Funzione per il controllo della distanza
                double dist = sqrt(pow(current_pos.x - x, 2) + pow(current_pos.y - y, 2));

                list_handle_t *iter;
                list_for_each(iter, &messages)
                {
                    msg *message = list_entry(iter, msg, list_handle);
                    //Blocco la ACK table
                    if (lock_ack_table() == -1)
                        fatal("Device error: Bloccando la ACK table");
                    //Controllo per inviare o no il messaggio
                    bool should_send = dist <= message->max_dist && !has_dev_received_msg(target_pid, message->id);
                    //Sblocco la ACK table
                    if (unlock_ack_table() == -1)
                        fatal("Device error: Sbloccando la ACK table");
                    //Se deve inviarlo
                    if (should_send)
                    {
                        message->pid_sender = pid;
                        message->pid_receiver = target_pid;
                        //Invio messaggio
                        if (send_msg(message) == -1)
                            fatal("Device error: Trasmettendo il messaggio");
                    }
                }
            }
        }

        //Cancellazione dei messaggi perchè li abbiamo appena inviati
        list_handle_t *iter;
        list_for_each(iter, &messages)
        {
            msg *message = list_entry(iter, msg, list_handle);
            free(message);
        }
        messages.next = NULL;

        // 2) Ricezione messaggi
        // Continua a leggere i messaggi ma malloc sull'heap solo se esiste un messaggio
    read_loop:
        while (true)
        {
            //Messaggio salvato nello stack.
            msg msg_temp;
            int br = read(fifo_fd, &msg_temp, sizeof(msg));
            if (br < sizeof(msg))
                break;

            //Salta il messaggio se lo ha gia ricevuto
            list_for_each(iter, &messages)
            {
                msg *message = list_entry(iter, msg, list_handle);
                if (message->id == msg_temp.id)
                    goto read_loop;
            }

            //Malloc del messaggio
            msg *message_ptr = malloc(sizeof(msg));
            if (message_ptr == NULL)
                fatal("Device error: Allocando la memoria per i messaggi");

            //Copia del messaggio dallo stack all'heap
            memcpy(message_ptr, &msg_temp, sizeof(msg));
            //Controllo se la lista è "pulita"
            message_ptr->list_handle = (list_handle_t)null_list_handle;
            list_insert_after(&messages, &message_ptr->list_handle);

            //Blocco la ACK table
            if (lock_ack_table() == -1)
                fatal("Device error: Bloccando la tabella degli ACK");
            //Aggiungo ACK
            int add_ack_res = add_ack(message_ptr);
            //Sblocco la ACK table
            if (unlock_ack_table() == -1)
                fatal("Device error: Sbloccando la tabella degli ACK");
            //Controllo se ACK ci sta nella tabella
            if (add_ack_res == -1)
            {
                printf("Device error: La tabella degli ACK è piena");
                kill(getppid(), SIGTERM);
                exit(1);
            }
        }

        // 3) Movimento
        if (current_movement > 0)
        {
            //Set a 0 della posizione corrente
            board_set(current_pos, 0);
        }

        // Set nuova posizione
        current_pos = movements_mem_ptr[current_movement][device_i];
        board_set(current_pos, pid);

        if (pass_turn(device_i) == -1)
            fatal("Device error: Errore nel turno di passaggio, 1° scansione");

        //Seconda scansione, stampa dello status e rimozione dei messaggi
        if (wait_turn(device_i) == -1)
            fatal("Device error: Errore nel turno di attesa, 2° scansione");
        print_status();
        if (pass_turn(device_i) == -1)
            fatal("Device error: Errore nel turno di passaggio, 2° scansione");

        current_movement++;
    }
}
