#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <time.h>
#include <sys/time.h>
#include "test_api.h"

int get_sys_uname()
{
	int ret = 0;
	char hostname[20] = {0};

	struct utsname os_info;
	struct sysinfo sys_info;
	
	printf("********get sys uname info*******\n");
	ret = uname(&os_info);
	if(ret == -1){
		perror("get uname error");
		return -1;
	}
	printf("操作系统名称:%s\n",os_info.sysname);
	printf("主机名称:%s\n",os_info.nodename);
	printf("内核版本:%s\n",os_info.release);
	printf("发行版本:%s\n",os_info.version);
	printf("体系架构:%s\n",os_info.machine);
	ret = sysinfo(&sys_info);
	if(ret == -1){
		perror("get sysinfo error");
		return -1;
	}	
	printf("系统启动后经过的时间:%ld\n",sys_info.uptime);
	printf("总共可用的内存:%ldMB\n",sys_info.totalram/1024/1024);
	printf("还剩内存:%ldMB\n",sys_info.freeram/1024/1024);
	printf("系统当前进程数:%d\n",sys_info.procs);
	ret = gethostname(hostname,sizeof(hostname));
	if(ret == -1){
		perror("gethostname error");	
		return -3;
	}
	printf("gethostnme:%s\n",hostname);
	return 0;
}

int test_sys_gettime()
{
	int ret = 0;
	struct timeval tval;
	time_t tm;
	struct tm tm_r,*p_tm = NULL;
	char *ptr = NULL,tm_str[100] = {0};

	printf("**********获取系统的时间*********\n");
	tm = time(NULL);			//UTC时间
	if(tm == -1){
		perror("time error");
		return -1;	
	}
	printf("time tm=%ld\n",tm);
	ptr = ctime(&tm);			//不可重入函数 本地时间
	printf("本地时间为:%s\n",ptr);
	p_tm = localtime(&tm);		//不可重入函数 本地时间
	printf("CST %d-%02d-%02d %02d:%02d:%02d\n",p_tm->tm_year+1900,
	p_tm->tm_mon,p_tm->tm_mday,p_tm->tm_hour,p_tm->tm_min,p_tm->tm_sec);
	p_tm = gmtime(&tm);
	printf("UTC %d-%02d-%02d %02d:%02d:%02d\n",p_tm->tm_year+1900,
	p_tm->tm_mon,p_tm->tm_mday,p_tm->tm_hour,p_tm->tm_min,p_tm->tm_sec);


	ret = gettimeofday(&tval,NULL);
	if(ret == -1){
		perror("gettimeofday error");
		return -2;
	}
	printf("gettimeofday tv_sec=%ld tv_usec=%ld\n",tval.tv_sec,tval.tv_usec);
	ctime_r(&tval.tv_sec,tm_str);	//可重入函数,本地时间
	printf("本地时间为:%s\n",tm_str);
	localtime_r(&tm,&tm_r);
	printf("CST %d-%02d-%02d %02d:%02d:%02d\n",tm_r.tm_year+1900,
	tm_r.tm_mon,tm_r.tm_mday,tm_r.tm_hour,tm_r.tm_min,tm_r.tm_sec);
	gmtime_r(&tm,&tm_r);
	printf("UTC %d-%02d-%02d %02d:%02d:%02d\n",tm_r.tm_year + 1900,
	tm_r.tm_mon,tm_r.tm_mday,tm_r.tm_hour,tm_r.tm_min,tm_r.tm_sec);

	return 0;
}



int sys_info_test()
{
	get_sys_uname();
	test_sys_gettime();
	return 0;
}
