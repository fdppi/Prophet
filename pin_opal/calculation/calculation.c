/*------------------------------------------------------------------------*/
/* Includes                                                               */
/*------------------------------------------------------------------------*/
#include "calculation.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sched.h>
#include <iostream>
#include <time.h>
#include <sys/time.h>

//easton added
extern bool am_i_in_magicbreak;
long ins_count_between_magic;

uint64_t total_cache =0;
uint64_t big_inst = 0;
uint64_t origin_cache = 0;
uint64_t* tb_count = 0;
uint64_t hash_count = 0;
uint64_t* new_count = 0;
uint64_t* duplicate_count = 0;

int sample_num = 5;
int global_cpu_count;
int qemu_instr_buffer_size;

int base_cycle[NUM_PROCS];
int block_retire_ptr[NUM_PROCS];
    double* search_time;
    double* old_time;
    double* new_time;
#ifdef JXF_BLOCK
    tb_info_struct_t** current_tb; //
    bool* in_dup;
    int* rm_block;
//    tb_info_struct_t **tb_info_list;
     tb_info_struct_t *tb_info_header;
    int is_a_newtb = 0;
tb_info_struct_t* tb_buffer[TB_CACHE_SIZE];
#endif

typedef struct _gather_tb{
    vector<instr_info_buffer_t> *entry;
    uint64_t tb_addr;
} gather_tb_t;
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
	1, // integer divisor
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
	32, // FU_INTDIV,               // integer divider
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

extern  x86_instr_info_buffer_t **trifs_trace_shared_buf_head(int core_num);
extern  x86_instr_info_buffer_t **trifs_trace_shared_tb_inst_buf_head(int core_num);
extern  x86_mem_info_buffer_t **trifs_mem_shared_buf_head(int core_num);
extern  instr_info_buffer_flag_t *get_minqh_flag_buf_head(int core_num);

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

    search_count = 0;
    block_nu = 0;
    BBV = (uint64_t*)calloc(BBV_DIM,sizeof(uint64_t));
    former_BBV = (uint64_t*)calloc(BBV_DIM,sizeof(uint64_t));
    former_dup = false;
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
	pseq_trifs_trace_uops_buf = trifs_trace_uops_global_buf;
	pseq_trifs_trace_uops_ptr = trifs_trace_uops_global_buf;

	trifs_trace_mem_buf = trifs_trace_mem_global_buf;
	trifs_trace_mem_ptr = trifs_trace_mem_global_buf;

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

	for(unsigned int i = 0; i < CAT_assoc; i++) {
		if(set->cache_line_list[i].flag == CAT_CACHE_FULL && set->cache_line_list[i].block_addr == ba) {
//            printf("find set i %d %lx\n",i,address);
			for(unsigned int j = i; j > 0; j--) {
				set->cache_line_list[j].block_addr = set->cache_line_list[j - 1].block_addr;
				set->cache_line_list[j].flag = set->cache_line_list[j - 1].flag;
			}
			set->cache_line_list[0].block_addr = ba;
			set->cache_line_list[0].flag = CAT_CACHE_FULL;
			return false;
		}
	}
#ifdef JXF_debug
    printf("L1 full!!!\n");
#endif
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

	for(unsigned int i = 0; i < CAT_L2_assoc; i++) {
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

#ifdef JXF_debug
    printf("cycle %d instruction %d\n",m_local_cycles,m_local_fetch_count);
    printf("L2 full!!!\n");
#endif
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
	for(unsigned int i = 0 ; i < CAT_L2_assoc ; i ++) {
		// if it is found in the cache ...
		if((set->cache_line_list[i].flag == CAT_CACHE_FULL) && BlockAddress(set->cache_line_list[i].block_addr) == ba) {
			cachehit = true;
			break;
		}
	}
	return cachehit;
}
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

bool pseq_t::trifs_trace_translate_x86_inst_reset() {
	memset(&(trifs_trace_uops_buf[0]), 0, 512 * sizeof(struct TransOp));
	trifs_trace_num_uops = 0;
	return true;
}

#ifdef JXF_BLOCK
int pseq_t::find_duplicate(uint64_t tb_addr,tb_info_struct_t **tb_info){
    uint64_t mask = (1<<TB_CACHE_BITS)-1;
    int off = (int)(tb_addr & mask);
    tb_info_struct_t *finder = tb_buffer[off];
    if (tb_buffer[off] == NULL){
        return 0;
    }
    while (finder->next != NULL){
        search_count++;
        int c=0;
        uint64_t s1 =0,s2=0;
        if (finder->tb_addr == tb_addr){
            for (int i =0; i<BBV_DIM;i++){
                int t = (BBV[i]-finder->BBV[i]);
                s1 += t*t;
                t = former_BBV[i]-finder->former_BBV[i];
                s2 += t*t;
            }
            if (s1 < MAX_MISTAKE && s2 < MAX_MISTAKE){
                *tb_info = finder;
                return 1;
            }
        }            
        finder = finder->next;
    }
    return 0;
}

void pseq_t::count_duplicate(tb_info_struct_t *tb_info){
//	m_inorder_fetch_rob_count -= m_inorder_x86_retire_info.inorder_uop_sum;
    m_inorder_x86_retire_info. inorder_uop_sum = 1;
    inorder_wait_num += tb_info->inorder_wait_num;
    m_local_instr_count += tb_info->m_local_instr_count;
    m_local_fetch_count += tb_info->m_local_fetch_count;
    inorder_l1_dcache_hit_count += tb_info-> inorder_l1_dcache_hit_count;
    inorder_l1_dcache_miss_count += tb_info-> inorder_l1_dcache_miss_count;
    inorder_l2_cache_hit_count += tb_info-> inorder_l2_cache_hit_count;
    inorder_l2_cache_miss_count += tb_info-> inorder_l2_cache_miss_count;
//    m_local_size_count += tb_info->m_local_size_count;
//    printf("m_local_size %d\n",tb_info->tb_addr);
//    m_inorder_fetch_rob_count += tb_info-> m_inorder_fetch_rob_count;
    inorder_branch_count += tb_info->inorder_branch_count;
    inorder_branch_misprediction_count += tb_info->inorder_branch_misprediction_count;
    for (int i = 0; i< tb_info->correct_timing_release_ptr;i++){
        m_inorder_retire_inst_cycle[correct_timing_release_ptr] = m_local_cycles+tb_info->m_block_retire_buf[i%CORRECT_RETIRE_BUF_SIZE];
        m_next_pc[correct_timing_release_ptr] = tb_info->m_next[i%CORRECT_RETIRE_BUF_SIZE];
		correct_timing_release_ptr = (correct_timing_release_ptr + 1) % CORRECT_RETIRE_BUF_SIZE;

    }
    int timing2 = (correct_timing_ptr+tb_info-> correct_timing_ptr)% CORRECT_RETIRE_BUF_SIZE;
    //judge if retiring too much inst
    if (correct_timing_ptr <= correct_timing_release_ptr && timing2 > correct_timing_release_ptr)
        correct_timing_ptr = correct_timing_release_ptr;
    else correct_timing_ptr = timing2;
    //judge if retirng too less inst
    int l = correct_timing_release_ptr-correct_timing_ptr;
    if (l<0) l = correct_timing_release_ptr+ CORRECT_RETIRE_BUF_SIZE -correct_timing_ptr;
    if (l>INORDER_MAX_STALL_INST_NUM ) {
        l = INORDER_MAX_STALL_INST_NUM;
        correct_timing_ptr = (correct_timing_release_ptr + CORRECT_RETIRE_BUF_SIZE-INORDER_MAX_STALL_INST_NUM)%CORRECT_RETIRE_BUF_SIZE;
    }
    m_inorder_fetch_rob_count = l;
//    while (k < tb_info->m_local_fetch_count){
    m_local_cycles += tb_info->m_local_cycles;

}
#endif


bool pseq_t::TRANS_inorder_simpleLookupInstruction(instr_info_buffer_t * entry) {
	vector<instr_info_buffer_t> *uops;
	if(uops_index <= 0) {
		while(minqh_flag_buffer[minqh_flag_buffer_index].flag == 0) {
			//cout << "buffer flag is 0" << endl;
			return false;
			if(zmf_finished == true) {
				printf("Return inside waiting loop.\n");
				return false;
			}
		}
		/*if(m_id == 1) {*/
		/*}*/
		x86_tb_instr_buffer_t *x86_tb_info_entry =
			&(correct_qemu_tb_instr_buf[minqh_flag_buffer_index][correct_tb_fetch_ptr]);
		uint64_t tb_addr = x86_tb_info_entry->tb_addr;
		int tb_fetch_inst_num = x86_tb_info_entry->inst_num;
		x86_eip = x86_tb_info_entry->x86_eip;

		if(tb_fetch_inst_num) {
			assert(bb_cache_map.count(tb_addr) == 0);
			//printf("tb_fetch_inst_num is %d\n", tb_fetch_inst_num);
			fflush(stdout);
			temp_cache = new code_cache();
			cur_uop = new instr_info_buffer_t();
			temp_cache->tb_mem_num = 0;
			while(tb_fetch_inst_num) {
				x86_instr_info_buffer_t *x86_info_buf_entry =
					&(correct_qemu_instr_info_buf[minqh_flag_buffer_index][correct_timing_fetch_ptr]);

				if(x86_info_buf_entry->tb_mem_num) {
					temp_cache->tb_mem_num = x86_info_buf_entry->tb_mem_num;
				}
				unsigned char trifs_trace_x86_buf[MAX_TRIFS_TRACE_X86_BUF_SIZE];
				memcpy((void *)&trifs_trace_x86_buf, (void *) & (x86_info_buf_entry->x86_inst), MAX_TRIFS_TRACE_X86_BUF_SIZE);
				uint64_t virt_pc = x86_info_buf_entry->x86_virt_pc;
				trifs_trace_translate_x86_inst_reset();

				pthread_mutex_lock(&mutex);
				trifs_trace_ptlsim_decoder(
					trifs_trace_x86_buf,  16, //trifs_trace_num_bytes
					trifs_trace_uops_buf, &trifs_trace_num_uops,
					virt_pc, true);
				pthread_mutex_unlock(&mutex);

				for(int j = 0; j < trifs_trace_num_uops; j++) {
					trifs_trace_translate_uops(&trifs_trace_uops_buf[j], cur_uop, virt_pc);
					(temp_cache->uops).push_back(*cur_uop);
				}
				tb_fetch_inst_num--;
				correct_timing_fetch_ptr++;
			}

			bb_cache_map[tb_addr] = temp_cache;
			uops = &(temp_cache->uops);
			tb_num_uops = uops->size();
			uops_index = tb_num_uops;
			delete cur_uop;
		} else {
			assert(bb_cache_map.count(tb_addr) == 1);
			int flag = bb_cache_map.count(tb_addr);
			if(!flag) {
				//printf("0x%x not in cache\n", tb_addr);
				correct_tb_fetch_ptr++;
				if(correct_tb_fetch_ptr == MAX_TRIFS_TRACE_BUF_SIZE) {
					//cout << "set buffer flag to 0" << endl;
					minqh_flag_buffer[minqh_flag_buffer_index].flag = 0;
					correct_tb_fetch_ptr = 0;
					correct_timing_fetch_ptr = 0;
					correct_mem_fetch_ptr = 0;
					minqh_flag_buffer_index = (minqh_flag_buffer_index + 1) % MAX_TRIFS_TRACE_BUF_BLOCK_SIZE;
				}
				return false;
			}

			temp_cache = bb_cache_map[tb_addr];
			uops = &(temp_cache->uops);
			tb_num_uops = uops->size();
			uops_index = tb_num_uops;
		}

		if(temp_cache && temp_cache->tb_mem_num) {
			//printf("mem num is %d\n", temp_cache->tb_mem_num);
			tb_mem_num = temp_cache->tb_mem_num;
			trifs_trace_mem_ptr = trifs_trace_mem_buf;
			trifs_trace_mem_uop_fetch = trifs_trace_mem_buf;
			same_logical_pc = 0;
			same_access_size = 0;
			same_physical_addr = 0;
		} else {
			tb_mem_num = 0;
		}

		while(tb_mem_num) {
			x86_mem_info_buffer_t *x86_mem_info_buf_entry = &(correct_qemu_mem_info_buf[minqh_flag_buffer_index][correct_mem_fetch_ptr]);
			trifs_trace_mem_ptr->logical_pc = x86_mem_info_buf_entry->logical_pc;
			trifs_trace_mem_ptr->m_physical_addr = x86_mem_info_buf_entry->m_physical_addr & 0x1fffffff;

			trifs_trace_mem_ptr++;
			tb_mem_num--;
			correct_mem_fetch_ptr++;
		}
		correct_tb_fetch_ptr++;

		if(correct_tb_fetch_ptr == MAX_TRIFS_TRACE_BUF_SIZE) {
			//cout << "set buffer flag to 0" << endl;
			minqh_flag_buffer[minqh_flag_buffer_index].flag = 0;
			correct_tb_fetch_ptr = 0;
			correct_timing_fetch_ptr = 0;
			correct_mem_fetch_ptr = 0;
			minqh_flag_buffer_index = (minqh_flag_buffer_index + 1) % MAX_TRIFS_TRACE_BUF_BLOCK_SIZE;
		}
	}

	if(tb_num_uops <= 0) {
		//cout << "tb_num_uops is " << tb_num_uops << endl;
		return false;
	}
	int cache_index = tb_num_uops - uops_index;
	entry->logical_pc = (temp_cache->uops)[cache_index].logical_pc;
	entry->m_futype = (temp_cache->uops)[cache_index].m_futype;

	//printf("tb_mem_num is %d\n", temp_cache->tb_mem_num);
	if(temp_cache->tb_mem_num > 0 && ((entry->m_futype == FU_RDPORT) || (entry->m_futype == FU_WRPORT))) {
		if(entry->logical_pc == same_logical_pc) {
			entry->m_physical_addr = same_physical_addr;
		} else {
			//fflush(stdout);
			while(trifs_trace_mem_uop_fetch->logical_pc == same_logical_pc) {
				trifs_trace_mem_uop_fetch++;
			}
			entry->m_physical_addr = trifs_trace_mem_uop_fetch->m_physical_addr;
			trifs_trace_mem_uop_fetch++;
		}
		same_logical_pc = entry->logical_pc;
		same_physical_addr = entry->m_physical_addr; //*/
	}

#ifdef TRANSFORMER_MISPREDICTION
	if(entry->m_futype == FU_BRANCH) {
		entry->m_branch_result = NO_BRANCH;
		if(x86_eip == (temp_cache->uops)[cache_index].branch_taken_npc_pc) {
			entry->m_branch_result = TAKEN;
		}
		if(x86_eip == (temp_cache->uops)[cache_index].branch_untaken_npc_pc) {
			entry->m_branch_result = NOT_TAKEN;
		}
	}
#endif
	for(int i = 0; i < QEMU_SI_MAX_SOURCE; i++) {
		entry->m_source_reg[i] = ((temp_cache->uops)[cache_index].m_source_reg[i]) % 16 + 8;
	}
	for(int i = 0; i < QEMU_SI_MAX_DEST; i++) {
		entry->m_dest_reg[i] = ((temp_cache->uops)[cache_index].m_dest_reg[i]) % 16 + 8;
	}
	m_next_pc[correct_timing_release_ptr] = entry->logical_pc;
	//cout << "m_next pc is " << entry->logical_pc << endl;
	m_next_fu_type[correct_timing_release_ptr] = entry->m_futype;
	correct_timing_release_ptr = (correct_timing_release_ptr + 1) % CORRECT_RETIRE_BUF_SIZE;
	uops_index--;
	return true;
}

bool pseq_t::m_calculate(vector< instr_info_buffer_t > &entries, unsigned int& entry_index) {
	int phy_proc = m_id;
	if(phy_proc < 0 || phy_proc >= NUM_PROCS) {
		printf("core is %d\tpseq_t addr is %p\n", phy_proc, this);
	}
	assert(phy_proc >= 0 && phy_proc < NUM_PROCS);

	// If pipeline is stalled, we do not fetch and just return

//        printf("here\n");
		if(m_inorder_x86_retire_info.inorder_skip_cycle > 0) {
			for(int tt = 0; tt < m_inorder_x86_retire_info.inorder_skip_cycle; tt++) {
				uint64_t cycle_offset = m_local_cycles % INORDER_MAX_ESTIMATE_INST_NUM;
				m_inorder_execute_inst_width[cycle_offset] = 0;
				for(int mm = 0; mm < FU_NUM_FU_TYPES; mm++) {
					m_inorder_function_unit_width[cycle_offset][mm] = 0;
				}
#ifdef JXF_BLOCK
                tb_info_temp->m_local_cycles++;
#endif                
				m_local_cycles++;
			}
		} //*/
	//if(fetch_return_ready){
	//	inorder_dispatch_inst_count = dispatch_reserve;
	int inorder_dispatch_inst_count = MAX_FETCH; //pipeline width: process MAX_FETCH instrctions simultaneously
	bool fetch_ok = true;
	m_inorder_x86_retire_info.inorder_skip_cycle = 1;
	while(inorder_dispatch_inst_count > 0) { //handle each instruction
		if(m_inorder_fetch_rob_count < INORDER_MAX_STALL_INST_NUM) { //instruction window size
			// fetch
			instr_info_buffer_t entry;
			//pthread_mutex_lock(&mutex);
			if(entry_index < entries.size()) {
				assert(entry_index < entries.size());
				fetch_ok = true;
				entry = entries[entry_index++]; //fetch an instrction
				assert(correct_timing_release_ptr >= 0 && correct_timing_release_ptr < CORRECT_RETIRE_BUF_SIZE);
				m_next_pc[correct_timing_release_ptr] = entry.logical_pc;
//				cout << "correct_timing_release_ptr" << correct_timing_release_ptr <<
//				" m_next pc is " << m_next_pc[correct_timing_release_ptr] << endl;
				m_next_fu_type[correct_timing_release_ptr] = entry.m_futype;
				correct_timing_release_ptr = (correct_timing_release_ptr + 1) % CORRECT_RETIRE_BUF_SIZE;
				assert(correct_timing_release_ptr >= 0 && correct_timing_release_ptr < CORRECT_RETIRE_BUF_SIZE);
                m_local_fetch_count++;
#ifdef JXF_BLOCK
                    tb_info_temp->m_local_fetch_count++;
#endif            
				//continue;
			} else {
				fetch_ok = false;
			}
			if(fetch_ok == false) {
				//cout << "fetch ok is false" << endl;
				inorder_wait_num++;
				break;
			}
			//continue;
			/*if (fetch_ok == false){
				inorder_wait_num++;
				break;
			} //*/
			//assert(entry.);
			int fu_type = entry.m_futype;
#ifdef ANASIM_DEBUG
			assert(fu_type < 13);
			assert(fu_type >= 0);
#endif
			int fu_mapped = CONFIG_ALU_MAPPING[fu_type];
			m_inorder_fetch_rob_count++;
#ifdef JXF_BLOCK
            tb_info_temp->m_inorder_fetch_rob_count++;
#endif            
			uint64_t fetch_cycle = m_local_cycles;

			// schedule
			uint64_t max_value = fetch_cycle;

			for(int i = 0; i < QEMU_SI_MAX_SOURCE; i++) {
#ifdef ANASIM_DEBUG
				assert(entry.m_source_reg[i] > 0);
				assert(entry.m_source_reg[i] < QEMU_NUM_REGS);
#endif
				if(entry.m_source_reg[i] != 0) {
					uint64_t finish_cycle = m_inorder_register_finish_cycle[entry.m_source_reg[i]] + 1;
					if(max_value < finish_cycle) {
						max_value = finish_cycle;
					}
				}
			}
			// WHJ lsq
			uint64_t issue_cycle = max_value;
			// Execute stage
			// We do two things.
			// 1. If current cycle has executed 4 instructions, we execute this instr. in next cycle.
			uint64_t execute_offset = issue_cycle % INORDER_MAX_ESTIMATE_INST_NUM;
			while(m_inorder_execute_inst_width[execute_offset] >= MAX_EXECUTE) {
//                printf("loop debug inorder:%d %d\n",execute_offset,m_inorder_execute_inst_width[execute_offset]);
				execute_offset++;
				if(execute_offset >= INORDER_MAX_ESTIMATE_INST_NUM) {
					execute_offset = 0;
				}
#ifdef ANASIM_DEBUG
				assert(execute_offset != (issue_cycle % INORDER_MAX_ESTIMATE_INST_NUM));
#endif
			}
			// 2. If FunctionUnit is used in current cycle, we execute this instr. in next cycle.
			while(m_inorder_function_unit_width[execute_offset][fu_mapped] >= CONFIG_NUM_ALUS[fu_mapped]) {
//                printf("loop debug 2\n");
				execute_offset++;
				if(execute_offset >= INORDER_MAX_ESTIMATE_INST_NUM) {
					execute_offset = 0;
				}
#ifdef ANASIM_DEBUG
				assert(execute_offset != (issue_cycle % INORDER_MAX_ESTIMATE_INST_NUM));
#endif
			}
#ifdef ANASIM_DEBUG
			assert(fu_mapped < 9);
			assert(fu_mapped >= 0);
#endif
            m_inorder_execute_inst_width[execute_offset]++;
			m_inorder_function_unit_width[execute_offset][fu_mapped]++; //*/
			//m_inorder_execute_inst_width[execute_offset]++;
            //JXF0316: add latency for mul and div
//            if (fu_mapped == FU_INTMULT) {
//                for (int i =1; i< MUL_LATENCY; i++){
//				    execute_offset++;
//    				if(execute_offset >= INORDER_MAX_ESTIMATE_INST_NUM) {
//	    				execute_offset = 0;
//		    		}
//                    m_inorder_execute_inst_width[execute_offset]++;
//        			m_inorder_function_unit_width[execute_offset][fu_mapped]++; //*/
//                }
//            }else if (fu_mapped == FU_INTDIV) {
//                for (int i =0; i< DIV_LATENCY; i++){
//				    execute_offset++;
//    				if(execute_offset >= INORDER_MAX_ESTIMATE_INST_NUM) {
//	    				execute_offset = 0;
//		    		}
//                    m_inorder_execute_inst_width[execute_offset]++;
//        			m_inorder_function_unit_width[execute_offset][fu_mapped]++; //*/
//                }
//            }

			uint64_t execute_cycle = issue_cycle + (execute_offset - issue_cycle + INORDER_MAX_ESTIMATE_INST_NUM) % INORDER_MAX_ESTIMATE_INST_NUM;

			// complete
#ifdef ANASIM_DEBUG
			assert(fu_type < 13);
			assert(fu_type >= 0);
#endif
			uint64_t penalty_cycle = CONFIG_ALU_LATENCY[fu_type];
			//printf("calculation\n");
#ifdef TRANSFORMER_INORDER_CACHE
			uint64_t cache_cycle = 0;
			if((fu_type == FU_RDPORT) || (fu_type == FU_WRPORT)) {

				uint64_t addr = entry.m_physical_addr;
				int thread_id = phy_proc;
				if(CAT_pseudo_line_insert(addr, execute_cycle)) {
//                    printf("addr %lx\n",addr);
					int in_L2 = CAT_pseudo_L2_tag_search(addr);
					if(in_L2) {
						inorder_l2_cache_hit_count++;
#ifdef JXF_BLOCK
                        tb_info_temp->inorder_l2_cache_hit_count++;
#endif            
						CAT_pseudo_L2_line_insert(addr, execute_cycle);
					} else {
						//l2 cache miss
						inorder_l2_cache_miss_count++;
                        origin_cache ++;
#ifdef JXF_debug
                        printf("big MISS!!\n");
#endif                        
#ifdef JXF_BLOCK
                        tb_info_temp->inorder_l2_cache_miss_count++;
#endif            
						cache_cycle += INORDER_MEMORY_LATENCY;
					}

					// l1 cache miss
					inorder_l1_dcache_miss_count++;
#ifdef JXF_BLOCK
                    tb_info_temp->inorder_l1_dcache_miss_count++;
#endif            
					cache_cycle += INORDER_L2_CACHE_LATENCY;

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
					inorder_l1_dcache_hit_count++;
#ifdef JXF_BLOCK
                        tb_info_temp->inorder_l1_dcache_hit_count++;
#endif            

#ifdef TRANSFORMER_USE_CACHE_COHERENCE
#ifdef TRANSFORMER_USE_CACHE_COHERENCE_buffer
					if(fu_type == FU_WRPORT) {
						m_write_coherence_buffer(addr, Coherence_store_hit, execute_cycle, 0);
					}
#endif
#endif
				}
				cache_cycle += INORDER_L1_CACHE_LATENCY;
			}
			penalty_cycle += cache_cycle;
#endif //*/

#ifdef TRANSFORMER_GLOBAL_CACHE
			uint64_t cache_cycle = 0;
			if((fu_type == FU_RDPORT) || (fu_type == FU_WRPORT)) {
				//uint64_t addr = s_instr->m_physical_addr;
				pthread_mutex_lock(&mutex);
				uint64_t addr = entry.m_physical_addr;
				int thread_id = phy_proc;
				//uint64_t l1_target_cycle = CAT_pseudo_line_get_target_cycle(addr, thread_id);
				//if (CAT_pseudo_line_insert(addr, thread_id) || (l1_target_cycle > execute_cycle))
				if(CAT_pseudo_line_insert(addr, thread_id, execute_cycle)) {
					//uint64_t l2_target_cycle = CAT_pseudo_L2_get_target_cycle(addr);
					//if (/*(!CAT_in_L1_cache(addr)) &&*/ !CAT_pseudo_L2_tag_search(addr) || (l2_target_cycle > execute_cycle))
					if(!CAT_pseudo_L2_tag_search(addr)) {
						//l2 cache miss
						inorder_l2_cache_miss_count++;
						cache_cycle += INORDER_MEMORY_LATENCY;
						/*uint64_t target_cycle = CAT_pseudo_L2_get_target_cycle(addr);
						uint64_t worst_cycle = execute_cycle + cache_cycle;
						if (target_cycle <= execute_cycle) {
							CAT_pseudo_L2_set_target_cycle(addr, worst_cycle);
						} else if (target_cycle <= worst_cycle) {
							cache_cycle = target_cycle - execute_cycle;
						}// */
					} else {
						inorder_l2_cache_hit_count++;
						CAT_pseudo_L2_line_insert(addr, execute_cycle);
					}

					// l1 cache miss
					inorder_l1_dcache_miss_count++;
					cache_cycle += INORDER_L2_CACHE_LATENCY;
					/*
					            uint64_t target_cycle = CAT_pseudo_line_get_target_cycle(addr, thread_id);
					            uint64_t worst_cycle = execute_cycle + cache_cycle;
					            if (target_cycle <= execute_cycle) {
					                CAT_pseudo_line_set_target_cycle(addr, thread_id, worst_cycle);
					            } else if (target_cycle <= worst_cycle) {
					                cache_cycle = target_cycle - execute_cycle;
					            }// */

#ifdef TRANSFORMER_USE_CACHE_COHERENCE
#ifdef TRANSFORMER_USE_CACHE_COHERENCE_buffer
					if(fu_type == FU_WRPORT) {
						CAT_pseudo_access_coherence_buffer(addr, thread_id, Coherence_store_miss, execute_cycle);
					} else if(fu_type == FU_RDPORT) {
						CAT_pseudo_access_coherence_buffer(addr, thread_id, Coherence_load_miss, execute_cycle);
					}
#else
					if(fu_type == FU_WRPORT) {
						CAT_pseudo_access_directory(addr, thread_id, Coherence_store_miss, execute_cycle);
					} else if(fu_type == FU_RDPORT) {
						CAT_pseudo_access_directory(addr, thread_id, Coherence_load_miss, execute_cycle);
					}
#endif
#endif
				} else {
					inorder_l1_dcache_hit_count++;
					//cache_cycle += INORDER_L1_CACHE_LATENCY;

#ifdef TRANSFORMER_USE_CACHE_COHERENCE
#ifdef TRANSFORMER_USE_CACHE_COHERENCE_buffer
					if(fu_type == FU_WRPORT) {
						CAT_pseudo_access_coherence_buffer(addr, thread_id, Coherence_store_hit, execute_cycle);
					}
#else
					if(fu_type == FU_WRPORT) {
						CAT_pseudo_access_directory(addr, thread_id, Coherence_store_hit, execute_cycle);
					}
#endif
#endif
				}
				pthread_mutex_unlock(&mutex);
				cache_cycle += INORDER_L1_CACHE_LATENCY;
			}
			penalty_cycle += cache_cycle;
#endif

			uint64_t finish_cycle = execute_cycle + penalty_cycle;
#ifdef ANASIM_DEBUG
			assert(entry.m_dest_reg[0] < QEMU_NUM_REGS);
#endif
			if(entry.m_dest_reg[0] != 0) {
				m_inorder_register_finish_cycle[entry.m_dest_reg[0]] = finish_cycle;
			} //*/

			// Added by WHJ 2013.06.25 LSQ
			// WHJ lsq

			// retire
			uint64_t retire_cycle = finish_cycle + 1;
			int index = (correct_timing_release_ptr - 1 + CORRECT_RETIRE_BUF_SIZE) % CORRECT_RETIRE_BUF_SIZE;
#ifdef ANASIM_DEBUG
			assert(index >= 0);
			assert(index < CORRECT_RETIRE_BUF_SIZE);
			assert(correct_timing_release_ptr < CORRECT_RETIRE_BUF_SIZE);
#endif
            //JXF: record when inst:index can retire
			m_inorder_retire_inst_cycle[index] = retire_cycle;
//            if (cache_cycle > 2) printf("cache_cycle %d\n",cache_cycle);
//            if (retire_cycle- m_local_cycles > 200) {
//#ifdef JXF_debug            
//                printf("This is a big INST! cache:%d\n",cache_cycle);
//#endif                
//                big_inst++;
//                if (cache_cycle<200)
//                    cache_cycle += 200;
//            }
#ifdef JXF_debug            
            printf("index %d retire cycle %d\t",index,m_inorder_retire_inst_cycle[index]);
#endif            
#ifdef JXF_BLOCK
//            tb_info_temp->m_block_retire_buf[block_retire_ptr] += 
//                (retire_cycle - m_local_cycles - cache_cycle);
//            
//			tb_info_temp->addr[block_retire_ptr] = entry.m_physical_addr;
            int temp_retire = block_retire_ptr % CORRECT_RETIRE_BUF_SIZE;
            tb_info_temp->m_block_retire_buf[temp_retire] = retire_cycle-base_cycle;
//            if (tb_info_temp-> m_block_retire_buf[temp_retire] > 150)
//                tb_info_temp->m_block_retire_buf[temp_retire] -= 150;

            if (fu_type == FU_BRANCH)
                tb_info_temp->m_next[temp_retire] = m_next_pc[index]-1;
            else
                tb_info_temp->m_next[temp_retire] = m_next_pc[index];
            block_retire_ptr++;
#ifdef JXF_debug            
            printf("ptr:%d\tnpc:%lx\tfutype%d\n",block_retire_ptr,m_next_pc[index],fu_type);
#endif            
#endif            

#ifdef WHJ_TEST
			total_uops_num++;
			if(total_uops_num < TEST_UOPS_THREDSHOLD) {
				ttrifs_trace_log("%d,	\tpc: %llx, \tfu: %d,  \tsource: %d : %d : %d,  \tdest: %d, \tfetch: %lld, \tissue: %lld, \texecute: %lld, \tfinish: %lld, \tretire: %lld, \tlocal: %lld, \tmem_add: %llx\n",
								 total_uops_num, entry.logical_pc, entry.m_futype,
								 entry.m_source_reg[0], entry.m_source_reg[1], entry.m_source_reg[2], entry.m_dest_reg[0], fetch_cycle, issue_cycle, execute_cycle, finish_cycle, retire_cycle, m_local_cycles, entry.m_physical_addr);
			}
#endif

#ifdef TRANSFORMER_INORDER_BRANCH
			if(fu_type == FU_BRANCH) {
#ifdef JXF_BLOCK
                tb_info_temp->inorder_branch_count++;
#endif                
				inorder_branch_count++;
				uint64_t fetch_pc = entry.logical_pc;
				bool taken = m_predictor->Predict(fetch_pc, m_spec_bpred->cond_state, 0, phy_proc);
#ifdef ACCURACY_TEST
				printf("branch: %d\n", fu_type);
#endif
				m_spec_bpred->cond_state = ((m_spec_bpred->cond_state << 1) | (taken & 0x1));
                //JXF: 0x1: taken 0x2: not taken
                //so the following if means when branch misprediction happens
				if(!((taken && (entry.m_branch_result & 0x1)) || ((!taken) && (entry.m_branch_result & 0x2)))) {
#ifdef JXF_debug                    
					printf("branch break: %d\n", fu_type);
#endif                    
#ifdef ACCURACY_TEST
					printf("branch break: %d\n", fu_type);
#endif
#ifdef JXF_BLOCK
                    tb_info_temp->inorder_branch_misprediction_count++;
                    
#endif                    
                    inorder_branch_misprediction_count++;
                   //JXF: this accumulate the duration of this inst 
					m_inorder_x86_retire_info.inorder_skip_cycle += finish_cycle - fetch_cycle + INORDER_MISPREDICTION_LATENCY;
					break;
				}
			}
#endif
		}
		inorder_dispatch_inst_count--;
	}
	// Added by minqh 2013.06.20
	// Do not use loop here. If Branch (or other reason) stalls pipeline, we do not process the stall here.
	// [Deprecated] while (inorder_x86_retire_info[phy_proc][proc]->inorder_skip_cycle > 0)
	if(m_inorder_x86_retire_info.inorder_skip_cycle > 0) {
#ifdef JXF_debug
        printf("inst %d work %d retire cycle %d rob_count %d\t",correct_timing_ptr,correct_timing_release_ptr,m_inorder_retire_inst_cycle[correct_timing_ptr],m_inorder_fetch_rob_count);
        printf("current cycle %d\tcurrent instruction %d\n",m_local_cycles,m_local_fetch_count);
#endif   
        bool t = ((m_inorder_retire_inst_cycle[correct_timing_ptr]== (-1)) 
                && (m_inorder_fetch_rob_count == INORDER_MAX_STALL_INST_NUM));
        if (t) m_inorder_fetch_rob_count = 0;
//        assert(t != true);
		//cout << "inorder_skip_cycle " << m_inorder_x86_retire_info.inorder_skip_cycle << endl;
		//commit a whole X86 instruction
		//cout << "m_inorder_retire_inst_cycle " << m_inorder_retire_inst_cycle[correct_timing_ptr] << endl;
		//cout << "m_local_cycles " << m_local_cycles << endl;
		//cout << "before while" << endl;
//		if(m_inorder_fetch_rob_count >= INORDER_MAX_STALL_INST_NUM) {
//			m_inorder_fetch_rob_count = 1;
//		}
		//this uop instruction can retire now
		while((m_inorder_retire_inst_cycle[correct_timing_ptr] > 0) &&
				(m_inorder_retire_inst_cycle[correct_timing_ptr] != (uint64_t)-1) &&
				(m_inorder_retire_inst_cycle[correct_timing_ptr] <= m_local_cycles)&&
                (m_inorder_fetch_rob_count>0)) {
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
#ifdef JXF_BLOCK
            tb_info_temp->m_local_instr_count++;
            tb_info_temp->correct_timing_ptr ++;
#endif
			//printf("local instr count is %d\n", m_local_instr_count);
			//cout << "next pc " << next_pc << " inorder_last_pc " << m_inorder_x86_retire_info.inorder_last_pc << endl;
			//cout << "FU_BRANCH " << FU_BRANCH << " inorder_last_fu_type "
			//<< m_inorder_x86_retire_info.inorder_last_fu_type << endl;

			//this uop instruction shares the same pc with the last one, two from a single x86 instruction
            if (m_inorder_x86_retire_info.inorder_uop_sum >40){
//                    printf("big uop!! %d release %d rob %d\n",m_inorder_x86_retire_info.inorder_uop_sum,correct_timing_ptr,correct_timing_release_ptr,m_inorder_fetch_rob_count);
				m_inorder_fetch_rob_count -= m_inorder_x86_retire_info.inorder_uop_sum;
				m_inorder_x86_retire_info.inorder_uop_sum = 1;

            }else if((next_pc == m_inorder_x86_retire_info.inorder_last_pc)
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
#ifdef JXF_BLOCK
                tb_info_temp->m_inorder_fetch_rob_count-= m_inorder_x86_retire_info.inorder_uop_sum;
#endif            
				//cout << "decrease m_inorder_fetch_rob_count is " << m_inorder_fetch_rob_count << endl;
				if(m_inorder_fetch_rob_count < 0) {
					m_inorder_fetch_rob_count = 0;
				}
				assert(m_inorder_fetch_rob_count >= 0);
				m_inorder_x86_retire_info.inorder_uop_sum = 1;
			}
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
#ifdef JXF_BLOCK
        tb_info_temp->m_local_cycles++;
#endif
	}
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

extern int max_tb_per_exec;
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

/* 
 *  check for how to deal with this tb:
 *  if this is in a big block(read rm_block num from tb_info struct)(judge by rm_block):
 *      if this is a duplicate block:
 *          record all the current info for the big block(base cycle, retire pointer)
 *      if not:
 *          record remain instruction num and change rm_block according to this
 *          apply m_calculate and recore base cycle and retire pointer
 *          at the end of the blocks record the block num in the tb_info struct
 *  if this is the start of an old tb?
 *      set rm_block to 0
 *      goto count_duplicate
 *  if this is the start of an new tb?
 *      goto m_calculate
 *  if this is the start of an sampling old tb?
 *      goto m_calculate
 *
 * */
void do_cal(int core_num, vector< instr_info_buffer_t > &entries,uint64_t tb_addr) {
        struct timeval start,end;
	assert(core_num >= 0 && core_num < NUM_PROCS);
	//assert(m_seq[core_num]->m_inorder_fetch_rob_count == 0);
	//cout << "entires size is " << entries.size() << endl;
	//cout << "m_inorder_fetch_rob_count is " << m_seq[core_num]->m_inorder_fetch_rob_count << endl;
        tb_count[core_num]++;

        //record 8 bit of each former block,record 8 former block in total. 

#ifdef JXF_debug        
        printf("tb_count:%lld core num %d\n",tb_count[core_num]);
        printf("calculate cpu:%d %lx block nu %d size %d addr %lx\n",core_num,tb_addr,m_seq[core_num]->block_nu,entries.size(),&entries);
#endif        
            int exist;


      //  RUN_TIME(old_time[core_num],                   
        gather_tb_t* g_current_tb = (gather_tb_t*)calloc(1,sizeof(gather_tb_t));
        g_current_tb->tb_addr = tb_addr;
        m_seq[core_num]->BBV[tb_addr % BBV_DIM] += entries.size();
        m_seq[core_num]-> m_local_size_count += entries.size();
        g_current_tb->entry = &entries;
//        g_current_tb-> entry.assign(entries.begin(),entries.end());
        m_seq[core_num]->current_tb_array[m_seq[core_num]->block_nu] = g_current_tb;
//    if (m_seq[core_num]-> m_local_fetch_count > 23500){
//        printf("tb_count:%lld\n",tb_count[core_num]);
//        printf("calculate cpu:%d %lx\n block nu %d\n",core_num,tb_addr,m_seq[core_num]->block_nu);
//    }
        m_seq[core_num]->block_nu++;
#ifdef JXF_BLOCK
        if (m_seq[core_num]->block_nu == GATHER_BLOCK_NUM ){
            m_seq[core_num]->block_nu = 0;
            tb_info_struct_t *tb_info;
            exist = m_seq[core_num]->find_duplicate(tb_addr,&tb_info);
            if (exist ){
                duplicate_count[core_num]++;
                m_seq[core_num]->count_duplicate(tb_info);
                m_seq[core_num]->former_dup = true;
            }else{
                if (m_seq[core_num]->former_dup){
         	        for(int k = 0; k < INORDER_MAX_ESTIMATE_INST_NUM; k++) {
        		        m_seq[core_num]->m_inorder_execute_inst_width[k] = 0;
            	        for(int kk = 0; kk < FU_NUM_FU_TYPES; kk++) {
    		    	        m_seq[core_num]->m_inorder_function_unit_width[k][kk] = 0;
	    	            }
    	            }

                }

                m_seq[core_num]->former_dup = false;
                tb_info_struct_t *tb_info_new;
                tb_info_new = (tb_info_struct_t *)calloc(1,sizeof(tb_info_struct_t));
                tb_info_new->m_inorder_x86_retire_info = (struct inorder_retire_info *)calloc(1,sizeof(struct inorder_retire_info));
                tb_info_new->tb_addr = tb_addr;
                tb_info_new->BBV = m_seq[core_num]->BBV;
                tb_info_new->former_BBV = m_seq[core_num]->former_BBV;
                m_seq[core_num]->tb_info_temp = tb_info_new;
                new_count[core_num]++;
                m_seq[core_num]->block_retire_ptr = 0; 
                m_seq[core_num]->base_cycle = m_seq[core_num]->m_local_cycles;
            RUN_TIME(new_time[core_num],
                for (int i =0; i<GATHER_BLOCK_NUM;i++){
                    unsigned int index = 0;
                    gather_tb_t* gg = (gather_tb_t*)m_seq[core_num]->current_tb_array[i];
		            while(true) {
	                    bool exit_flag = m_seq[core_num]->m_calculate(*(gg->entry), index);
			            if(!exit_flag) {
#ifdef JXF_debug                
                printf("inst origin %ld\n",m_seq[core_num]->m_local_instr_count);
                printf("size %d\n",(*(gg->entry)).size());
//                printf("size %d\n",gg->entry.size());
#endif                
				            break;
			            }
		            }
                }
            );
                    
                uint64_t mask = (1<<TB_CACHE_BITS)-1;
#ifdef JXF_debug                
                printf("retire_ptr %d\n",m_seq[core_num]->block_retire_ptr);
//                printf("inst %ld\n",tb_info_new->m_local_instr_count);
#endif              
                tb_info_new->correct_timing_release_ptr = m_seq[core_num]->block_retire_ptr;
                int off = (int)(tb_addr & mask);
                tb_info_new->next = tb_buffer[off];
                if (tb_buffer[off] == NULL)
                    hash_count++;
                tb_buffer[off] = tb_info_new;
            }
            m_seq[core_num]->former_BBV = m_seq[core_num]->BBV;
            m_seq[core_num]->BBV = (uint64_t *)calloc(BBV_DIM,sizeof(uint64_t));
#ifdef JXF_debug                
            printf("-----big block end-----\n");
#endif              
        }
        return;
#endif
}



void zmf_init() {
	printf("ZMF_init: cpu_count=%d\n", NUM_PROCS);
	m_seq = (pseq_t **)malloc(NUM_PROCS * sizeof(pseq_t *));
        
    int global_cpu_count = NUM_PROCS;
        tb_count = (uint64_t *)calloc(global_cpu_count, sizeof(uint64_t));
        new_count = (uint64_t *)calloc(global_cpu_count, sizeof(uint64_t));
        duplicate_count = (uint64_t *)calloc(global_cpu_count, sizeof(uint64_t));
//        inst_size_count = (uint64_t *)calloc(20, sizeof(uint64_t));
//        cyc_size_count = (uint64_t *)calloc(20, sizeof(uint64_t));
		search_time = (double *)calloc(global_cpu_count,  sizeof(double));
		old_time = (double *)calloc(global_cpu_count,  sizeof(double));
		new_time = (double *)calloc(global_cpu_count,  sizeof(double));

        hash_count = 0;

#ifdef  JXF_BLOCK
        tb_info_header = (tb_info_struct_t *)malloc(sizeof(tb_info_struct_t));
//        current_tb = (tb_info_struct_t **)calloc(global_cpu_count,sizeof(*tb_info_struct_t));
//        rm_block = (int*)calloc(global_cpu_count,sizeof(int));
//        in_dup = (bool*)calloc(global_cpu_count,sizeof(bool));
        memset(tb_info_header,0,sizeof(tb_info_struct_t));
        tb_info_header->next = NULL;
        for (int i = 0;i<TB_CACHE_SIZE;i++)
            tb_buffer[i] = NULL;
#endif

	for(int i = 0; i < NUM_PROCS; i++) {
		m_seq[i] = new pseq_t(i);
#ifdef  JXF_BLOCK
//        rm_block[i] = 0;
#endif
		printf("pseq_t addr is %p, m_id is %d\n", m_seq[i], m_seq[i]->m_id);
	}
	stepInorder();
}

void calculation_hello() {
	std::cout << "inside calculation" << std::endl;
}

void print_result(ostream & OutFile) {

#ifdef TRANSFORMER_USE_CACHE_COHERENCE
	CAT_pseudo_dump_coherence_stat();
#endif
	printf("\n[Calculation-based simulator]-Wait_Number: ");
	OutFile << endl << "[Calculation-based simulator]-Wait_Number: ";
	for(int j = 0; j < NUM_PROCS; j++) {
		printf("\t%ld", m_seq[j]->inorder_wait_num);
		OutFile << "\t" << m_seq[j]->inorder_wait_num;
	}
	printf("\n[Calculation-based simulator]- Instructions: ");
	OutFile << endl << "[Calculation-based simulator]- Instructions: ";
	uint64_t jxf_fetch_instr = 0;
	for(int j = 0; j < NUM_PROCS; j++) {
		printf("\t%ld", m_seq[j]->m_local_fetch_count);
		OutFile << "\t" << m_seq[j]->m_local_fetch_count;
		jxf_fetch_instr += m_seq[j]->m_local_fetch_count;
	}
	printf("\n[Calculation-based simulator]-Size Instructions: ");
	for(int j = 0; j < NUM_PROCS; j++) {
		printf("\t%ld", m_seq[j]->m_local_size_count);
	}
	printf("\n[Calculation-based simulator]-Total fetch instructions %ld\n", jxf_fetch_instr);
	OutFile << endl << "[Calculation-based simulator]-Total fetch instructions " << jxf_fetch_instr << endl;
	printf("[Calculation-based simulator]-Cycles: ");
	OutFile << "[Calculation-based simulator]-Cycles: ";
    uint64_t jxf_total_cycle = 0;
	for(int j = 0; j < NUM_PROCS; j++) {
#ifdef PARALLIZED_SHARED_CACHE
		printf("\t%ld", *(m_seq[j]->L2_hit_increase_back));
		OutFile << "\t" << *(m_seq[j]->L2_hit_increase_back);
		printf("\t%ld", (m_seq[j]->m_local_cycles - * (m_seq[j]->L2_hit_increase_back) * INORDER_MEMORY_LATENCY));
		OutFile << "\t" << (m_seq[j]->m_local_cycles - * (m_seq[j]->L2_hit_increase_back) * INORDER_MEMORY_LATENCY);
#else
        jxf_total_cycle += m_seq[j]->m_local_cycles;
		printf("\t%ld", m_seq[j]->m_local_cycles);
		OutFile << "\t" << m_seq[j]->m_local_cycles;
#endif
	}
	printf("\n[Calculation-based simulator]-Total cycles %ld\n", jxf_total_cycle);
	OutFile << endl << "[Calculation-based simulator]-Total instructions " << jxf_total_cycle << endl;

#ifdef TRANSFORMER_INORDER_BRANCH
	uint64_t total_inorder_branch_count = 0;
	uint64_t total_inorder_branch_misprediction_count = 0;
	for(int j = 0; j < NUM_PROCS; j++) {
		total_inorder_branch_count += m_seq[j]->inorder_branch_count;
		total_inorder_branch_misprediction_count += m_seq[j]->inorder_branch_misprediction_count;
	}
	printf("[Calculation-based simulator]-Branches: Total %ld  Mispred %ld  Mispred-Percentage %0.2f\n",
		   total_inorder_branch_count, total_inorder_branch_misprediction_count, total_inorder_branch_misprediction_count / (total_inorder_branch_count + 0.0) * 100);
	OutFile << "[Calculation-based simulator]-Branches: Total " << total_inorder_branch_count <<  " Mispred " << total_inorder_branch_misprediction_count <<
			" Mispred-Percentage " << total_inorder_branch_misprediction_count / (total_inorder_branch_count + 0.0) * 100 << endl;
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
	printf("[Calculation-based simulator]-Caches: L1-I hit %ld miss %ld L1-D hit %ld miss %ld L2 hit %ld miss %ld \n",
		   total_inorder_l1_icache_hit_count, total_inorder_l1_icache_miss_count, total_l1_dcache_hit_count, total_l1_dcache_miss_count,
		   total_l2_dcache_hit_count, total_l2_dcache_miss_count);
	OutFile << "[Calculation-based simulator]-Caches: L1-I hit " << total_inorder_l1_icache_hit_count
			<< " miss " << total_inorder_l1_icache_miss_count << " L1-D hit " << total_l1_dcache_hit_count << " miss " << total_l1_dcache_miss_count
			<< " L2 hit " << total_l2_dcache_hit_count << " miss " << total_l2_dcache_miss_count << endl;
#endif


	printf("[Calculation-based simulator] Simulation terminates.\n");
	OutFile << "[Calculation-based simulator] Simulation terminates." << endl;
	printf("[Calculation-based simulator] Goodbye.Pin JXF!!!\n");
#ifdef JXF_BLOCK
    printf("This is Block version!\n");
#endif    
	OutFile << "[Calculation-based simulator] Goodbye." << endl;

	fflush(stdout);
	fflush(stderr);

	return;
}
