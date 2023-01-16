#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <signal.h>
#include <pthread.h>
#include "uart.h"
#include "queue.h"

const int MSG_MAX_SIZE = 1024*1024;
//const char uart_dev[5][15] = {"/dev/ttymxc0","/dev/ttymxc1","/dev/ttymxc2"};
//const char uart_dev[5][15] = {"/dev/ttyS0","/dev/ttyS1","/dev/ttyS2","/dev/ttyS3","/dev/ttyS4"};
const char uart_dev[5][15] = {"/dev/ttyUSB0","/dev/ttyUSB1","/dev/ttyUSB2","/dev/ttyUSB3"};
static S_QUEUE uart_queue[5] = {0};
char *port_buf[5] = {NULL,NULL,NULL,NULL,NULL};

typedef void (*mrecvcb_t)(char *buff,int len);

mrecvcb_t mrecvcb = NULL;
static int exitflag = 0;


static int g_fd [5] = {0};
static struct termios old_cfg = {0};

int getPortNum(int fd)
{
    int i;
    for(i = 0;i < 5;i++){
        if(g_fd[i] == fd){
            return i;
        }
    }
    return -1;
}
int print_hex(unsigned char *buffer,int len)
{
    int i;
    //printf("print_hex len = %d \n",len);
    for(i = 0;i < len;i ++)
    {
      printf("%02x",buffer[i]);
    }
    printf("\n");
    return 0;
}
long long Sys_GetTickCount()
{
    struct timeval te; 
    gettimeofday(&te, NULL); // get current time
    long long milliseconds = te.tv_sec*1000LL + te.tv_usec/1000; // calculate milliseconds
    
    return milliseconds;
}
void *threadReadData(void *arg)
{
    fd_set fdsread;
    int fd = (int)arg;
    char pbuf[100] = {0};
    struct timeval tv_read_timeout;
    int port = getPortNum(fd);

    if(port < 0 || port > 5){
        printf("get port error!\n");
        return NULL;
    }
    printf("handle_serial_dev_reading start fd =%d. port:%d ..\n",fd,port);

    
    while (1 ) {
        FD_ZERO(&fdsread);
        FD_SET(fd, &fdsread);

        tv_read_timeout.tv_sec  = 2;
        tv_read_timeout.tv_usec = 0;

        int rc = select(fd + 1, &fdsread, NULL, NULL, &tv_read_timeout);
        if ((rc < 0 && errno == EINTR) ||  rc == 0) {
            continue;
        }
        memset(pbuf,0,100);
        if (FD_ISSET(fd, &fdsread)) {
            int rc = read(fd, pbuf, 100);
            if (rc < 0) {
                printf("[threadReadData]recv data header error %s %d", strerror(errno), errno);
                break;
            } else if (rc == 0) {
                printf("[threadReadData]device close..");
                break;
            }
            if(mrecvcb){
                mrecvcb(pbuf, rc);
            }
            if(rc > 0){
                //printf("rcv:%d pbuf=%02x\n",rc,pbuf[0]);
                //printf("%s",pbuf);
                printf("recv string:%s \n\n",pbuf);
                printf("recv from uart:\n");
                print_hex(pbuf,rc);
                QueuePut(&uart_queue[port],(unsigned char*)pbuf,rc);
            }
        }
    }
    return NULL;    
}

int uart_start(int fd)
{
    pthread_t thrd;
    int ret = 0;

    if(fd < 0) return -1;
    printf("uart_start fd = %d\n",fd);
    ret = pthread_create(&thrd, NULL,threadReadData, (void*)fd);
    return ret;
}


int uart_open(int port,int baudrate,int dbit,char parity,stop_bit_t stb)
{
    char *dev_name = NULL;
    speed_t speed = 0;
    int fd = 0;
    struct termios cfg = {0};

    if(port <= 5){
        dev_name = (char*)uart_dev[port];
    }else{
        return -1;
    }
    if(g_fd[port] > 0)
        return 0;

   fd  = open(dev_name,O_RDWR|O_NOCTTY);
    if(fd == -1){
        printf("Couldn't open %s !\r\n",dev_name);
        return -2;
    }
    printf("fd = %d\n",fd);
    if(tcgetattr(fd,&old_cfg) < 0){
        printf("get cfg erro!");
        close(g_fd[port]);
        g_fd[port] = -1;
        return -3;
    }
    cfmakeraw(&cfg);

    switch(baudrate){
        case 4800:
        speed = B4800;
        break;
        case 9600:
        speed = B9600;
        break;
        case 115200:
        printf("baud:115200\n");
        speed = B115200;
        break;
        case 230400:
        speed = B230400;
        break;
        case 460800:
        speed = B460800;
        break;
        default:
        speed = B115200;
        break;
    }

    cfg.c_cflag |= CREAD;
    if(cfsetspeed(&cfg,speed) < 0){
        printf("set speed error!\n");
        close(fd);
        return -4;
    }
    cfg.c_cflag &= ~CSIZE;
    switch(dbit){
        case 6:
        cfg.c_cflag |= CS6;
        break;
        case 7:
        cfg.c_cflag |= CS7;
        break;
        case 8:
        cfg.c_cflag |= CS8;
        break;
        default:
        cfg.c_cflag |= CS8;
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
    }else{
        cfg.c_cflag &= ~CSTOPB;
    }
 
    cfg.c_cc[VTIME] = 0;
    cfg.c_cc[VMIN] = 0;
    if(tcflush(fd,TCIOFLUSH) < 0){
        printf("tc flush error!\n");
        close(fd);
        return -5;
    }
    if(tcsetattr(fd,TCSANOW,&cfg) < 0){
        printf("tc set attrbute error!\n");
        close(fd);
        return -6;
    }
    g_fd[port] = fd;

    uart_start(fd);
    port_buf[port] = (char*)malloc(MSG_MAX_SIZE);
    memset(port_buf[port],0,sizeof(MSG_MAX_SIZE));
    memset(&uart_queue[port],0,sizeof(S_QUEUE));
    QueueInit(&uart_queue[port],port_buf[port],MSG_MAX_SIZE);
    
    return 0;
}

int uart_read(int port,char *buff,int len,int timeout)
{
    int ret = 0, recv_len = 0;
    long long begin_time = Sys_GetTickCount();
    
    while(1){
        ret = QueueLen(&uart_queue[port]);
        if(ret > 0){
           ret = QueueGet(&uart_queue[port],(unsigned char*)buff+recv_len,len - recv_len);            
           recv_len += ret;
        }
        
		if(recv_len >= len){           
			break;
        }
		if((timeout >= 0) &&(Sys_GetTickCount() - begin_time > timeout)){
			break;
        }
        usleep(100);
    }
    
    return recv_len;
}
int uart_write(int port,char *buff,int len)
{
    int fd = 0;
    int ret = 0;
    if(port > 5){
        return -1;
    }
    fd = g_fd[port];
    if(fd < 0){
        return -1;
    }
    ret = write(fd,buff,len);
    return ret;
}
int uart_close(int port)
{
    int fd = 0;

    if(port > 5){
        return -1;
    }
    fd = g_fd[port];
    if(fd < 0){
        return -1;
    }
    exitflag = 1;
    sleep(1);
    tcsetattr(fd,TCSANOW,&old_cfg);
    close(fd);
    g_fd[port] = -1;
    printf("close ok!\n");
    return 0;
}



