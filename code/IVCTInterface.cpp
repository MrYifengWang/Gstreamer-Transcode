#include "IVCTInterface.h"
#include "TaskManager.h"
#include "../common/ts_time.h"
#include "define.h"
#include "DebugLog.h"
IVCTInterface::IVCTInterface(void)
{
}

IVCTInterface::~IVCTInterface(void)
{
}

void IVCTInterface::Release() { TaskManager::GetInstance()->delAllTask(); }
err_sdk_code IVCTInterface::Init(string& apikey, int chiptype) {
	int retcode = TaskManager::GetInstance()->checkAuth(apikey);
	if (retcode != 0)
	{
		return ERR_AUTH_FAIL;
	}
	else {
		TaskManager::GetInstance()->chip_type_ = chiptype;
		DebugLog::writeLogF("user select encode type = %d\n", chiptype);
	}
	return ERR_NO_ERROR;
}
string IVCTInterface::createTask(string& task_config) {
	string teststr;
	if (TaskManager::GetInstance()->isSDKAuth_ != 0) {
		return teststr;
	}
	TaskManager::GetInstance()->addTask(task_config);

	return teststr;
}
bool IVCTInterface::deleteTask(string& tid) {

	if (TaskManager::GetInstance()->isSDKAuth_ != 0) {
		return false;
	}
	TaskManager::GetInstance()->delTask(tid);
}
string IVCTInterface::pullNotifyMessage() {
	string teststr = "{\"error\":\"not auth\"}";
	if (TaskManager::GetInstance()->isSDKAuth_ != 0) {
		ts_time::wait(2000);
		return teststr;
	}
	string tmpret = TaskManager::GetInstance()->pullStatus();
	return tmpret;
}
string IVCTInterface::queryStatus(string& tid) {
	string teststr = "{\"error\":\"emtpy\"}";
	if (TaskManager::GetInstance()->isSDKAuth_ != 0) {
		ts_time::wait(2000);
		return teststr;
	}
	string tmpret = TaskManager::GetInstance()->queryTask(tid);
	return tmpret;
}
string IVCTInterface::queryLicense() {
	string teststr = "{\"error\":\"invalid\"}";
	string emptystr;
	int retcode = TaskManager::GetInstance()->checkAuth(emptystr);
	if (retcode != 0)
	{
		return teststr;
	}
	else {
		char tmpbuf[256] = {0};
		sprintf(tmpbuf,"{\"exp_date\":\"%s\"}", TaskManager::GetInstance()->expLicDate_.c_str());
		teststr = tmpbuf;
	}
	return teststr;
}