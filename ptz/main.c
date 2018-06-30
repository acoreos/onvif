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
 * 日期：2017-05-03
 * 描述：PTZ控制示例代码
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "onvif_comm.h"
#include "onvif_dump.h"


void cb_discovery(char *DeviceXAddr)
{
    struct tagCapabilities capa;
    struct tagProfile *profiles = NULL;                                         // 设备配置文件列表
	int result = 0;
	int speed_p = 100;
	int speed_t = 100;
	int speed_z = 10;

    ONVIF_GetCapabilities(DeviceXAddr, &capa);

	ONVIF_GetProfiles(capa.MediaXAddr, &profiles);
	//if (ONVIF_PTZ_ContinuousMove(profiles->token, capa.PTZXAddr, speed_p, speed_t, speed_z) != 0)
	//  return -1;

	//sleep(1);

	//ONVIF_PTZ_Stop(profiles->token, capa.PTZXAddr);
	
	ONVIF_PTZ_GotoHomePosition(profiles->token, capa.PTZXAddr, speed_p, speed_t, speed_z);
	//ONVIF_PTZ_SetHomePosition(profiles->token, capa.PTZXAddr);
	//ONVIF_PTZ_AbsoluteMove(profiles->token, capa.PTZXAddr, speed_p, speed_t, speed_z);

}

int main(void)
{
    ONVIF_DetectDevice(cb_discovery);

	return 0;
}

