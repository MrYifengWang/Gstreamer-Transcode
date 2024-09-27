#pragma once

#include <map>
#include <string>
#include "../common/ts_json.h"
#include "../common/ts_thread.h"
#include "define.h"

typedef enum
{
	CHIP_NOTHING = 0,
	CHIP_SOFTWARE,
	CHIP_ROCKCHIP,
	CHIP_NVV,
	CHIP_TRYCPU
}chip_type_code;


using namespace std;
class RtspTask;
class FileTask;
class YuvTask;
class TaskFlow;
class StatusQueue;
class TaskManager //:public IVCTSInterface
{

private:
	TaskManager();
	~TaskManager();
private:
	static TaskManager* pInstance;
public:
	static TaskManager * GetInstance()
	{
		if (pInstance == NULL)
			pInstance = new TaskManager();
		return pInstance;
	}

public:
	string pullStatus();
	int delAllTask();//
	string queryTask(string& tid);
	int addTask(string& jsonconfig);
	int delTask(string tHandle);
	int checkAuth(string apikey);
	int checkOnlineLic(string apikey);
	int checkOfflienLic();
	int removeTask(string tHandle);

	int onAnyWorkStatus(TSJson::Value &message, int type);

public:
	string expLicDate_;

private:
	string devMac_;
	string apiKey_;
	string ExpDate_;
	
	int getDevMac();

private:
	bool bGstInit;

	map<string, TaskFlow*> mFlows;
	typedef map<string, TaskFlow*>::iterator tfIt;
	ts_mutex tf_lock;
	StatusQueue* pTaskStatus;
	ts_mutex mq_lock;

public:
	static int total_tid;
	int isSDKAuth_ = -1;
	int chip_type_ = 0; //0,auto 1,software,2 ,rockchip,3,omx
	ts_task_status_notify Callback=NULL;

};

