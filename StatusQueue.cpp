/*
 * StatusQueue.cpp
 *
 *  Created on: Feb 5, 2018
 *      Author: wangyifeng
 */

#include "StatusQueue.h"
#include <stdio.h>
#include <string.h>

TaskMessage::TaskMessage()
{
	// TODO Auto-generated constructor stub

}
TaskMessage::TaskMessage(int type, TSJson::Value& msg)
{
	/*type_ = type;
	tid_ = tid;
	command_ = command;*/
	timestamp_ = time(NULL);
	TSJson::FastWriter writer;
	message_ = writer.write(msg);
//	puts(message_.c_str());
}

TaskMessage::~TaskMessage()
{
	// TODO Auto-generated destructor stub
}
StatusQueue::StatusQueue()
{
	// TODO Auto-generated constructor stub

}

StatusQueue::~StatusQueue()
{
	// TODO Auto-generated destructor stub
}

void StatusQueue::push(TaskMessage* transmit)
{

	lock_queue.lock();

	message_queue.push(transmit);

	lock_queue.unlock();

	sem_queue.release();

}
void StatusQueue::cache(TaskMessage* transmit)
{
	lock_queue.lock();

	message_queue.push(transmit);

	lock_queue.unlock();
}

void StatusQueue::flush() {

	int cursize = message_queue.size();
	for (int i = 0; i < cursize; i++) {
		sem_queue.release();
	}

}


TaskMessage* StatusQueue::pop()
{
	sem_queue.wait();

	lock_queue.lock();

	TaskMessage* transmit = message_queue.front();
	message_queue.pop();

	//printf("-------status queue size = %d\n", message_queue.size());

	lock_queue.unlock();

	return transmit;
}
TaskMessage* StatusQueue::trypop(int seconds)
{
	sem_queue.wait_sec(seconds);

	lock_queue.lock();

	TaskMessage* transmit = NULL;
	if (!message_queue.empty())
	{
		transmit = message_queue.front();
		message_queue.pop();
	}

	lock_queue.unlock();

	return transmit;
}
int StatusQueue::size()
{
	return message_queue.size();
}