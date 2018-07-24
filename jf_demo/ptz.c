/*******************************************************************+
 *                             _ooOoo_								*
 *                            o8888888o								*
 *                            88" . "88								*
 *                            (| -_- |)								*
 *                            O\  =  /O								*
 *                         ____/`---'\____							*
 *                       .'  \\|     |//  `.						*
 *                      /  \\|||  :  |||//  \						*
 *                     /  _||||| -:- |||||-  \						*
 *                     |   | \\\  -  /// |   |						*
 *                     | \_|  ''\---/''  |   |						*
 *                     \  .-\__  `-`  ___/-. /						*
 *                   ___`. .'  /--.--\  `. . __						*
 *                ."" '<  `.___\_<|>_/___.'  >'"".					*
 *               | | :  `- \`.;`\ _ /`;.`/ - ` : | |				*
 *               \  \ `-.   \_ __\ /__ _/   .-` /  /				*
 *          ======`-.____`-.___\_____/___.-`____.-'======			*
 *                             `=---='								*
 *          ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^			*
 *                     佛祖保佑        永无BUG						*
 *																	*
 +******************************************************************/

/*
 * 作者：梁友
 * 日期：2018-07-02
 * 描述：PTZ控制示例代码
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include "onvif_comm.h"
#include "onvif_dump.h"


static void cb_discovery(char *DeviceXAddr)
{
    struct tagCapabilities capa;
    struct tagProfile *profiles = NULL;                                         // 设备配置文件列表
	int result = 0;
	int speed_p = 0;
	int speed_t = 0;
	int speed_z = 0;


    ONVIF_GetCapabilities(DeviceXAddr, &capa);

	ONVIF_GetProfiles(capa.MediaXAddr, &profiles);

	while (1) {
		int i = 0;
		srand( (unsigned)time( NULL ) );          //初始化随机数
		speed_p = (rand() - rand()) % 100;
		speed_t = (rand() - rand()) % 100;
		if (i ++ % 2)
			speed_z = (rand() - rand()) % 100;
		ONVIF_PTZ_AbsoluteMove(profiles->token, capa.PTZXAddr, speed_p, speed_t, speed_z);
		sleep(30);
		ONVIF_PTZ_GotoHomePosition(profiles->token, capa.PTZXAddr, speed_p, speed_t, speed_z);
		sleep(30);
	}

}

int ptz_main(void)
{
    ONVIF_DetectDevice(cb_discovery);

	return 0;
}

