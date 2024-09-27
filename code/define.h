#ifndef GST__DEFINE_H_
#define GST__DEFINE_H_

typedef enum
{
	ERR_NO_ERROR = -100,		//成功
	ERR_TOS,					//task 完成
	ERR_RTSP_LOST,				//10秒内没有收到视频数据，关闭任务
	ERR_RTSP_PASSWORD,			//密码错误
	ERR_DECODE_ERR,				//解码流程错，解码流程创建失败
	ERR_ENODE_ERR,				//编码流程错，编码流程创建失败
	ERR_USER_CLOSE,				//用户关闭任务
	ERR_AUTH_FAIL,				//SDK 鉴权失败
	ERR_RTSP_PUSH_FAIL,			//Rtsp 推流失败
	ERR_SRC_AV_OUT_SYNC,	        //
	ERR_RTSP_NET_TIMEOUT,
	ERR_RESL_CHANGED,
	ERR_FPS_CHANGED,
}err_sdk_code;

typedef void(*ts_task_status_notify)(const char* jsonMessage);

/*
task_config example

{
	"taskID": "ipc_192.168.110.66_h265_1024KB",
	"rtspUrl": "rtsp://admin:123456@192.168.110.66:554/h264/ch1/main/av_stream",
	"inputUri": "rtmp://192.168.110.66/h264/ch1/main/av_stream",
	"outputType": "mp4",
	"outputUri": "/home/wyfeng/cvts/dst",
	"bitRate": 1024,
	"gop": 60,
	"rcmode":"vbr" // cbr vbr
	"profile": "main",//baseline main high
}
*/
#endif