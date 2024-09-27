#ifndef GST__WRAPPER_H_
#define GST__WRAPPER_H_

#include "define.h"
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
  char* TS_getversion();

	/*
	初始化：
	apikey：从供应商处获取的Lience 字符串
	chiptype：编码器选择 1 software 2 rockchip hardware 3 nivida handware
	pcallback：工作状态回调函数，系统和任务执行中的状态变化，以json 字符串的格式通知调用者，如果Null，则不通知
	返回值：Key无效/成功
	*/
	err_sdk_code TS_Init(char* apikey, int chiptype, ts_task_status_notify pcallback); 
	/*
	释放SDK：将会停止所有通道的转码工作，并释放资源。在客户结束进程是需要调用
	*/
	void TS_Release();
	/*
	添加任务：
	task_config：任务参数，json字符串格式。具体选项参考define.h的例子
	返回值：
	*/
	char* TS_createTask(char* task_config);
	/*
	删除任务：
	关闭一个通道的转码任务。
	tid：任务id，对应task_config中的taskID
	返回值：成功
	
	*/
	bool TS_deleteTask(char* tid);
	/*
	查询指定通道的任务状态：
	tid：任务id，对应task_config中的taskID
	返回值：json 字符串
	*/
	char* TS_queryStatus(char* tid);

	/*
	* 查询许可证日期
	*/
	char* TS_queryLicense();



#ifdef __cplusplus
}
#endif
#endif
