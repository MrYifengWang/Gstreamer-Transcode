#pragma once
#include <string>
using namespace std;

/*
task_config example

{
	"taskID": "ipc_192.168.110.66_h265_1024KB",
	"rtspUrl": "rtsp://admin:123456@192.168.110.66:554/h264/ch1/main/av_stream",
	"inputUri": "rtmp://192.168.110.66/h264/ch1/main/av_stream",
	"fileType": "mp4",
	"saveDir": "/home/wyfeng/cvts/dst",
	"biteRate": 1024,
	"gop": 60,
	"rcmode":"vbr" // cbr vbr
	"profile": "main",//baseline main high
}
*/
#include "define.h"


class IVCTInterface
{
public:
static void Release();
	static err_sdk_code Init(string& apikey,int chiptype = 0); //chiptype: 1 software 2 rockchip 3 nvv
	static string createTask(string& task_config);
	static bool deleteTask(string& tid);
	static string pullNotifyMessage();
	static string queryStatus(string& tid);
	static string queryLicense();

private:
	IVCTInterface(void);
	virtual ~IVCTInterface(void);
};

