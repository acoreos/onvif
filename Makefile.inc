SHELL = /bin/bash

ARCH		:= arm-
CROSS_COMPILE=/home/millet/software/toolchain/arm/4.8.5/usr/bin/arm-linux-

CC           := ${CROSS_COMPILE}gcc
CPP          := ${CROSS_COMPILE}g++
LD           := ${CROSS_COMPILE}ld
AR           := ${CROSS_COMPILE}ar
STRIP        := ${CROSS_COMPILE}strip

# -DWITH_DOM -DWITH_OPENSSL两个宏是链接openssl时要用的；
# -D__STDC_CONSTANT_MACROS宏是cpp源码调用ffmpeg库接口函数时要用的，如果是c源码调用ffmpeg库函数接口就不用这个宏；
# -DDEBUG宏用于开启SOAP协议收发日志，日志存于文件RECV.log、SENT.log、TEST.log之中
CFLAGS += -c -g -Wall -DWITH_DOM -DWITH_OPENSSL 
CFLAGS += $(INCLUDE)

# openssl目录名
OPENSSL_DIR = ../lib/${ARCH}openssl-linux

# ffmpeg目录名
FFMPEG_DIR = ../lib/${ARCH}ffmpeg-linux-3.2

# 源文件
SOURCES_ONVIF += \
           ../onvif/soapC.c                          \
           ../onvif/soapClient.c                     \
           ../onvif/stdsoap2.c                       \
           ../onvif/wsaapi.c                         \
           ../onvif/dom.c                            \
           ../onvif/mecevp.c                         \
           ../onvif/smdevp.c                         \
           ../onvif/threads.c                        \
           ../onvif/wsseapi.c                        \

SOURCES_COMM += \
           ../comm/onvif_dump.c                      \
           ../comm/onvif_comm.c                      \

# 目标文件
OBJECTS_ONVIF := $(patsubst %.c,$(TEMPDIR)%.o,$(filter %.c, $(SOURCES_ONVIF)))
OBJECTS_COMM  := $(patsubst %.c,$(TEMPDIR)%.o,$(filter %.c, $(SOURCES_COMM)))

# 头文件路径
INCLUDE += -I../onvif/                               \
           -I../comm/                                \
           -I$(OPENSSL_DIR)/include                  \
           -I$(FFMPEG_DIR)/include                   \

# 共享库链接OpenSSL
LDLIBS += -L$(OPENSSL_DIR)                      \
          -lssl                                     \
          -lcrypto                                  \

# 静态库链接OpenSSL
#LDLIBS += $(OPENSSL_DIR)/lib/libssl.a                \
#          $(OPENSSL_DIR)/lib/libcrypto.a             \
#          -ldl                                       \

# 静态库链接ffmpeg
#LDLIBS += $(FFMPEG_DIR)/lib/libavfilter.a            \
          $(FFMPEG_DIR)/lib/libswscale.a             \
          $(FFMPEG_DIR)/lib/libavdevice.a            \
          $(FFMPEG_DIR)/lib/libavformat.a            \
          $(FFMPEG_DIR)/lib/libavcodec.a             \
          $(FFMPEG_DIR)/lib/libavutil.a              \
          $(FFMPEG_DIR)/lib/libswresample.a          \
          -lm                                        \
          -lrt                                       \
          -lz                                        \

# 共享库链接ffmpeg
LDLIBS += -L$(FFMPEG_DIR)/lib                       \
          -lavcodec -lavdevice -lavfilter -lavformat\
          -lavutil -lswresample -lswscale           \

# 链接库(其他)
LDLIBS += -lpthread

%.o: %.cpp
	@echo "  CPP     " $@;
	$(CPP) $(CFLAGS) -c -o $@ $<

%.o: %.c
#@echo "  CC      " $@;
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: all clean
