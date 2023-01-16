#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/time.h>
#include <linux/videodev2.h>
#include "test_frambuffer.h"

#define SUPPORT_LCD      0
#define VIDEO_DEV       "/dev/video0"

typedef struct{
    unsigned short *start;
    unsigned long len;
}cam_buf_info;

static int camera_fd = 0;
static const int video_width = 640,video_height = 480;
static cam_buf_info usr_buf[3];


int camera_init(void)
{
    struct v4l2_capability cap ={0};

    /*打开摄像头*/
    camera_fd = open(VIDEO_DEV,O_RDWR);
    if(camera_fd < 0){
        printf("open %s failed !\n",VIDEO_DEV);
        return -1;
    }
    /*查询设备功能*/
    ioctl(camera_fd,VIDIOC_QUERYCAP,&cap);
    /*判断设备是否三视频采集设备*/
    if(!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)){
        printf("%s not capture video device !\n",VIDEO_DEV);
        return -2;
    }
    return 0;
}
int camera_support_formats(void)
{
    struct v4l2_fmtdesc fmtdesc = {0};
    struct v4l2_frmsizeenum frmsize = {0};
    struct v4l2_frmivalenum frmival = {0};

    /*枚举摄像头所支持的所有像素格式*/
    fmtdesc.index = 0;
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    while(ioctl(camera_fd,VIDIOC_ENUM_FMT,&fmtdesc) == 0){
        /**/
        printf("camera suport fmt:%s\n",fmtdesc.description);
        /*枚举出视频所有的采集分辨率*/
        frmsize.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        frmival.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        frmsize.index = 0;
        frmsize.pixel_format = fmtdesc.pixelformat;
        frmival.pixel_format = fmtdesc.pixelformat;
        while(ioctl(camera_fd,VIDIOC_ENUM_FRAMESIZES,&frmsize) == 0){
            printf("size: %d*%d ",frmsize.discrete.width,frmsize.discrete.height);
            frmsize.index++;
            /*获取视频采集帧率*/
            frmival.index = 0;
            frmival.width = frmsize.discrete.width;
            frmival.height = frmsize.discrete.height;
            while(ioctl(camera_fd,VIDIOC_ENUM_FRAMEINTERVALS,&frmival) == 0){
                printf("<%dfps>",frmival.discrete.denominator/frmival.discrete.numerator);
                frmival.index++;
            }
            printf("\n");
        }

        fmtdesc.index++;
    }
    
    return 0;
}
int camera_set_format(void)
{
    struct v4l2_format fmt = {0};
    struct v4l2_streamparm parm = {0};

    printf("*****set camera format ******\n");
    /*设置帧格式*/
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width =video_width;
    fmt.fmt.pix.height = video_height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    if(ioctl(camera_fd, VIDIOC_S_FMT,&fmt) < 0){
        printf("Failed to set video fomat !\n");
        return -1;
    }
    /*判断是否已经设置成功*/
    if(fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_MJPEG){
        printf("Error came not support JPEG format!\n");
        return -2;
    }
    printf("Video Size: <%d * %d >\n",fmt.fmt.pix.width,fmt.fmt.pix.height);
    /*获取streamparm*/
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(camera_fd,VIDIOC_G_PARM,&parm);
    /*判断是否支持帧率设置*/
    if(parm.parm.capture.capability & V4L2_CAP_TIMEPERFRAME){
        parm.parm.capture.timeperframe.numerator = 1;
        parm.parm.capture.timeperframe.denominator = 15;   //5fps 10 ,15,20,25,30
        if(ioctl(camera_fd,VIDIOC_S_PARM,&parm) < 0){
            printf("frame set error! use default frames\n");

        }
    }
    printf("vide frame:%d\n",parm.parm.capture.timeperframe.denominator/parm.parm.capture.timeperframe.numerator );
    return 0;
}
int camera_mmap_buffer(void)
{
    struct v4l2_requestbuffers reqbuf = {0};
    struct v4l2_buffer buf = {0};
    int i = 0,ret = 0;

    /*申请帧缓冲*/
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbuf.count = 3;
    reqbuf.memory = V4L2_MEMORY_MMAP;
    if(ioctl(camera_fd,VIDIOC_REQBUFS,&reqbuf) < 0){
        printf("camera request buffer error !\n");
        return -1;
    }
    /*建立内存映射*/
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;
    for(i = 0;i < reqbuf.count;i++){
        ioctl(camera_fd,VIDIOC_QUERYBUF,&buf);
        usr_buf[i].len = buf.length;
        usr_buf[i].start = mmap(NULL, buf.length, PROT_READ|PROT_WRITE, MAP_SHARED, camera_fd,buf.m.offset);
        if(usr_buf[i].start == MAP_FAILED){
            printf("mmap error error !\n");
            ret = -2;
            goto exit;
        }
    }
    /*入队*/
    buf.index = 0;
    for(i = 0;i < reqbuf.count;i++){
        if(ioctl(camera_fd,VIDIOC_QBUF,&buf) < 0){
            printf("camera QBUF error!\n");
            ret = -3;
            goto exit;
        }
        buf.index++;
    }
    return 0;
exit:
    for(i = 0;i < reqbuf.count;i++){
        if(usr_buf[i].start != MAP_FAILED){
            munmap(usr_buf[i].start,usr_buf[i].len);
        }
    }
    return ret;
}
int camera_start_capture(void)
{
    /*开始采集数据*/
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if(ioctl(camera_fd,VIDIOC_STREAMON,&type) < 0){
        printf("camera start capture failed !\n");
        return -1;
    }
    return 0;
}
void camera_stop_capture(void)
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if(ioctl(camera_fd,VIDIOC_STREAMOFF,&type) < 0){
        printf("camera stop capture failed !\n");
    }
}
void camera_close(void)
{
	unsigned int i;
	for(i = 0;i < 3; i++){
		if(-1 == munmap(usr_buf[i].start,usr_buf[i].len)){
			exit(-1);
		}
	}

	if(-1 == close(camera_fd)){
		perror("Fail to close fd");
		exit(EXIT_FAILURE);
	}
}

int camera_able_read(void)
{
    fd_set fds;
    struct timeval tv;
    int ret = 0;
    
    FD_ZERO(&fds);
    FD_SET(camera_fd, &fds);
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    ret = select(camera_fd + 1, &fds,NULL,NULL,&tv);
    if(ret == -1) {
        if(errno == EINTR)  
            return -1;
        printf("Fail to select\n");
        exit(-1);
    }
    if(ret == 0) {
        printf("select Timeout\n");        
    }
    return ret;
}
void read_one_frame(unsigned char *img,unsigned int *len)
{
    struct v4l2_buffer buf;

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.index = 0;
    buf.memory = V4L2_MEMORY_MMAP;
    /*出队*/
    ioctl(camera_fd, VIDIOC_DQBUF,&buf);
    printf("index: %d  size: %d\n", buf.index, buf.bytesused);
    memcpy(img,usr_buf[buf.index].start,buf.bytesused);
    *len = buf.bytesused;
    /*入队*/
    ioctl(camera_fd, VIDIOC_QBUF,&buf);
}
void store_one_jepg_img(unsigned char *jpeg,unsigned int len)
{
    FILE *fp = NULL;

    fp = fopen("./camera.jpg","w+");
    if(fp != NULL){
        printf("write one jpeg frame!\n");
        fwrite(jpeg,1,len,fp);
        fclose(fp);
    }
}
void lcd_show_camera_img()
{
#if SUPPORT_LCD    
    unsigned char *jpg = NULL;
    int jpg_size = 0;
    unsigned int jpg_len = 0;

    jpg_size = video_width*video_height*2;
    camera_start_capture();
    jpg = (unsigned char*)malloc(jpg_size);
    while(1){
        if(camera_able_read() > 0){
            read_one_frame(jpg,&jpg_len);
            lcd_display_jpeg_mem(50,0,jpg,jpg_len,align_center);
        }else{
            usleep(30000);
        }
    }
#else
    unsigned char *jpg = NULL;
    int jpg_size = 0;
    unsigned int jpg_len = 0;

    jpg_size = video_width*video_height*2;
    camera_start_capture();
    jpg = (unsigned char*)malloc(jpg_size);   
    if(camera_able_read() > 0){
        read_one_frame(jpg,&jpg_len);
        store_one_jepg_img(jpg,jpg_len);
        sleep(1);
    } 
    free(jpg);
#endif    
}
void show_camera_img_test()
{
    int ret = camera_init();
    if(ret < 0){
        printf("camera init failed\n");
        return;
    }
    ret = camera_support_formats();
    if(ret < 0){
        printf("camera support format error\n");
        close(camera_fd);
        return ;
    }
    ret = camera_set_format();
    if(ret < 0){
        printf("camera set format error !\n");
        close(camera_fd);
        return;
    }
    ret = camera_mmap_buffer();
    if(ret < 0) {
        printf("camera mmap buffer error");
        camera_close();
        return;
    }
    lcd_show_camera_img();

}

