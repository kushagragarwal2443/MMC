/*
  This file is a part of KMC software distributed under GNU GPL 3 licence.
  The homepage of the KMC project is http://sun.aei.polsl.pl/kmc
  
  Authors: Sebastian Deorowicz, Agnieszka Debudaj-Grabysz, Marek Kokot
  
  Version: 3.1.1
  Date   : 2019-05-19
*/

#ifndef _SPLITTER_H
#define _SPLITTER_H

#include "defs.h"
#include "kmer.h"
#include "kb_collector.h"
#include "queues.h"
#include "s_mapper.h"
#include "../kmc_api/mmer.h"
#include <stdio.h>
#include <vector>
#include <iostream>
#include "small_k_buf.h"
#include "bam_utils.h"

using namespace std;

//************************************************************************************************************
// CSplitter class - splits kmers into bins according to their signatures
//************************************************************************************************************
class CSplitter
{
	uint64 total_kmers = 0;	
	uchar *part;
	uint64_t part_size, part_pos;
	std::vector<std::unique_ptr<CKmerBinCollector>> bins;
	CBinPartQueue *bin_part_queue;
	CMemoryPool *pmm_reads;
	int64 mem_part_pmm_bins;
	int64 mem_part_pmm_reads;

	char codes[256];
	InputType file_type;
	bool both_strands;

	uint32_t curr_read_len = 0;

	uint32 kmer_len;
	uint32 minimizerVersion;
	uint32 delta_val;
	uint32 window_len; // Souvadra's addition
	//uint32 prefix_len;
	uint32 signature_len;
	uint32 n_bins;	
	uint64 n_reads;//for multifasta its a sequences counter	

	CSignatureMapper* s_mapper;

	bool homopolymer_compressed;

	CntHashEstimator* ntHashEstimator;

	bool GetSeqLongRead(char *seq, uint32 &seq_size, uchar header_marker);

	bool GetSeq(char *seq, uint32 &seq_size, ReadType read_type);

	void HomopolymerCompressSeq(char* seq, uint32 &seq_size);

public:
	static uint32 MAX_LINE_SIZE;
	
	CSplitter(CKMCParams &Params, CKMCQueues &Queues); 
	void InitBins(CKMCParams &Params, CKMCQueues &Queues);	
	void CalcStats(uchar* _part, uint64 _part_size, ReadType read_type, uint32* _stats);
	bool ProcessReadsOnlyEstimate(uchar* _part, uint64 _part_size, ReadType read_type);
	bool ProcessReads(uchar *_part, uint64 _part_size, ReadType read_type);
	template<typename COUNTER_TYPE> bool ProcessReadsSmallK(uchar *_part, uint64 _part_size, ReadType read_type, CSmallKBuf<COUNTER_TYPE>& small_k_buf);
	void Complete();
	inline void GetTotal(uint64 &_n_reads);
	inline uint64 GetTotalKmers();
	inline uint64_t hash64(uint64_t key, uint64_t mask); // Kush added this


};

//----------------------------------------------------------------------------------
// Added this function which returns a hash for a key

uint64_t CSplitter::hash64(uint64_t key, uint64_t mask) {
	key = (~key + (key << 21)) & mask; // key = (key << 21) - key - 1;
	key = key ^ key >> 24;
	key = ((key + (key << 3)) + (key << 8)) & mask; // key * 265
	key = key ^ key >> 14;
	key = ((key + (key << 2)) + (key << 4)) & mask; // key * 21
	key = key ^ key >> 28;
	key = (key + (key << 31)) & mask;
	return key;
}

//----------------------------------------------------------------------------------
// Return the number of reads processed by splitter
void CSplitter::GetTotal(uint64 &_n_reads)
{
	_n_reads = n_reads;
}

//----------------------------------------------------------------------------------
// Return the number of kmers processed by splitter (!!! only for small k optimization)
uint64 CSplitter::GetTotalKmers()
{
	return total_kmers;
}

//************************************************************************************************************
// CWSplitter class - wrapper for multithreading purposes
//************************************************************************************************************

//----------------------------------------------------------------------------------
class CWSplitter {
	CPartQueue *pq;
	CBinPartQueue *bpq;
	CMemoryPool *pmm_fastq;

	std::unique_ptr<CSplitter> spl;
	uint64 n_reads;

public:
	CWSplitter(CKMCParams &Params, CKMCQueues &Queues);	
	void operator()();
	void GetTotal(uint64 &_n_reads);
	~CWSplitter();
};

//************************************************************************************************************
// CWStatsSplitter class - wrapper for multithreading purposes
//************************************************************************************************************

//----------------------------------------------------------------------------------
class CWStatsSplitter {
	CStatsPartQueue *spq;
	CMemoryPool *pmm_fastq, *pmm_stats;
	uint32 *stats;
	std::unique_ptr<CSplitter> spl;
	uint32 signature_len;
	KMC::IProgressObserver* progressObserver;
public:
	CWStatsSplitter(CKMCParams &Params, CKMCQueues &Queues);
	~CWStatsSplitter();

	void operator()();
	void GetStats(uint32* _stats);
};


//************************************************************************************************************
// CWSmallKSplitter class - wrapper for multithreading purposes
//************************************************************************************************************
//----------------------------------------------------------------------------------
template <typename COUNTER_TYPE> class CWSmallKSplitter {
	CPartQueue *pq;	
	CMemoryPool *pmm_fastq, *pmm_small_k;	
	CSmallKBuf<COUNTER_TYPE> small_k_buf;

	std::unique_ptr<CSplitter> spl;
	uint64 n_reads;
	uint64 total_kmers;
	uint32 kmer_len;
	uint32 minimizerVersion;
	uint32 delta_val;
	uint32 window_len; 
public:
	CWSmallKSplitter(CKMCParams &Params, CKMCQueues &Queues);
	~CWSmallKSplitter();

	void operator()();
	void GetTotal(uint64 &_n_reads);

	CSmallKBuf<COUNTER_TYPE> GetResult()
	{
		return small_k_buf;
	}

	uint64 GetTotalKmers()
	{
		if (spl)
			return spl->GetTotalKmers();
		return total_kmers;
	}

	void Release()
	{
		pmm_small_k->free(small_k_buf.buf);
	}
};



//************************************************************************************************************
// CWEstimateOnlySplitter class - wrapper for multithreading purposes
//************************************************************************************************************

//----------------------------------------------------------------------------------
class CWEstimateOnlySplitter {
	CPartQueue* pq;
	CBinPartQueue* bpq;
	CMemoryPool* pmm_fastq;

	std::unique_ptr<CSplitter> spl;
	uint64 n_reads;

public:
	CWEstimateOnlySplitter(CKMCParams& Params, CKMCQueues& Queues);
	void operator()();
	void GetTotal(uint64& _n_reads);
	~CWEstimateOnlySplitter();
};



#endif

// ***** EOF
