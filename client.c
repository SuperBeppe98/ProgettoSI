#include <stdio.h>
#include <sys/msg.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>

#include "message.h"
#include "ack.h"

//Usato per effettuare il sort degli ack con il loro timestamp
static int comparator(const void *a, const void *b)
{
    return ((ack *)a)->timestamp - ((ack *)b)->timestamp;
}

//File di output
int output_file_fd;

//Gestione errori
static void fatal(char *msg)
{
    if (output_file_fd != 0 && output_file_fd != -1)
        if (close(output_file_fd) == -1)
            perror("Client error: Chiusura file di output");
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Usage: Client <msg_queue_key>\n");
        return 1;
    }

    //Creazione message queue
    int queue_id = msgget(atoi(argv[1]), S_IRUSR | S_IWUSR);
    if (queue_id == -1)
        fatal("Client error: Aprendo la coda dei feedback");

    //Setto il pid del mittente
    msg msg = {
        .pid_sender = getpid(),
    };

    //Riempio i campi del messaggio

    printf("PID: ");
    scanf(" %d", &msg.pid_receiver);

    printf("ID DEL MESSAGGIO: ");
    scanf(" %d", &msg.id);

    printf("CONTENUTO: ");
    scanf(" %[^\n]s", msg.content);

    printf("DISTANZA MASSIMA: ");
    scanf(" %lf", &msg.max_dist);

    //Invio il messaggio
    if (send_msg(&msg) == -1)
        fatal("Client error: Inviando il messaggio alla coda FIFO");

    feedback feedback;
    //Ricezione del messaggio da queue a feedback
    if (msgrcv(queue_id, &feedback, sizeof(feedback) - sizeof(long), msg.id, 0) == -1)
        fatal("Client error: Ricevendo il feedback dall'ACK manager");

    qsort(feedback.acks, DEVICE_COUNT, sizeof(ack), comparator);

    char output_path[256];
    sprintf(output_path, "out_%d.txt", msg.id);

    //Creo il file di output
    output_file_fd = open(output_path, O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR);
    if (output_file_fd == -1)
        fatal("Client error: aprendo il file di output");

    //Setto l'header del file di output
    char header[512];
    int char_count = sprintf(header, "Messaggio %d: %s\nLista acknowledgement:\n", msg.id, msg.content);

    //Scrivo l'header sul file di output
    write(output_file_fd, header, char_count);

    for (int i = 0; i < DEVICE_COUNT; i++)
    {
        ack *ack_ptr = feedback.acks + i;

        //Set timestamp
        char formatted_time[64];
        strftime(formatted_time, 64, "%Y-%M-%d %H:%m:%S", localtime(&ack_ptr->timestamp));

        //Set riga del messaggio
        char row[256];
        char_count = sprintf(row, "%d, %d, %s\n", ack_ptr->pid_sender, ack_ptr->pid_receiver, formatted_time);

        //Scrivo sul file di outputs
        if (write(output_file_fd, row, char_count) < char_count)
            fatal("Client error: Scrivendo nel file di output");
    }

    //Chiudo il file di output
    if (close(output_file_fd) == -1)
        fatal("Client error: Chiudendo il file di output");

    return 0;
}