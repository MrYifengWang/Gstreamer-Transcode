/*
 * StatusQueue.h
 *
 *  Created on: Feb 5, 2018
 *      Author: wangyifeng
 */

#ifndef SDK_SIMPLERTSP_STATUSQUEUE_H_
#define SDK_SIMPLERTSP_STATUSQUEUE_H_

#include "../common/ts_thread.h"
#include <queue>

/*
 * this is a magic code
 * for the same channel, real play must start one by one,so we need complete stop before start
 * when rtsp thread end, it put one notify to this queue,
 * caller thread wait on the queue for message, with a timeout seconds.
 */
#include <string>
#include "../common/ts_json.h"
using namespace std;
class TaskMessage
{
public:
	TaskMessage();
	TaskMessage(int type,TSJson::Value& msg);
	virtual ~TaskMessage();

public:
	int type_;
	int timestamp_;
	string tid_;
	string command_;
	string message_;
};
class StatusQueue
{
public:
	StatusQueue();
	virtual ~StatusQueue();
public:
	void push(TaskMessage* msg);
	void cache(TaskMessage* msg);
	void flush();
	TaskMessage* pop();
	TaskMessage* trypop(int seconds);
	int size();

private:
	mutable ts_mutex lock_queue;
	ts_semaphore sem_queue;
	std::queue<TaskMessage*> message_queue;
};

#endif /* SDK_SIMPLERTSP_STATUSQUEUE_H_ */
