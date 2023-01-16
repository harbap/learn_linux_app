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


typedef enum {
    STOP_1,
    STOP_2,
}stop_bit_t;

const int MSG_MAX_SIZE = 1000;
//const char uart_dev[5][15] = {"/dev/ttymxc0","/dev/ttymxc1","/dev/ttymxc2"};
//const char uart_dev[5][15] = {"/dev/ttyS0","/dev/ttyS1","/dev/ttyS2","/dev/ttyS3","/dev/ttyS4"};
const char uart_dev[5][15] = {"/dev/ttyUSB0","/dev/ttyUSB1","/dev/ttyUSB2","/dev/ttyUSB3"};

typedef void (*mrecvcb_t)(char *buff,int len);

mrecvcb_t mrecvcb = NULL;
static int exitflag = 0;


static int g_fd [5] = {0};
static struct termios old_cfg = {0};
int uart_open(int port,int baudrate,int dbit,char parity,stop_bit_t stb)
{
    char *dev_name = NULL;
    speed_t speed = 0;
    int fd = 0;
    struct termios cfg = {0};

    if(port <= 5){
        dev_name = uart_dev[port];
    }else{
        return -1;
    }
    if(g_fd[port] > 0)
        return 0;

   fd  = open(dev_name,O_RDWR|O_NOCTTY);
    if(g_fd[port] == -1){
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
    printf("uart %d open okay!",port);
    return 0;
}



int uart_read(int port,char *buff,int len,int timeout)
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
    ret = read(fd,buff,len);

    return ret;
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
void threadReadData(void *arg)
{
    fd_set fdsread;
    //int fd = *(int*)arg;
    int fd = (int)arg;
    struct timeval tv_read_timeout;

    printf("handle_serial_dev_reading start fd =%d...",fd);

    char* pbuf = malloc(MSG_MAX_SIZE);// char[MSG_MAX_SIZE];
    while (!exitflag ) {
        FD_ZERO(&fdsread);
        FD_SET(fd, &fdsread);

        tv_read_timeout.tv_sec  = 2;
        tv_read_timeout.tv_usec = 0;

        int rc = select(fd + 1, &fdsread, NULL, NULL, &tv_read_timeout);
        if ((rc < 0 && errno == EINTR) ||  rc == 0) {
            //si->state = DEV_OFFLINE;
            continue;
        }
        memset(pbuf,0,MSG_MAX_SIZE);
        if (FD_ISSET(fd, &fdsread)) {
            //si->state = DEV_ONLINE;
            int rc = read(fd, pbuf, MSG_MAX_SIZE);
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
                printf("%s",pbuf);
            }
        }
    }
    free(pbuf);
    
    printf("threadReadData exit ...\n");

}

int uart_start(int port)
{
    pthread_t thrd;
    int fd = g_fd[port];
    int ret = 0;

    if(fd < 0)
    return -1;
    printf("fd = %d\n",fd);
    ret = pthread_create(&thrd, NULL,threadReadData, (void*)fd);

    return ret;
}


int uart_test()
{
    int ret = 0;
    int cnt = 0;
    char cmd[100] = {0};
    ret = uart_open(1,115200,8,'N',STOP_1);
    if(ret < 0){
        printf("open uart error!\n");
    }else{
        printf("open /dev/ttyS1 ok! \n");
    }

    uart_start(1);
    sprintf(cmd,"%s\r\n","ATE0");
    uart_write(1,cmd,strlen(cmd));
    sleep(1);
    while(!exitflag){
        printf("===cnt %d=====\n",cnt++);
        sprintf(cmd,"%s\r\n","AT");
        printf("cmd:%s",cmd);
        ret = uart_write(1,cmd,strlen(cmd));
        sleep(2);

        sprintf(cmd,"%s\r\n","AT+GMR");
        printf("cmd:%s",cmd);
        ret = uart_write(1,cmd,strlen(cmd));
        sleep(2);

        
        sprintf(cmd,"%s\r\n","AT+BLENAME?");
        printf("cmd:%s",cmd);
        ret = uart_write(1,cmd,strlen(cmd));
        sleep(2);

        sprintf(cmd,"%s\r\n","AT+BLEMAC?");
        printf("cmd:%s",cmd);
        ret = uart_write(1,cmd,strlen(cmd));
        sleep(2);

        sprintf(cmd,"%s\r\n","AT+BLESTATE?");
        printf("cmd:%s",cmd);
        ret = uart_write(1,cmd,strlen(cmd));
        sleep(2);

        sprintf(cmd,"%s\r\n","AT+BLEMODE?");
        printf("cmd:%s",cmd);
        ret = uart_write(1,cmd,strlen(cmd));
        sleep(2);

        sprintf(cmd,"%s\r\n","AT+BLEMODE=1");
        printf("cmd:%s",cmd);
        ret = uart_write(1,cmd,strlen(cmd));
        sleep(5);

        sprintf(cmd,"%s\r\n","AT+BLESCAN");
        printf("cmd:%s",cmd);
        ret = uart_write(1,cmd,strlen(cmd));
        sleep(2);
#if 0
        sprintf(cmd,"%s\r\n","");
        ret = uart_write(1,cmd,strlen(cmd));
        sleep(2);

        sprintf(cmd,"%s\r\n","");
        ret = uart_write(1,cmd,strlen(cmd));
        sleep(2);

        sprintf(cmd,"%s\r\n","");
        ret = uart_write(1,cmd,strlen(cmd));
        sleep(2);
#endif
        sleep(3);
    }
    uart_close(1);
    return 0;
}

int cfg_gpios(cd)
{

    return 0;
}


int gps_test(void)
{
    int ret = 0,cnt = 0;
    char cmd[100] = {0};
    printf("=====gps test======\n");
    ret = uart_open(4,115200,8,'N',STOP_1);
    if(ret < 0){
        printf("open uart error!\n");
    }else{
        printf("open /dev/ttyS4 ok! \n");
    }
    uart_start(4);
    while(!exitflag){
        printf("==========%d===========\n",cnt++);
        sleep(10);
    }
    uart_close(4);


    return 0;
}

void handle_sig(int sig)
{
    printf("singal :%d \n",sig);
    if(sig == SIGINT){
        exitflag = 1;
    }
}

int main()
{
    __sighandler_t ret = 0;
    printf("==============uart test===========\n");

    ret = signal(SIGINT,handle_sig);
    if(ret == SIG_ERR){
        printf("signal error\n");
        return -1;
    }
    //while(1){sleep(1);if(exitflag)break;}
     uart_test();
    //gps_test();
    return 0;
}

