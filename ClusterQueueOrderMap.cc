/*
 * QCluster
 * avoid disorder by setting recording the packet number of each flow in the queue
 */

#include "ClusterQueueOrderMap.h"
#include "flags.h"
#include "math.h"

#define MAX_(arg1, arg2) ((arg1) > (arg2) ? (arg1) : (arg2))
#define MIN_(arg1, arg2) ((arg1) < (arg2) ? (arg1) : (arg2))

static class ClusterQueueOrderMapClass : public TclClass{
public:
	ClusterQueueOrderMapClass():TclClass("Queue/ClusterQueueOrderMap"){}
	TclObject* create(int, const char*const*){
		return (new ClusterQueueOrderMap);
	}
} class_clusterqueueordermap;


void ClusterQueueOrderMap::enque(Packet* p)
{
	hdr_ip *iph = hdr_ip::access(p);
	int flow_id = iph->flowid();
	int size = hdr_cmn::access(p)->size() - 40;
	packet_t type = hdr_cmn::access(p)->ptype();
	hdr_flags* hf = hdr_flags::access(p);
	int qlimBytes = qlim_ * mean_pktsize_;
	double now = iph->connection();

	// 1 <= queue_num <= MAX_QUEUE_NUM
	queue_num_ = MAX_(MIN_(queue_num_, MAX_QUEUE_NUM), 1);

	// queue length exceeds the queue limit
	if(TotalByteLength() + size + 40 > qlimBytes){
		drop(p);
		return;
	}

	int queue_chose = 0;
	double value = size;
	int queue_recommend = queue_num_ - 1;

	if(type == PT_TCP && size > 0)
	{
		if(count_packet.find(flow_id) == count_packet.end()
			|| now != last_time[flow_id])
		{
			count_packet[flow_id] = 0;
			count_byte[flow_id] = 0;
			last_en_qid[flow_id] = 0;
			last_time[flow_id] = now;

			queue_recommend = 0; 
		}

		if(size >= 1460)
		{
			count_byte[flow_id] += size;
			value = count_byte[flow_id];

			if(count_packet[flow_id] == 0)
				queue_recommend = MAX_(last_en_qid[flow_id] - 1, 0);
			else
				queue_recommend = last_en_qid[flow_id];
		}

		double thresholds[queue_num_ - 1] = {0};
		for(int i = 0; i < queue_num_ - 1; ++i)
		{
			if(info[i].distinct < 1){
				if(i == 0) thresholds[i] = (1 << 25);
				else thresholds[i] = (1 << 25) + thresholds[i - 1];
				continue;
			}
			thresholds[i] = MAX_(info[i].average(),
				sqrt(info[i].average() * info[i + 1].average()));

			if(i > 0 && thresholds[i] <= thresholds[i - 1])
				thresholds[i] = thresholds[i - 1] + 1460;
		}

		for(queue_chose = 0; queue_chose < queue_num_ - 1; ++queue_chose)
			if(value <= thresholds[queue_chose] + 1)
				break;
		queue_chose = MAX_(queue_chose, queue_recommend);
	}

	// enqueue packet
	q_[queue_chose]->enque(p);

	if(type == PT_TCP && size > 0)
	{
		last_en_qid[flow_id] = queue_chose;
		last_time[flow_id] = now;
		count_packet[flow_id] += 1;

		info[queue_chose].distinct += 1;
		info[queue_chose].counting += value;
		if(Scheduler::instance().clock() - last_update > 1)
		{
			last_update = Scheduler::instance().clock();
			Update();
		}
	}

	// enqueue ECN marking: per-queue or per-port
	if((marking_scheme_ == PER_QUEUE_ECN && 
		q_[queue_chose]->byteLength() > thresh_ * mean_pktsize_) ||
		(marking_scheme_ == PER_PORT_ECN &&
		TotalByteLength() > thresh_ * mean_pktsize_))
	{
		if(hf->ect())		// if this packet is ECN-capable
			hf->ce() = 1;
	}
}

Packet* ClusterQueueOrderMap::deque()
{
	if(TotalByteLength() > 0)
	{
		// high->low: 0-7
		for(int i = 0; i < queue_num_; ++i)
			if(q_[i]->length() > 0){
				Packet *p = q_[i]->deque();

				/* when deque a packet, decrement its packet count */
				hdr_ip *iph = hdr_ip::access(p);
				int flow_id = iph->flowid();
				packet_t type = hdr_cmn::access(p)->ptype();
				int size = hdr_cmn::access(p)->size() - 40;
				if(type == PT_TCP && size > 0)
					count_packet[flow_id] = MAX_(count_packet[flow_id] - 1, 0);
				/***************************************************/

				return (p);
			}
	}
	return NULL;
}
