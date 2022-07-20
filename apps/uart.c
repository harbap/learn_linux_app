#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <signal.h>

typedef enum {
    STOP_1,
    STOP_2,
}stop_bit_t;

const char uart_dev[3][15] = {"/dev/ttymxc0","/dev/ttymxc1","/dev/ttymxc2"};
static int fd [3] = {0};
int uart_open(int port,int baudrate,int dbit,char parity,stop_bit_t stb)
{
    char *dev_name = NULL;
    speed_t speed = 0;
    struct termios cfg = {0};

    if(port <= 2){
        dev_name = uart_dev[port];
    }else{
        return -1;
    }
    if(fd[port] > 0)
        return 0;
    switch(baudrate){
        case 4800:
        speed = B4800;
        break;
        case 9600:
        speed = B9600;
        break;
        case 115200:
        speed = B115200;
        break;
        case 230400:
        speed = B230400;
        break;
        case 460800:
        speed = B460800;
        break;
    }
    cfg.c_cflag &= CSIZE;
    switch(dbit){
        case 6:
        cfg.c_cflag = CS6;
        break;
        case 7:
        cfg.c_cflag = CS7;
        break;
        case 8:
        cfg.c_cflag = CS8;
        break;
        default:
        cfg.c_cflag = CS8;
        break;
    }
    switch(parity){
        case 'N':
        cfg.c_cflag &= ~PARENB;
        cfg.c_iflag &= ~INPCK;
        break;
        case 'O':
        cfg.c_cflag |= (PARENB | PARODD);
        cfg.c_iflag |= INPCK;
        break;
        case 'E':
        cfg.c_cflag |= (PARENB);
        cfg.c_cflag &= ~PARODD;
        cfg.c_iflag |= INPCK;
        break;
        default:
        cfg.c_cflag &= ~PARENB;
        cfg.c_iflag &= ~INPCK;
        break;
    }
    if (stb == STOP_1){
        cfg.c_cflag &= ~CSTOPB;
    }else if(stb == STOP_2){
        cfg.c_cflag |= CSTOPB;
    }
    fd[port]  = open(dev_name,O_RDWR|O_NOCTTY);
    if(fd[port] == -1){
        printf("Couldn't open %s !\r\n",dev_name);
        return -2;
    }



    return 0;
}
