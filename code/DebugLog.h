
#ifndef SDK_DEBUGLOG_H_
#define SDK_DEBUGLOG_H_
#include "../common/ts_thread.h"

#define LOG_MAX_MSG_LEN	2048

/*
 * a tool class for debug,not use in release
 * will write a tmp logfile in ./sdklog
 * */
class DebugLog
{
private:
	DebugLog();
	virtual ~DebugLog();
public:
	static void writeLog(char* log);
	static void writeLogF(const char *fmt, ...);
private:
	static void writefile(char* log);
private:
	static ts_mutex lock_log;

};

#endif /* SDK_DEBUGLOG_H_ */
