#ifndef _CM_SKETCH_FOR_KNOWN_H_
#define _CM_SKETCH_FOR_KNOWN_H_

#include "bobhash32.h"
using namespace std;

class CMSketch_known{
	const uint HASH_NUM;
	const uint LENGTH;

	uint* counter;
	uint* last_en_qid; 

public:
	CMSketch_known(uint _HASH_NUM = 4, uint _LENGTH 6401)
		:HASH_NUM(_HASH_NUM), LENGTH(_LENGTH)
	{
		counter = new uint[LENGTH];
		last_en_qid = new uint[LENGTH];
		memset(counter, 0, sizeof(uint)*LENGTH);
		memset(last_en_qid, 0, sizeof(uint)*LENGTH);
	}

	~CMSketch_known(){
		delete[] counter;
		delete[] last_en_qid;
	}

public:
	std::pair<int, int> lookup(uint flow_id)
	{
		uint pkt_count = 0x7fffffff;
		uint qid = 0;
		for(uint i = 0; i < HASH_NUM; ++i)
		{
			uint pos = BOBHash32((uchar*)(&flow_id), sizeof(uint), i) % LENGTH;
			pkt_count = MIN(pkt_count, counter[pos]);
			qid = MAX(qid, last_en_qid[pos]);
		}
		return std::make_pair(pkt_count, qid);
	}

	void reset_qid(uint flow_id)
	{
		for(uint i = 0; i < HASH_NUM; ++i)
		{
			uint pos = BOBHash32((uchar*)(&flow_id), sizeof(uint), i) % LENGTH;
			last_en_qid[pos] = 0;
		}
	}

	void update(uint flow_id, int qid)
	{
		for(uint i = 0; i < HASH_NUM; ++i)
		{
			uint pos = BOBHash32((uchar*)(&flow_id), sizeof(uint), i) % LENGTH;
			counter[pos] += 1;
			last_en_qid[pos] = MAX(last_en_qid[pos], qid);
		}
	}

	void deque_pkt(uint flow_id)
	{
		for(uint i = 0; i < HASH_NUM; ++i)
		{
			uint pos = BOBHash32((uchar*)(&flow_id), sizeof(uint), i) % LENGTH;
			counter[pos] = MIN(counter[pos] - 1, 0);
		}
	}
};

#endif