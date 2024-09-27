/*
 * PacketQueue.cpp
 *
 *  Created on: Aug 27, 2017
 *      Author: wangyifeng
 */

#include "PacketQueue.h"
#include <stdio.h>
#include <string.h>

Packet::Packet()
{
	len_ = 0;
}
Packet::Packet(void* pdata, int len)
{
	// TODO Auto-generated constructor stub
	len_ = len;
	if (len_ > 0)
	{
		pdata_ = new char[len];
		memcpy(pdata_, pdata, len);
	}

}

Packet::~Packet()
{
	// TODO Auto-generated destructor stub
	if (len_ > 0)
	{
		delete[] pdata_;
	}
}
PacketQueue::PacketQueue()
{
	// TODO Auto-generated constructor stub
	//puts("new PacketQueue");
}

PacketQueue::~PacketQueue()
{
	// TODO Auto-generated destructor stub
	//puts("~PacketQueue");
}

void PacketQueue::push(void* pdata, int len)
{

	Packet* item = new Packet(pdata, len);
	push(item);

}
void PacketQueue::push(Packet* transmit)
{

	lock_queue.lock();

	message_queue.push(transmit);

	lock_queue.unlock();

	sem_queue.release();

}
void PacketQueue::cache(Packet* transmit)
{
	lock_queue.lock();

	message_queue.push(transmit);

	lock_queue.unlock();
}

void PacketQueue::flush() {

	int cursize = message_queue.size();
	for (int i = 0; i < cursize; i++) {
		sem_queue.release();
	}

}

Packet* PacketQueue::pop()
{
	sem_queue.wait();

	lock_queue.lock();

	Packet* transmit = message_queue.front();
	message_queue.pop();

	//printf("-------data queue size = %d\n", message_queue.size());

	lock_queue.unlock();

	return transmit;
}
Packet* PacketQueue::trypop(int seconds)
{
	sem_queue.wait_sec(seconds);

	lock_queue.lock();

	Packet* transmit = NULL;
	if (!message_queue.empty())
	{
		transmit = message_queue.front();
		message_queue.pop();
	}

	lock_queue.unlock();

	return transmit;
}

int PacketQueue::size()
{
	return message_queue.size();
}
void PacketQueue::clear()
{
	lock_queue.lock();
	while (message_queue.size()>2)
	{
		sem_queue.wait();
		Packet*  transmit = message_queue.front();
		message_queue.pop();
		if(transmit!=NULL)
		delete transmit;
	}
	lock_queue.unlock();
}