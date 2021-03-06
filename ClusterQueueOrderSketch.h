/*
 * QCluster
 * avoid disorder by setting recording the packet number of each flow in the queue
 */
 

#ifndef _NS_QCLUSTER_QUEUE_ORDER_SKETCH_H_
#define _NS_QCLUSTER_QUEUE_ORDER_SKETCH_H_

#define MAX_QUEUE_NUM 8

#define DISABLE_ECN 0
#define PER_QUEUE_ECN 1
#define PER_PORT_ECN 2

#include <string.h>
#include "queue.h"
#include "config.h"
#include "TimeSketchOrder.h"

using namespace std;

struct Info{
	double counting;
	double distinct;

	double average(){
		if(distinct < 1)
			return 0;
		return counting / distinct;
	}
};

class ClusterQueueOrderSketch : public Queue{
public:
	ClusterQueueOrderSketch();
	~ClusterQueueOrderSketch();
protected:
	void enque(Packet*);	// enqueue function
	Packet* deque();		// dequeue function

	PacketQueue **q_;		// underlyinig multi-FIFO (CoS) queues
	int mean_pktsize_;		// configured mean packet size in bytes
	int thresh_;			// single ECN marking threshold
	int queue_num_;			// number of CoS queues.
	int marking_scheme_;	// Disable ECN (0), Per-queue ECN (1) and Per-port ECN (2)

	double interval_msg_;	// threshold to split message
	// TimeSketch for recording:
	//     count_packet, count_byte, last_time, last_en_qid
	TimeSketchOrder *tso;	

	double last_update;
	Info info[MAX_QUEUE_NUM];

	void Update_Queue(int q);
	void Update();
	// Return total queue length (bytes) of all the queues
	int TotalByteLength();	
};


ClusterQueueOrderSketch::ClusterQueueOrderSketch()
{
	queue_num_ = MAX_QUEUE_NUM;
	thresh_ = 65;
	mean_pktsize_ = 1500;
	marking_scheme_ = PER_PORT_ECN;
	last_update = 0;

	// Bind variables
	bind("queue_num_", &queue_num_);
	bind("thresh_", &thresh_);
	bind("mean_pktsize_", &mean_pktsize_);
	bind("marking_scheme_", &marking_scheme_);
	bind("interval_msg_", &interval_msg_);
	
	tso = new TimeSketchOrder((uint)queue_num_, interval_msg_);

	// Init queues
	q_ = new PacketQueue*[MAX_QUEUE_NUM];
	for(int i = 0; i < MAX_QUEUE_NUM; ++i)
	{
		q_[i] = new PacketQueue;
		info[i].counting = 0;
		info[i].distinct = 0;
	}
}

ClusterQueueOrderSketch::~ClusterQueueOrderSketch()
{
	for(int i = 0; i < MAX_QUEUE_NUM; ++i)
		delete q_[i];
	delete[] q_;
	delete tso;
}

void ClusterQueueOrderSketch::Update_Queue(int q)
{
	info[q].counting /= 2.0;
	info[q].distinct /= 2.0;
	return;
}

void ClusterQueueOrderSketch::Update()
{
	for(int i = 0; i < queue_num_; ++i){
		if(info[i].distinct > 128)
			Update_Queue(i);
		fprintf(stderr, "%d %lf %lf %lf\n",
			i, info[i].average(), info[i].counting, info[i].distinct);
	}
	fprintf(stderr, "\n");
}

//Return total queue length (bytes) of all the queues
int ClusterQueueOrderSketch::TotalByteLength()
{
	int bytelength = 0;
	for(int i = 0; i < MAX_QUEUE_NUM; ++i)
		bytelength += q_[i]->byteLength();
	return bytelength;
}



#endif
