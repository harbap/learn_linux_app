#include <stdio.h>
#include "test_api.h"
#include "test_frambuffer.h"
#include "camera.h"



int main(void)
{
	int ret = 0;
	printf("*************linux-app v1.0*************\n");
	//test_io_entry();
   // sys_info_test();
	//test_signal_entry();
	//test_frambufer_main();
	ret = lcd_and_font_init();
	if(ret < 0){
		return -1;
	}
	show_camera_img_test();
	return 0;	
}
