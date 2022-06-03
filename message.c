#include <string.h>
#include <stdio.h>
#include <fcntl.h>

#include "message.h"

//Funzione di invio messaggio
int send_msg(msg *m)
{
    //Seleziono la path del device ricevente
    char fifo_path[64];
    sprintf(fifo_path, "/tmp/dev_fifo.%d", m->pid_receiver);

    //Apro il file FIFO
    int fifo_fd = open(fifo_path, O_WRONLY);
    if (fifo_fd == -1)
        return -1;

    //Scrivo il messaggio nel file
    if (write(fifo_fd, m, sizeof(msg)) < sizeof(msg))
        return -1;

    //Chiudo il file FIFO
    if (close(fifo_fd) == -1)
        return -1;
    return 0;
}
