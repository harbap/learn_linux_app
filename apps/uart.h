#ifndef _UART_H_
#define _UART_H_


typedef enum {
    STOP_1,
    STOP_2,
}stop_bit_t;

int uart_open(int port,int baudrate,int dbit,char parity,stop_bit_t stb);
int uart_read(int port,char *buff,int len,int timeout);
int uart_write(int port,char *buff,int len);
int uart_close(int port);
int print_hex(unsigned char *buffer,int len);
long long Sys_GetTickCount();
#endif 

