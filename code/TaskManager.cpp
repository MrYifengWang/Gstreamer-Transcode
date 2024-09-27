#include "TaskManager.h"
#include "RtspTask.h"
#include "RtspTask1.h"

#include "TaskFlow.h"
#include "../common/ts_time.h"
#include "../common/ptsdef.h"
#include "../common/aydes.h"
#include "../common/EdUrlParser.h"
#include <curl/curl.h>
#include "StatusQueue.h"
#include "DebugLog.h"
string gdevMac_;
string genstr_;
string keyfilePath = "./Keyfile.dat";
TaskManager* TaskManager::pInstance = NULL;
int TaskManager::total_tid = 0;

TaskManager::TaskManager()
{
	bGstInit = false;
	if (!bGstInit) {
		bGstInit = true;
		gst_init(NULL, NULL);
	}

	pTaskStatus = new StatusQueue();
	Callback = NULL;

	chip_type_ = 2;
}
TaskManager::~TaskManager()
{
	if (pTaskStatus != NULL) {
		delete pTaskStatus;
		pTaskStatus = NULL;
	}
}

int TaskManager::delAllTask()
{
	ts_autoLock lock(tf_lock);
	DebugLog::writeLogF("close all tasks\n");
	tfIt iter = mFlows.begin();
	while (iter != mFlows.end()) {
		TaskFlow* tmptask = iter->second;
		tmptask->stop();
		mFlows.erase(iter++);
		//delete tmptask;
	}
}
string TaskManager::queryTask(string& tid) {
	ts_autoLock lock(tf_lock);

	bool found = false;
	TSJson::Value root, arr;
	tfIt  iter;
	for (iter = mFlows.begin(); iter != mFlows.end(); iter++) {
		TaskFlow * pFlow = iter->second;
		if (tid == pFlow->taskid_) {
			root["taskID"] = pFlow->taskid_;
			root["rtspUrl"] = pFlow->rtspUrl_;
			root["starttime"] = pFlow->task_start_time_;
			root["dur_seconds"] = ts_time::current() - pFlow->task_start_time_;
			found = true;
			break;
		}


	}
	/*	for (iter = mFlows.begin(); iter != mFlows.end(); iter++) {

			TSJson::Value item;
			TaskFlow * pFlow = iter->second;

			item["taskID"] = pFlow->taskid_;
			item["rtspUrl"] = pFlow->rtspUrl_;
			item["starttime"] = pFlow->task_start_time_;
			item["dur_seconds"] = ts_time::current() - pFlow->task_start_time_;

			arr.append(item);

		}

		root["tasklist"] = arr;
	*/

	string tmpret;
	if (found) {
		TSJson::FastWriter writer;
		tmpret = writer.write(root);
	}

	return tmpret;


};
int TaskManager::addTask(string& jsonconfig) {
	ts_autoLock lock(tf_lock);

	/*if (mFlows.size() > 10) {
		return -2;
	}*/

	TSJson::Reader reader;
	TSJson::Value jmessage;
	if (reader.parse(jsonconfig, jmessage))
	{
		string taskid, rtspurl, outputtype, outputpath, rtspuname;
		int bitrate = 0;
		if (jmessage["rtspUrl"].isString())
			rtspurl = jmessage["rtspUrl"].asString();
		if (jmessage["taskID"].isString())
			taskid = jmessage["taskID"].asString();
		if (jmessage["outputType"].isString())
			outputtype = jmessage["outputType"].asString();
		if (jmessage["outputUri"].isString())
			outputpath = jmessage["outputUri"].asString();
		if (jmessage["bitRate"].isInt())
			bitrate = jmessage["bitRate"].asInt();

		//check params

		//if (rtspurl.length() > 0) 
		{
			TaskFlow* newFlow = new TaskFlow(jmessage);
			mFlows[taskid] = newFlow;
			DebugLog::writeLogF("open one task %s\n", newFlow->taskid_.c_str());

			newFlow->start();
			return 1;
		}
		/*else {
			return -1;
		}*/
	}
	return -1;
}

int TaskManager::removeTask(string tHandle)
{
	ts_autoLock lock(tf_lock);
	tfIt it = mFlows.find(tHandle);
	if (it != mFlows.end()) {
		DebugLog::writeLogF("self close one task %s\n", tHandle.c_str());
		mFlows.erase(it);
	}
}

int TaskManager::delTask(string tHandle) {
	ts_autoLock lock(tf_lock);
	tfIt it = mFlows.find(tHandle);
	if (it != mFlows.end()) {
		TaskFlow* tmptask = mFlows[tHandle];
		tmptask->stop();
		DebugLog::writeLogF("close one task %s\n", tmptask->taskid_.c_str());
		mFlows.erase(it);
		//delete tmptask;
	}
}

int responseCallback(char *ptr, size_t size, size_t nmemb, string *buf)
{
	unsigned long sizes = size * nmemb;
	if (buf == NULL)
		return -1;
	buf->append(ptr, sizes);
	return sizes;

}

int getDevMacLoc() {
	//devMac_ = "11:22:33:44:55:66";
	gdevMac_ = "11:22:33:44:55:66";
	genstr_ += gdevMac_;
	return 0;

	int sock, if_count, i;
	struct ifconf ifc;
	struct ifreq ifr[10];
	unsigned char mac[6];
	char macstr[64] = { 0 };

	memset(&ifc, 0, sizeof(struct ifconf));
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	ifc.ifc_len = 10 * sizeof(struct ifreq);
	ifc.ifc_buf = (char *)ifr;

	ioctl(sock, SIOCGIFCONF, (char *)&ifc);
	if_count = ifc.ifc_len / (sizeof(struct ifreq));
	for (i = 0; i < if_count; i++) {
		if (ioctl(sock, SIOCGIFHWADDR, &ifr[i]) == 0) {
			if (strcmp(ifr[i].ifr_name, "lo") == 0)
				continue;
			memcpy(mac, ifr[i].ifr_hwaddr.sa_data, 6);
			if (mac[0] == 0 && mac[1] == 0 && mac[2] == 0 && mac[3] == 0
				&& mac[4] == 0 && mac[5] == 0)
				continue;
			/*	printf("eth: %s, mac: %02x:%02x:%02x:%02x:%02x:%02x\n",
					ifr[i].ifr_name, mac[0], mac[1], mac[2], mac[3], mac[4],
					mac[5]);*/
			sprintf(macstr, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1],
				mac[2], mac[3], mac[4], mac[5]);
			gdevMac_ = macstr;
			genstr_ += gdevMac_;
			genstr_ = EdUrlParser::trimmed(genstr_);
			/*
						if (genstr_.length() < 32) {
							int padlen = 32 - genstr_.length();
							for (int i = 0; i < padlen; i++) {
								genstr_ += "0";
							}
						}
				  */

			break;
		}
	}
	close(sock);

	return 0;

}

int get_dev_id() {
	/*

	char testBuf[512] = { 0 };
	char buf[512] = "cat /proc/cpuinfo|grep Serial > sntmp.md";
	//	char buf[128] = "cat /proc/cpuinfo|grep fpu_exception > sntmp.md";
	system(buf);

	FILE *fp = fopen("sntmp.md", "rb"); //write correct path
	if (NULL == fp) {
		return -1;
	}
	int size = fread(testBuf, sizeof(char), sizeof(testBuf), fp);
	//	char * test1 = fgets(testBuf, sizeof(testBuf), fp);
	fclose(fp);

	system("rm -f sntmp.md");

	string plainstr(testBuf);
	//	puts(plainstr.c_str());


	std::vector < std::string > itemlist;
	split(plainstr, ":", &itemlist);


	if (itemlist.size() < 2) return -2;
	//		puts(itemlist[1].c_str());

	genstr_ = itemlist[1];
	genstr_ = EdUrlParser::trimmed(genstr_);
	*/

	{
		char testBuf[512] = { 0 };
		char buf[512] = "dmidecode -s baseboard-serial-number > sntmp.md";
		//	char buf[128] = "cat /proc/cpuinfo|grep fpu_exception > sntmp.md";
		system(buf);

		FILE *fp = fopen("sntmp.md", "rb");//write correct path
		if (NULL == fp) {
			return -1;
		}
		int size = fread(testBuf, sizeof(char), sizeof(testBuf), fp);
		//	char * test1 = fgets(testBuf, sizeof(testBuf), fp);
		fclose(fp);
		if (size > 0) {
			string plainstr(testBuf);
			//puts(plainstr.c_str());
			genstr_ += plainstr;
			genstr_ = EdUrlParser::trimmed(genstr_);
		}
		system("rm -f sntmp.md");
	}
	{
		char testBuf[512] = { 0 };
		char buf[512] = "dmidecode -s system-uuid > sntmp.md";
		system(buf);

		FILE *fp = fopen("sntmp.md", "rb");//write correct path
		if (NULL == fp) {
			return -1;
		}
		int size = fread(testBuf, sizeof(char), sizeof(testBuf), fp);
		fclose(fp);
		if (size > 0) {
			string plainstr(testBuf);
			//puts(plainstr.c_str());
			genstr_ += plainstr;
			genstr_ = EdUrlParser::trimmed(genstr_);
		}
		system("rm -f sntmp.md");
	}

	{
		//
		char testBuf[512] = { 0 };
		char buf[512] = "cat /proc/cpuinfo | grep name | cut -f2 -d: | uniq -c > sntmp.md";
		system(buf);

		FILE *fp = fopen("sntmp.md", "rb");//write correct path
		if (NULL == fp) {
			return -1;
		}
		int size = fread(testBuf, sizeof(char), sizeof(testBuf), fp);
		fclose(fp);

		if (size > 0) {
			string plainstr(testBuf);
			//puts(plainstr.c_str());
			genstr_ += plainstr;
			genstr_ = EdUrlParser::trimmed(genstr_);
		}

		system("rm -f sntmp.md");
	}

	return 0;
}


int getDevMacLoc_arm() {
	//devMac_ = "A0:B8:0C:E2:35:ED";
	gdevMac_ = "A0:B8:0C:E2:35:ED";
	genstr_ += gdevMac_;
	genstr_ = EdUrlParser::trimmed(genstr_);

	return 0;

	int sock, if_count, i;
	struct ifconf ifc;
	struct ifreq ifr[10];
	unsigned char mac[6];
	char macstr[64] = { 0 };

	memset(&ifc, 0, sizeof(struct ifconf));
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	ifc.ifc_len = 10 * sizeof(struct ifreq);
	ifc.ifc_buf = (char *)ifr;

	ioctl(sock, SIOCGIFCONF, (char *)&ifc);
	if_count = ifc.ifc_len / (sizeof(struct ifreq));
	for (i = 0; i < if_count; i++) {
		if (ioctl(sock, SIOCGIFHWADDR, &ifr[i]) == 0) {
			if (strcmp(ifr[i].ifr_name, "lo") == 0)
				continue;
			memcpy(mac, ifr[i].ifr_hwaddr.sa_data, 6);
			if (mac[0] == 0 && mac[1] == 0 && mac[2] == 0 && mac[3] == 0
				&& mac[4] == 0 && mac[5] == 0)
				continue;
			/*	printf("eth: %s, mac: %02x:%02x:%02x:%02x:%02x:%02x\n",
					ifr[i].ifr_name, mac[0], mac[1], mac[2], mac[3], mac[4],
					mac[5]);*/
			sprintf(macstr, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1],
				mac[2], mac[3], mac[4], mac[5]);
			gdevMac_ = macstr;
			genstr_ += gdevMac_;
			genstr_ = EdUrlParser::trimmed(genstr_);
			/*
						if (genstr_.length() < 32) {
							int padlen = 32 - genstr_.length();
							for (int i = 0; i < padlen; i++) {
								genstr_ += "0";
							}
						}
				  */

			break;
		}
	}
	close(sock);

	return 0;

}

int get_dev_id_arm() {

	char testBuf[512] = { 0 };
	char buf[512] = "cat /proc/cpuinfo|grep Serial > sntmp.md";
	//	char buf[128] = "cat /proc/cpuinfo|grep fpu_exception > sntmp.md";
	system(buf);

	FILE *fp = fopen("sntmp.md", "rb"); //write correct path
	if (NULL == fp) {
		return -1;
	}
	int size = fread(testBuf, sizeof(char), sizeof(testBuf), fp);
	//	char * test1 = fgets(testBuf, sizeof(testBuf), fp);
	fclose(fp);

	system("rm -f sntmp.md");

	string plainstr(testBuf);
	//	puts(plainstr.c_str());


	std::vector < std::string > itemlist;
	EdUrlParser::split(plainstr, ":", &itemlist);


	if (itemlist.size() < 2) return -2;
	//		puts(itemlist[1].c_str());

	genstr_ = itemlist[1];
	genstr_ = EdUrlParser::trimmed(genstr_);

	return 0;
}

int TaskManager::checkOnlineLic(string apikey)
{

#ifndef WIN32
	//-----------------------------


	apiKey_ = apikey;
	puts(apikey.c_str());
	if (devMac_.empty()) {
		getDevMac();
	}
	if (devMac_.empty()) {
		puts("get mac failed------------");
		return -1;
	}
	//puts(devMac_.c_str());
	isSDKAuth_ = -1;
	CURL *curl;
	CURLcode res;

	curl = curl_easy_init();
	if (curl)
	{
		char strURL[512] = { 0 };
#ifdef MAKE_GDK
		sprintf(strURL, "http://120.79.36.189:8000/api/v1/secure/");
#else
		sprintf(strURL, "http://39.108.168.119:8000/api/v1/secure/");
#endif // MAKE_GDK

		string respBuf;

		struct curl_slist *headers = NULL;
		{
			char headbuf[256] = { 0 };
			sprintf(headbuf, "X-Mac-Add:%s", this->devMac_.c_str());
			headers = curl_slist_append(headers, headbuf);
			puts(headbuf);
		}
		{
			char headbuf[256] = { 0 };
			sprintf(headbuf, "X-Api-Key:%s", this->apiKey_.c_str());
			headers = curl_slist_append(headers, headbuf);
			puts(headbuf);
		}

		//headers = curl_slist_append(headers, "Host: 120.79.36.189:8000");
		//headers = curl_slist_append(headers, "Upgrade-Insecure-Requests: 1");
		headers = curl_slist_append(headers, "Connection: keep - alive");
		headers = curl_slist_append(headers, "User-Agent: Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/108.0.5359.95 Safari/537.36");
		headers = curl_slist_append(headers, "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.9");
		headers = curl_slist_append(headers, "Accept-Language: zh-CN,zh;q=0.9");
		headers = curl_slist_append(headers, "Accept-Encoding: gzip, deflate");

		//curl_easy_setopt(curl, CURLOPT_HTTPAUTH, (long)CURLAUTH_DIGEST);
		//curl_easy_setopt(curl, CURLOPT_USERNAME, username.c_str());
		//curl_easy_setopt(curl, CURLOPT_PASSWORD, password.c_str());
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10);
		//curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
		curl_easy_setopt(curl, CURLOPT_URL, strURL);
		curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, responseCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &respBuf);
		res = curl_easy_perform(curl);

		if (res == 0)
		{
			long responsecode = 200;
			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responsecode);
			printf("auth response code = %d\n", responsecode);
			if (responsecode == 200) //==401
			{
				isSDKAuth_ = 0;
			}
		}
		curl_slist_free_all(headers);
		curl_easy_cleanup(curl);
	}

#endif
	return isSDKAuth_;
}
int TaskManager::checkOfflienLic() {
	

//isSDKAuth_ = 0;
//	 return 0;
#ifdef MAKE_X86
	get_dev_id();
	getDevMacLoc();
#else
	get_dev_id_arm();
	getDevMacLoc_arm();
#endif

	//puts("-------offline check--------------1");
	unsigned char genbuf[1024] = { 0 };
	FILE* pf = fopen(keyfilePath.c_str(), "rb");
	if (pf == NULL) return -5;
	int glen = fread(genbuf, 1, 1024, pf);
	fclose(pf);

	int des_len = 1024;
	int outlen = 1024;
	unsigned char outbuf[1024] = { 0 };

	if (DES_Decrypt(genbuf, glen, (unsigned char*)_ANYAN_KEYFILE_KEY, 8,
		(unsigned char*)outbuf, outlen, &des_len) != ANYAN_DES_OK) {
		puts("DES_Decrypt err3\n");
		return -1;
	}
	//puts("-------offline check--------------2");
	std::vector < std::string > itemlist;
	string plainstr = (char*)outbuf;
	//	puts(plainstr.c_str());
	EdUrlParser::split(plainstr, "|", &itemlist);
	string trim_id = EdUrlParser::trimmed(genstr_);

	// puts(genstr_.c_str());
	// puts(trim_id.c_str());

	if (itemlist.size() < 2)
		return -1;
		//puts(itemlist[0].c_str());
		//puts(itemlist[1].c_str());
	if (itemlist[0] != trim_id)
		return -2;
	if (itemlist[1].size() < 8)
		return -3;
	int year, day, moth;
	sscanf(itemlist[1].c_str(), "%04d%02d%02d", &year, &moth, &day);
	//puts("-------offline check--------------3");
	time_t t = time(NULL);
	struct tm* l = localtime(&t);

	int curyear = l->tm_year + 1900;
	int curmonth = l->tm_mon + 1;
	int curday = l->tm_mday;

	int kdate = year * 10000 + moth * 100 + day;
	int cdate = curyear * 10000 + curmonth * 100 + curday;

		//printf("offline check---Key date=%d  Now date=%d\n", kdate, cdate);
	if (cdate > kdate)
		return -4;

	{
		char tmpbuf[128] = { 0 };
		sprintf(tmpbuf, "%d-%d-%d", year, moth, day);
		expLicDate_ = tmpbuf;
	}

	if(0){
		TSJson::Value root;
		root["cmd"] = "license_notify";
		root["type"] = "offline";
		root["stat"] = 0;
		char tmpbuf[128] = {0};
		sprintf(tmpbuf,"%d-%d-%d", year, moth, day);
		root["exp_date"] = tmpbuf;
		TSJson::FastWriter writer;
		string tmpstr = writer.write(root);
		Callback(tmpstr.c_str());
	}

	isSDKAuth_ = 0;
	return 0;

}

int TaskManager::checkAuth(string apikey)
{
	int ret;

	if (apikey.empty()) {
	 ret = checkOfflienLic();
		//DebugLog::writeLogF("offline check ret == %d\n", ret);
	}
	else {
		ret= checkOnlineLic(apikey);
	}
	if (ret != 0 && 0) {
		{
			TSJson::Value root;
			root["cmd"] = "license_notify";
			root["type"] = "offline";
			root["stat"] = 1;
			TSJson::FastWriter writer;
			string tmpstr = writer.write(root);
			Callback(tmpstr.c_str());
		}
	}

	return ret;



}

bool getethIF(char* ethname)
{
	char buf[255] = { 0 };
	char *datastart;
	int bufsize;
	FILE *devfd;
	bool flag = false;

	bufsize = 255;
	devfd = fopen("/proc/net/dev", "r");

	fgets(buf, bufsize, devfd);
	DebugLog::writeLogF("---l1  %s\n", buf);
	fgets(buf, bufsize, devfd);
	DebugLog::writeLogF("---l2  %s\n", buf);

	while (fgets(buf, bufsize, devfd))
	{
		printf("---ln  %s\n", buf);

		if ((datastart = strstr(buf, "lo:")) == NULL && (datastart = strstr(buf, "eth0")) == NULL)
		{
		//	puts("------------>>>>>>>>>>>>>>>>>>>>");
			datastart = strstr(buf, ":");
			strncpy(ethname, buf, (datastart - &buf[0]));
			flag = true;
			break;
		}
	}

	fclose(devfd);

	puts(ethname);

	return flag;

}



int TaskManager::getDevMac() {
	//devMac_ = "11:22:33:44:55:66";

#ifndef WIN32

	int sock, if_count, i;
	struct ifconf ifc;
	struct ifreq ifr[10];
	unsigned char mac[6];
	char macstr[64] = { 0 };


	memset(&ifc, 0, sizeof(struct ifconf));
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	ifc.ifc_len = 10 * sizeof(struct ifreq);
	ifc.ifc_buf = (char *)ifr;

	ioctl(sock, SIOCGIFCONF, (char *)&ifc);
	if_count = ifc.ifc_len / (sizeof(struct ifreq));
	for (i = 0; i < if_count; i++) {
		if (ioctl(sock, SIOCGIFHWADDR, &ifr[i]) == 0) {
			if (strcmp(ifr[i].ifr_name, "lo") == 0) continue;
			memcpy(mac, ifr[i].ifr_hwaddr.sa_data, 6);
			if (mac[0] == 0 && mac[1] == 0 && mac[2] == 0 && mac[3] == 0 && mac[4] == 0 && mac[5] == 0) continue;
			DebugLog::writeLogF("eth: %s, mac: %02x:%02x:%02x:%02x:%02x:%02x\n", ifr[i].ifr_name, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
			sprintf(macstr, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
			devMac_ = macstr;
			break;
		}
	}
	close(sock);
#endif
	return 0;

}

int TaskManager::onAnyWorkStatus(TSJson::Value& message, int type)
{
	ts_autoLock lock(mq_lock);

	if (Callback != NULL) {
		TSJson::FastWriter writer;
		string tmpstr = writer.write(message);
		Callback(tmpstr.c_str());
	}
	if (pTaskStatus->size() > 10)
		return 0;

	if (type == 1 || type == 0) {
		TaskMessage* tmmsg = new TaskMessage(type, message);

		pTaskStatus->push(tmmsg);
	}
	else {
		TaskMessage* tmmsg = new TaskMessage(type, message);

		pTaskStatus->push(tmmsg);
	}
	return 0;
}

string TaskManager::pullStatus() {
	TaskMessage* tMsg = pTaskStatus->pop();
	string result = tMsg->message_;
	//delete tMsg;
	return result;
};
