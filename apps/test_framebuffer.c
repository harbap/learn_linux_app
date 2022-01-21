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
#include <math.h>
#include "test_frambuffer.h"

typedef struct{
    unsigned char type[2];      //file types
    unsigned int size;          //file sizes
    unsigned short reserved[2]; //reserverd
    unsigned int offset;        //bitmap date ofset
}__attribute__((packed))bmp_file_header;
typedef struct{
    unsigned int size;          //bitmap infomation size
    int width;                  //image width    
    int height;                 //image height
    unsigned short planes;      //bit planes
    unsigned short bpp;         //pixel deepth
    unsigned int compression;   //compression method
    unsigned int image_size;    //image size
    int x_pels;                 //x pixels per meter
    int y_pels;                 //y pixels per meter
    unsigned int clr_used; 
    unsigned int clr_omportant;
}__attribute__((packed))bmp_info_header;

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
        if(end >= lcd_height)
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
        for(off = x1;off <= x2;off++){
            *(unsigned int*)(pFrambuff + temp + off) = (int)color;
        }
    }
    
}
void lcd_draw_rectangle(unsigned short x1,unsigned short y1,unsigned short x2,unsigned short y2,unsigned int color,unsigned char fill)
{
    int x_len = x2 - x1 + 1;
    int y_len = y2 - y1 - 1;

    if(x_len < 0 || y_len < 0)
        return;
    //printf("x_len:%d y_len:%d\n",x_len,y_len);
    lcd_draw_line(x1,y1,1,x_len,color);
    lcd_draw_line(x1,y2,1,x_len,color);
    lcd_draw_line(x1,y1 + 1,0,y_len,color);
    lcd_draw_line(x2,y1 + 1,0,y_len,color);
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
    pFrambuff = (unsigned int*)mmap(NULL,screen_size,PROT_READ|PROT_WRITE,MAP_SHARED,framb_fd,0);
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

int read_bmp_file(const char *fname,unsigned char **imgdata,bmp_info_header *info)
{
    bmp_file_header header;
    int fd = 0,ret = 0,imgdata_len = 0;

    if((fd = open(fname,O_RDONLY)) < 0){
        printf("%s file open filed!\n",fname);
        return -1;
    }
    ret = read(fd,&header,sizeof(bmp_file_header));
    if(ret < 0){
        printf("read bmp head failed!\n");
        close(fd);
        return -2;
    }
    if(memcmp(header.type,"BM",2) != 0){
        printf("It's not a bmp file!\n");
        return -3;
    }
    ret = read(fd,info,sizeof(bmp_info_header));
    if(ret < 0){
        printf("read bmp info failed ! \n");
        close(fd);
        return -4;
    }
    printf("文件大小:%d\n"
            "位图数据偏移量:%d\n"
            "位图信息头大小:%d\n"
            "图像分辨率:%d*%d\n"
            "像素深度:%d\n",header.size,header.offset,info->size,info->width,info->height,info->bpp);
    lseek(fd,header.offset,SEEK_SET);
    if(info->bpp == 16)
        imgdata_len = info->width * info->height * 2;
    else if(info->bpp == 24)
        imgdata_len = info->width * info->height * 3;
    else{
        printf("不支持的图像深度!\n");
        close(fd);
        return -6;
    }
    *imgdata = malloc(imgdata_len);
    if(*imgdata == NULL){
        printf("malloc error!");
        close(fd);
        return -5;
    }
    read(fd,*imgdata,imgdata_len);
    close(fd);
    return 0;
}
int lcd_show_image(unsigned short x,unsigned short y,unsigned short w,unsigned short h,unsigned char *data,unsigned short bpp,unsigned char dir)
{
    short i = 0,j = 0;
    int off = 0,idx = 0;

    off = y * lcd_width + x;
    if(dir == 0)
        idx = 0;
    else
        idx = w * h * bpp / 8 - 1;
    w -= 1;
    printf("off = %d x = %d y = %d w = %d h = %d dir = %d\n",off,x,y,w,h,dir);
    if(bpp == 24){
        for(j = 0;j < h;j ++){
            for(i = w;i >= 0;i --){
                if(dir == 0){
                    *(unsigned int*)(pFrambuff + off + i) = data[idx] | data[idx+1] << 8 | data[idx+2] << 16;
                    idx += 3;
                }else {
                    *(unsigned int*)(pFrambuff + off + i) = data[idx-2] | data[idx-1] << 8 | data[idx] << 16; 
                    idx -= 3;
                }
            }
            off += lcd_width;
        }

    }else if(bpp == 16){
        unsigned short color;
        unsigned char R,G,B;
       
         for(j = 0;j < h;j ++){
            for(i = w;i >= 0;i --){
                if(dir == 0){
                    color = (data[idx] << 0 )|( data[idx + 1] << 8);            //未验证
                    //RGB565 转 RGB888
                    R = (color & 0xF800) >> 11; 
                    G = (color & 0x07E0) >> 4;              // ??? why not 5
                    B = (color & 0x001F);
                    R = (unsigned char)(R * 255/ 31);
                    G = (unsigned char)(G * 255 / 63 );
                    B = (unsigned char)(B * 255.0 / 31);
                    *(unsigned int*)(pFrambuff + off + i) = (B << 0) | (G << 8) |(R << 16);   
                    idx += 2;
                }else {
                    #if 1 
                    color = (data[idx - 1] << 0 )|( data[idx] << 8);                                    
                    R = (color & 0xF800) >> 11;
                    G = (color & 0x07E0) >> 4;              // ??? why not 5
                    B = (color & 0x001F);
                    R = (unsigned char)(R * 255/ 31);
                    G = (unsigned char)(G * 255 / 63 );
                    B = (unsigned char)(B * 255.0 / 31);
                   *(unsigned int*)(pFrambuff + off + i) = (B << 0) | (G << 8) |(R << 16);
                    #else
                    *(unsigned int*)(pFrambuff + off + i) = ((color&0x001F) << 3) | ((color& 0x07E0) << 4) | ((color & 0xF800) << 8); 
                    #endif
                    idx -= 2;
                   // printf("G = %02X ",G);
                }
            }
            off += lcd_width;
        }
    }
    printf("lcd_show_image out\n");
    return 0;
}
int show_bmp_image(unsigned short x,unsigned short y,char *fname,unsigned char isCenter)
{
    unsigned char *imgdata = NULL;
    bmp_info_header info;
    int ret = 0;
    printf("show_bmp_image...\n");
    ret = read_bmp_file(fname,&imgdata,&info);
    if(ret < 0){
        return ret;
    }
    printf("lcd_show_image...\n");
    if(isCenter){
        x = (lcd_width - info.width)/2;
        y = (lcd_height - info.height)/2;
    }
    if(info.height > 0)
        lcd_show_image(x,y,info.width,info.height,imgdata,info.bpp,1);      //倒向位图
    else
        lcd_show_image(x,y,info.width,info.height,imgdata,info.bpp,0);      //正向位图
    free(imgdata);
    return 0;
}

void set_rgb_color(unsigned char *data,int len,unsigned short color)
{
    int i;
    unsigned short *p = (unsigned char*)data;
    for(i = 0;i < len;i += 2){
        *p = color;
        p++;
    }
}

void lcd_base_ui_test(void)
{
    int i = 1,cnt = 10;
    unsigned char align = 0;
    char fname[20] = {0};
    unsigned char img[10000] = {0};
    lcd_clearn(0xFFFFFF);
    lcd_draw_line(5,100,1,200,0xFF0000);
    lcd_draw_line(105,0,0,200,0xFF0000);
  //  sleep(1);
    lcd_draw_point(50,200,0xFF00FF);
    lcd_draw_point(51,200,0xFF00FF);
    lcd_draw_point(50,201,0xFF00FF);
    lcd_draw_point(51,201,0xFF00FF);
   // sleep(1);
    lcd_draw_rectangle(200,10,240,40,0xFF22F0,0);
    lcd_draw_rectangle(250,10,290,40,0xFF22F0,0);
    lcd_draw_rectangle(150,60,180,100,0xFF22F0,1);
    lcd_draw_rectangle(250,60,300,110,0xFF22F0,1);
    sleep(1);
    set_rgb_color(img,sizeof(img),0xF800);
    lcd_show_image(600,0,20,20,img,16,1);
    set_rgb_color(img,sizeof(img),0x07E0);
    lcd_show_image(630,0,20,20,img,16,1);
    set_rgb_color(img,sizeof(img),0x001F);
    lcd_show_image(660,0,20,20,img,16,1);
    sprintf(fname,"./img/%d.bmp",10);    
    show_bmp_image(0,0,fname,0);
    sleep(5);
    while(cnt --){
    for(i = 1; i <= 15;i ++){
        sprintf(fname,"./img/%d.bmp",i);
        if(i >= 8){
            align = 1;
            lcd_clearn(0xFFFFFF);
        }else{
            align = 0;
        }
        show_bmp_image(0,0,fname,align);
        sleep(2);
    }
    }
}
void lcd_set_bklight_alwaslon(void)
{
    printf("set backlight alwasl on\n");
    int fd = open("/dev/tty1",O_RDWR);
    write(fd,"\033[9,0]",8);
    close(fd);
}
int test_frambufer_main()
{
    printf("*********test frambuffer***********\n");
    framb_dev_init();
    //lcd_set_bklight_alwaslon();
    lcd_base_ui_test();
    framb_dev_uinit();
    return 0;
}

