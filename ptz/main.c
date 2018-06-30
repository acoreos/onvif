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
 *******************************************************************/

/*
 * 作者：梁友
 * 日期：2017-05-03
 * 描述：PTZ控制示例代码
 *
 **/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "onvif_comm.h"
#include "onvif_dump.h"


void cb_discovery(char *DeviceXAddr)
{
    struct tagCapabilities capa;
    struct soap *soap = NULL;
    struct tagProfile *profiles = NULL;                                         // 设备配置文件列表

    ONVIF_GetCapabilities(DeviceXAddr, &capa);

	int speed_x = 100;
	int speed_y = 0;
	int speed_z = 0;
	struct _tds__GetCapabilities            	req;
   	struct _tds__GetCapabilitiesResponse    	rep;
	struct _trt__GetProfiles 					getProfiles;
	struct _trt__GetProfilesResponse			response;
	struct _tptz__ContinuousMove 	 			continuousMove;
	struct _tptz__ContinuousMoveResponse 		continuousMoveresponse;
	struct _tptz__Stop 							stop;
	struct _tptz__StopResponse 					stopresponse;
	char endpoint[255] = {0};
	int result = 0;
	char profile[255] = {0};

    SOAP_ASSERT(NULL != (soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT)));
    ONVIF_SetAuthInfo(soap, USERNAME, PASSWORD);

	result = soap_call___trt__GetProfiles(soap, capa.MediaXAddr, NULL, &getProfiles, &response);
	if(result == SOAP_OK)
	{
		strcpy(profile, response.Profiles[0].token);
		printf("profile====%s\n", profile);
	} else {
		printf("%d\n", result);
		return;
	}
		
	continuousMove.ProfileToken = profile;
	//continuousMove.Timeout  = "0.0001"; 

	struct tt__PTZSpeed* velocity = soap_new_tt__PTZSpeed(soap, -1);
	continuousMove.Velocity = velocity;

	struct tt__Vector2D* panTilt = soap_new_tt__Vector2D(soap, -1);
	continuousMove.Velocity->PanTilt = panTilt;
	continuousMove.Velocity->PanTilt->x = (float)speed_x/ 100;
	continuousMove.Velocity->PanTilt->y = (float) speed_y/ 100;
	continuousMove.Velocity->PanTilt->space = "http://www.onvif.org/ver10/tptz/PanTiltSpaces/VelocityGenericSpace";

	struct tt__Vector1D* zoom = soap_new_tt__Vector1D(soap, -1);
	continuousMove.Velocity->Zoom = zoom;
	continuousMove.Velocity->Zoom->x = (float)speed_z / 100;

    ONVIF_SetAuthInfo(soap, USERNAME, PASSWORD);
	//if((result = soap_call___tptz__ContinuousMove(soap,"http://10.10.6.29/onvif/PTZ",NULL,&continuousMove,&continuousMoveresponse)) == SOAP_OK) {
	//	printf("======SetPTZcontinuous_move is success!!!=======\n");
	//} else {
	//	SOAP_CHECK_ERROR(result, soap, "soap_call___tptz__ContinuousMove");
	//}

	sleep(5);
	stop.ProfileToken = profile;
	enum xsd__boolean *PT_boolean = soap_new_xsd__boolean(soap, -1);
	enum xsd__boolean *Z_boolean = soap_new_xsd__boolean(soap, -1);
	*PT_boolean = xsd__boolean__true_;
	*Z_boolean = xsd__boolean__true_;
	stop.PanTilt = PT_boolean;
	stop.Zoom = Z_boolean;

    //ONVIF_SetAuthInfo(soap, USERNAME, PASSWORD);
	//result = soap_call___tptz__Stop(soap, "http://10.10.6.29/onvif/PTZ", NULL, &stop, &stopresponse);
	SOAP_CHECK_ERROR(result, soap, "soap_call__tptz__Stop");
	printf("soap_call___tptz__Stop succesfull\n");


	struct _tptz__SetHomePosition homePosition;
	struct _tptz__SetHomePositionResponse homePositionResponse;
	homePosition.ProfileToken = profile;
    ONVIF_SetAuthInfo(soap, USERNAME, PASSWORD);
	result = soap_call___tptz__SetHomePosition(soap, "http://10.10.6.29/onvif/PTZ", NULL, &homePosition, &homePositionResponse);
	SOAP_CHECK_ERROR(result, soap, "SetHomePosition");
	printf("soap_call___tptz__SetHomePosition succesfull\n");

EXIT:
    if (NULL != soap) {
        ONVIF_soap_delete(soap);
    }
}

int main(void)
{
    ONVIF_DetectDevice(cb_discovery);

	return 0;
}

