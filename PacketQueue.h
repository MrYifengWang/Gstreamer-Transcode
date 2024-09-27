/*
 * PacketQueue.h
 *
 *  Created on: Aug 27, 2017
 *      Author: wangyifeng
 */

#ifndef SDK_SIMPLERTSP_PACKETQUEUE_H_
#define SDK_SIMPLERTSP_PACKETQUEUE_H_

#include "../common/ts_thread.h"
#include <queue>
//typedef unsigned long long  uint64;
struct FrameData {
	char * data = NULL;
	int size;
	int width;
	int height;
	int isRun = 0;// STATUS_INIT;
	int mtype;
};

class Packet
{
public:
	Packet();
	Packet(void* pdata, int len);
	virtual ~Packet();
public:
	char* pdata_;
	int type_;
	int len_;
	int size=0;
	int width;
	int height;
	int seq_=0;
	unsigned long long pts_;
};
class PacketQueue
{
public:
	PacketQueue();
	virtual ~PacketQueue();
public:
	void push(void* pdata,int len);
	void push(Packet* msg);
	void cache(Packet* msg);
	void flush();
	Packet* pop();
	Packet* trypop(int seconds);
	void clear();

	int size();

private:
	mutable ts_mutex lock_queue;
	ts_semaphore sem_queue;
	std::queue<Packet*> message_queue;
};

#endif /* SDK_SIMPLERTSP_PACKETQUEUE_H_ */
