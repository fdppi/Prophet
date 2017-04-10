/*BEGIN_LEGAL
Intel Open Source License

Copyright (c) 2002-2013 Intel Corporation. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.  Redistributions
in binary form must reproduce the above copyright notice, this list of
conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.  Neither the name of
the Intel Corporation nor the names of its contributors may be used to
endorse or promote products derived from this software without
specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
END_LEGAL */
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cassert>
#include <map>
#include <vector>
#include <numeric>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <scif.h>
#include <time.h>
#include <set>
#include <utility>
#include "pin.H"
#include "calculation/calculation.h"
#include "calculation/yags.h"
#include "calculation/trans_cache.h"

//easton added
//#define EA_TIMING

//easton added
#include <sys/time.h>
struct timeval starttime, endtime;
pthread_mutex_t ea_mutex;
UINT32 numThreads = 0;

using namespace std;
//easton added
vector<long long> easton_ins_num;
int count_nop = 0;
bool am_i_in_magicbreak = false;
extern long ins_count_between_magic;
int queue_size = 100000;
uint16_t *nodes;

typedef struct _bb_map_t{
    ADDRINT last_tb_addr;
    vector< instr_info_buffer_t >* entry;
    struct _bb_map_t* next;
} bb_map_t;


extern "C" {
	int trifs_trace_ptlsim_decoder(W8 *x86_buf, int num_bytes,
								   TransOp *upos_buf, int *num_uops, W64 rip, bool is64bit);
}

#define NUM_BUF_PAGES 8192
#define PAGE_SIZE 4096

struct mem_ref { 
	ADDRINT ins_addr; //instruction address
	ADDRINT mem_addr; //memory address
	UINT32 access_size;
	int is_full;
};


BUFFER_ID bufId;

ofstream OutFile;
vector< map< ADDRINT, vector< x86_inst_t > * > > bb_cache_map;
vector< map< ADDRINT, vector< instr_info_buffer_t > > > bb_cache_map2;
vector< map< ADDRINT, int > > bb_mem_map;

// The running count of instructions is kept here
// make it static to help the compiler optimize docount
//static int num_cached[NUM_PROCS];
//static int num_not_cached[NUM_PROCS];
static int num_bb = 0;
static int num_fetch_inst = 0;
static int num_last_instr = 0;
static int num_transfer_instr = 0;
static UINT64 icount = 0;
static int num_reads = 0;
static int num_writes = 0;
static int num_buffer_full = 0;
static int num_missed_mem = 0;
static ADDRINT test_addr = 0;
//static int num_mem_inst = 0;
static int num_tb_mem_diff = 0;
static const int max_buffer_entry = NUM_BUF_PAGES * PAGE_SIZE / sizeof(struct mem_ref);

PIN_LOCK lock;
TLS_KEY buf_key;

//int first_tb_flag[NUM_PROCS];
//int last_tb_mem_num[NUM_PROCS];
//x86_tb_instr_buffer_t last_tb[NUM_PROCS];
//vector< x86_inst_t >* last_tb_inst[NUM_PROCS];

/* shared buffer  */
/*
x86_tb_instr_buffer_t ***trifs_trace_shared_tb_inst_buf;
x86_tb_instr_buffer_t *trifs_trace_shared_tb_inst_end[NUM_PROCS][MAX_TRIFS_TRACE_BUF_BLOCK_SIZE];
x86_tb_instr_buffer_t *trifs_trace_shared_tb_inst_ptr[NUM_PROCS];

x86_instr_info_buffer_t ***trifs_trace_shared_buf;
x86_instr_info_buffer_t *trifs_trace_shared_end[NUM_PROCS][MAX_TRIFS_TRACE_BUF_BLOCK_SIZE];
x86_instr_info_buffer_t *trifs_trace_shared_ptr[NUM_PROCS];

x86_mem_info_buffer_t ***trifs_mem_shared_buf;
x86_mem_info_buffer_t *trifs_mem_shared_end[NUM_PROCS][MAX_TRIFS_TRACE_BUF_BLOCK_SIZE];
x86_mem_info_buffer_t *trifs_mem_shared_ptr[NUM_PROCS];

instr_info_buffer_flag_t **minqh_flag_buf;
int *minqh_buf_block_index;

x86_tb_instr_buffer_t **trifs_trace_shared_tb_inst_buf_head(int core_num)
{
    cout <<"QEMU : tb_buf_head : " << trifs_trace_shared_tb_inst_buf[core_num][0] << endl;
    return trifs_trace_shared_tb_inst_buf[core_num];
}

x86_instr_info_buffer_t **trifs_trace_shared_buf_head(int core_num)
{
    cout << "QEMU : buf_head : " << trifs_trace_shared_buf[core_num][0] << endl;
    return trifs_trace_shared_buf[core_num];
}

x86_mem_info_buffer_t **trifs_mem_shared_buf_head(int core_num)
{
    cout << "QEMU : mem_buf_head : " << trifs_mem_shared_buf[core_num][0] << endl;
    return trifs_mem_shared_buf[core_num];
}

instr_info_buffer_flag_t *get_minqh_flag_buf_head(int core_num){
    return minqh_flag_buf[core_num];
}

void trifs_trace_shared_buf_init(void)
{
    int i, j;

	//tb
	trifs_trace_shared_tb_inst_buf = (x86_tb_instr_buffer_t ***)malloc(NUM_PROCS * sizeof(x86_tb_instr_buffer_t **));
    memset(trifs_trace_shared_tb_inst_buf, 0, NUM_PROCS * sizeof(x86_tb_instr_buffer_t **));
	cout << "x86_tb_inst_buffer_t size : " << sizeof(x86_tb_instr_buffer_t) << endl;

    for (i = 0; i < NUM_PROCS; i++) {
        trifs_trace_shared_tb_inst_buf[i] = ( x86_tb_instr_buffer_t **)malloc(MAX_TRIFS_TRACE_BUF_BLOCK_SIZE * sizeof( x86_tb_instr_buffer_t *));
        memset(trifs_trace_shared_tb_inst_buf[i], 0, MAX_TRIFS_TRACE_BUF_BLOCK_SIZE * sizeof(x86_tb_instr_buffer_t *));

		for(j = 0; j < MAX_TRIFS_TRACE_BUF_BLOCK_SIZE; j++){
			trifs_trace_shared_tb_inst_buf[i][j] = ( x86_tb_instr_buffer_t *)malloc(MAX_TRIFS_TRACE_BUF_SIZE * sizeof( x86_tb_instr_buffer_t));
            memset((void*)(trifs_trace_shared_tb_inst_buf[i][j]), 0, MAX_TRIFS_TRACE_BUF_SIZE * sizeof(x86_tb_instr_buffer_t));
			trifs_trace_shared_tb_inst_end[i][j] = (x86_tb_instr_buffer_t*)(trifs_trace_shared_tb_inst_buf[i][j]) + MAX_TRIFS_TRACE_BUF_SIZE;
		}

		trifs_trace_shared_tb_inst_ptr[i] = (x86_tb_instr_buffer_t*)(trifs_trace_shared_tb_inst_buf[i][0]);

    }

	//instruction
    trifs_trace_shared_buf = ( x86_instr_info_buffer_t ***)malloc(NUM_PROCS * sizeof( x86_instr_info_buffer_t **));
    memset(trifs_trace_shared_buf, 0, NUM_PROCS * sizeof(x86_instr_info_buffer_t **));
	cout<< "x86_instr_info_buffer_t size : " << sizeof(x86_instr_info_buffer_t) << endl;

    for (i = 0; i < NUM_PROCS; i++) {
        trifs_trace_shared_buf[i] = ( x86_instr_info_buffer_t **)malloc(MAX_TRIFS_TRACE_BUF_BLOCK_SIZE * sizeof( x86_instr_info_buffer_t *));
        memset(trifs_trace_shared_buf[i], 0, MAX_TRIFS_TRACE_BUF_BLOCK_SIZE * sizeof(x86_instr_info_buffer_t *));

	    for(j = 0; j < MAX_TRIFS_TRACE_BUF_BLOCK_SIZE; j++){
			trifs_trace_shared_buf[i][j] = ( x86_instr_info_buffer_t *)malloc((MAX_TRIFS_TRACE_BUF_SIZE * MAX_TRIFS_TRACE_TB_INST_NUM) * sizeof( x86_instr_info_buffer_t));
            memset((void*)trifs_trace_shared_buf[i][j], 0, (MAX_TRIFS_TRACE_BUF_SIZE * MAX_TRIFS_TRACE_TB_INST_NUM) * sizeof(x86_instr_info_buffer_t));
			trifs_trace_shared_end[i][j] = (x86_instr_info_buffer_t*)(trifs_trace_shared_buf[i][j]) + (MAX_TRIFS_TRACE_BUF_SIZE * MAX_TRIFS_TRACE_TB_INST_NUM);
		}

		trifs_trace_shared_ptr[i] = (x86_instr_info_buffer_t*)(trifs_trace_shared_buf[i][0]);
    }

	//memory
	trifs_mem_shared_buf = ( x86_mem_info_buffer_t ***)malloc(NUM_PROCS * sizeof( x86_mem_info_buffer_t **));
    memset(trifs_mem_shared_buf, 0, NUM_PROCS * sizeof(x86_mem_info_buffer_t **));
	cout << "x86_mem_buffer_t size : " << sizeof(x86_mem_info_buffer_t) << endl;

    for (i = 0; i < NUM_PROCS; i++) {
        trifs_mem_shared_buf[i] = ( x86_mem_info_buffer_t **)malloc(MAX_TRIFS_TRACE_BUF_BLOCK_SIZE * sizeof( x86_mem_info_buffer_t *));
        memset(trifs_mem_shared_buf[i], 0, MAX_TRIFS_TRACE_BUF_BLOCK_SIZE * sizeof(x86_mem_info_buffer_t *));

		for(j = 0; j < MAX_TRIFS_TRACE_BUF_BLOCK_SIZE; j++){
			trifs_mem_shared_buf[i][j] = ( x86_mem_info_buffer_t *)malloc(MAX_TRIFS_TRACE_BUF_SIZE * TRIFS_TRACE_MEM_SIZE_PER_TB * sizeof( x86_mem_info_buffer_t));
            memset((void*)trifs_mem_shared_buf[i][j], 0, MAX_TRIFS_TRACE_BUF_SIZE * TRIFS_TRACE_MEM_SIZE_PER_TB * sizeof(x86_mem_info_buffer_t));
			trifs_mem_shared_end[i][j] = (x86_mem_info_buffer_t*)(trifs_mem_shared_buf[i][j]) + MAX_TRIFS_TRACE_BUF_SIZE * TRIFS_TRACE_MEM_SIZE_PER_TB;
		}

		trifs_mem_shared_ptr[i] = (x86_mem_info_buffer_t*)(trifs_mem_shared_buf[i][0]);
    }

    minqh_flag_buf = ( instr_info_buffer_flag_t **)malloc(NUM_PROCS * sizeof( instr_info_buffer_flag_t *));
    memset(minqh_flag_buf, 0, NUM_PROCS * sizeof( instr_info_buffer_flag_t *));
	minqh_buf_block_index = (int *)malloc(NUM_PROCS * sizeof(int));

    for (i = 0; i < NUM_PROCS; i++) {
        minqh_flag_buf[i] = ( instr_info_buffer_flag_t *)malloc(MAX_TRIFS_TRACE_BUF_BLOCK_SIZE * sizeof( instr_info_buffer_flag_t));
        memset((void*)minqh_flag_buf[i], 0, MAX_TRIFS_TRACE_BUF_BLOCK_SIZE * sizeof( instr_info_buffer_flag_t));
        minqh_buf_block_index[i] = 0;
    }

	cout << "init finish" << endl;
}

// */

VOID * BufferFull(BUFFER_ID id, THREADID threadid, const CONTEXT *ctxt, VOID *buf, UINT64 numElements, VOID *v) {
	struct thread_data_t *thread_data = (struct thread_data_t*)PIN_GetThreadData(buf_key, threadid);

	thread_data->mem_buf = (struct mem_ref*)buf;
    //cout << "buffer full" << endl;
	PIN_SetThreadData(buf_key, thread_data, threadid);

	return buf;
}

vector< x86_inst_t >* get_inst(BBL bbl) {
	ADDRINT ins_addr = INS_Address(BBL_InsHead(bbl));
	vector< x86_inst_t > *result = new vector< x86_inst_t >();
	unsigned int ins_size = 0;
	for(INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
		x86_inst_t cur_inst;
		string s;
		ADDRINT addr = INS_Address(ins);
		for(unsigned int i = 0; i < INS_Size(ins); i++) {
			s.push_back(*(char*)(addr + i));
		}
		cur_inst.inst = s;
		cur_inst.addr = ins_addr;
		result->push_back(cur_inst);
		ins_size = INS_Size(ins);
		ins_addr += ins_size;
	}
	return result;
}


vector< instr_info_buffer_t >* find_entry(bb_map_t** local_bb_map, ADDRINT last_tb_addr){
    int offset = last_tb_addr % 200;
    bb_map_t* list = local_bb_map[offset];
    while (list != NULL && list != 0){
        if (list->last_tb_addr == last_tb_addr)
            return list->entry;
        else list = list->next;
    }
    return NULL;
}

//void send_sig(struct thread_data_t* thread_data, int core_num, vector< instr_info_buffer_t > *entries){
//    //cout <<"before "<< entries->size() << endl;
//    //if ((queue_out[core_num ]-1) %queue_size == queue_in[core_num] )
//    //    cout<<" queue full "<<core_num<<endl;
//    while ((queue_out[core_num ]-1) %queue_size == queue_in[core_num] )
//        sleep(1);
//    thread_data->entry_queue[queue_in[core_num]] = entries;
//    queue_in[core_num] = (queue_in[core_num] +1) % queue_size;
//    //cout <<"af "<< entries->size() << endl;
//    //printf("send %d :%d %d\n",core_num, queue_out[core_num],queue_in[core_num]);
//    //pthread_kill(pid, SIGUSER1);
//
//}

void bbl_callback(THREADID threadid, ADDRINT first_inst_addr, ADDRINT last_inst_addr, uint32_t bbl_numinst,
				  vector< x86_inst_t >* tb_inst) {
#if 1
	struct thread_data_t* thread_data = (struct thread_data_t*)PIN_GetThreadData(buf_key, threadid);
    thread_data->total_inst_num += bbl_numinst;
	if(thread_data->first_tb_flag) {
		thread_data->first_tb_flag = 0;
	} else if (threadid != 0) {
#if 1
//        timespec st1, st2;
//        clock_gettime(CLOCK_REALTIME, &st1);
//        if (thread_data->total_inst_num > thread_data->count*10000000){
//            thread_data->count++;
//            cout<<"thread "<<threadid<<"inst "<<thread_data->total_inst_num<<endl;
//        }
            int err = 0;
		struct mem_ref *mem_buf = thread_data->mem_buf;
        uint64_t* current_queue = (thread_data->addr_queue+ QUEUE_LENGTH* thread_data->queue_no);
		map< ADDRINT, vector< instr_info_buffer_t > > * local_bb_map = thread_data->bb_cache_map;
        //bb_map_t** local_bb_map = thread_data->bb_cache_map;
		ADDRINT last_tb_addr = thread_data->last_tb->tb_addr;
		//map< ADDRINT, vector< instr_info_buffer_t >* >::iterator fi = local_bb_map->find(last_tb_addr);
        //cout<<"last "<<hex<<last_tb_addr<<endl;
        
		//cout << "count "<<threadid<<" "<<local_bb_map->count(last_tb_addr)<<endl;
		if(local_bb_map->count(last_tb_addr) == 0) {
        //if (fi == local_bb_map->end()){
        
        //cout<<"count "<<a<<endl;
        //if (a == NULL || a == 0){
            //find slow
            //cout<< "slow " <<hex << last_tb_addr<<endl;
			//vector< instr_info_buffer_t > ent = *(*local_bb_map)[thread_data->last_tb->tb_addr];
			vector< x86_inst_t > * last_inst = thread_data->last_tb_inst;
			struct TransOp trifs_trace_uops_buf[MAX_X86_UOP_NUM];
			int num_uops;
			//vector< instr_info_buffer_t > entries = new vector< instr_info_buffer_t >();
            vector< instr_info_buffer_t > entries;
			//instr_info_buffer_t* entries = (instr_info_buffer_t*)malloc(sizeof(instr_buffer_t)*num_uops);
			unsigned char trifs_trace_x86_buf[MAX_TRIFS_TRACE_X86_BUF_SIZE];
			for(unsigned int i = 0; i < last_inst->size(); i++) {
				memset(trifs_trace_x86_buf, 0, MAX_TRIFS_TRACE_X86_BUF_SIZE);
				memcpy(trifs_trace_x86_buf, (*last_inst)[i].inst.c_str(), (*last_inst)[i].inst.size());

				GetLock(&lock, threadid + 1);
				trifs_trace_ptlsim_decoder(
					trifs_trace_x86_buf,
					MAX_TRIFS_TRACE_X86_BUF_SIZE,
					trifs_trace_uops_buf,
					&num_uops,
					(*last_inst)[i].addr,
					true
				);
				ReleaseLock(&lock);

				for(int j = 0; j < num_uops; j++) {
					instr_info_buffer_t entry;
                    //translate opcode and registers from uops to entruy
					pseq_t::trifs_trace_translate_uops(&trifs_trace_uops_buf[j],
													   &entry, (*last_inst)[j].addr);

					if(entry.m_futype == FU_RDPORT || entry.m_futype == FU_WRPORT) {
						entry.m_physical_addr = mem_buf->mem_addr;
                        current_queue[thread_data->queue_offset+1] = entry.m_physical_addr;
						mem_buf++;
					}
                    
					if(entry.m_futype == FU_BRANCH) {
						entry.m_branch_result = NO_BRANCH;
						if(first_inst_addr == entry.branch_taken_npc_pc) {
							entry.m_branch_result = TAKEN;
						}
						if(first_inst_addr == entry.branch_untaken_npc_pc) {
							entry.m_branch_result = NOT_TAKEN;
						}
					}
					entries.push_back(entry);
                    entry.no = thread_data->entry_offset;
                    current_queue[thread_data->queue_offset] = entry.no;
                    thread_data->queue_offset += 2;
                    thread_data->entries[thread_data->entry_offset] = entry;
                    thread_data->entry_offset++;
				}
			}
            //push a new translation buffer into thread_data->bb_cache_map
            //cout<<"entries "<<hex<<entries<<endl;
			(*local_bb_map)[thread_data->last_tb->tb_addr] = entries;
            int offset = thread_data->entry_offset - entries.size();
            instr_info_buffer_t* r_p = (instr_info_buffer_t*)((char*)thread_data->remote_ptr + sizeof(uint64_t)*2*THRESHOLD*QUEUE_SIZE+16) + offset;
            int len =  entries.size()*sizeof(instr_info_buffer_t);
            if (offset > NEW_ENTRY) printf("offset overflow %d\n", offset);
            if ((err = scif_writeto(thread_data->epd, (long)(thread_data->entries+offset), len, (long)r_p,0 )) <= 0){
                if (err < 0){
                    printf("scif_writeto entry failed with err %d in thread %d\n", errno, threadid);
                    fflush(stdout);
                    exit(1);
                }
            }
		} else {
            //find fast
			vector< instr_info_buffer_t > *uops = &(*local_bb_map)[last_tb_addr];
			//vector< instr_info_buffer_t > *uops = a;
            
            //cout<< "uops " << uops->size()<<" "<<uops<<" "<< hex << last_tb_addr<<endl;
			for(unsigned int i = 0; i < uops->size(); i++) {
//                thread_data->current_data.push_back((*uops)[i]);
				byte_t futype_tmp = (*uops)[i].m_futype;
				if(futype_tmp == FU_RDPORT || futype_tmp == FU_WRPORT) {
					//(*uops)[i].m_physical_addr = mem_buf->mem_addr;
                    current_queue[thread_data->queue_offset+1] = mem_buf->mem_addr;
					mem_buf++;
				}
                current_queue[thread_data->queue_offset] = (*uops)[i].no;
                thread_data->queue_offset += 2;
			}
			//send_sig(thread_data, threadid/2, &(*local_bb_map)[last_tb_addr]);
			//easton added the do_cal condition, to ensure only instructions between magicbreak is counted
/*			
			if(am_i_in_magicbreak) {
#ifdef EA_TIMING
				do_cal(threadid, (*local_bb_map)[thread_data->last_tb->tb_addr]);
#else
				easton_ins_num[threadid] += ((*local_bb_map)[thread_data->last_tb->tb_addr]).size();
#endif
			}
*/
		}
        if (threadid != 0 && thread_data->queue_offset/2 > THRESHOLD-100){
            //cout<<"send"<<endl;
            int remote_p = 1;
            scif_epd_t epd = thread_data->epd;
            uint64_t* cur = thread_data->p + 1;
            uint64_t* remote_cur = (uint64_t *)thread_data->remote_ptr +1;
//            timespec t1, t2;
//            clock_gettime(CLOCK_REALTIME, &t1);
            while (thread_data->remote_no == thread_data->queue_no ){
                if ((err = scif_readfrom(epd, (long)cur, sizeof(uint64_t), (long)remote_cur,0 )) <= 0){
                    if (err < 0){
                        printf("scif_readfrom failed with err %d in thread %d\n", errno, threadid);
                        fflush(stdout);
                        exit(1);
                    }
                }
                thread_data->remote_no = *cur;

            }
//            clock_gettime(CLOCK_REALTIME, &t2);
//            thread_data->time += t2.tv_sec * 1E9 - t1.tv_sec * 1E9+t2.tv_nsec- t1.tv_nsec;
            int length = thread_data->queue_offset * sizeof(uint64_t);
            *current_queue = thread_data->queue_offset - 1;
            //printf("length %d %d\n",*current_queue, thread_data->queue_no);
            int block = SCIF_SEND_BLOCK;
//            clock_gettime(CLOCK_REALTIME, &t3);
//            if (err = scif_send(epd, &length, sizeof(int), 0) <=0 ){
//                if (err < 0){
//                    printf("scif_send failed with err %d\n", errno);
//                    fflush(stdout);
//                    exit(1);
//                }
//            }  
            uint64_t* remote_queue = (uint64_t*)thread_data->remote_ptr + QUEUE_LENGTH * thread_data->queue_no + 2;
            if ((err = scif_writeto(epd, (long)current_queue, length, (long)remote_queue,0 )) <= 0){
                if (err < 0){
                    printf("scif_writeto queue failed with err %d in thread %d\n", errno, threadid);
                    fflush(stdout);
                    exit(1);
                }
            }
            scif_fence_signal(epd, 0, 0, (long)thread_data->remote_ptr, thread_data->queue_no, SCIF_FENCE_INIT_SELF | SCIF_SIGNAL_REMOTE);
            //int volatile *p = (int volatile *)senddata;
            //while (*p != -1) usleep(10);
            //thread_data->time2 += t1.tv_sec * 1E9 - t3.tv_sec * 1E9+t1.tv_nsec- t3.tv_nsec;
            thread_data->queue_no = (thread_data->queue_no+1) % QUEUE_SIZE;
            thread_data->queue_offset = 1;
        }
//        clock_gettime(CLOCK_REALTIME, &st2);
//        thread_data->time2 += st2.tv_sec * 1E9 - st1.tv_sec * 1E9+st2.tv_nsec- st1.tv_nsec;
#endif
	}

#endif
	thread_data->last_tb->tb_addr = first_inst_addr;
	thread_data->last_tb->inst_num = bbl_numinst;
	thread_data->last_tb_inst = tb_inst;
	PIN_SetThreadData(buf_key, thread_data, threadid);
}

// static UINT64 icount = 0;
// Pin calls this function every time a new basic block is encountered
// It inserts a call to docount
VOID Trace(TRACE trace, VOID *v) {
	// Visit every basic block  in the trace
	for(BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
#if 1
		BBL_InsertCall(bbl, IPOINT_ANYWHERE, AFUNPTR(bbl_callback),
					   IARG_THREAD_ID,
					   IARG_ADDRINT, INS_Address(BBL_InsHead(bbl)),
					   IARG_ADDRINT, INS_Address(BBL_InsTail(bbl)),
					   IARG_UINT32, BBL_NumIns(bbl),
					   IARG_PTR, get_inst(bbl),
					   IARG_END);
#endif

		for(INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
			//easton added
			string ins_str = INS_Disassemble(ins);
			ADDRINT address = INS_Address(ins);
			//if(ins_str.compare("nop ") == 0)
			//	cout << ins_str << endl;
			if(ins_str.compare("nop ") != 0) {
				if(count_nop >= 8) {
					if(am_i_in_magicbreak == false) {
						printf("[easton]magicbreak begines!\n");
						am_i_in_magicbreak = true;
						//easton: this is for instruction count, now we do uop count
						//easton added
						gettimeofday(&starttime, 0);
					} else {
						printf("[easton]magicbreak ends!\n");
						am_i_in_magicbreak = false;
						gettimeofday(&endtime, 0);
						double timeuse = 1000000 * (endtime.tv_sec - starttime.tv_sec) + endtime.tv_usec - starttime.tv_usec;
						timeuse /= 1000000;
						printf("[easton][pin_opal]: it takes %fs \nfrom first thread begins to last thread ends\n", timeuse);
						//easton: if only the magicbreak exits, just output the calulation result and just exit!
						//easton: we don't need to wait for the benchmark exits, wait the magicbreak ends is enough
#ifdef EA_TIMING
						print_result(OutFile);
#else
						long long ea_total_ins = accumulate(easton_ins_num.begin(), easton_ins_num.end(), ea_total_ins);
						printf("\n[easton] Total instructions %lld\n", ea_total_ins);
						printf("[easton] MIPS %f\n", ea_total_ins / timeuse);
#endif
						exit(0);            			//easton: instrction count output not here any more
						//printf("[easton]%lld instructions between magicbreak!\n",ins_count_between_magic);
					}
				}
				count_nop = 0;
			} else {
				//cout << "[easton]in address " << setbase(16) << setw(16) << address << endl;
				count_nop++;
				//cout << "[easton]nop " << count_nop << endl;
			}
#if 1
			if(INS_IsMemoryRead(ins)) {
				INS_InsertFillBuffer(ins, IPOINT_BEFORE, bufId,
									 IARG_ADDRINT, INS_Address(ins), offsetof(struct mem_ref, ins_addr),
									 IARG_MEMORYREAD_EA, offsetof(struct mem_ref, mem_addr),
									 IARG_MEMORYREAD_SIZE, offsetof(struct mem_ref, access_size),
									 IARG_UINT32, 1, offsetof(struct mem_ref, is_full),
									 IARG_END);
			} else if(INS_IsMemoryWrite(ins)) {
				INS_InsertFillBuffer(ins, IPOINT_BEFORE, bufId,
									 IARG_ADDRINT, INS_Address(ins), offsetof(struct mem_ref, ins_addr),
									 IARG_MEMORYWRITE_EA, offsetof(struct mem_ref, mem_addr),
									 IARG_MEMORYWRITE_SIZE, offsetof(struct mem_ref, access_size),
									 IARG_UINT32, 1, offsetof(struct mem_ref, is_full),
									 IARG_END);
			}
#endif
		}
	}
}

//called when starting a new thread
VOID ThreadStart(THREADID threadid, CONTEXT *ctxt, INT32 flags, VOID *v) {
	//easton added
	GetLock(&lock, threadid + 1);
	numThreads++;
    //cout<<"pinthread "<<threadid<<endl;
	easton_ins_num.push_back(0);
	ReleaseLock(&lock);
	struct thread_data_t *thread_data = (struct thread_data_t*)malloc(sizeof(struct thread_data_t));
	struct mem_ref* mem_buf = (struct mem_ref*)PIN_GetBufferPointer(ctxt, bufId);
    thread_data->count = 1;
	thread_data->mem_buf = mem_buf;
    thread_data->total_inst_num = 0;
	thread_data->bb_cache_map = new map< ADDRINT, vector< instr_info_buffer_t > >();
    //thread_data->bb_cache_map = (bb_map_t**)calloc(200,sizeof(bb_map_t*));
	thread_data->last_tb = (x86_tb_instr_buffer_t*)malloc(sizeof(x86_tb_instr_buffer_t));
	thread_data->first_tb_flag = 1;
//    thread_data->current_data_size = 1;

    thread_data->entry_offset = 0;
    thread_data->queue_no = 0;
    thread_data->queue_offset = 1;
    thread_data->time = 0.0;
    thread_data->time2 = 0.0;
    thread_data->remote_no = QUEUE_SIZE-1;
    int err;
//    if ((err=pthread_sigmask(SIG_BLOCK, &sigmask, &oldmask)) != 0)
//        printf("SIG BLOCK err");
//    err = pthread_create(&(thread_data->pid), NULL, cal_buffer, (void*)threadid);

    if (threadid != 0){

    
    scif_epd_t epd;
    int req_pn;
    req_pn = 3000+threadid;
    int con_pn;
    struct scif_portID portID;
    int i, num_loops = 0, total_loop = 10;
    char* senddata;
    char* recvdata;
    int msg_size;
    int node;
    int MIC_NUM = 2;
    portID.node = threadid % MIC_NUM +1;
    portID.port = 3000+(threadid/MIC_NUM + threadid % MIC_NUM );
    //printf("Open the scif driver\n");
    if ((epd = scif_open()) < 0){
        printf("scif_open failed\n");
        exit(1);

    }
   // printf("scif_bind to port 11\n");
    if ((con_pn = scif_bind(epd,req_pn)) < 0){
        printf("scif_bind failed with error %d\n",errno);
        exit(2);
    }
    //printf("req_pn = %d\n",req_pn);
retry:
    if ((scif_connect(epd, &portID)) < 0){
        if (ECONNREFUSED == errno){
            printf("scif_connect failed with error %d retrying in thread %d\n", errno, threadid);
        }
        printf("scif_connect failed with error %d\n", errno);
        exit(3);
    }
    void* remote_ptr;
    if (err = scif_recv(epd, &remote_ptr, sizeof(void*), SCIF_RECV_BLOCK) <=0 ){
        if (err < 0){
            printf("scif_recv failed with err %d\n", errno);
            fflush(stdout);
            exit(1);
        }
    }   
    int pagesize = sysconf(_SC_PAGESIZE);
    instr_info_buffer_t* current_d;
    //len must be multiple of pagesize
    //int len = (240+THRESHOLD)*sizeof(instr_info_buffer_t);
    int len = THRESHOLD*QUEUE_SIZE*(sizeof(uint64_t)*2) + NEW_ENTRY*sizeof(instr_info_buffer_t);
    len = (len / pagesize+1) * pagesize;
    int ret = posix_memalign((void **)&current_d, pagesize, len);

    cout << pagesize << " " << len << " "<<(long)current_d<<" "<<ret<<endl;

    if (SCIF_REGISTER_FAILED == scif_register(epd, (void *)current_d, len, (off_t)current_d, SCIF_PROT_READ | SCIF_PROT_WRITE, SCIF_MAP_FIXED)){
        printf("scif register failed in %d with error %d\n", threadid, errno);
    }
    /* *p signal stand for the latest queue that the host had sent 
     * Host will write to it */
    thread_data->p = (uint64_t*)current_d;
    thread_data->addr_queue = (uint64_t*)current_d + 2;
    thread_data-> entries= (instr_info_buffer_t*)((char*)current_d + sizeof(uint64_t)*2*THRESHOLD*QUEUE_SIZE+16);
    //printf("scif_connect success\n");
    printf("ndoe = %d, port= %d\n", portID.node, portID.port);
    thread_data->epd = epd;
    thread_data->remote_ptr = remote_ptr;
//    thread_data->current_data = current_d;

    }
    //cout<<"fin start"<<endl;
	PIN_SetThreadData(buf_key, thread_data, threadid);
}





KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
							"o", "inscount.out", "specify output file name");

// This function is called when the application exits
VOID Fini(INT32 code, VOID *v) {
//#ifdef EA_TIMING
	//print_result(OutFile);
//#else
	//long long ea_total_ins = accumulate(easton_ins_num.begin(), easton_ins_num.end(), ea_total_ins);
	//printf("\n[easton] Total instructions %lld\n", ea_total_ins);
//#endif
	// Write to a file since cout and cerr maybe closed by the application
	OutFile.setf(ios::showbase);
    printf("Fini\n");
    UINT64 total_inst_num_sum = 0;
    for(UINT32 t = 0; t < numThreads; t++){
        struct thread_data_t* thread_data = (struct thread_data_t*)PIN_GetThreadData(buf_key, t);
        scif_epd_t epd = thread_data->epd;
        int block = 1;
        int length = -1;
        int err;
        if (t != 0){

        
        uint64_t* ptr =(thread_data->p);
        *ptr = 0xFFFF;
        if ((err = scif_writeto(epd, (long)ptr, sizeof(uint64_t), (long)thread_data->remote_ptr,0 )) <= 0){
            if (err < 0){
                printf("scif_send failed with err %d\n", errno);
                fflush(stdout);
                exit(1);
            }
        }
        close(thread_data->epd);

        }
//        cout << "thread "<<t<<" consumes "<<thread_data->time<<endl;
//        cout <<"consumes "<<thread_data->time2<<" total"<<endl;
        total_inst_num_sum += thread_data->total_inst_num;
        //OutFile << "Thread[" << t <<"] total_inst_num = "<<thread_data->total_inst_num<<endl;
    } 
    cout << "[Calculation-based simulator]-Total x86 Instructions: "<< total_inst_num_sum<<endl;

	OutFile << "Count " << icount << endl;
	OutFile << "num_bb " << num_bb << endl;
	OutFile << "num_last_instr " << num_last_instr << endl;
	OutFile << "num_fetch_inst " << num_fetch_inst << endl;
	OutFile << "num_tb_mem_diff " << num_tb_mem_diff << endl;
	OutFile << "num_transfer_instr " << num_transfer_instr << endl;
	OutFile << "bb_cache_map size " << bb_cache_map[0].size() << endl;
	OutFile << "num_reads " << num_reads << " num_writes " << num_writes << endl;
	OutFile << "num buffer full " << num_buffer_full << endl;
	OutFile << "num missed mem " << num_missed_mem << endl;
	OutFile << "test addr " << test_addr << endl;
	OutFile.close();
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */
INT32 Usage() {
	cerr << "This tool counts the number of dynamic instructions executed" << endl;
	cerr << endl << KNOB_BASE::StringKnobSummary() << endl;
	return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */
int main(int argc, char * argv[]) {
	for(int i = 0; i < argc; i++) {
		cout << argv[i] << endl;
	}

	//easton added
	pthread_mutex_init(&ea_mutex, NULL);

	//init calculation
	//zmf_init();

	// Initialize pin
	if(PIN_Init(argc, argv)) return Usage();

	for(int i = 0; i < NUM_PROCS; i++) {
		bb_cache_map.push_back(map< ADDRINT, vector< x86_inst_t >* >());
		bb_cache_map2.push_back(map< ADDRINT, vector< instr_info_buffer_t > >());
		bb_mem_map.push_back(map< ADDRINT, int >());
	}

    nodes = (uint16_t *)malloc(sizeof(uint16_t)*3);
    uint16_t self;
    int num = scif_get_nodeIDs(nodes, 3, &self);
	InitLock(&lock);

	bufId = PIN_DefineTraceBuffer(sizeof(struct mem_ref), NUM_BUF_PAGES, BufferFull, 0);

	OutFile.open(KnobOutputFile.Value().c_str());

	PIN_AddThreadStartFunction(ThreadStart, 0);

	// Register Instruction to be called to instrument instructions
	TRACE_AddInstrumentFunction(Trace, 0);

	// Register Fini to be called when the application exits
	PIN_AddFiniFunction(Fini, 0);

	// Start the program, never returns
	PIN_StartProgram();

	return 0;
}
