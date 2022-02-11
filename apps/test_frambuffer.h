#ifndef _TEST_FRAMBUFFER_H_
#define _TEST_FRAMBUFFER_H_

typedef enum{
    align_left = 0, 
    align_right = 1,
    align_center = 2,
}lcd_align_t;
typedef enum{
    FONT_SONG = 0,
    FONT_KAI = 1,
}FontType_t;
int test_frambufer_main();
int lcd_display_jpeg_mem(unsigned short x,unsigned short y,unsigned char *img,unsigned int len,lcd_align_t align);
int lcd_and_font_init();

#endif // _TEST_FRAMBUFFER_H_
