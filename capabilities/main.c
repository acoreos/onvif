/************************************************************************
**
** 作者：许振坪
** 日期：2017-05-03
** 博客：http://blog.csdn.net/benkaoya
** 描述：IPC获取设备能力信息示例代码
**
************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "onvif_comm.h"
#include "onvif_dump.h"

void cb_discovery(char *DeviceXAddr)
{
    struct tagCapabilities capa;

    ONVIF_GetCapabilities(DeviceXAddr, &capa);
}

int main(int argc, char **argv)
{
    ONVIF_DetectDevice(cb_discovery);

    return 0;
}