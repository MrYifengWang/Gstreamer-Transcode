#ifndef GST__DEFINE_H_
#define GST__DEFINE_H_

typedef enum
{
	ERR_NO_ERROR = -100,		//�ɹ�
	ERR_TOS,					//task ���
	ERR_RTSP_LOST,				//10����û���յ���Ƶ���ݣ��ر�����
	ERR_RTSP_PASSWORD,			//�������
	ERR_DECODE_ERR,				//�������̴��������̴���ʧ��
	ERR_ENODE_ERR,				//�������̴��������̴���ʧ��
	ERR_USER_CLOSE,				//�û��ر�����
	ERR_AUTH_FAIL,				//SDK ��Ȩʧ��
	ERR_RTSP_PUSH_FAIL,			//Rtsp ����ʧ��
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