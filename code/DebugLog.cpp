
#include "DebugLog.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

ts_mutex DebugLog::lock_log;

DebugLog::DebugLog()
{
	// TODO Auto-generated constructor stub
}

DebugLog::~DebugLog()
{
	// TODO Auto-generated destructor stub
}
void DebugLog::writefile(char* log)
{
	char pStrTime[64] = { 0 };
	time_t t;
	time(&t);
	tm* local = localtime(&t);
	strftime(pStrTime, 64, "\n[%Y-%m-%d %H:%M:%S] - ", local);
	FILE* file = fopen("./sdklog", "a");
	int flen = ftell(file);

	if (file)
	{
		fwrite(pStrTime, strlen(pStrTime), 1, file);
		fwrite(log, strlen(log), 1, file);
		fclose(file);
	}
	if (flen > 1024 * 1024*10)
	{
		char buf[128] = "rm sdklog";
		system(buf);
	}
}

void DebugLog::writeLog(char* log)
{

	lock_log.lock();

	writefile(log);

	lock_log.unlock();

}
void DebugLog::writeLogF(const char *fmt, ...)
{
	lock_log.lock();

	va_list ap;
	char msg[LOG_MAX_MSG_LEN];

	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	puts(msg);
//	writefile(msg);
	va_end(ap);

	lock_log.unlock();

}

