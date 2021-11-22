#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>

static void sig_handler(int sig)
{
    printf("Receive signal:%d \n",sig);
}

void signal_test()
{
    sig_t ret = NULL;

    ret = signal(SIGINT,(sig_t)sig_handler);
    if(ret == SIG_ERR){
        perror("signal error");
        return;
    }
    for(;;){sleep(1);}
}


int test_signal_entry()
{
    printf("*******test signal entry********\n");

    signal_test();
    return 0;
}
