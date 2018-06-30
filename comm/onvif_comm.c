/************************************************************************
**
** 作者：梁友
** 日期：2018-06-28
** 描述：ONVIF封装函数
**
************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "wsseapi.h"
#include "onvif_comm.h"
#include "onvif_dump.h"

void soap_perror(struct soap *soap, const char *str)
{
    if (NULL == str) {
        SOAP_DBGERR("[soap] error: %d, %s, %s\n", soap->error, *soap_faultcode(soap), *soap_faultstring(soap));
    } else {
        SOAP_DBGERR("[soap] %s error: %d, %s, %s\n", str, soap->error, *soap_faultcode(soap), *soap_faultstring(soap));
    }
    return;
}

void* ONVIF_soap_malloc(struct soap *soap, unsigned int n)
{
    void *p = NULL;

    if (n > 0) {
        p = soap_malloc(soap, n);
        SOAP_ASSERT(NULL != p);
        memset(p, 0x00 ,n);
    }
    return p;
}

struct soap *ONVIF_soap_new(int timeout)
{
    struct soap *soap = NULL;                                                   // soap环境变量

    SOAP_ASSERT(NULL != (soap = soap_new()));

    soap_set_namespaces(soap, namespaces);                                      // 设置soap的namespaces
    soap->recv_timeout    = timeout;                                            // 设置超时（超过指定时间没有数据就退出）
    soap->send_timeout    = timeout;
    soap->connect_timeout = timeout;

#if defined(__linux__) || defined(__linux)                                      // 参考https://www.genivia.com/dev.html#client-c的修改：
    soap->socket_flags = MSG_NOSIGNAL;                                          // To prevent connection reset errors
#endif

    soap_set_mode(soap, SOAP_C_UTFSTRING);                                      // 设置为UTF-8编码，否则叠加中文OSD会乱码

    return soap;
}

void ONVIF_soap_delete(struct soap *soap)
{
    soap_destroy(soap);                                                         // remove deserialized class instances (C++ only)
    soap_end(soap);                                                             // Clean up deserialized data (except class instances) and temporary data
    soap_done(soap);                                                            // Reset, close communications, and remove callbacks
    soap_free(soap);                                                            // Reset and deallocate the context created with soap_new or soap_copy
}

/************************************************************************
**函数：ONVIF_SetAuthInfo
**功能：设置认证信息
**参数：
        [in] soap     - soap环境变量
        [in] username - 用户名
        [in] password - 密码
**返回：
        0表明成功，非0表明失败
**备注：
************************************************************************/
int ONVIF_SetAuthInfo(struct soap *soap, const char *username, const char *password)
{
    int result = 0;

    SOAP_ASSERT(NULL != username);
    SOAP_ASSERT(NULL != password);

    result = soap_wsse_add_UsernameTokenDigest(soap, NULL, username, password);
    SOAP_CHECK_ERROR(result, soap, "add_UsernameTokenDigest");

EXIT:

    return result;
}

/************************************************************************
**函数：ONVIF_init_header
**功能：初始化soap描述消息头
**参数：
        [in] soap - soap环境变量
**返回：无
**备注：
    1). 在本函数内部通过ONVIF_soap_malloc分配的内存，将在ONVIF_soap_delete中被释放
************************************************************************/
void ONVIF_init_header(struct soap *soap)
{
    struct SOAP_ENV__Header *header = NULL;

    SOAP_ASSERT(NULL != soap);

    header = (struct SOAP_ENV__Header *)ONVIF_soap_malloc(soap, sizeof(struct SOAP_ENV__Header));
    soap_default_SOAP_ENV__Header(soap, header);
    header->wsa__MessageID = (char*)soap_wsa_rand_uuid(soap);
    header->wsa__To        = (char*)ONVIF_soap_malloc(soap, strlen(SOAP_TO) + 1);
    header->wsa__Action    = (char*)ONVIF_soap_malloc(soap, strlen(SOAP_ACTION) + 1);
    strcpy(header->wsa__To, SOAP_TO);
    strcpy(header->wsa__Action, SOAP_ACTION);
    soap->header = header;

    return;
}

/************************************************************************
**函数：ONVIF_init_ProbeType
**功能：初始化探测设备的范围和类型
**参数：
        [in]  soap  - soap环境变量
        [out] probe - 填充要探测的设备范围和类型
**返回：
        0表明探测到，非0表明未探测到
**备注：
    1). 在本函数内部通过ONVIF_soap_malloc分配的内存，将在ONVIF_soap_delete中被释放
************************************************************************/
void ONVIF_init_ProbeType(struct soap *soap, struct wsdd__ProbeType *probe)
{
    struct wsdd__ScopesType *scope = NULL;                                      // 用于描述查找哪类的Web服务

    SOAP_ASSERT(NULL != soap);
    SOAP_ASSERT(NULL != probe);

    scope = (struct wsdd__ScopesType *)ONVIF_soap_malloc(soap, sizeof(struct wsdd__ScopesType));
    soap_default_wsdd__ScopesType(soap, scope);                                 // 设置寻找设备的范围
    scope->__item = (char*)ONVIF_soap_malloc(soap, strlen(SOAP_ITEM) + 1);
    strcpy(scope->__item, SOAP_ITEM);

    memset(probe, 0x00, sizeof(struct wsdd__ProbeType));
    soap_default_wsdd__ProbeType(soap, probe);
    probe->Scopes = scope;
    probe->Types  = (char*)ONVIF_soap_malloc(soap, strlen(SOAP_TYPES) + 1);     // 设置寻找设备的类型
    strcpy(probe->Types, SOAP_TYPES);

    return;
}

void ONVIF_DetectDevice(void (*cb)(char *DeviceXAddr))
{
    int i;
    int result = 0;
    unsigned int count = 0;                                                     // 搜索到的设备个数
    struct soap *soap = NULL;                                                   // soap环境变量
    struct wsdd__ProbeType      req;                                            // 用于发送Probe消息
    struct __wsdd__ProbeMatches rep;                                            // 用于接收Probe应答
    struct wsdd__ProbeMatchType *probeMatch;

    SOAP_ASSERT(NULL != (soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT)));

    ONVIF_init_header(soap);                                                    // 设置消息头描述
    ONVIF_init_ProbeType(soap, &req);                                           // 设置寻找的设备的范围和类型
    result = soap_send___wsdd__Probe(soap, SOAP_MCAST_ADDR, NULL, &req);        // 向组播地址广播Probe消息
    while (SOAP_OK == result)                                                   // 开始循环接收设备发送过来的消息
    {
        memset(&rep, 0x00, sizeof(rep));
        result = soap_recv___wsdd__ProbeMatches(soap, &rep);
        if (SOAP_OK == result) {
            if (soap->error) {
                soap_perror(soap, "ProbeMatches");
            } else {                                                            // 成功接收到设备的应答消息
                dump__wsdd__ProbeMatches(&rep);

                if (NULL != rep.wsdd__ProbeMatches) {
                    count += rep.wsdd__ProbeMatches->__sizeProbeMatch;
                    for(i = 0; i < rep.wsdd__ProbeMatches->__sizeProbeMatch; i++) {
                        probeMatch = rep.wsdd__ProbeMatches->ProbeMatch + i;
                        if (NULL != cb) {
                            cb(probeMatch->XAddrs);                             // 使用设备服务地址执行函数回调
                        }
                    }
                }
            }
        } else if (soap->error) {
            break;
        }
    }

    SOAP_DBGLOG("\ndetect end! It has detected %d devices!\n", count);

    if (NULL != soap) {
        ONVIF_soap_delete(soap);
    }

    return ;
}

/************************************************************************
**函数：ONVIF_GetProfiles
**功能：获取设备的音视频码流配置信息
**参数：
        [in] MediaXAddr - 媒体服务地址
        [out] profiles  - 返回的设备音视频码流配置信息列表，调用者有责任使用free释放该缓存
**返回：
        返回设备可支持的码流数量（通常是主/辅码流），即使profiles列表个数
**备注：
        1). 注意：一个码流（如主码流）可以包含视频和音频数据，也可以仅仅包含视频数据。
************************************************************************/
int ONVIF_GetProfiles(const char *MediaXAddr, struct tagProfile **profiles)
{
    int i = 0;
    int result = 0;
    struct soap *soap = NULL;
    struct _trt__GetProfiles            req;
    struct _trt__GetProfilesResponse    rep;

    SOAP_ASSERT(NULL != MediaXAddr);
    SOAP_ASSERT(NULL != (soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT)));

    ONVIF_SetAuthInfo(soap, USERNAME, PASSWORD);

    memset(&req, 0x00, sizeof(req));
    memset(&rep, 0x00, sizeof(rep));
    result = soap_call___trt__GetProfiles(soap, MediaXAddr, NULL, &req, &rep);
    SOAP_CHECK_ERROR(result, soap, "GetProfiles");

    dump_trt__GetProfilesResponse(&rep);

    if (rep.__sizeProfiles > 0) {                                               // 分配缓存
        (*profiles) = (struct tagProfile *)malloc(rep.__sizeProfiles * sizeof(struct tagProfile));
        SOAP_ASSERT(NULL != (*profiles));
        memset((*profiles), 0x00, rep.__sizeProfiles * sizeof(struct tagProfile));
    }

    for(i = 0; i < rep.__sizeProfiles; i++) {                                   // 提取所有配置文件信息（我们所关心的）
        struct tt__Profile *ttProfile = &rep.Profiles[i];
        struct tagProfile *plst = &(*profiles)[i];

        if (NULL != ttProfile->token) {                                         // 配置文件Token
            strncpy(plst->token, ttProfile->token, sizeof(plst->token) - 1);
        }

        if (NULL != ttProfile->VideoEncoderConfiguration) {                     // 视频编码器配置信息
            if (NULL != ttProfile->VideoEncoderConfiguration->token) {          // 视频编码器Token
                strncpy(plst->venc.token, ttProfile->VideoEncoderConfiguration->token, sizeof(plst->venc.token) - 1);
            }
            if (NULL != ttProfile->VideoEncoderConfiguration->Resolution) {     // 视频编码器分辨率
                plst->venc.Width  = ttProfile->VideoEncoderConfiguration->Resolution->Width;
                plst->venc.Height = ttProfile->VideoEncoderConfiguration->Resolution->Height;
            }
        }
    }

EXIT:

    if (NULL != soap) {
        ONVIF_soap_delete(soap);
    }

    return rep.__sizeProfiles;
}

/************************************************************************
**函数：ONVIF_GetCapabilities
**功能：获取设备能力信息
**参数：
        [in] DeviceXAddr - 设备服务地址
        [out] capa       - 返回设备能力信息信息
**返回：
        0表明成功，非0表明失败
**备注：
    1). 其中最主要的参数之一是媒体服务地址
************************************************************************/
int ONVIF_GetCapabilities(const char *DeviceXAddr, struct tagCapabilities *capa)
{
    int result = 0;
    struct soap *soap = NULL;
    struct _tds__GetCapabilities            req;
    struct _tds__GetCapabilitiesResponse    rep;

    SOAP_ASSERT(NULL != DeviceXAddr);
    SOAP_ASSERT(NULL != capa);
    SOAP_ASSERT(NULL != (soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT)));

    ONVIF_SetAuthInfo(soap, USERNAME, PASSWORD);

    memset(&req, 0x00, sizeof(req));
    memset(&rep, 0x00, sizeof(rep));
    result = soap_call___tds__GetCapabilities(soap, DeviceXAddr, NULL, &req, &rep);
    SOAP_CHECK_ERROR(result, soap, "GetCapabilities");

    dump_tds__GetCapabilitiesResponse(&rep);

    memset(capa, 0x00, sizeof(struct tagCapabilities));
    if (NULL != rep.Capabilities) {
        if (NULL != rep.Capabilities->Media) {
            if (NULL != rep.Capabilities->Media->XAddr) {
                strncpy(capa->MediaXAddr, rep.Capabilities->Media->XAddr, sizeof(capa->MediaXAddr) - 1);
            }
        }
        if (NULL != rep.Capabilities->Events) {
            if (NULL != rep.Capabilities->Events->XAddr) {
                strncpy(capa->EventXAddr, rep.Capabilities->Events->XAddr, sizeof(capa->EventXAddr) - 1);
            }
        }
        if (NULL != rep.Capabilities->PTZ) {
            if (NULL != rep.Capabilities->PTZ->XAddr) {
                strncpy(capa->PTZXAddr, rep.Capabilities->PTZ->XAddr, sizeof(capa->PTZXAddr) - 1);
            }
        }
    }

EXIT:

    if (NULL != soap) {
        ONVIF_soap_delete(soap);
    }

    return result;
}

/************************************************************************
**函数：ONVIF_PTZ_ContinuousMove
**功能：使设备按照设置的PTZ速度持续移动
**参数：
        [in] profile    - MediaProfile的引用 
        [in] endpoint   - 设备服务地址
        [in] speed_Pan  - 水平移动速度, 取值范围(-100, +100)
        [in] speed_Tilt - 倾斜的移动速度, 取值范围(-100, +100)
        [in] speed_Zoom - 焦距的移动速度, 取值范围(-100, +100) 
**返回：
        0表明成功，非0表明失败
**备注：
		1) 焦距值越大， 距离目标越近， 焦距值越小， 距离目标越远
		2) 设备的移动速度与焦距有关， 焦距越近速度越慢	
		3) 速度的三个参数， PAN 正值代表顺时针转动，负值代表逆时针转动, 0值代表不移动
		                    Tilt 正值代表向上移动， 负值代表向下移动, 0值代表不移动
							Zoom 值越大代表焦距越大， 0值代表不移动
************************************************************************/

int ONVIF_PTZ_ContinuousMove(char *profile, char *endpoint, int speed_Pan, int speed_Tilt, int speed_Zoom)
{
	struct _tptz__ContinuousMove 	 			tptz__ContinuousMove;
	struct _tptz__ContinuousMoveResponse 		tptz__ContinuousMoveresponse;
    struct soap *soap = NULL;
	int result = 0;

    SOAP_ASSERT(NULL != profile);
    SOAP_ASSERT(NULL != endpoint);
    SOAP_ASSERT(speed_Pan <= 100 && speed_Pan >= -100);
    SOAP_ASSERT(speed_Tilt <= 100 && speed_Tilt >= -100);
    SOAP_ASSERT(speed_Zoom <= 100 && speed_Zoom >= -100);
    SOAP_ASSERT(NULL != (soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT)));

    memset(&tptz__ContinuousMove, 0x00, sizeof(tptz__ContinuousMove));
    memset(&tptz__ContinuousMoveresponse, 0x00, sizeof(tptz__ContinuousMoveresponse));

    ONVIF_SetAuthInfo(soap, USERNAME, PASSWORD);

	struct tt__PTZSpeed* velocity = soap_new_tt__PTZSpeed(soap, -1);
    SOAP_ASSERT(NULL != velocity);
	tptz__ContinuousMove.Velocity = velocity;

	struct tt__Vector2D* panTilt = soap_new_tt__Vector2D(soap, -1);
    SOAP_ASSERT(NULL != panTilt);
	tptz__ContinuousMove.Velocity->PanTilt = panTilt;

	struct tt__Vector1D* zoom = soap_new_tt__Vector1D(soap, -1);
    SOAP_ASSERT(NULL != zoom);
	tptz__ContinuousMove.Velocity->Zoom = zoom;

	tptz__ContinuousMove.ProfileToken = profile;
	tptz__ContinuousMove.Velocity->Zoom->x = (float)speed_Zoom / 100;
	tptz__ContinuousMove.Velocity->PanTilt->x = (float)speed_Pan / 100;
	tptz__ContinuousMove.Velocity->PanTilt->y = (float)speed_Tilt / 100;
	tptz__ContinuousMove.Velocity->PanTilt->space = "http://www.onvif.org/ver10/tptz/PanTiltSpaces/VelocityGenericSpace";

	result = soap_call___tptz__ContinuousMove(soap, endpoint, NULL, &tptz__ContinuousMove, &tptz__ContinuousMoveresponse);
	SOAP_CHECK_ERROR(result, soap, "soap_call___tptz__ContinuousMove");

EXIT:
    if (NULL != soap) {
        ONVIF_soap_delete(soap);
    }

    return result;
}

int ONVIF_PTZ_AbsoluteMove(char *profile, char *endpoint, int pos_Pan, int pos_Tilt, int pos_Zoom)
{
	struct _tptz__AbsoluteMove			tptz__AbsoluteMove;
	struct _tptz__AbsoluteMoveResponse  tptz__AbsoluteMoveResponse;
    struct soap *soap = NULL;
	int result = 0;

    SOAP_ASSERT(NULL != profile);
    SOAP_ASSERT(NULL != endpoint);
    SOAP_ASSERT(NULL != (soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT)));

    memset(&tptz__AbsoluteMove, 0x00, sizeof(tptz__AbsoluteMove));
    memset(&tptz__AbsoluteMoveResponse, 0x00, sizeof(tptz__AbsoluteMoveResponse));

    ONVIF_SetAuthInfo(soap, USERNAME, PASSWORD);

	struct tt__PTZVector* vector = soap_new_tt__PTZVector(soap, -1);
    SOAP_ASSERT(NULL != vector);
	tptz__AbsoluteMove.Position = vector;

	struct tt__Vector2D* panTilt = soap_new_tt__Vector2D(soap, -1);
    SOAP_ASSERT(NULL != panTilt);
	tptz__AbsoluteMove.Position->PanTilt = panTilt;

	struct tt__Vector1D* zoom = soap_new_tt__Vector1D(soap, -1);
    SOAP_ASSERT(NULL != zoom);
	tptz__AbsoluteMove.Position->Zoom = zoom;

	tptz__AbsoluteMove.ProfileToken = profile;
	tptz__AbsoluteMove.Position->Zoom->x = (float)pos_Zoom   / 100;
	tptz__AbsoluteMove.Position->PanTilt->x = (float)pos_Pan / 100;
	tptz__AbsoluteMove.Position->PanTilt->y = (float)pos_Tilt / 100;

	printf("p:%f  t: %f z: %f\n", tptz__AbsoluteMove.Position->PanTilt->x, tptz__AbsoluteMove.Position->PanTilt->y , tptz__AbsoluteMove.Position->Zoom->x);

	struct tt__PTZSpeed* s_velocity = soap_new_tt__PTZSpeed(soap, -1);
    SOAP_ASSERT(NULL != s_velocity);
	tptz__AbsoluteMove.Speed = s_velocity;

	struct tt__Vector2D* s_panTilt = soap_new_tt__Vector2D(soap, -1);
    SOAP_ASSERT(NULL != s_panTilt);
	tptz__AbsoluteMove.Speed->PanTilt = s_panTilt;

	struct tt__Vector1D* s_zoom = soap_new_tt__Vector1D(soap, -1);
    SOAP_ASSERT(NULL != s_zoom);
	tptz__AbsoluteMove.Speed->Zoom = s_zoom;

	tptz__AbsoluteMove.ProfileToken = profile;
	tptz__AbsoluteMove.Speed->Zoom->x = (float)(100 / 100);
	tptz__AbsoluteMove.Speed->PanTilt->x = (float)1;
	tptz__AbsoluteMove.Speed->PanTilt->y = (float)1;

	printf("p:%f  t: %f z: %f\n",  tptz__AbsoluteMove.Speed->PanTilt->x,  tptz__AbsoluteMove.Speed->PanTilt->y, tptz__AbsoluteMove.Speed->Zoom->x);
	result = soap_call___tptz__AbsoluteMove(soap, endpoint, NULL, &tptz__AbsoluteMove, &tptz__AbsoluteMoveResponse);
	SOAP_CHECK_ERROR(result, soap, "soap_call___tptz__AbsoluteMove");

EXIT:
    if (NULL != soap) {
        ONVIF_soap_delete(soap);
    }

    return result;
}

/************************************************************************
**函数：ONVIF_PTZ_Stop
**功能：使设备停止移动
**参数：
        [in] profile    - MediaProfile的引用 
        [in] endpoint   - 设备服务地址
**返回：
        0表明成功，非0表明失败
**备注：
		无
************************************************************************/
int ONVIF_PTZ_Stop(char *profile, char *endpoint)
{
	struct _tptz__Stop 							stop;
	struct _tptz__StopResponse 					stopresponse;
    struct soap *soap = NULL;
	int result = 0;

    SOAP_ASSERT(NULL != profile);
    SOAP_ASSERT(NULL != endpoint);
    SOAP_ASSERT(NULL != (soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT)));

	memset(&stop, 0, sizeof (stop));
	memset(&stopresponse, 0, sizeof (stopresponse));

    ONVIF_SetAuthInfo(soap, USERNAME, PASSWORD);

	stop.ProfileToken = profile;
	enum xsd__boolean *PT_boolean = soap_new_xsd__boolean(soap, -1);
	enum xsd__boolean *Z_boolean = soap_new_xsd__boolean(soap, -1);
	*PT_boolean = xsd__boolean__true_;
	*Z_boolean = xsd__boolean__true_;
	stop.PanTilt = PT_boolean;
	stop.Zoom = Z_boolean;

	result = soap_call___tptz__Stop(soap, endpoint, NULL, &stop, &stopresponse);
	SOAP_CHECK_ERROR(result, soap, "soap_call__tptz__Stop");

EXIT:
	if (NULL != soap) {
		ONVIF_soap_delete(soap);
	}

	return result;
}

/************************************************************************
**函数：ONVIF_PTZ_GotoHomePosition
**功能：使设备的PTZ值返回到设定的初始状态
**参数：
        [in] profile    - MediaProfile的引用 
        [in] endpoint   - 设备服务地址
        [in] speed_Pan  - 水平移动速度, 取值范围(-100, +100)
        [in] speed_Tilt - 倾斜的移动速度, 取值范围(-100, +100)
        [in] speed_Zoom - 焦距的移动速度, 取值范围(-100, +100) 
**返回：
        0表明成功，非0表明失败
**备注：
		无
************************************************************************/
int ONVIF_PTZ_GotoHomePosition(char *profile, char *endpoint, int speed_Pan, int speed_Tilt, int speed_Zoom)
{
    struct soap *soap = NULL;
	int result = 0;
	struct _tptz__GotoHomePosition			tptz__GotoHomePosition; 
	struct _tptz__GotoHomePositionResponse  tptz__GotoHomePositionResponse;

    SOAP_ASSERT(NULL != profile);
    SOAP_ASSERT(NULL != endpoint);
    SOAP_ASSERT(speed_Pan <= 100 && speed_Pan >= -100);
    SOAP_ASSERT(speed_Tilt <= 100 && speed_Tilt >= -100);
    SOAP_ASSERT(speed_Zoom <= 100 && speed_Zoom >= -100);
    SOAP_ASSERT(NULL != (soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT)));
	
	memset(&tptz__GotoHomePosition, 0, sizeof (tptz__GotoHomePosition));
	memset(&tptz__GotoHomePositionResponse, 0, sizeof (tptz__GotoHomePositionResponse));

    ONVIF_SetAuthInfo(soap, USERNAME, PASSWORD);

	struct tt__PTZSpeed* velocity = soap_new_tt__PTZSpeed(soap, -1);
    SOAP_ASSERT(NULL != velocity);
	tptz__GotoHomePosition.Speed = velocity;

	struct tt__Vector2D* panTilt = soap_new_tt__Vector2D(soap, -1);
    SOAP_ASSERT(NULL != panTilt);
	tptz__GotoHomePosition.Speed->PanTilt = panTilt;

	struct tt__Vector1D* zoom = soap_new_tt__Vector1D(soap, -1);
    SOAP_ASSERT(NULL != zoom);
	tptz__GotoHomePosition.Speed->Zoom = zoom;

	tptz__GotoHomePosition.ProfileToken = profile;
	tptz__GotoHomePosition.Speed->Zoom->x = (float)speed_Zoom / 100;
	tptz__GotoHomePosition.Speed->PanTilt->x = (float)speed_Pan / 100;
	tptz__GotoHomePosition.Speed->PanTilt->y = (float)speed_Tilt / 100;
	tptz__GotoHomePosition.Speed->PanTilt->space = "http://www.onvif.org/ver10/tptz/PanTiltSpaces/VelocityGenericSpace";

	result = soap_call___tptz__GotoHomePosition(soap, endpoint, NULL, &tptz__GotoHomePosition, &tptz__GotoHomePositionResponse);
	SOAP_CHECK_ERROR(result, soap, "soap_call___tptz__GotoHomePosition");

EXIT:
	if (NULL != soap) {
		ONVIF_soap_delete(soap);
	}

	return result;
}

/************************************************************************
**函数：ONVIF_PTZ_SetHomePosition
**功能：设定当前PTZ的值为初始状态
**参数：
        [in] profile    - MediaProfile的引用 
        [in] endpoint   - 设备服务地址
**返回：
        0表明成功，非0表明失败
**备注：
		无
************************************************************************/
int ONVIF_PTZ_SetHomePosition(char *profile,char *endpoint)
{
	struct _tptz__SetHomePosition homePosition;
	struct _tptz__SetHomePositionResponse homePositionResponse;
    struct soap *soap = NULL;
	int result = 0;

    SOAP_ASSERT(NULL != profile);
    SOAP_ASSERT(NULL != endpoint);
    SOAP_ASSERT(NULL != (soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT)));

	memset(&homePosition, 0, sizeof (homePosition));
	memset(&homePositionResponse, 0, sizeof (homePositionResponse));

    ONVIF_SetAuthInfo(soap, USERNAME, PASSWORD);

	homePosition.ProfileToken = profile;
	result = soap_call___tptz__SetHomePosition(soap, endpoint, NULL, &homePosition, &homePositionResponse);
	SOAP_CHECK_ERROR(result, soap, "soap_call___tptz__GotoHomePosition");

EXIT:
	if (NULL != soap) {
		ONVIF_soap_delete(soap);
	}

	return result;
}

/************************************************************************
**函数：make_uri_withauth
**功能：构造带有认证信息的URI地址
**参数：
        [in]  src_uri       - 未带认证信息的URI地址
        [in]  username      - 用户名
        [in]  password      - 密码
        [out] dest_uri      - 返回的带认证信息的URI地址
        [in]  size_dest_uri - dest_uri缓存大小
**返回：
        0成功，非0失败
**备注：
    1). 例子：
    无认证信息的uri：rtsp://100.100.100.140:554/av0_0
    带认证信息的uri：rtsp://username:password@100.100.100.140:554/av0_0
************************************************************************/
int make_uri_withauth(char *src_uri, char *username, char *password, char *dest_uri, unsigned int size_dest_uri)
{
    int result = 0;
    unsigned int needBufSize = 0;

    SOAP_ASSERT(NULL != src_uri);
    SOAP_ASSERT(NULL != username);
    SOAP_ASSERT(NULL != password);
    SOAP_ASSERT(NULL != dest_uri);
    memset(dest_uri, 0x00, size_dest_uri);

    needBufSize = strlen(src_uri) + strlen(username) + strlen(password) + 3;    // 检查缓存是否足够，包括‘:’和‘@’和字符串结束符
    if (size_dest_uri < needBufSize) {
        SOAP_DBGERR("dest uri buf size is not enough.\n");
        result = -1;
        goto EXIT;
    }

    if (0 == strlen(username) && 0 == strlen(password)) {                       // 生成新的uri地址
        strcpy(dest_uri, src_uri);
    } else {
        char *p = strstr(src_uri, "//");
        if (NULL == p) {
            SOAP_DBGERR("can't found '//', src uri is: %s.\n", src_uri);
            result = -1;
            goto EXIT;
        }
        p += 2;

        memcpy(dest_uri, src_uri, p - src_uri);
        sprintf(dest_uri + strlen(dest_uri), "%s:%s@", username, password);
        strcat(dest_uri, p);
    }

EXIT:

    return result;
}
