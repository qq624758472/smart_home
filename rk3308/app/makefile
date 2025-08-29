DIR_INC = ./inc
DIR_INC += -I./src/op_common
# DIR_SRC = ./src
DIR_BIN = .

ver=0.90_B

SUBDIRS := $(shell find . -type d ! -path "./.*")

# 获取所有.c文件
# DIR_SRC := $(foreach dir,$(SUBDIRS),$(wildcard $(dir)/*.c))
DIR_SRC := $(shell find src -type f -name '*.c')



# SOURCE  := $(wildcard ${DIR_SRC}/*.c)
SOURCE := $(DIR_SRC)

OBJS    := $(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(SOURCE)))
OBJS-DEBUG    := $(patsubst %.c,%-debug.o,$(patsubst %.cpp,%-debug.o,$(SOURCE)))
  
TARGET  := prjrk3308-soc-post-v$(ver)
TARGET-DEBUG  := prjrk3308-soc-post-debug-v$(ver)
ALL=$(TARGET) #$(TARGET-DEBUG)

CC      := arm-linux-gnueabihf-gcc
LIBS    := -L ./lib/
DEFINES := -DPF_CAN=29 -DAF_CAN=PF_CAN
INCLUDE := -I. -I${DIR_INC}

CFLAGS  := -g -Wall -O2  $(DEFINES) $(INCLUDE) $(LIBS) -static
CFLAGS += -DAPP_VERSION="\"v$(ver)\""
CXXFLAGS:= $(CFLAGS) -DHAVE_CONFIG_H

CFLAGS_DEBUG=-DDEBUG_LOG_ON
CFLAGS_SYSLOG=-DSYSLOG

.PHONY : everything objs clean veryclean rebuild
  
everything : $(ALL)
  
all : $(ALL)
  
#objs : $(OBJS)
send:
	scp $(TARGET) root@10.0.3.222:/run/media/mmcblk0p3/liushihao/

%.o: %.c
	$(CC)  $(CFLAGS) $(CFLAGS_SYSLOG) -c $< -o $@

%-debug.o: %.c
	$(CC)  $(CFLAGS) $(CFLAGS_DEBUG) -c $< -o $@

  
rebuild: veryclean everything
                
clean :
	rm -fr *.so
	rm -fr ./*/*.o
	rm -fr *.o
	rm -rf prjrk3308-soc-post*
	rm $(OBJS)
    
veryclean : clean
	rm -fr $(ALL)
  
$(TARGET) : $(OBJS)
	$(CC) $(CXXFLAGS) -o $@ $(OBJS) -lpthread -lsocketcan
	cp $(TARGET) prjrk3308-soc-post

$(TARGET-DEBUG) : $(OBJS-DEBUG)
	$(CC) $(CXXFLAGS) -o $@ $(OBJS-DEBUG) -lpthread -lsocketcan

