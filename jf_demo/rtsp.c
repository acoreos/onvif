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

/************************************************************************
**
** 作者：梁友
** 日期：2018-07-02
** 描述：读取IPC音视频流数据示例代码
**
************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include "onvif_comm.h"
#include "onvif_dump.h"

#include "libavcodec/avcodec.h"
#include "libavdevice/avdevice.h"
#include "libavformat/avformat.h"
#include "libavfilter/avfilter.h"
#include "libavutil/avutil.h"
#include "libswscale/swscale.h"
#include "libavutil/pixdesc.h"
#include "libavutil/mathematics.h"
/************************************************************************
**函数：open_rtsp
**功能：从RTSP获取音视频流数据
**参数：
        [in]  rtsp - RTSP地址
**返回：无
************************************************************************/
void open_rtsp(char *rtsp)
{
    unsigned int    i;
    int             ret;
    int             video_st_index = -1;
    int             audio_st_index = -1;
    AVFormatContext *ifmt_ctx = NULL;
    AVFormatContext *ofmt_ctx = NULL;
    AVStream        *st = NULL;
    AVStream        *o_video_stream = NULL;
	AVStream        *i_video_stream = NULL;
    char            errbuf[64];

	AVCodec			*pCodec;
	AVFrame *pFrame, *pFrameYUV;
	AVCodecContext  *pCodecCtx;
	uint8_t *out_buffer;
	struct SwsContext *img_convert_ctx;

    av_register_all();                                                          // Register all codecs and formats so that they can be used.
    avformat_network_init();                                                    // Initialization of network components

    if ((ret = avformat_open_input(&ifmt_ctx, rtsp, 0, NULL)) < 0) {            // Open the input file for reading.
        printf("Could not open input file '%s' (error '%s')\n", rtsp, av_make_error_string(errbuf, sizeof(errbuf), ret));
        goto EXIT;
    }

    if ((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0) {                // Get information on the input file (number of streams etc.).
        printf("Could not open find stream info (error '%s')\n", av_make_error_string(errbuf, sizeof(errbuf), ret));
        goto EXIT;
    }

    for (i = 0; i < ifmt_ctx->nb_streams; i++) {                                // dump information
        av_dump_format(ifmt_ctx, i, rtsp, 0);
    }

    for (i = 0; i < ifmt_ctx->nb_streams; i++) {                                // find video stream index
        st = ifmt_ctx->streams[i];
        switch(st->codec->codec_type) {
        case AVMEDIA_TYPE_AUDIO: audio_st_index = i; break;
        case AVMEDIA_TYPE_VIDEO: video_st_index = i; break;
        default: break;
        }
    }

    if (-1 == video_st_index) {
        printf("No H.264 video stream in the input file\n");
        goto EXIT;
    }

#if 1
	i_video_stream = ifmt_ctx->streams[video_st_index];

	char *filename = strdup("/root/1.mp4");
	if ((ret = avformat_alloc_output_context2(&ofmt_ctx, NULL, "mp4", filename)) < 0) {
        printf("Could not alloc output context'%s' (error '%s')\n", rtsp, av_make_error_string(errbuf, sizeof(errbuf), ret));
        goto EXIT;
	}

	o_video_stream = avformat_new_stream(ofmt_ctx, i_video_stream->codec->codec);
	if (o_video_stream == NULL) {
        printf("Could not new stream '%s' (error '%s')\n", rtsp, av_make_error_string(errbuf, sizeof(errbuf), ret));
        goto EXIT;
	}

	if ((ret = avcodec_copy_context(o_video_stream->codec, i_video_stream->codec)) < 0) {
        printf("Could not avcodec_copy_context'%s' (error '%s')\n", rtsp, av_make_error_string(errbuf, sizeof(errbuf), ret));
        goto EXIT;
	}

    if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
    {
        o_video_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }


	if ((ret = avio_open(&ofmt_ctx->pb, filename, AVIO_FLAG_WRITE)) < 0) {
        printf("Could not avio open '%s' (error '%s')\n", rtsp, av_make_error_string(errbuf, sizeof(errbuf), ret));
        goto EXIT;
	}

	if ((ret = avformat_write_header(ofmt_ctx, NULL)) < 0) {
        printf("Could not write header '%s' (error '%s')\n", rtsp, av_make_error_string(errbuf, sizeof(errbuf), ret));
        goto EXIT;

	}
	av_dump_format(ofmt_ctx, 0, filename, 1);

    AVPacket pkt; 
    av_init_packet(&pkt); 

    while (1) 
    { 
        static int s_num = 0; 
        if (av_read_frame(ifmt_ctx, &pkt) < 0) 
            break; 

		if (pkt.stream_index == video_st_index) {
			pkt.pts = av_rescale_q_rnd(pkt.pts, i_video_stream->time_base, o_video_stream->time_base, (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        	pkt.dts = av_rescale_q_rnd(pkt.dts, i_video_stream->time_base, o_video_stream->time_base, (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        	pkt.duration = av_rescale_q(pkt.duration, i_video_stream->time_base, o_video_stream->time_base);
        	pkt.pos = -1;

			printf("s_num: %d\n", s_num);
#if 1
			static int64_t last_pts = 0;
			static int64_t last_dts = 0;
			static char first_frame_to_write = 1;
			if (s_num == 0) {
				last_pts = pkt.pts;
				last_dts = pkt.dts;
				s_num = s_num + 1;
			}
			if(pkt.pts > last_pts) {
				printf("pkt.dts: %lld  pkt.pts: %lld\n", pkt.dts,  pkt.pts);
				if ((first_frame_to_write == 1) && (pkt.flags & AV_PKT_FLAG_KEY)) {
					first_frame_to_write = 0;
				}
				if (first_frame_to_write == 0) {
					av_interleaved_write_frame(ofmt_ctx, &pkt); 
					if (s_num++ >= 250)
						break;
				}
			}
#else
			printf("pkt.dts: %lld  pkt.pts: %lld\n", pkt.dts,  pkt.pts);
			av_interleaved_write_frame(ofmt_ctx, &pkt); 

			if (s_num++ >= 250)
				break;
#endif
		}

		av_free_packet(&pkt);

    } 

	av_write_trailer(ofmt_ctx);
	printf("over!!!!!!!!!!!\n"); 
	return ;
#else
    AVPacket *pkt; 
	pCodecCtx = ifmt_ctx->streams[video_st_index]->codec;
	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
	if (NULL == pCodec) {
		printf("Codec not found.\n");
		goto EXIT;
	}

	if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
		printf("can not open codec\n");
		goto EXIT;
	}

	pFrame = av_frame_alloc();
	pFrameYUV = av_frame_alloc();
	out_buffer = (uint8_t *)av_malloc(avpicture_get_size(AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height));
	avpicture_fill((AVPicture *)pFrameYUV, out_buffer, AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height);

	img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt,
						pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);
 

	pkt = (AVPacket *)av_malloc(sizeof(AVPacket));


    while (1)
    {
		FILE *file = get_valied_fp();
		if (file == NULL) {
			perror("get_valied_fp");
			goto EXIT;
		}

        do {
            ret = av_read_frame(ifmt_ctx, pkt);                                // read frames
        } while (ret == AVERROR(EAGAIN));

        if (ret < 0) {
            printf("Could not read frame (error '%s')\n", av_make_error_string(errbuf, sizeof(errbuf), ret));
            break;
        }

        if (pkt->stream_index == video_st_index) {                               // video frame
			printf("pkt.dts: %ld  pkt.pts: %ld\n", pkt->dts,  pkt->pts);
            //printf("Video Packet size = %d\n", pkt->size);
			fwrite(pkt->data, 1, pkt->size, file);
        } else if(pkt->stream_index == audio_st_index) {                         // audio frame
            //printf("Audio Packet size = %d\n", pkt->size);
        } else {
            printf("Unknow Packet size = %d\n", pkt->size);
        }

		av_free_packet(pkt);
    }

#endif
EXIT:

    if (NULL != ifmt_ctx) {
        avformat_close_input(&ifmt_ctx);
        ifmt_ctx = NULL;
    }

    return ;
}

/************************************************************************
**函数：ONVIF_GetStreamUri
**功能：获取设备码流地址(RTSP)
**参数：
        [in]  MediaXAddr    - 媒体服务地址
        [in]  ProfileToken  - the media profile token
        [out] uri           - 返回的地址
        [in]  sizeuri       - 地址缓存大小
**返回：
        0表明成功，非0表明失败
**备注：
************************************************************************/
int ONVIF_GetStreamUri(const char *MediaXAddr, char *ProfileToken, char *uri, unsigned int sizeuri)
{
    int result = 0;
    struct soap *soap = NULL;
    struct tt__StreamSetup              ttStreamSetup;
    struct tt__Transport                ttTransport;
    struct _trt__GetStreamUri           req;
    struct _trt__GetStreamUriResponse   rep;

    SOAP_ASSERT(NULL != MediaXAddr);
    SOAP_ASSERT(NULL != uri);
    memset(uri, 0x00, sizeuri);

    SOAP_ASSERT(NULL != (soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT)));

    memset(&req, 0x00, sizeof(req));
    memset(&rep, 0x00, sizeof(rep));
    memset(&ttStreamSetup, 0x00, sizeof(ttStreamSetup));
    memset(&ttTransport, 0x00, sizeof(ttTransport));
    ttStreamSetup.Stream                = tt__StreamType__RTP_Unicast;
    ttStreamSetup.Transport             = &ttTransport;
    ttStreamSetup.Transport->Protocol   = tt__TransportProtocol__RTSP;
    ttStreamSetup.Transport->Tunnel     = NULL;
    req.StreamSetup                     = &ttStreamSetup;
    req.ProfileToken                    = ProfileToken;

    ONVIF_SetAuthInfo(soap, USERNAME, PASSWORD);
    result = soap_call___trt__GetStreamUri(soap, MediaXAddr, NULL, &req, &rep);
    SOAP_CHECK_ERROR(result, soap, "GetServices");

    dump_trt__GetStreamUriResponse(&rep);

    result = -1;
    if (NULL != rep.MediaUri) {
        if (NULL != rep.MediaUri->Uri) {
            if (sizeuri > strlen(rep.MediaUri->Uri)) {
                strcpy(uri, rep.MediaUri->Uri);
                result = 0;
            } else {
                SOAP_DBGERR("Not enough cache!\n");
            }
        }
    }

EXIT:

    if (NULL != soap) {
        ONVIF_soap_delete(soap);
    }

    return result;
}

void cb_discovery(char *DeviceXAddr)
{
    int stmno = 0;                                                              // 码流序号，0为主码流，1为辅码流
    int profile_cnt = 0;                                                        // 设备配置文件个数
    struct tagProfile *profiles = NULL;                                         // 设备配置文件列表
    struct tagCapabilities capa;                                                // 设备能力信息

    char uri[ONVIF_ADDRESS_SIZE] = {0};                                         // 不带认证信息的URI地址
    char uri_auth[ONVIF_ADDRESS_SIZE + 50] = {0};                               // 带有认证信息的URI地址

    ONVIF_GetCapabilities(DeviceXAddr, &capa);                                  // 获取设备能力信息（获取媒体服务地址）

    profile_cnt = ONVIF_GetProfiles(capa.MediaXAddr, &profiles);                // 获取媒体配置信息（主/辅码流配置信息）

    if (profile_cnt > stmno) {
        ONVIF_GetStreamUri(capa.MediaXAddr, profiles[stmno].token, uri, sizeof(uri)); // 获取RTSP地址

        make_uri_withauth(uri, USERNAME, PASSWORD, uri_auth, sizeof(uri_auth)); // 生成带认证信息的URI（有的IPC要求认证）

        open_rtsp(uri_auth);                                                    // 读取主码流的音视频数据
    }

    if (NULL != profiles) {
        free(profiles);
        profiles = NULL;
    }
}

int rtsp_main(void)
{
    ONVIF_DetectDevice(cb_discovery);

    return 0;
}
