
/*------------------------------------------------------------------------*/
/* Includes                                                               */
/*------------------------------------------------------------------------*/
#include "calculation.h"

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <time.h>
#include <sys/time.h>

//easton added

uint64_t total_cache =0;
uint64_t big_inst = 0;
uint64_t origin_cache = 0;
uint64_t* tb_count = 0;
uint64_t hash_count = 0;
uint64_t* new_count = 0;
uint64_t* duplicate_count = 0;

sigset_t sigmask;
sigset_t oldmask;
vector< instr_info_buffer_t > ***entry_queue;
int* queue_in;
int* queue_out;

double* search_time;
double* old_time;
double* new_time;

int global_cpu_count;
int qemu_instr_buffer_size;
#ifdef JXF_BLOCK
//    tb_info_struct_t **tb_info_list;
     tb_info_struct_t *tb_info_header;
    int is_a_newtb = 0;
tb_info_struct_t* tb_buffer[TB_CACHE_SIZE];
#endif

/*------------------------------------------------------------------------*/
/* Macro declarations                                                     */
/*------------------------------------------------------------------------*/

/*------------------------------------------------------------------------*/
/* Data type and variable declaration(s)                                                   */
/*------------------------------------------------------------------------*/

/*get the index of opcode used in CONFIG_NUM_ALUS and CONFIG_ALU_LATENCY*/
int CONFIG_ALU_MAPPING[] = {
	0, // FU_NONE,                 // inst does not use a functional unit
	1, // FU_INTALU,               // integer ALU
	1, // FU_INTMULT,              // integer multiplier
	2, // FU_INTDIV,               // integer divider
	3, // FU_BRANCH,               // compare / branch units
	4, // FU_FLOATADD,             // floating point adder/subtractor
	4, // FU_FLOATCMP,             // floating point comparator
	4, // FU_FLOATCVT,             // floating point<->integer converter
	5, // FU_FLOATMULT,            // floating point multiplier
	6, // FU_FLOATDIV,             // floating point divider
	6, // FU_FLOATSQRT,            // floating point square root
	7, // FU_RDPORT,               // memory read port
	8  // FU_WRPORT,               // memory write port
	// FU_NUM_FU_TYPES          // total functional unit classes
};

/*the number of available ALUs*/
int CONFIG_NUM_ALUS[] = {
	127, // inst does not use a functional unit
	2, // integer ALU (fused multiply/add)
	2, // integer divisor
	2, // compare branch units
	1, // FP ALU
	1, // FP multiply
	1, // FP divisor / square-root
	2, // load unit (memory read)
	2, // store unit (memory write)
	0,
	0,
	0,
	0
};

int CONFIG_ALU_LATENCY[] = {
	1, // FU_NONE,                 // inst does not use a functional unit
	1, // FU_INTALU,               // integer ALU
	4, // FU_INTMULT,              // integer multiplier
	20, // FU_INTDIV,               // integer divider
	1, // FU_BRANCH,               // compare / branch units
	2, // FU_FLOATADD,             // floating point adder/subtractor
	2, // FU_FLOATCMP,             // floating point comparator
	2, // FU_FLOATCVT,             // floating point<->integer converter
	6, // FU_FLOATMULT,            // floating point multiplier
	32, // FU_FLOATDIV,             // floating point divider
	32, // FU_FLOATSQRT,            // floating point square root
	2, // FU_RDPORT,               // memory read port
	1  // FU_WRPORT,               // memory write port
	// FU_NUM_FU_TYPES          // total functional unit classes
};

pthread_mutex_t mutex;

bool zmf_finished = false;

//WHJ
#ifdef TRANSFORMER_PARALLEL
#ifdef PARALLELIZED_FUNCTIONAL_AND_TIMING
#ifdef PARALLELIZED_TIMING_BY_CORE
pthread_t *timing_tid;
#else
pthread_t timing_tid;
#endif
#endif
#endif

pseq_t **m_seq;

/// thread arguments to start the timing core thread
typedef struct {
	int core_id;
} timing_core_thread_arg;

/*------------------------------------------------------------------------*/
/* Function definition(s)                                                   */
/*------------------------------------------------------------------------*/


extern "C"
{
	extern int trifs_trace_ptlsim_decoder(W8 *x86_buf, int num_bytes,
										  TransOp *upos_buf, int *num_uops, W64 rip, bool is64bit);
}

void set_cpu(int cpu_no) {
	cpu_set_t mask;
	CPU_ZERO(&mask);
	CPU_SET(cpu_no, &mask);
	sched_setaffinity(syscall(SYS_gettid), sizeof(cpu_set_t), &mask);
}

pseq_t::pseq_t(int id) {
	m_id = id;
	m_local_cycles = 0;
	m_local_instr_count = 0;
	calculation_wait_num = 0;
    queue_in = 0;
    queue_out = 0;
    entry_queue = (instr_info_buffer_t**)malloc(sizeof(instr_info_buffer_t*)*QUEUE_SIZE);
    size_queue = (int*)malloc(sizeof(int)*QUEUE_SIZE);
    t1 = t2 = t3 = 0.0;

#ifdef TRANSFORMER_INORDER
	minqh_flag_buffer_index = 0;
	minqh_mem_flag_buffer_index = 0;
	correct_timing_ptr = 0;
	correct_timing_release_ptr = 0;
	correct_timing_fetch_ptr = 0;
	correct_mem_fetch_ptr = 0;
	correct_tb_fetch_ptr = 0;
	//printf("before getting tb buffer head\n");
	fflush(stdout);
	/*
	correct_qemu_tb_instr_buf = ( x86_tb_instr_buffer_t **)trifs_trace_shared_tb_inst_buf_head(id);
	correct_qemu_instr_info_buf = ( x86_instr_info_buffer_t **)trifs_trace_shared_buf_head(id);
	correct_qemu_mem_info_buf = ( x86_mem_info_buffer_t **)trifs_mem_shared_buf_head(id);

	minqh_flag_buffer = ( instr_info_buffer_flag_t *)get_minqh_flag_buf_head(id);
	std::cout << "timing tb struct size: " << sizeof(x86_tb_instr_buffer_t) << " addr: " << (void *)correct_qemu_tb_instr_buf[0] << std::endl;
	//printf("timing tb struct size: %d %x\n", (int)sizeof(x86_tb_instr_buffer_t), (unsigned int)(correct_qemu_tb_instr_buf[0]));
	std::cout << "timing struct size: " << sizeof(x86_instr_info_buffer_t) << " addr: " << (void *)correct_qemu_instr_info_buf[0] << std::endl;
	//printf("timing struct size: %d %x\n", (int)sizeof(x86_instr_info_buffer_t), (unsigned int)correct_qemu_instr_info_buf[0]);
	std::cout << "timing mem struct size: " << sizeof(x86_mem_info_buffer_t) << "addr: " << (void *)correct_qemu_mem_info_buf[0] << std::endl;
	//printf("timing mem struct size: %d %x\n", (int)sizeof(x86_mem_info_buffer_t), (unsigned int)correct_qemu_mem_info_buf[0]);
	*/


	code_cache_found = 0;
	tb_start = 1;
	tb_end = 0;
	tb_num_uops = 0;
	uops_index = 0;
	mem_tb_start = 1;
	tb_has_mem = 0;
	print_flag = 0;

	m_inorder_fetch_rob_count = 0;
	for(int k = 0; k < QEMU_NUM_REGS; k++) {
		m_inorder_register_finish_cycle[k] = 0;
	}
	for(int k = 0; k < INORDER_MAX_ESTIMATE_INST_NUM; k++) {
		m_inorder_execute_inst_width[k] = 0;
		for(int kk = 0; kk < FU_NUM_FU_TYPES; kk++) {
			m_inorder_function_unit_width[k][kk] = 0;
		}
	}
	m_inorder_retire_inst_cycle = (uint64_t *)malloc(CORRECT_RETIRE_BUF_SIZE * sizeof(uint64_t));
	m_next_pc = (uint64_t *)malloc(CORRECT_RETIRE_BUF_SIZE * sizeof(uint64_t));
	m_next_fu_type = (int *)malloc(CORRECT_RETIRE_BUF_SIZE * sizeof(int));
	for(int k = 0; k < CORRECT_RETIRE_BUF_SIZE; k++) {
		m_inorder_retire_inst_cycle[k] = 0;
		m_next_pc[k] = 0;
		m_next_fu_type[k] = 0;
	}
	m_inorder_x86_retire_info.inorder_uop_sum = 1;
	m_inorder_x86_retire_info.inorder_last_pc = 0;
	m_inorder_x86_retire_info.inorder_last_fu_type = -1;
	m_inorder_x86_retire_info.inorder_skip_cycle = 0;

	cal_lsq_store_queue_ptr = 0;

#endif

#ifdef TRANSFORMER_USE_CACHE_COHERENCE_buffer
	m_coherence_buffer = (coherence_buffer_t *)malloc(Coherence_buffer_size * sizeof(coherence_buffer_t));
	memset((void *)m_coherence_buffer, 0, Coherence_buffer_size * sizeof(coherence_buffer_t));
	m_coherence_buffer_write_ptr = 0;
	L2_hit_increase_back = (long long *)malloc(sizeof(long long));
#endif

	m_predictor = new yags_t(BRANCHPRED_PHT_BITS, BRANCHPRED_EXCEPTION_BITS, BRANCHPRED_TAG_BITS);
	m_spec_bpred = (predictor_state_t *)malloc(sizeof(predictor_state_t));
	m_spec_bpred->cond_state = 0;
	m_spec_bpred->indirect_state = 0;
	m_spec_bpred->ras_state.TOS = 0;
	m_spec_bpred->ras_state.next_free = 0;

#ifdef TRANSFORMER_INORDER_CACHE
	// private cache
	CAT_set_bit = DL1_SET_BITS;
	CAT_assoc = DL1_ASSOC;
	CAT_set_size = (1 << CAT_set_bit);
	CAT_set_mask = CAT_set_size - 1;
	CAT_block_bit = DL1_BLOCK_BITS;
	CAT_block_size = (1 << CAT_block_bit);
	CAT_block_mask = CAT_block_size - 1;

	CAT_pseudo_core_cache = (CAT_pseudo_cache_set_t*)malloc(CAT_set_size * sizeof(CAT_pseudo_cache_set_t*));
	for(int j = 0; j < CAT_set_size; j++) {
		CAT_pseudo_core_cache[j].cache_line_list = (CAT_pseudo_cache_line_t*)malloc(CAT_assoc * sizeof(CAT_pseudo_cache_line_t));
		for(int k = 0; k < CAT_assoc; k++) {
			CAT_pseudo_core_cache[j].cache_line_list[k].flag = CAT_CACHE_EMPTY;
		}
	}

	CAT_L2_set_bit = L2_SET_BITS;
	CAT_L2_assoc = L2_ASSOC;
	CAT_L2_set_size = (1 << CAT_L2_set_bit);
	CAT_L2_set_mask = CAT_L2_set_size - 1;

	CAT_pseudo_L2_cache = (CAT_pseudo_cache_set_t*)malloc(CAT_L2_set_size * sizeof(CAT_pseudo_cache_set_t));
	for(int i = 0; i < CAT_L2_set_size; i++) {
		CAT_pseudo_L2_cache[i].cache_line_list = (CAT_pseudo_cache_line_t*)malloc(CAT_L2_assoc * sizeof(CAT_pseudo_cache_line_t));
		for(int j = 0; j < CAT_L2_assoc; j++) {
			CAT_pseudo_L2_cache[i].cache_line_list[j].flag = CAT_CACHE_EMPTY;
		}
	}

	//analysis count parameter
	inorder_wait_num = 0;
	inorder_l1_icache_hit_count = 0;
	inorder_l1_icache_miss_count = 0;
	inorder_l1_dcache_hit_count = 0;
	inorder_l1_dcache_miss_count = 0;
	inorder_l2_cache_hit_count = 0;
	inorder_l2_cache_miss_count = 0;
	inorder_branch_count = 0;
	inorder_branch_misprediction_count = 0;
#endif
}

pseq_t::~pseq_t() {
	free(m_inorder_retire_inst_cycle);
	//free(cal_lsq_store_queue);
	//free(m_spec_bpred);
	//delete(correct_qemu_instr_info_buf);
	//delete(correct_qemu_tb_instr_buf);
	//delete(minqh_flag_buffer);
	//delete(m_predictor);
}


#ifdef TRANSFORMER_INORDER_CACHE
//false means alredy in cache
bool pseq_t::CAT_pseudo_line_insert(long long address, long long cycle) {
	unsigned int index = Set(address);
	long long ba = BlockAddress(address);

	CAT_pseudo_cache_set_t *set = &CAT_pseudo_core_cache[index];

	for(int i = 0; i < CAT_assoc; i++) {
		if(set->cache_line_list[i].flag == CAT_CACHE_FULL && set->cache_line_list[i].block_addr == ba) {
			for(unsigned int j = i; j > 0; j--) {
				set->cache_line_list[j].block_addr = set->cache_line_list[j - 1].block_addr;
				set->cache_line_list[j].flag = set->cache_line_list[j - 1].flag;
			}
			set->cache_line_list[0].block_addr = ba;
			set->cache_line_list[0].flag = CAT_CACHE_FULL;
			return false;
		}
	}

	//if the set is full, we need to put the replaced line into L2 cache
	if(set->cache_line_list[CAT_assoc - 1].flag == CAT_CACHE_FULL) {
#ifdef TRANSFORMER_USE_CACHE_COHERENCE
#ifdef TRANSFORMER_USE_CACHE_COHERENCE_buffer
		m_write_coherence_buffer(set->cache_line_list[CAT_assoc - 1].block_addr, Coherence_replace, cycle, 0);
#else
		//CAT_pseudo_access_directory(address, core, Coherence_replace, cycle);
#endif
#endif

		CAT_pseudo_L2_line_insert(set->cache_line_list[CAT_assoc - 1].block_addr, cycle);
	}

	for(unsigned int i = CAT_assoc - 1; i > 0; i--) {
		set->cache_line_list[i].block_addr = set->cache_line_list[i - 1].block_addr;
		set->cache_line_list[i].flag = set->cache_line_list[i - 1].flag;
	}
	set->cache_line_list[0].block_addr = ba;
	set->cache_line_list[0].flag = CAT_CACHE_FULL;

	return true;
}

/*
**called only under one the the TWO conditions:
**1. Line is replaced from L1 cache (when CAT_pseudo_line_insert() is called)
**2. L1 miss && not in other cores' L1 cache && in L2 cache
*/
bool pseq_t::CAT_pseudo_L2_line_insert(long long address, long long cycle) {
	unsigned int index = L2_Set(address);
	long long ba = BlockAddress(address);

	CAT_pseudo_cache_set_t *set = &CAT_pseudo_L2_cache[index];

	for(int i = 0; i < CAT_L2_assoc; i++) {
		if(set->cache_line_list[i].flag == CAT_CACHE_FULL && set->cache_line_list[i].block_addr == ba) {
			for(unsigned int j = i; j > 0; j--) {
				set->cache_line_list[j].block_addr = set->cache_line_list[j - 1].block_addr;
				set->cache_line_list[j].flag = set->cache_line_list[j - 1].flag;
			}
			set->cache_line_list[0].block_addr = ba;
			set->cache_line_list[0].flag = CAT_CACHE_FULL;
			return false;
		}
	}

	for(unsigned int i = CAT_L2_assoc - 1; i > 0; i--) {
		set->cache_line_list[i].block_addr = set->cache_line_list[i - 1].block_addr;
		set->cache_line_list[i].flag = set->cache_line_list[i - 1].flag;
	}
	set->cache_line_list[0].block_addr = ba;
	set->cache_line_list[0].flag = CAT_CACHE_FULL;

	return true;
}

//check if L2 hit or miss, return false if not in L2
bool pseq_t::CAT_pseudo_L2_tag_search(long long address) {
	unsigned int index = L2_Set(address);
	long long   ba = BlockAddress(address);
	bool   cachehit = false;

	/* search all sets until we find a match */
	CAT_pseudo_cache_set_t *set = &CAT_pseudo_L2_cache[index];
	for(int i = 0 ; i < CAT_L2_assoc ; i ++) {
		// if it is found in the cache ...
		if((set->cache_line_list[i].flag == CAT_CACHE_FULL) && BlockAddress(set->cache_line_list[i].block_addr) == ba) {
			cachehit = true;
			break;
		}
	}
	return cachehit;
}

#ifdef TRANSFORMER_USE_CACHE_COHERENCE_buffer
// Write coherence buffer
bool pseq_t::m_write_coherence_buffer(long long address, int type, long long cycle, int in_L2) {
	while(m_coherence_buffer[m_coherence_buffer_write_ptr].flag == FULL) {
		;
	}
	m_coherence_buffer[m_coherence_buffer_write_ptr].address = address;
	m_coherence_buffer[m_coherence_buffer_write_ptr].cycle = cycle;
	m_coherence_buffer[m_coherence_buffer_write_ptr].type = type;
	m_coherence_buffer[m_coherence_buffer_write_ptr].in_L2 = in_L2;
	m_coherence_buffer[m_coherence_buffer_write_ptr].flag = FULL;
	m_coherence_buffer_write_ptr = (m_coherence_buffer_write_ptr + 1) % Coherence_buffer_size;//circular buffer
	return true;
}
#endif
#endif

/*translate TransOp structure to instr_info_buffer_t structure*/
bool pseq_t::trifs_trace_translate_uops(struct TransOp* uop, instr_info_buffer_t* instr_info, uint64_t trifs_trace_virt_pc) {
	uint32_t inst;
	inst = uop->opcode;
	instr_info->logical_pc = trifs_trace_virt_pc;
	instr_info->m_futype = uops_gems_map_table[inst].gems_futype;
	instr_info->m_dest_reg[0] = uop->rd + 1;
	instr_info->m_source_reg[0] = uop->ra + 1;
	instr_info->m_source_reg[1] = uop->rb + 1;
	instr_info->m_source_reg[2] = uop->rc + 1;
#ifdef TRANSFORMER_MISPREDICTION
	if(instr_info->m_futype == FU_BRANCH) {
		instr_info->branch_taken_npc_pc = uop->riptaken;
		instr_info->branch_untaken_npc_pc = uop->ripseq;
	}
#endif
	return true;
}


bool pseq_t::m_calculate(instr_info_buffer_t* entries, uint64_t* queue, unsigned int& entry_index, int length) {
	int phy_proc = m_id;
    struct timeval s1, s2, s3, s4;
	// If pipeline is stalled, we do not fetch and just return
//	gettimeofday(&s1, 0);
	if(m_inorder_x86_retire_info.inorder_skip_cycle > 0) {
		for(uint64_t tt = 0; tt < m_inorder_x86_retire_info.inorder_skip_cycle; tt++) {
			uint64_t offset = m_local_cycles % INORDER_MAX_ESTIMATE_INST_NUM;
			assert(offset >= 0 && offset < INORDER_MAX_ESTIMATE_INST_NUM);
			m_inorder_execute_inst_width[offset] = 0;
			for(int mm = 0; mm < FU_NUM_FU_TYPES; mm++) {
				m_inorder_function_unit_width[offset][mm] = 0;
			}
			m_local_cycles++;
		}
	}
    //printf("in cal\n");
	int inorder_dispatch_inst_count = MAX_FETCH; //pipeline width: process MAX_FETCH instrctions simultaneously
	bool fetch_ok = true;
	m_inorder_x86_retire_info.inorder_skip_cycle = 1;
	while(inorder_dispatch_inst_count > 0) { //handle each instruction
		if(m_inorder_fetch_rob_count >= INORDER_MAX_STALL_INST_NUM) {
			//cout << "core"<< m_id << " m_inorder_fetch_rob_count " << m_inorder_fetch_rob_count << endl;
		}
		if(m_inorder_fetch_rob_count < INORDER_MAX_STALL_INST_NUM) { //instruction window size
			// fetch
			instr_info_buffer_t entry;

			//fetch_ok = TRANS_inorder_simpleLookupInstruction(&entry);
			//cout << "entry index is " << entry_index << "\tentries size is " << length << endl;
			if(entry_index < length) {
				assert(entry_index < length);
				fetch_ok = true;
                uint64_t p = queue[entry_index];
				entry = entries[p]; //fetch an instrction
                entry.m_physical_addr = queue[entry_index+1];
                entry_index += 2;
				assert(correct_timing_release_ptr >= 0 && correct_timing_release_ptr < CORRECT_RETIRE_BUF_SIZE);
				m_next_pc[correct_timing_release_ptr] = entry.logical_pc;
				//cout << "correct_timing_release_ptr" << correct_timing_release_ptr <<
				//" m_next pc is " << m_next_pc[correct_timing_release_ptr] << endl;
				m_next_fu_type[correct_timing_release_ptr] = entry.m_futype;
				correct_timing_release_ptr = (correct_timing_release_ptr + 1) % CORRECT_RETIRE_BUF_SIZE;
				assert(correct_timing_release_ptr >= 0 && correct_timing_release_ptr < CORRECT_RETIRE_BUF_SIZE);
				//continue;
			} else {
				fetch_ok = false;
			}
			if(fetch_ok == false) {
				//cout << "fetch ok is false" << endl;
				inorder_wait_num++;
				break;
			}
            
			int fu_type = entry.m_futype;
			int fu_mapped = CONFIG_ALU_MAPPING[fu_type];
			m_inorder_fetch_rob_count++;
			//cout << "new inst pc is " << entry.logical_pc << endl;
			//cout << "increase m_inorder_fetch_rob_count is " << m_inorder_fetch_rob_count << endl;
			if(m_inorder_fetch_rob_count > INORDER_MAX_STALL_INST_NUM) {
				cout << m_inorder_fetch_rob_count << endl;
			}
			assert(m_inorder_fetch_rob_count <= INORDER_MAX_STALL_INST_NUM);
			uint64_t fetch_cycle = m_local_cycles;
			//cout << "after fetch" << endl;
			//cout << "before schedule" << endl;
			// schedule
			uint64_t max_value = fetch_cycle;
			for(int i = 0; i < QEMU_SI_MAX_SOURCE; i++) {
				if(entry.m_source_reg[i] != 0) {
					/*access the finish cycle of each relevant register*/
					uint64_t finish_cycle = m_inorder_register_finish_cycle[entry.m_source_reg[i]] + 1;
					if(max_value < finish_cycle) {
						max_value = finish_cycle;
					}
				}
			}
			// WHJ lsq
			if(entry.m_futype == FU_RDPORT) {
				uint64_t temp_addr = entry.m_physical_addr;
				if(cal_lsq_store_queue_ptr) {
					if(temp_addr == cal_lsq_store_queue_0.store_address) {
						if(max_value < cal_lsq_store_queue_0.store_finish_cycle) {
							max_value = cal_lsq_store_queue_0.store_finish_cycle;
						}
					} else if(temp_addr == cal_lsq_store_queue_1.store_address) {
						if(max_value < cal_lsq_store_queue_1.store_finish_cycle) {
							max_value = cal_lsq_store_queue_1.store_finish_cycle;
						}
					}
				} else {
					if(temp_addr == cal_lsq_store_queue_1.store_address) {
						if(max_value < cal_lsq_store_queue_1.store_finish_cycle) {
							max_value = cal_lsq_store_queue_1.store_finish_cycle;
						}
					} else if(temp_addr == cal_lsq_store_queue_0.store_address) {
						if(max_value < cal_lsq_store_queue_0.store_finish_cycle) {
							max_value = cal_lsq_store_queue_0.store_finish_cycle;
						}
					}
				}
			} //
			uint64_t issue_cycle = max_value;
			//cout << "after schedule" << endl;

			//cout << "before execute" << endl;
			// Execute stage
			// We do two things.
			// 1. If current cycle has executed 4 instructions, we execute this instr. in next cycle.
			uint64_t execute_offset = issue_cycle % INORDER_MAX_ESTIMATE_INST_NUM;
			while(m_inorder_execute_inst_width[execute_offset] >= MAX_EXECUTE) {
				execute_offset++;
				if(execute_offset >= INORDER_MAX_ESTIMATE_INST_NUM) {
					execute_offset = 0;
				}
				assert(execute_offset != (issue_cycle % INORDER_MAX_ESTIMATE_INST_NUM));
			}
//			m_inorder_execute_inst_width[execute_offset]++;//increase the number of instructions being executed
			// 2. If FunctionUnit is used in current cycle, we execute this instr. in next cycle.
			while(m_inorder_function_unit_width[execute_offset][fu_mapped] >= CONFIG_NUM_ALUS[fu_mapped]) {
				execute_offset++;
				if(execute_offset >= INORDER_MAX_ESTIMATE_INST_NUM) {
					execute_offset = 0;
				}
				assert(execute_offset != (issue_cycle % INORDER_MAX_ESTIMATE_INST_NUM));
//			    m_inorder_execute_inst_width[execute_offset]++;//increase the number of instructions being executed
			}
            m_inorder_execute_inst_width[execute_offset]++;
			m_inorder_function_unit_width[execute_offset][fu_mapped]++;//increase the number of occupied register
            //JXF0316: add latency for mul and div
//            if (fu_mapped == FU_INTMULT) {
//                for (int i =1; i< MUL_LATENCY; i++){
//  			    execute_offset++;
//    				if(execute_offset >= INORDER_MAX_ESTIMATE_INST_NUM) {
//      				execute_offset = 0;
//  	    		}
//                    m_inorder_execute_inst_width[execute_offset]++;
//        			m_inorder_function_unit_width[execute_offset][fu_mapped]++; //*/
//                }
//            }
//            else if (fu_mapped == FU_INTDIV) {
//                for (int i =1; i< DIV_LATENCY; i++){
//				    execute_offset++;
//    				if(execute_offset >= INORDER_MAX_ESTIMATE_INST_NUM) {
//	    				execute_offset = 0;
//		    		}
////                    m_inorder_execute_inst_width[execute_offset]++;
//        			m_inorder_function_unit_width[execute_offset][fu_mapped]++; //*/
//                }
//            }

			uint64_t execute_cycle = issue_cycle + (execute_offset - issue_cycle + INORDER_MAX_ESTIMATE_INST_NUM) % INORDER_MAX_ESTIMATE_INST_NUM;
			//cout << "after execute" << endl;
			//cout << "before complete" << endl;
			//complete
			uint64_t penalty_cycle = CONFIG_ALU_LATENCY[fu_type];//add the penalty cycle of this instruction
#ifdef TRANSFORMER_INORDER_CACHE
			uint64_t cache_cycle = 0;
			if((fu_type == FU_RDPORT) || (fu_type == FU_WRPORT)) {
				uint64_t addr = entry.m_physical_addr;
				if(CAT_pseudo_line_insert(addr, execute_cycle)) {
					//L1 cache miss
					int in_L2 = CAT_pseudo_L2_tag_search(addr);
					if(in_L2) {
						inorder_l2_cache_hit_count++;
						CAT_pseudo_L2_line_insert(addr, execute_cycle);
					} else {
						//l2 cache miss
						inorder_l2_cache_miss_count++;
						cache_cycle += INORDER_MEMORY_LATENCY;//add the latency of memory access
					}
					inorder_l1_dcache_miss_count++;
					cache_cycle += INORDER_L2_CACHE_LATENCY;//add the latency of L2 cache access

    #ifdef TRANSFORMER_USE_CACHE_COHERENCE
        #ifdef TRANSFORMER_USE_CACHE_COHERENCE_buffer
					if(fu_type == FU_WRPORT) {
						m_write_coherence_buffer(addr, Coherence_store_miss, execute_cycle, in_L2);
					} else if(fu_type == FU_RDPORT) {
						m_write_coherence_buffer(addr, Coherence_load_miss, execute_cycle, in_L2);
					}
        #endif
    #endif
				} else {
					// l1 cache hit
					inorder_l1_dcache_hit_count++;
    #ifdef TRANSFORMER_USE_CACHE_COHERENCE
        #ifdef TRANSFORMER_USE_CACHE_COHERENCE_buffer
					if(fu_type == FU_WRPORT) {
						m_write_coherence_buffer(addr, Coherence_store_hit, execute_cycle, 0);
					}
        #endif
    #endif
				}
				cache_cycle += INORDER_L1_CACHE_LATENCY;//add the latency of L1 cache access
			}
			penalty_cycle += cache_cycle;
#endif
			uint64_t finish_cycle = execute_cycle + penalty_cycle;
			/*record the finish cycle of destination register*/
			if(entry.m_dest_reg[0] != 0) {
				m_inorder_register_finish_cycle[entry.m_dest_reg[0]] = finish_cycle;
			}
			//Added by WHJ 2013.06.25 LSQ
			if(fu_type == FU_WRPORT) {
				if(cal_lsq_store_queue_ptr) {
					cal_lsq_store_queue_ptr = 0;
					cal_lsq_store_queue_1.store_address = entry.m_physical_addr;
					cal_lsq_store_queue_1.store_finish_cycle = finish_cycle;
				} else {
					cal_lsq_store_queue_ptr = 1;
					cal_lsq_store_queue_0.store_address = entry.m_physical_addr;
					cal_lsq_store_queue_0.store_finish_cycle = finish_cycle;
				}
			}
			//cout << "after complete" << endl;
			//cout << "before retire" << endl;
			//retire
			uint64_t retire_cycle = finish_cycle + 1;
			int index = (correct_timing_release_ptr - 1 + CORRECT_RETIRE_BUF_SIZE) % CORRECT_RETIRE_BUF_SIZE;
			m_inorder_retire_inst_cycle[index] = retire_cycle;
#ifdef JXF_debug            
            printf("index %d retire cycle %d\n",index,m_inorder_retire_inst_cycle[index]);
#endif            
			//cout << "retire cycle is " << retire_cycle << " index is " << index << endl;
#ifdef TRANSFORMER_INORDER_BRANCH
			if(fu_type == FU_BRANCH) {
				inorder_branch_count++;
				uint64_t fetch_pc = entry.logical_pc;
				/*predict by two-bits saturing mechanism*/
				bool taken = m_predictor->Predict(fetch_pc, m_spec_bpred->cond_state, 0, phy_proc);
				m_spec_bpred->cond_state = ((m_spec_bpred->cond_state << 1) | (taken & 0x1));
				bool predict_hit = (taken && (TAKEN == entry.m_branch_result)) \
								   || ((!taken) && (NOT_TAKEN == entry.m_branch_result));
				if(!predict_hit) {
					inorder_branch_misprediction_count++;
					m_inorder_x86_retire_info.inorder_skip_cycle += finish_cycle - fetch_cycle + INORDER_MISPREDICTION_LATENCY;
					break;
				}
			}
#endif
			//cout << "after retire" << endl;
		}
		inorder_dispatch_inst_count--;
	}
	// Added by minqh 2013.06.20
	// Do not use loop here. If Branch (or other reason) stalls pipeline, we do not process the stall here.
	// [Deprecated] while (inorder_x86_retire_info[phy_proc][proc]->inorder_skip_cycle > 0)
	if(m_inorder_x86_retire_info.inorder_skip_cycle > 0) {
#ifdef JXF_debug
        printf("inst %d retire cycle %d rob_count %d\t",correct_timing_ptr,m_inorder_retire_inst_cycle[correct_timing_ptr],m_inorder_fetch_rob_count);
#endif        
		//cout << "inorder_skip_cycle " << m_inorder_x86_retire_info.inorder_skip_cycle << endl;
		//commit a whole X86 instruction
		//cout << "m_inorder_retire_inst_cycle " << m_inorder_retire_inst_cycle[correct_timing_ptr] << endl;
		//cout << "m_local_cycles " << m_local_cycles << endl;
		//cout << "before while" << endl;
		if(m_inorder_fetch_rob_count >= INORDER_MAX_STALL_INST_NUM) {
			m_inorder_fetch_rob_count = 1;
		}
		//this uop instruction can retire now
		while((m_inorder_retire_inst_cycle[correct_timing_ptr] > 0) &&
				(m_inorder_retire_inst_cycle[correct_timing_ptr] != (uint64_t)-1) &&
				(m_inorder_retire_inst_cycle[correct_timing_ptr] <= m_local_cycles)) {
			assert(correct_timing_ptr < CORRECT_RETIRE_BUF_SIZE);
			m_inorder_retire_inst_cycle[correct_timing_ptr] = (uint64_t)-1;
			uint64_t next_pc = m_next_pc[correct_timing_ptr];
			int next_fu_type = m_next_fu_type[correct_timing_ptr];
			//cout << "correct_timing_ptr " << correct_timing_ptr << endl;
			//cout << "m_next pc " << m_next_pc[correct_timing_ptr] << endl;
			correct_timing_ptr = (correct_timing_ptr + 1) % CORRECT_RETIRE_BUF_SIZE;
#ifdef JXF_debug
            printf("inst %d retire cycle %d\t",correct_timing_ptr,m_inorder_retire_inst_cycle[correct_timing_ptr]);
#endif        
			m_local_instr_count++;
			//printf("local instr count is %d\n", m_local_instr_count);
			//cout << "next pc " << next_pc << " inorder_last_pc " << m_inorder_x86_retire_info.inorder_last_pc << endl;
			//cout << "FU_BRANCH " << FU_BRANCH << " inorder_last_fu_type "
			//<< m_inorder_x86_retire_info.inorder_last_fu_type << endl;

			//this uop instruction shares the same pc with the last one, two from a single x86 instruction
			if((next_pc == m_inorder_x86_retire_info.inorder_last_pc)
					&& (FU_BRANCH != m_inorder_x86_retire_info.inorder_last_fu_type)) {
#ifdef JXF_debug                
                printf("!!not  end\n");
#endif                
				m_inorder_x86_retire_info.inorder_uop_sum++; //multiple uop instructions from a single x86 instruction
			} else {//a new x86 instruction
				m_inorder_fetch_rob_count -= m_inorder_x86_retire_info.inorder_uop_sum;
#ifdef JXF_debug                
                printf("!!inst end\n");
                if ((correct_timing_ptr+m_inorder_fetch_rob_count) % CORRECT_RETIRE_BUF_SIZE != correct_timing_release_ptr)
                    printf("checking timing %d release %d rob %d\n",correct_timing_ptr,correct_timing_release_ptr,m_inorder_fetch_rob_count);
#endif                
				//cout << "decrease m_inorder_fetch_rob_count is " << m_inorder_fetch_rob_count << endl;
				if(m_inorder_fetch_rob_count < 0) {
					m_inorder_fetch_rob_count = 0;
				}
				assert(m_inorder_fetch_rob_count >= 0);
				m_inorder_x86_retire_info.inorder_uop_sum = 1;
			}
            //JXF0316: add latency for mul and div
//		    uint64_t execute_offset = m_local_cycles % INORDER_MAX_ESTIMATE_INST_NUM;
//            if (next_fu_type == FU_INTMULT) {
//                for (int i =1; i< MUL_LATENCY; i++){
//				    execute_offset++;
//    				if(execute_offset >= INORDER_MAX_ESTIMATE_INST_NUM) {
//	    				execute_offset = 0;
//		    		}
//                    m_inorder_execute_inst_width[execute_offset]--;
//        			m_inorder_function_unit_width[execute_offset][next_fu_type]--; //*/
//                }
//            }
//            else if (next_fu_type == FU_INTDIV) {
//                for (int i =0; i< DIV_LATENCY; i++){
//				    execute_offset++;
//    				if(execute_offset >= INORDER_MAX_ESTIMATE_INST_NUM) {
//	    				execute_offset = 0;
//		    		}
//                    m_inorder_execute_inst_width[execute_offset]--;
//        			m_inorder_function_unit_width[execute_offset][next_fu_type]--; //*/
//                }
//            }
			m_inorder_x86_retire_info.inorder_last_pc = next_pc;
			m_inorder_x86_retire_info.inorder_last_fu_type = next_fu_type;
		}
		uint64_t offset = m_local_cycles % INORDER_MAX_ESTIMATE_INST_NUM;
		m_inorder_execute_inst_width[offset] = 0;
		for(int mm = 0; mm < FU_NUM_FU_TYPES; mm++) {
			m_inorder_function_unit_width[offset][mm] = 0;
		}
		m_local_cycles++;
		m_inorder_x86_retire_info.inorder_skip_cycle--;
	}
//	gettimeofday(&s3, 0);
//    t1 += 1000000 * (s2.tv_sec - s1.tv_sec) + s2.tv_usec - s1.tv_usec;
//    t2 += 1000000 * (s3.tv_sec - s2.tv_sec) + s3.tv_usec - s2.tv_usec;
	//#endif
	return fetch_ok;
}

void transformer_inorder_timing() {
	while(true) {
		for(int i = 0; i < NUM_PROCS; i++) {
			//m_seq[i]->m_calculate(NULL, NULL);
			if(zmf_finished == true) {
				break;
			}
		}
		if(zmf_finished == true) {
			break;
		}
	}
}

int transformer_inorder_qemu_function(int first_phy_proc) {
	//int timing_fetch_ptr = correct_timing_fetch_ptr[first_phy_proc][0];
	while(1) {
		int ret = 1;
		//int ret = trifs_trace_cpu_exec(first_phy_proc);
		if(ret == -1) {
			printf(" Step qemu failes!\n");
			assert(0);
		} else if(ret == 1) {
			printf(" Qemu terminates!\n");
			fflush(stdout);
			zmf_finished = true;
			return QEMU_ADVANCE_TERMINATE;
		}
		return 0;
	}
}

int total_max_tb_per_exec;

void * transformer_inorder_qemu_function_thread(void * arg) {
#ifdef PARALLELIZED_TIMING_BY_CORE
	set_cpu(0);
#endif

#ifdef PARALLELIZED_FUNCTIONAL_AND_TIMING
	while(1) {
		if(zmf_finished == true) {
			printf("Functional break out of execution loop. ZMF_FINISHED = true\n");
			break;
		}
		// Check all cores
		for(int i = 0; i < NUM_PROCS; i++) {
			// If instr. buffer is empty, call qemu_function
			int phy_proc = i;
			transformer_inorder_qemu_function(phy_proc);
		}
	}
#else
	//sequential version functional only
	while(1) {
		if(zmf_finished == true) {
			printf("Functional break out of execution loop. ZMF_FINISHED = true\n");
			break;
		}
		// Check all cores
		for(int i = 0; i < NUM_PROCS; i++) {
			// If instr. buffer is empty, call qemu_function
			int phy_proc = i;
			int proc = 0;
			transformer_inorder_qemu_function(phy_proc);
		}
	}
#endif

	return NULL;
}

void * transformer_inorder_timing_thread(void * arg) {
	// For convenience, we use zmf_timing directly.
	transformer_inorder_timing();
	return NULL;
}

void * transformer_inorder_cache_thread(void * arg) {
	printf("Simulating shared cache, pid = %u, thread id = %u\n", (unsigned int)getpid(), (int)pthread_self());

#ifndef PARALLIZED_SHARED_CACHE
	printf(" Init cache thread needs to open PARALLIZED_SHARED_CACHE macro.\n");
	fflush(stdout);
	assert(0);
#endif
	while(1) {
#ifdef PARALLIZED_SHARED_CACHE
		CAT_read_coherence_buffer();
#endif
		if(zmf_finished == true) {
			break;
		}
	}
	printf(" Shared cache thread terminates.\n");
	fflush(stdout);
}

#ifdef PARALLELIZED_TIMING_BY_CORE
void * transformer_inorder_timing_core_thread_simple(void * arg) {
	//printf("start\n");
	int thread_id = ((timing_core_thread_arg *)arg)->core_id;
	int base_core_id = thread_id * trans_core_per_thread;

	set_cpu(thread_id + 1);
	printf("Simulating thread %d, base_core_id=%d, pid = %u, thread id = %u\n", thread_id, base_core_id, (unsigned int)getpid(), (int)pthread_self());
	while(true) {
		//printf("timing start\n");
		//m_seq[base_core_id]->m_calculate(NULL, NULL);
		//printf("timing end\n");
		if(zmf_finished == true) {
			printf("thread: %d , calculation wait num: %ld\n", thread_id, m_seq[base_core_id]->calculation_wait_num);
			break;
		}
	}
	return NULL;
}
#endif
// endif for by-core parallelization

void stepInorder() {
	printf(" Init correct instr buffer ok.\n");
	fflush(stdout);
	pthread_mutex_init(&mutex, NULL);

#ifdef TRANSFORMER_INORDER_CACHE
	init_cache();
#endif

	// Version control
	printf("\nVersion: Calculation-based simulator (inorder).\n");
	printf("----------Configuration---------------\n");

	printf("[CBS] m_numSMTProcs : %d | ", NUM_PROCS);
	printf("QEMU_NUM_REGS : %d \n", QEMU_NUM_REGS);
	printf("FU_NUM_FU_TYPES : %d \n", FU_NUM_FU_TYPES);
	for(int m = 0; m < FU_NUM_FU_TYPES; m++) {
		printf("   FU-%d:  num=%d  latency=%d\n", m, CONFIG_NUM_ALUS[CONFIG_ALU_MAPPING[m]], CONFIG_ALU_LATENCY[m]);
	}
	printf("Iwin_size: %d, max_fetch: %d, max_execute %d", INORDER_MAX_STALL_INST_NUM, MAX_FETCH, MAX_EXECUTE);


	// Version output.
	printf("\n---------------- Version---------------\n");
#ifdef TRANSFORMER_PARALLEL
	printf("-----------Parallel Version---------------\n");
#ifdef PARALLELIZED_FUNCTIONAL_AND_TIMING
	printf("[P] Parallelized Functional and Timing.\n");
#endif

#ifdef PARALLIZED_SHARED_CACHE
	printf("[P] Parallelized Cache Model.\n");
#endif

#ifdef PARALLELIZED_TIMING_BY_CORE
	printf("[P] Parallelized Timing by core.\ncore_per_thread=%d\n", trans_core_per_thread);
#ifdef TRANSFORMER_USE_SLACK
#ifdef TRANSFORMER_USE_STRICT_SYNC
	printf(" SLACK & STRICT_SYNC conflict, exit.\n");
	fflush(stdout);
	assert(0);
#else
	printf("\tSlack version, slack size = %d\n", TRANS_SLACK_SIZE);
#endif
#else
#ifdef TRANSFORMER_USE_STRICT_SYNC
#ifdef TRANSFORMER_USE_SLACK
	printf(" SLACK & STRICT_SYNC conflict, exit.\n");
	fflush(stdout);
	assert(0);
#else
	printf("\tStrict sync version, guaranteed ruby request.");
#endif
#else
	printf("Unbounded core synchronization.\n");
#endif
#endif
#endif
#else
	printf("-----------Sequential Version---------------\n");
#endif

	if(NUM_PROCS == 1) {
		printf("---------Single Core Version-----------\n");
	} else {
		printf("---------Multi Core Version------------\n");
		printf("\t[M] Simulating %d cores.\n", NUM_PROCS);
	}

#ifdef TRANSFORMER_INORDER_BRANCH
	printf("---------Branch Version---------------\n");
	printf("\t[B] branch_squash_latency = %d\n", INORDER_MISPREDICTION_LATENCY);
#else
	printf("---------No Branch Version------------\n");
#endif

#ifdef TRANSFORMER_INORDER_CACHE
	printf("---------Cache Version---------------\n");
#ifdef TRANSFORMER_USE_CACHE_COHERENCE
	printf("--------Coherence Version------------\n");
#ifdef TRANSFORMER_USE_CACHE_COHERENCE_buffer
	printf("\t[C] Coherence using buffer (parallelized).\n");
#endif
#ifdef TRANSFORMER_USE_CACHE_COHERENCE_history
	printf("\t[C] Coherence using history.\n");
#endif
#else
	printf("------No coherence Version------------\n");
#endif

	printf("[C] L1 assoc_bits = %d set_bits = %d block_bits = %d latency: = %d\n", DL1_ASSOC, DL1_SET_BITS, DL1_BLOCK_BITS, INORDER_L1_CACHE_LATENCY);
	printf("[C] L2 assoc_bits = %d set_bits = %d block_bits = %d latency: = %d\n", L2_ASSOC, L2_SET_BITS, L2_BLOCK_BITS, INORDER_L2_CACHE_LATENCY);
	printf("[C] MEM cache latency = %d \n", INORDER_MEMORY_LATENCY);
#else
	printf("---------No Cache Version------------\n");
#endif

	printf("\n\n[Calculation-based simulator] Simulation begins.\n");
	fflush(stdout);
	fflush(stderr);

	// Begin simulation work

	// Added by minqh 2013.05.07 for inorder version
	// Parallel Transformer logic
#ifdef TRANSFORMER_PARALLEL

	int num_sim_threads = NUM_PROCS;

	// Qemu version

	// 1. Initialize parallelized models, like cache ...
#ifdef PARALLIZED_SHARED_CACHE
	// Parallel cache
	pthread_t cache_tid;
	// Start up the cache_thread
	pthread_create(&cache_tid, NULL, transformer_inorder_cache_thread, NULL);//*/
#endif

	// 2. Initialze parallelized functional and timing
#ifdef PARALLELIZED_FUNCTIONAL_AND_TIMING


#ifdef PARALLELIZED_TIMING_BY_CORE
	assert(NUM_PROCS % (trans_core_per_thread) == 0);
	num_sim_threads = NUM_PROCS / (trans_core_per_thread);
	timing_tid = (pthread_t *)malloc(num_sim_threads * sizeof(pthread_t));
	timing_core_thread_arg *timing_arg = (timing_core_thread_arg *)malloc(num_sim_threads * sizeof(timing_core_thread_arg));
	for(int i = 0; i < num_sim_threads; i++) {
		timing_arg[i].core_id = i;
		//pthread_create(&(timing_tid[i]), NULL, transformer_inorder_timing_core_thread_simple, (void *)&(timing_arg[i]));
	}

#else

	//pthread_t timing_tid;
	pthread_create(&timing_tid, NULL, transformer_inorder_timing_thread, NULL);
#endif

	//transformer_inorder_qemu_function_thread(NULL);
#else
	transformer_inorder_timing();
#endif

	return;

	// 3. Simulation finished, check status.
	if(zmf_finished != true) {
		printf("Invalid termination!!!!\n");
		assert(0);
	}

	// 4. Cleanup jobs: Join created threads
#ifdef PARALLELIZED_FUNCTIONAL_AND_TIMING
	// Join timing thread
#ifdef PARALLELIZED_TIMING_BY_CORE
	for(int i = 0; i < num_sim_threads; i++) {
		pthread_join((timing_tid[i]), NULL);
	}
#else
	pthread_join(timing_tid, NULL);
#endif
#endif

#ifdef PARALLIZED_SHARED_CACHE
	// Join cache thread
	pthread_join(cache_tid, NULL);
#endif

#else
	// whole seq version goes here !!!
#ifdef TRANSFORMER_USE_QEMU
	transformer_inorder_timing();
#else
	// Inorder version needs combing QEMU or COREMU!!!
	printf(" [ERROR] Inorder version needs combing QEMU or COREMU!!!\n");
	fflush(stdout);
	assert(0);
#endif
#endif

	assert(zmf_finished == true);
	if(zmf_finished == true) {
#ifdef TRANSFORMER_USE_CACHE_COHERENCE
		CAT_pseudo_dump_coherence_stat();
#endif

		printf("\n[Calculation-based simulator]-Wait_Number: ");
		for(int j = 0; j < NUM_PROCS; j++) {
			printf("\t%ld", m_seq[j]->inorder_wait_num);
		}
		printf("\n[Calculation-based simulator]-Instructions: ");
		uint64_t minqh_total_instr = 0;
		for(int j = 0; j < NUM_PROCS; j++) {
			printf("\t%ld", m_seq[j]->m_local_instr_count);
			minqh_total_instr += m_seq[j]->m_local_instr_count;
		}
		printf("\n[Calculation-based simulator]-Total instructions %ld\n", minqh_total_instr);
		printf("[Calculation-based simulator]-Cycles: ");
		for(int j = 0; j < NUM_PROCS; j++) {
#ifdef PARALLIZED_SHARED_CACHE
			printf("\t%ld", *(m_seq[j]->L2_hit_increase_back));
			printf("\t%ld", (m_seq[j]->m_local_cycles - * (m_seq[j]->L2_hit_increase_back) * INORDER_MEMORY_LATENCY));
#else
			printf("\t%ld", m_seq[j]->m_local_cycles);
#endif
		}
		printf("\n");

#ifdef TRANSFORMER_INORDER_BRANCH
		uint64_t total_inorder_branch_count = 0;
		uint64_t total_inorder_branch_misprediction_count = 0;
		for(int j = 0; j < NUM_PROCS; j++) {
			total_inorder_branch_count += m_seq[j]->inorder_branch_count;
			total_inorder_branch_misprediction_count += m_seq[j]->inorder_branch_misprediction_count;
		}
		printf("[Calculation-based simulator]-Branches: Total = %ld  Mispred = %ld  Mispred-Percentage = %0.2f\n",
			   total_inorder_branch_count, total_inorder_branch_misprediction_count, total_inorder_branch_misprediction_count / (total_inorder_branch_count + 0.0) * 100);
#endif

#ifdef TRANSFORMER_INORDER_CACHE
		uint64_t total_inorder_l1_icache_hit_count = 0;
		uint64_t total_inorder_l1_icache_miss_count = 0;
		uint64_t total_l1_dcache_hit_count = 0;
		uint64_t total_l1_dcache_miss_count = 0;
		uint64_t total_l2_dcache_hit_count = 0;
		uint64_t total_l2_dcache_miss_count = 0;
		for(int j = 0; j < NUM_PROCS; j++) {
			total_inorder_l1_icache_hit_count += m_seq[j]->inorder_l1_icache_hit_count;
			total_inorder_l1_icache_miss_count += m_seq[j]->inorder_l1_icache_miss_count;
			total_l1_dcache_hit_count += m_seq[j]->inorder_l1_dcache_hit_count;
			total_l1_dcache_miss_count += m_seq[j]->inorder_l1_dcache_miss_count;
			total_l2_dcache_hit_count += m_seq[j]->inorder_l2_cache_hit_count;
			total_l2_dcache_miss_count += m_seq[j]->inorder_l2_cache_miss_count;
#ifdef PARALLIZED_SHARED_CACHE
			total_l2_dcache_hit_count += *(m_seq[j]->L2_hit_increase_back);
			total_l2_dcache_miss_count -= *(m_seq[j]->L2_hit_increase_back);
#endif
		}
		printf("[Calculation-based simulator]-Caches: L1-I hit = %ld miss = %ld L1-D hit = %ld miss = %ld L2 hit = %ld miss = %ld \n",
			   total_inorder_l1_icache_hit_count, total_inorder_l1_icache_miss_count, total_l1_dcache_hit_count, total_l1_dcache_miss_count,
			   total_l2_dcache_hit_count, total_l2_dcache_miss_count);
#endif


		printf("[Calculation-based simulator] Simulation terminates.\n");
		printf("[Calculation-based simulator] Goodbye.\n");

		fflush(stdout);
		fflush(stderr);

		return;
	}
}

void do_cal(int core_num, instr_info_buffer_t* entries, uint64_t* queue, int length) {
	assert(core_num >= 0 );
	//assert(m_seq[core_num]->m_inorder_fetch_rob_count == 0);
	//cout << "entires size is " << length << endl;
	//cout << "m_inorder_fetch_rob_count is " << m_seq[core_num]->m_inorder_fetch_rob_count << endl;
	unsigned int index = 0;

		while(true) {
#ifdef ACCURACY_TEST
			printf("outer loop\n");
#endif
	        bool exit_flag = m_seq[core_num]->m_calculate(entries, queue, index, length);
			if(!exit_flag) {
				break;
			}
		}
}

//void* cal_buffer(void* vargp){
//    int tid = (long int)vargp;
//    
//    //pthread_detach(pthread_self());
//    int signo;
//    printf("thread start %d\n", tid);
//    while(tid != 0){
//        //sigwait(&sigmask,&signo);
//        if (m_seq[tid]->queue_in != -1){
//            while (m_seq[tid]->queue_out %QUEUE_SIZE != m_seq[tid]->queue_in ){
//                int out = m_seq[tid]->queue_out;
//                instr_info_buffer_t* entries = m_seq[tid]->entry_queue[out];
//                int length = m_seq[tid]->size_queue[out]; 
////                do_cal(tid, *entry_queue[tid][queue_out[tid]]);
////                if (i % 2 = 0dd)
//                do_cal(tid, entries, length);
//                free(entries);
//                //cout <<"after size "<<  entry_queue[tid][queue_out[tid]]<<endl;
//                m_seq[tid]->queue_out = (out+1) % QUEUE_SIZE;
//            }
//
//            
//	//PIN_SetThreadData(buf_key, thread_data, threadid);
//            //printf("%d %d\n",queue_out[tid], queue_in[tid]);
//        }else 
//            return NULL;
//    }
//}
//
//void send_sig(int core_num, instr_info_buffer_t *entries, int length, int* size_count){
//    //cout <<"before "<< entries->size() << endl;
//    //if ((queue_out[core_num ]-1) %queue_size == queue_in[core_num] )
//    //    cout<<" queue full "<<core_num<<endl;
//    int in = m_seq[core_num]->queue_in;
////    while ((m_seq[core_num]->queue_out-1) % QUEUE_SIZE == in )
////        sleep(1);
//    if ((m_seq[core_num]->queue_out-1) % QUEUE_SIZE == in ){
//        free(entries);
//        (*size_count)++;
//        printf("omit in %d\n", core_num);
//        return;
//    }
//    m_seq[core_num]->entry_queue[in] = entries;
//    m_seq[core_num]->size_queue[in] = length;
//    m_seq[core_num]->queue_in = (in +1) % QUEUE_SIZE;
//    //cout <<"af "<< entries->size() << endl;
//    //printf("send %d :%d %d\n",core_num, queue_out[core_num],queue_in[core_num]);
//    //pthread_kill(pid, SIGUSER1);
//}

void* listen_msg(void* vargp){
    int tid = (long int)vargp;
    int epd;
    int newepd;
    int err = 0;
    int req_pn = 3000+tid;
    int con_pn;
    int backlog = 16;
    struct scif_portID portID;
    int block = 1;
    instr_info_buffer_t* recvdata;
    portID.node = 2;
    portID.port = 3000+tid;
    if ((epd = scif_open()) < 0){
        printf("scif_open fialed with error %d in %d\n", errno, tid);
        return NULL;
    }
    printf("scif_bind to port %d\n",req_pn);
    if ((con_pn = scif_bind(epd, req_pn)) < 0){
        printf("scif_bind failed with erro %d in %d\n", errno, tid);
        return NULL;
    }
    printf("scif_listen with backlog of 16\n");
    if ((scif_listen(epd, backlog)) < 0){
        printf("scif_listen faield with error %d in %d\n", errno, tid);
        return NULL;
    }
    
    int pagesize = sysconf(_SC_PAGESIZE);
    // len = (1000*48) + (12 * 1000)*50 + payload = 4096 * 160 
    int len = THRESHOLD*QUEUE_SIZE*(sizeof(uint64_t)*2) + NEW_ENTRY*sizeof(instr_info_buffer_t);
    len = (len / pagesize+1) * pagesize;
    printf("len %d page %d instr %d int %d\n", len, pagesize,sizeof(instr_info_buffer_t), sizeof(int));
    int ret = posix_memalign((void**) &recvdata, pagesize, len);
    if (ret) {
        printf("could not allocate\n");
        return NULL;
    }

    printf("scif_accept in syncronous model\n");
    if (((scif_accept(epd, &portID, &newepd, SCIF_ACCEPT_SYNC))<0) && (errno != EAGAIN)){
        printf("scif_accept failed with errno %d in %d\n", errno, tid);
        return NULL;
    }
    if (SCIF_REGISTER_FAILED == scif_register (newepd, (void *)recvdata, len, (long)recvdata, SCIF_PROT_READ | SCIF_PROT_WRITE, SCIF_MAP_FIXED)){
        printf("scif register failed with err %d in %d\n", errno, tid);
    }
    scif_send(newepd, &recvdata, sizeof(void*), SCIF_SEND_BLOCK);
    printf("scif_accept complete\n");
    printf("node = %d, port = %d\n", portID.node, portID.port);
    int* skip_count = (int *)calloc(1, sizeof(int));

    /* *p signal stand for the latest queue that the host already sent 
     * MIC will read from it */
    uint64_t volatile *p = (uint64_t volatile *)recvdata;
    *p = QUEUE_SIZE-1;

    /* *cur signal stand for the latest queue that calculation already processed 
     * MIC will write to it */
    uint64_t volatile *cur = (uint64_t volatile *)recvdata + 1;
    *cur = QUEUE_SIZE-1;

    uint64_t* addr_queue = (uint64_t*)recvdata + 2;
    instr_info_buffer_t* entries= (instr_info_buffer_t*)((char*)recvdata + sizeof(uint64_t)*2*THRESHOLD*QUEUE_SIZE+16);
    /* the current queue that MIC processes, *cur will sync with this*/
    int current_p = QUEUE_SIZE-1;
    int ini = 1;
    double timeuse = 0.0;
    while (1){
        struct timeval starttime, endtime; 
//		gettimeofday(&starttime, 0);
        while (*p == current_p) usleep(10);
//		gettimeofday(&endtime, 0);
        if (*p == 0xFFFF || *p >= QUEUE_SIZE)
            goto close;
//        if (ini == 0){
//	        timeuse += 1000000 * (endtime.tv_sec - starttime.tv_sec) + endtime.tv_usec - starttime.tv_usec;

//        }
        //cout << "queue end  "<<*p<<endl;
        current_p = (current_p+1) % QUEUE_SIZE;
        err = 0;
        uint64_t* current_queue = addr_queue+ THRESHOLD*2* current_p;
        uint64_t length = *current_queue; 
        //cout << "length "<<length<<" "<<current_p<<endl;
        do_cal(tid, entries, current_queue+1, length);
        *cur = current_p;
//        ini = 0;
        //free(copydata);
        
        err = 0;
    }
close:

    free(skip_count);
    fflush(stdout);
    m_seq[tid]->queue_in = -1;
    scif_close(newepd);
    fflush(stdout);
    if (err)
        printf("Test failed\n");
//    printf("time %.2f in thread %d\n", timeuse, tid);
    scif_close(epd);
    return NULL;

}


void generateServer(int core_num){
//    cpu_set_t mask;
//    cpu_set_t get;
    pthread_t* pids = (pthread_t*)malloc(sizeof(pthread_t)*(core_num+1));
//    pthread_t* pids_cal = (pthread_t*)malloc(sizeof(pthread_t)*(core_num+1));
    int err;
    for (int i = 1; i< core_num; i++){
//        CPU_ZERO(&mask);
//        CPU_SET(i%60, &mask);
        err = pthread_create(pids+i, NULL, listen_msg, (void*)i);
//        err = pthread_setaffinity_np(pids[i], sizeof(mask), &mask);
//        err = pthread_create(pids_cal+i,NULL, cal_buffer, (void*)i);
    }
    for (int i = 1; i< core_num; i++){
        pthread_join(pids[i], NULL);
//        pthread_join(pids_cal[i], NULL);
    }
    return;
}

int main(int argc, char **argv) {
    if (argc != 2){
        exit(1);
    }
    int core_num = atoi(argv[1]) +1;
	printf("ZMF_init: cpu_count=%d\n", core_num);
	m_seq = (pseq_t **)malloc(core_num * sizeof(pseq_t *));
        
    int global_cpu_count = core_num;

	for(int i = 0; i < core_num; i++) {
		m_seq[i] = new pseq_t(i);
		printf("pseq_t addr is %p, m_id is %d\n", m_seq[i], m_seq[i]->m_id);
        printf("%d\n", sizeof(pseq_t));
	}
	stepInorder();
    generateServer(core_num);
    print_result(core_num);
    return 0;
}

void calculation_hello() {
	std::cout << "inside calculation" << std::endl;
}

void print_result(int core_num) {
#ifdef TRANSFORMER_USE_CACHE_COHERENCE
	CAT_pseudo_dump_coherence_stat();
#endif
//	printf("\n[Calculation-based simulator]-Process Time: \n");
//	for(int j = 0; j < core_num; j++) {
//		printf("%.2f %.2f %.2f\n", m_seq[j]->t1, m_seq[j]->t2, m_seq[j]->t3);
//	}
	printf("\n[Calculation-based simulator]-Wait_Number: ");
	for(int j = 0; j < core_num; j++) {
		printf("\t%ld", m_seq[j]->inorder_wait_num);
	}
	printf("\n[Calculation-based simulator]-Instructions: ");
	uint64_t minqh_total_instr = 0;
	for(int j = 0; j < core_num; j++) {
		printf("\t%ld", m_seq[j]->m_local_instr_count);
		minqh_total_instr += m_seq[j]->m_local_instr_count;
	}
	printf("\n[Calculation-based simulator]-Total instructions %ld\n", minqh_total_instr);

    printf("[Calculation-based simulator]-Cycles: ");
    uint64_t jxf_total_cycle = 0;
	for(int j = 0; j < core_num; j++) {
#ifdef PARALLIZED_SHARED_CACHE
		printf("\t%ld", *(m_seq[j]->L2_hit_increase_back));
		printf("\t%ld", (m_seq[j]->m_local_cycles - * (m_seq[j]->L2_hit_increase_back) * INORDER_MEMORY_LATENCY));
#else
        jxf_total_cycle += m_seq[j]->m_local_cycles;
		printf("\t%ld", m_seq[j]->m_local_cycles);
#endif
	}
	printf("\n[Calculation-based simulator]-Total cycles %ld\n", jxf_total_cycle);

#ifdef TRANSFORMER_INORDER_BRANCH
	uint64_t total_inorder_branch_count = 0;
	uint64_t total_inorder_branch_misprediction_count = 0;
	for(int j = 0; j < core_num; j++) {
		total_inorder_branch_count += m_seq[j]->inorder_branch_count;
		total_inorder_branch_misprediction_count += m_seq[j]->inorder_branch_misprediction_count;
	}
	printf("[Calculation-based simulator]-Branches: Total %ld  Mispred %ld  Mispred-Percentage %0.2f\n",
		   total_inorder_branch_count, total_inorder_branch_misprediction_count, total_inorder_branch_misprediction_count / (total_inorder_branch_count + 0.0) * 100);
#endif

#ifdef TRANSFORMER_INORDER_CACHE
	uint64_t total_inorder_l1_icache_hit_count = 0;
	uint64_t total_inorder_l1_icache_miss_count = 0;
	uint64_t total_l1_dcache_hit_count = 0;
	uint64_t total_l1_dcache_miss_count = 0;
	uint64_t total_l2_dcache_hit_count = 0;
	uint64_t total_l2_dcache_miss_count = 0;
	for(int j = 0; j < core_num; j++) {
		total_inorder_l1_icache_hit_count += m_seq[j]->inorder_l1_icache_hit_count;
		total_inorder_l1_icache_miss_count += m_seq[j]->inorder_l1_icache_miss_count;
		total_l1_dcache_hit_count += m_seq[j]->inorder_l1_dcache_hit_count;
		total_l1_dcache_miss_count += m_seq[j]->inorder_l1_dcache_miss_count;
		total_l2_dcache_hit_count += m_seq[j]->inorder_l2_cache_hit_count;
		total_l2_dcache_miss_count += m_seq[j]->inorder_l2_cache_miss_count;
#ifdef PARALLIZED_SHARED_CACHE
		total_l2_dcache_hit_count += *(m_seq[j]->L2_hit_increase_back);
		total_l2_dcache_miss_count -= *(m_seq[j]->L2_hit_increase_back);
#endif
	}
	printf("[Calculation-based simulator]-Caches: L1-I hit %ld miss %ld L1-D hit %ld miss %ld L2 hit %ld miss %ld \n",
		   total_inorder_l1_icache_hit_count, total_inorder_l1_icache_miss_count, total_l1_dcache_hit_count, total_l1_dcache_miss_count,
		   total_l2_dcache_hit_count, total_l2_dcache_miss_count);
#endif


	printf("[Calculation-based simulator] Simulation terminates.\n");
	printf("[Calculation-based simulator] Goodbye.Pin JXF!!!\n");

	fflush(stdout);
	fflush(stderr);

	return;
}
