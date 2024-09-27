#include "IGDKInterface.h"
#include "TaskManager.h"
#include "../common/ts_time.h"
#include "define.h"
IGDKInterface::IGDKInterface(void)
{
}

IGDKInterface::~IGDKInterface(void)
{
}

void IGDKInterface::Release() {}
err_sdk_code IGDKInterface::Init(string& apikey, int chiptype) {
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
string IGDKInterface::createTask(string& task_config) {
	string teststr;
	if(TaskManager::GetInstance()->isSDKAuth_ != 0){
		return teststr;
	}
	TaskManager::GetInstance()->addTask(task_config);
	
	return teststr;
}
bool IGDKInterface::deleteTask(string& tid) {

	if (TaskManager::GetInstance()->isSDKAuth_ != 0) {
		return false;
	}
	TaskManager::GetInstance()->delTask(tid);
}
string IGDKInterface::pullNotifyMessage() {
	string teststr = "{\"error\":\"not auth\"}";
	if (TaskManager::GetInstance()->isSDKAuth_ != 0) {
		ts_time::wait(2000);
		return teststr;
	}
	string tmpret = TaskManager::GetInstance()->pullStatus();
	return tmpret;
}
string IGDKInterface::queryStatus(string& tid) {
	string teststr = "{\"error\":\"not auth\"}";
	if (TaskManager::GetInstance()->isSDKAuth_ != 0) {
		ts_time::wait(2000);
		return teststr;
	}
	string tmpret = TaskManager::GetInstance()->queryTask();
	return tmpret;
}

string IGDKInterface::queryLicense() {
	string teststr = "{\"error\":\"invalid\"}";
	string emptystr;
	int retcode = TaskManager::GetInstance()->checkAuth(emptystr);
	if (retcode != 0)
	{
		return teststr;
	}
	else {
		char tmpbuf[256] = { 0 };
		sprintf(tmpbuf, "{\"exp_date\":\"%s\"}", TaskManager::GetInstance()->expLicDate_.c_str());
		teststr = tmpbuf;
	}
	return teststr;
}