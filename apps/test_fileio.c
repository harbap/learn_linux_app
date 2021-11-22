#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <utime.h>
#include <time.h>
#include <dirent.h>
#include "test_api.h"


int test_sternor()
{
	int fd;
	printf("open ./test_file...\n");
	fd = open("./test_file",O_RDONLY);
	if(fd == -1){
		printf("Error:%s\n",strerror(errno));
		perror("xxx error xxx");
		//这三个exit 函数用法一样，效果相同退出函数0:表示正常退出 其他:表示异常退出
		//_exit(-1);	//系统调用		
		//_Exit(-1);	//系统调用
		//exit(-1);    // 标准C库函数 stdlib
		return -1;
	}
	close(fd);
	return 0;
	//_exit(0);
	//_Exit(0);
}
int test_holefile()
{
	int fd =0,ret = 0,i;
	char buffer[1024];
	
	printf("*****create hole file******\n");
	fd = open("./tmp/hole_file",O_WRONLY|O_CREAT|O_EXCL,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
	if(fd == -1){
		perror("open error");
		//exit(-1);
		return -1;
	}
	ret = lseek(fd,4096,SEEK_SET);
	if(ret == -1){
		perror("lseek error");
		goto err;
	}
	memset(buffer,0x8f,sizeof(buffer));
	for(i = 0;i < 4;i++){
		ret = write(fd,buffer,sizeof(buffer));
		if(ret == -1){
			perror("write error");
			goto err;		
		}
	}
	ret = 0;
	memset(buffer,0,sizeof(buffer));
	lseek(fd,0,SEEK_SET);
	ret = read(fd,buffer,sizeof(buffer));
	printf("ret = %d \n",ret);
	if(ret == -1){
		perror("read error");
	}else{
		for(i = 0;i < 10;i++)
			printf("%02x",buffer[i]);
	}
err:
	close(fd);
	return ret;
}

int test_copy_fd()
{
	int fd1 = 0,fd2 = 0,ret = 0,i;
	unsigned char buf1[4],buf2[4];
	
	printf("********复制文件描述符******\n");
	fd1 = open("./tmp/test_file_cpy",O_RDWR|O_CREAT,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
	if(fd1 == -1){
		perror("open error");
		return -1;
	}
	/**复制文件描述符**/
	fd2 = dup(fd1);  //fd2 = dup2(fd1, 100);  指定要复制的文件描述符
	//fd2 = fcntl(fd1, F_DUPFD, 0);
	if(fd2 == -1){
		perror("dup error");
		close(fd1);
		return -2;
	}
	printf("fd1 = %d fd2 = %d \n",fd1,fd2);
	buf1[0] = 0x11; buf1[1] = 0x22;buf1[2] = 0x33;buf1[3] = 0x44;
	buf2[0] = 0xaa; buf2[1] = 0xbb;buf2[2] = 0xcc;buf2[3] = 0xdd;
	for(i = 0;i < 4;i++){
		ret = write(fd1,buf1,sizeof(buf1));
		if(ret == -1){
			perror("write fd1 error");
			close(fd1);
			close(fd2);
			return -3;
		}
		ret = write(fd2,buf2,sizeof(buf2));
		if(ret == -1){
		perror("write fd2 error");
		close(fd1);
		close(fd2);
		}	
	}
	ret = lseek(fd1,0,SEEK_SET);
	if(ret == -1){
		perror("lseek error");
		return -4;
	}
	printf("复制文件描述符度回结果:\n");
	for(i = 0;i < 8;i ++){
		ret = read(fd1,buf1,sizeof(buf1));
		if(ret == -1){
			perror("read error");
			close(fd1);
			close(fd2);
			return -5;
		}
		printf("%02x%02x%02x%02x ",buf1[0],buf1[1],buf1[2],buf1[3]);
	}
	printf("\n");
	close(fd1);
	close(fd2);
	return 0;
}
//原子操作读写函数,相当于 先执行lseek 然后调用read,write
//ssize_t pread(int fd, void *buf, size_t count, off_t offset);
//ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset);
//open过程中具有原子操作的标志: O_EXCL（判断文集是否存在，存在返回错误)  O_APEND(在文件末尾写入)  
int test_print_file_stat()
{
	struct stat file_stat;
	struct tm file_tm;
	int ret;
	char time_str[100] = {0};
	
	/*获取文件属性*/
	printf("*******获取文件属性*******\n");
	printf("uid:%d gid:%d\n",getuid(),getgid());
	//int fstat(int fd, struct stat *buf);
	//int lstat(const char *pathname, struct stat *buf);
	//lstat 与fstat，stat的区别是符合链接文件, lstat是符号链接文件本身的属性,fstat/stat是符号链接
	//文件所指向的文件属性信息。
	ret = stat("./tmp/test_file_cpy",&file_stat);
	if(ret == -1){
		perror("stat error");
		return -1;	
	}
	printf("文件大小:%ld Byte. inode number:%ld\n",file_stat.st_size,file_stat.st_ino);
	printf("文件类型为:");
	switch(file_stat.st_mode&S_IFMT){
	case S_IFSOCK:printf("socket文件\n");break;
	case S_IFLNK:printf("链接文件\n");break;
	case S_IFREG:printf("普通文件\n");break;
	case S_IFBLK:printf("块文件\n");break;
	case S_IFDIR:printf("目录文件\n");break;
	case S_IFCHR:printf("字符设备文件\n");break;
	case S_IFIFO:printf("管道文件\n");break;
	}
	printf("其他用户读写权限:");
	if(file_stat.st_mode & S_IROTH)
		printf("读:是 ");
	else
		printf("读:否 ");
	if(file_stat.st_mode & S_IWOTH)
		printf("写:是 ");
	else
		printf("写:否 ");
	printf("\n");
	printf("文件最后被访问时间:");
	localtime_r(&file_stat.st_atim.tv_sec,&file_tm);	
	strftime(time_str,sizeof(time_str),"%Y-%m-%d %H:%M:%S",&file_tm);
	printf("%s\n",time_str);

	printf("文件最后被修改时间:");
	localtime_r(&file_stat.st_mtim.tv_sec,&file_tm);
	strftime(time_str,sizeof(time_str),"%Y-%m-%d %H:%M:%S",&file_tm);
	printf("%s\n",time_str);
	/*更改文件的用户组*/
	/*
	if(chown("./tmp/test_file_copy",0,0) == -1){ // 更改用户id为root
		perror("chown error");
		return -1;
	}
	*/
	return 0;
}
int test_access()
{
	int ret ;
	struct utimbuf utm_buf;
	time_t cur_sec;
	
	printf("*********文件权限检查*********\n");
	ret = access("./tmp/test_file",F_OK);
	if(ret == -1){
		printf("file dose not exist.\n");	
		return -1;
	}
	ret = access("./tmp/test_file",R_OK);
	if(!ret)
		printf("Read permission:Yes\n");
	else
		printf("Read permission:No\n");
	ret = access("./tmp/test_file",W_OK);
	if(!ret)
		printf("Write permission:Yes\n");
	else
		printf("Write permission:No\n");
	ret = access("./tmp/test_file",X_OK);
	if(!ret)
		printf("Execution permission:Yes\n");
	else
		printf("Execution permission:No\n");
	/*修改文件权限*/
	//int chmod(const char *pathname, mode_t mode);
	//int fchmod(int fd, mode_t mode);
	printf("修改文件访问时间\n");
	time(&cur_sec);
	cur_sec -= 3600;		/*扣除1小时*/
	utm_buf.actime = cur_sec;
	utm_buf.modtime = cur_sec;
	//int utimes(const char *filename, const struct timeval times[2]);
	ret = utime("./tmp/test_file",&utm_buf);
	if(ret == -1){
		perror("utime error");
	}
	
	return 0;
}
int test_dirent_fuc()
{
	int ret = 0,fd = 0;
	struct dirent *dir;
	DIR *dirp;

	printf("*********创建目录**********\n");
	ret = mkdir("./tmp/test_dir",S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH);
	if(ret == -1){
		perror("create dir error");
		return -1;
	}
	getchar();
	
	fd = open("./tmp/test_dir/tmp1.txt",O_WRONLY|O_CREAT|O_EXCL,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
	close(fd);
	fd = open("./tmp/test_dir/tmp2.txt",O_WRONLY|O_CREAT|O_EXCL,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
	close(fd);
	fd = open("./tmp/test_dir/tmp3.txt",O_WRONLY|O_CREAT|O_EXCL,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
	close(fd);
	fd = open("./tmp/test_dir/tmp4.txt",O_WRONLY|O_CREAT|O_EXCL,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
	close(fd);

	printf("********打开目录*********\n");
	dirp = opendir("./tmp/test_dir");
	if(dirp == NULL){
		perror("open dir error");
		goto exit;
	}
	errno = 0;
	while((dir = readdir(dirp)) != NULL){
		if(dir->d_type==DT_REG||dir->d_type==DT_LNK)
			printf("文件名:%s  inode=%ld\n",dir->d_name,dir->d_ino);
	
	}
	if(errno != 0){
		perror("readdir error");	
	}else{
		printf("End of directory!\n");
	}
	closedir(dirp);
	getchar();

exit:
	printf("*******删除文件********\n");
	unlink("./tmp/test_dir/tmp1.txt");
	unlink("./tmp/test_dir/tmp2.txt");
	unlink("./tmp/test_dir/tmp3.txt");
	unlink("./tmp/test_dir/tmp4.txt");
	printf("********删除目录**********\n");
	ret = rmdir("./tmp/test_dir");
	if(ret == -1){
		perror("rm dir error");
		return -1;
	}
	return 0;
}


int test_io_entry()
{
	char buf[100] = {0},*ptr = NULL;
	
	printf("entery test io func\n");
	ptr = getcwd(buf,sizeof(buf));
	if(ptr == NULL){
		perror("getcwd error");
	}
	printf("当前软件工作目录:%s\n",buf);
	test_sternor();
	test_holefile();
	//test_copy_fd();
	//test_print_file_stat();
	//test_access();
	test_dirent_fuc();
	return 0;
}






