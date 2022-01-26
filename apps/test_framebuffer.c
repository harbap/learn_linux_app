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
#include <jpeglib.h>
#include <png.h>
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

typedef struct bgr888_color{
    unsigned char blue;
    unsigned char green;
    unsigned char red;
}__attribute__((packed))bgr888_t;

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
int lcd_show_bmp_image(unsigned short x,unsigned short y,unsigned short w,unsigned short h,unsigned char *data,unsigned short bpp,unsigned char dir)
{
    short i = 0,j = 0;
    int off = 0,idx = 0;

    off = y * lcd_width + x;
    if(dir == 0)
        idx = 0;
    else
        idx = w * h * bpp / 8 - 1;
    w -= 1;
 //   printf("off = %d x = %d y = %d w = %d h = %d dir = %d\n",off,x,y,w,h,dir);
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
  //  printf("show_bmp_image...\n");
    ret = read_bmp_file(fname,&imgdata,&info);
    if(ret < 0){
        return ret;
    }
 //   printf("lcd_show_image...\n");
    if(isCenter){
        x = (lcd_width - info.width)/2;
        y = (lcd_height - info.height)/2;
    }
    if(info.height > 0)
        lcd_show_bmp_image(x,y,info.width,info.height,imgdata,info.bpp,1);      //倒向位图
    else
        lcd_show_bmp_image(x,y,info.width,info.height,imgdata,info.bpp,0);      //正向位图
    free(imgdata);
    return 0;
}

void set_rgb_color(unsigned char *data,int len,unsigned short color)
{
    int i;
    unsigned short *p = (unsigned short*)data;
    for(i = 0;i < len;i += 2){
        *p = color;
        p++;
    }
}
int lcd_show_rgbimage(unsigned short x,unsigned short y,unsigned short w,unsigned short h,unsigned char *imgdata)
{
    short i = 0,j = 0;
    int off = 0,idx = 0;

    if(x + w > lcd_width){
        return -1;
    }
    if(y + h > lcd_width){
        return -2;
    }
    off = y * lcd_width + x;
   // printf("off = %d w = %d h = %d\n",off,w,h);
    for(j = 0;j < h;j ++){
        for(i = 0;i < w;i ++){                   //    B               G                      R
            *(unsigned int*)(pFrambuff + off + i) = imgdata[idx] | imgdata[idx+1] << 8 | imgdata[idx+2] << 16;
            idx += 3;
        }
        off += lcd_width;
    }
    return 0;
}
int read_jpeg_image(char *fname,unsigned char **imgdata,int *width,int *height)
{
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    bgr888_t *jpeg_line_buff = NULL;
    FILE* fp = NULL;
    int scale= 0, scalew = 0,scaleh = 0,i=0,idx = 0;

    printf("==============read_jpeg_imagen==================\n");
    //banding err handled function
    cinfo.err = jpeg_std_error(&jerr);
    fp = fopen(fname,"r");
    if(fp == NULL){
        printf("read %s failed\n",fname);
        return -1;
    }
    //创建解码对象
    jpeg_create_decompress(&cinfo);
    //指定图像信息
    jpeg_stdio_src(&cinfo,fp);
    //读取图像信息
    jpeg_read_header(&cinfo,TRUE);
    printf("pic size:%d * %d \n",cinfo.image_width,cinfo.image_height);
    cinfo.scale_num = 1;
    cinfo.scale_denom = 1;
    if(cinfo.image_width > lcd_width||cinfo.image_height > lcd_height){
        scalew = cinfo.image_width / lcd_width;
        if(cinfo.image_width % lcd_width != 0){
            scalew += 1;
        }
        scaleh = cinfo.image_height / lcd_height;
        if(cinfo.image_height % lcd_height != 0){
            scaleh += 1;
        }
        scale = scalew > scaleh ? scalew : scaleh;
        cinfo.scale_denom = scale;

    }
    //设置解码参数
    cinfo.out_color_space = JCS_RGB;
    jpeg_start_decompress(&cinfo);
    printf("output size:%d * %d  scale_demom:%d \n",cinfo.output_width,cinfo.output_height,cinfo.scale_denom);
    jpeg_line_buff = malloc(cinfo.output_components * cinfo.output_width);
    idx = cinfo.output_components * cinfo.output_width*cinfo.output_height;
   // printf("idx = %d\n",idx);
    *imgdata = (unsigned char*)malloc(idx);
    if(jpeg_line_buff == NULL || *imgdata == NULL){
        printf("malloc error!\n");
        return -2;
    }
    *width = cinfo.output_width;
    *height = cinfo.output_height;
    idx = 0;
    while(cinfo.output_scanline < cinfo.output_height){
        jpeg_read_scanlines(&cinfo,(unsigned char**)&jpeg_line_buff,1);
        //printf("lines = %d\n",cinfo.output_scanline);
        //将BGR888 转换为RGB888
        for(i = 0;i < cinfo.output_width;i++){
             (*imgdata)[idx++] = jpeg_line_buff[i].red;
             (*imgdata)[idx++] = jpeg_line_buff[i].green;
             (*imgdata)[idx++] = jpeg_line_buff[i].blue;
        }
    }
    //printf("finish decompress\n");

    jpeg_finish_decompress(&cinfo);
 
    jpeg_destroy_decompress(&cinfo);
    fclose(fp);

    return 0;
}

int lcd_display_jpeg(unsigned short x,unsigned short y,char *fname,lcd_align_t align)
{
    int w = 0,h = 0,ret = 0;
    unsigned char *imgdata = NULL;
    ret = read_jpeg_image(fname,&imgdata,&w,&h);
    if(ret != 0){
        return ret;
    }
    //printf("w = %d h = %d\n",w,h);
    if(align == align_center){
        x = (lcd_width - w)/2;
        y = (lcd_height - h)/2;
    }
    lcd_show_rgbimage(x,y,w,h,imgdata);
    free(imgdata);
    return 0;
}
int read_png_image(char *fname,unsigned char **imgdata,int *w,int *h)
{
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    unsigned int imgw = 0,imgh = 0,i,row_bytes = 0;
    png_bytepp row_pointers = NULL;
    unsigned char bitdep = 0,color_type = 0;
    FILE *fp = NULL;

    fp = fopen(fname,"r");
    if(fp == NULL){
        printf("open %s failed\n",fname);
        return -1;
    }
    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,NULL,NULL,NULL);
    if(png_ptr == NULL){
        fclose(fp);
        return -2;
    }
    info_ptr = png_create_info_struct(png_ptr);
    if(info_ptr == NULL){
        png_destroy_read_struct(&png_ptr,NULL,NULL);
        fclose(fp);
        return -2;
    }
    //设置错误返回点
    if(setjmp(png_jmpbuf(png_ptr))){
        png_destroy_read_struct(&png_ptr,NULL,NULL);
        fclose(fp);
        return -3;
    }
    //指定数据源
    png_init_io(png_ptr,fp);
    //读取png文件
    png_read_png(png_ptr,info_ptr,(PNG_TRANSFORM_STRIP_16|PNG_TRANSFORM_BGR|PNG_TRANSFORM_EXPAND|PNG_TRANSFORM_PACKING),NULL);
    imgw = png_get_image_width(png_ptr,info_ptr);
    imgh = png_get_image_height(png_ptr,info_ptr);
    printf("image size: %d * %d \n",imgw,imgh);
    //判断png数据，是不是RGB888
    bitdep = png_get_channels(png_ptr,info_ptr);
  // printf("channel count: %d\n",bitdep);
    bitdep = png_get_bit_depth(png_ptr,info_ptr);
    color_type = png_get_color_type(png_ptr,info_ptr);
   // printf("bit depth: %d color_type:%d \n",bitdep,color_type);
    if(bitdep != 8||(color_type != PNG_COLOR_TYPE_RGB)){
        printf("error image is not RGB888 !\n");
        png_destroy_read_struct(&png_ptr,&info_ptr,NULL);
        fclose(fp);
        return -4;
    }
 //   png_set_sCAL(png_ptr,info_ptr,1,255.0,140.0);  //不清楚该函数用处
 //   imgw = png_get_image_width(png_ptr,info_ptr);
 //   imgh = png_get_image_height(png_ptr,info_ptr);
 //   printf("image size: %d * %d \n",imgw,imgh);
    //读取解码后的数据
    row_bytes = png_get_rowbytes(png_ptr,info_ptr);
    row_pointers = png_get_rows(png_ptr,info_ptr);
    *imgdata = (unsigned char*)malloc(imgw*imgh*3);

   // imgh = 2;
   for(i = 0;i < imgh;i++){
       memcpy(*(imgdata) + (row_bytes * i),row_pointers[i],row_bytes);
   }
    *w = imgw;
    *h = imgh;

    //结束、销毁/释放内存
    png_destroy_read_struct(&png_ptr,&info_ptr,NULL);
    
    fclose(fp);

    return 0;
}
int lcd_display_png(unsigned short x,unsigned short y,char *fname,lcd_align_t align)
{
    int w = 0,h = 0,ret = 0;
    unsigned char *imgdata = NULL;
    ret = read_png_image(fname,&imgdata,&w,&h);
    if(ret != 0){
        return ret;
    }
    if(align == align_center){
        x = (lcd_width - w)/2;
        y = (lcd_height - h)/2;
    }
    printf("lcd_show_rgbimage\n");
    lcd_show_rgbimage(x,y,w,h,imgdata);
    free(imgdata);
    return 0;
}
void lcd_base_ui_test(void)
{
    int i = 1,cnt = 10;
    unsigned char align = 0;
    char fname[20] = {0};
    
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

  #if 1
    #if 0
    unsigned char img[10000] = {0};
    set_rgb_color(img,sizeof(img),0xF800);
    lcd_show_image(600,0,20,20,img,16,1);
    set_rgb_color(img,sizeof(img),0x07E0);
    lcd_show_image(630,0,20,20,img,16,1);
    set_rgb_color(img,sizeof(img),0x001F);
    lcd_show_image(660,0,20,20,img,16,1);
    sprintf(fname,"./img/%d.bmp",10);    
    show_bmp_image(0,0,fname,0);
    sleep(5);
    #endif 
    while(cnt --){
    for(i = 1; i <= 25;i ++){
        if(i >= 24){
            sprintf(fname,"./img/%d.png",i);
        }else if(i >= 16 && i < 24)
            sprintf(fname,"./img/%d.jpg",i);
        else
            sprintf(fname,"./img/%d.bmp",i);
        if(i >= 8){
            align = 1;
            lcd_clearn(0xFFFFFF);
        }else{
            align = 0;
        }
        if(i >= 24){
            lcd_display_png(0,0,fname,align_center);
        }else if(i >= 16 && i < 24)
            lcd_display_jpeg(0,0,fname,align_center);
        else
            show_bmp_image(0,0,fname,align);
        sleep(2);
    }
    }
#elif 0
    for(i = 16;i <= 22;i++){
        sprintf(fname,"./img/%d.jpg",i); 
        lcd_display_jpeg(0,0,fname,align_center);  
        sleep(2);
    }  
#elif 0
    lcd_display_png(0,0,"./img/pngbar.png",align_center);
    sleep(2);
    lcd_display_png(0,0,"./img/24.png",align_center);
    sleep(2);
#endif
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

