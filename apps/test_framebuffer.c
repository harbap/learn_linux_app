#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <string.h>
#include "test_frambuffer.h"

static int framb_fd = 0;
static short lcd_width = 0,lcd_height = 0;
static unsigned int *pFrambuff = NULL;        //maped screen addr
static int screen_size = 0;

int open_framb_dev()
{
    if(framb_fd > 0)
        return framb_fd;
    if((framb_fd = open("/dev/fb0",O_RDWR)) < 0){
        printf("Couldn't open /dev/fb0'\n");
        return -1;
    }
    return framb_fd;
}

int get_framb_info()
{
    struct fb_fix_screeninfo fb_fix;
    struct fb_var_screeninfo fb_var;
    
    /*get screen infomation*/
    ioctl(framb_fd,FBIOGET_VSCREENINFO,&fb_var);
    ioctl(framb_fd,FBIOGET_FSCREENINFO,&fb_fix);
    printf(
    "分辨率:%d * %d\n"
    "像素深度:%dbpp\n"
    "一行的字节数:%d\n"
    "像素格式:R<%d %d> G<%d %d> B<%d %d> \n",
    fb_var.xres,fb_var.yres,
    fb_var.bits_per_pixel,
    fb_fix.line_length,
    fb_var.red.offset,fb_var.red.length,
    fb_var.green.offset,fb_var.green.length,
    fb_var.blue.offset,fb_var.blue.length
    );
    lcd_width = fb_var.xres;
    lcd_height = fb_var.yres;
    screen_size = (fb_fix.line_length * fb_var.yres) ;
    return 0;
}

void lcd_draw_point(unsigned short x,unsigned short y,unsigned int color)
{
    int off = 0;
    if(x >= lcd_width)
        x = lcd_width - 1;
    if(y >= lcd_height)
        y = lcd_height - 1;
    off =  y * lcd_width + x;
    *(unsigned int*)(pFrambuff + off) = color;
}
void lcd_draw_line(unsigned short x,unsigned short y,int dir,unsigned short len,unsigned int color)
{
    unsigned int end;
    unsigned long temp;

    if(x >= lcd_width)
        x = lcd_width - 1;
    if(y >= lcd_height)
        y = lcd_height - 1;
    temp = y * lcd_width + x;
    if(dir){            //横
        end = x + len -1;
        if(end >= lcd_width)
            end = lcd_width - 1;
        for( ; x <= end;x++,temp++)
            *(unsigned int*)(pFrambuff + temp) = color;
    }else{            //竖
        end = y + len -1;
        if(end >= len)
            end = lcd_height - 1;
        for( ;y <= end; y++,temp += lcd_width)
            *(unsigned int*)(pFrambuff + temp) = color;
    }
}
void lcd_fill(unsigned short x1,unsigned short y1,unsigned short x2,unsigned short y2,unsigned int color)
{
    unsigned long temp;
    unsigned short off = 0;

    if(x2 >= lcd_width)
        x2 = lcd_width - 1;
    if(y2 >= lcd_height)
        y2 = lcd_height - 1;
    temp = y1 * lcd_width;
    for( ;y1 <= y2;y1++, temp += lcd_width){
        for(off = x1;x1 <= x2;off++){
            *(unsigned int*)(pFrambuff + temp + off) = (int)color;
        }
    }
    
}
void lcd_draw_rectangle(unsigned short x1,unsigned short y1,unsigned short x2,unsigned short y2,unsigned int color,unsigned char fill)
{
    int x_len = x2 - x1 + 1;
    int y_len = y2 - y1 + 1;

    lcd_draw_line(x1,y1,1,x_len,color);
    lcd_draw_line(x1,y2,1,x_len,color);
    lcd_draw_line(x1,y1 + 1,0,y_len,color);
    lcd_draw_line(x2,y2 + 1,0,y_len,color);
    if(fill)
       lcd_fill(x1+1,y1+1,x2-1,y2-1,color); 
}
void lcd_clearn(unsigned int color)
{
    int i = 0,j = 0;
    int locat = 0;
    for(i = 0;i < lcd_height;i++){
        for(j = 0;j < lcd_width;j++){
            locat = i * lcd_width+ j;
            *(unsigned int *)(pFrambuff + locat) = (unsigned int)color;
        }
    }
}
int framb_dev_init(void)
{
    if(open_framb_dev() < 0)
        return -1;
    get_framb_info();
    
    //void *mmap(void *addr, size_t length, int prot, int flags,int fd, off_t offset);
    //int munmap(void *addr, size_t length);
    pFrambuff = (unsigned char*)mmap(NULL,screen_size,PROT_READ|PROT_WRITE,MAP_SHARED,framb_fd,0);
    if((long)pFrambuff == -1){
        printf("mmap failed\n");
        return -2;
    }
    return 0;
}
int framb_dev_uinit(void)
{
    munmap(pFrambuff,screen_size);
    close(framb_fd);
    return 0;
}
void lcd_base_ui_test(void)
{
    lcd_clearn(0xFFFFFF);
    lcd_draw_line(5,100,1,200,0xFF0000);
    lcd_draw_line(105,0,0,200,0xFF0000);
    sleep(1);
    lcd_draw_point(50,200,0xFF00FF);
    lcd_draw_point(51,200,0xFF00FF);
    lcd_draw_point(50,201,0xFF00FF);
    lcd_draw_point(51,201,0xFF00FF);
    sleep(1);
}

int test_frambufer_main()
{
    printf("*********test frambuffer***********\n");
    framb_dev_init();
    lcd_base_ui_test();
    framb_dev_uinit();
    return 0;
}

