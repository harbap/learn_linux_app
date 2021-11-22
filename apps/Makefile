CROSS_COMPILE		?= 

CC		:= $(CROSS_COMPILE)gcc
CXX 	:= $(CROSS_COMPILE)g++
LD		:= $(CROSS_COMPILE)ld

obj 	:= build

CC_FLAG	:= -g -O2 -Wall -MD -I./ -I./include -I../lib/include  -I../middle/include -I/opt/i.mx/alsa-lib/include
LD_FLAG := -g -O2 -lpthread -lrt -lm -L../lib -L/opt/i.mx/alsa-lib/lib -L../middle

TARGET 	:= linux_app 

SOURCE  += main.o test_fileio.o sys_info.o test_signal.o

OBJECT 	:= $(patsubst %c,%o,$(SOURCE))
OBJ 	:= $(addprefix $(obj)/,$(OBJECT))

all:$(TARGET)
$(TARGET) : $(OBJ)
	$(CC) $^ $(LD_FLAG) -o $@
	
$(obj)/%.o : %.c
	$(CC) $(CC_FLAG) -o $@ -c $<
$(obj)/%.o : %.cpp
	$(CXX) $(CC_FLAG) -o $(@) -c $<
app_install:
	cp $(TARGET) /nfs/imxfs/linux_app/ -f	
###包含依赖文件  OBJ 在上文要有定义，否则包含失败，会影响.d 依赖文件生成	
-include $(OBJ:.o=.d)		
.PHONEY:
clean:
	rm -rf $(obj)/*.o
	rm -rf $(obj)/*.d
	rm -rf $(TARGET)