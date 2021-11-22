

#include <stdio.h>
#include "test_api.h"


int main(void)
{
	printf("*************linux-app v1.0*************\n");
	//test_io_entry();
    sys_info_test();
	test_signal_entry();
	return 0;	
}
