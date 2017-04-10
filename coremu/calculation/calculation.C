


/*------------------------------------------------------------------------*/
/* Includes                                                               */
/*------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sched.h>

#include "calculation.h"

using namespace std;

#ifdef TRANSFORMER_GLOBAL_CACHE
#include "trans_cache.h"
#endif

/*------------------------------------------------------------------------*/
/* Macro declarations                                                     */
/*------------------------------------------------------------------------*/


/*------------------------------------------------------------------------*/
/* Data type and variable declaration(s)                                                   */
/*------------------------------------------------------------------------*/

int global_cpu_count;
int qemu_instr_buffer_size;

pthread_mutex_t mutex;

volatile bool zmf_finished = false;

#ifdef DUMP_INSTRUCTION_FLOW
#include <fstream>
#endif
#ifdef DUMP_INSTRUCTION_FLOW
ofstream ins_log_file;
int dump_ins_num = 1000000;
#endif

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

//#define WHJ_TEST
#ifdef WHJ_TEST
volatile uint64_t total_uops_num = 0;
#define TEST_UOPS_THREDSHOLD 10000
#endif

pseq_t **m_seq;

#define CORRECT_BUF_SIZE (qemu_instr_buffer_size) 
#define MAX_TRIFS_TRACE_MEM_BUF_SIZE (CORRECT_BUF_SIZE * 512)
#define MAX_TRIFS_TRACE_BUF_BLOCK_SIZE (4)
#define CORRECT_RETIRE_BUF_SIZE 64
#define MAX_PHYSICAL_CORE_NUM 256



bool timing_period[MAX_PHYSICAL_CORE_NUM];

/// thread arguments to start the timing core thread
typedef struct{
	int core_id;
} timing_core_thread_arg;



/*------------------------------------------------------------------------*/
/* Function definition(s)                                                   */
/*------------------------------------------------------------------------*/



#ifdef __cplusplus
extern "C"
{
#endif

#ifdef ANALYTICAL_MODEL_TIMING_SHIFT
extern ana_tb_struct_t *trifs_trace_shared_tb_inst_buf_head(int core_num);
extern ana_instr_struct_t *trifs_trace_shared_buf_head(int core_num);
extern ana_mem_struct_t *trifs_mem_shared_buf_head(int core_num);
#endif
//extern int trifs_trace_shared_buf_size(void);
//extern int trifs_trace_cpu_exec(int cpu_index);
extern int trifs_trace_ptlsim_decoder(W8 *x86_buf, int num_bytes, 
            TransOp *upos_buf, int *num_uops, W64 rip, bool is64bit);

//extern instr_info_buffer_flag_t *get_minqh_flag_buf_head(int core_num);


#ifdef __cplusplus
}
#endif

void set_cpu(int cpu_no)
{
	cpu_set_t mask;
	CPU_ZERO(&mask);
	CPU_SET(cpu_no, &mask);
	sched_setaffinity(syscall(SYS_gettid), sizeof(cpu_set_t), &mask);
}

pseq_t::pseq_t( int id ){
	m_id = id;
	m_local_cycles = 0;
	m_local_instr_count = 0;
	//calculation_wait_num = 0;
	tb_fetch_end = 0;
	fetch_return_ready = 0;
	step = 0;
	char *tmp_file_name = "timing.log";
	char ana_log_file_name[16];
	sprintf(ana_log_file_name, "%s%d", tmp_file_name, m_id);
	printf("%s\n", ana_log_file_name);
	const char *analytical_log_file_name = (const char *)ana_log_file_name;
	analytical_log_file = fopen(analytical_log_file_name, "w+");
	timing_call_times_core = 0;

#ifdef WHJ_TEST
	ins_dump_log_file = fopen("dump_instruction.log", "w+");
#endif
	//cormu_memory_empty_conflict_num = 0;
	//temp_tb_mem_num = 0;
	//total_tb_mem_num_dias = 0;
	//total_tb_mem_num_plus_dias = 0;
	inorder_dispatch_inst_count = MAX_FETCH;
	dispatch_reserve = 0;

	CONFIG_ALU_MAPPING[0] = 0;
	CONFIG_ALU_MAPPING[1] = 1;
	CONFIG_ALU_MAPPING[2] = 1;
	CONFIG_ALU_MAPPING[3] = 2;
	CONFIG_ALU_MAPPING[4] = 3;
	CONFIG_ALU_MAPPING[5] = 4;
	CONFIG_ALU_MAPPING[6] = 4;
	CONFIG_ALU_MAPPING[7] = 4;
	CONFIG_ALU_MAPPING[8] = 5;
	CONFIG_ALU_MAPPING[9] = 6;
	CONFIG_ALU_MAPPING[10] = 6;
	CONFIG_ALU_MAPPING[11] = 7;
	CONFIG_ALU_MAPPING[12] = 8;
	/*{
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
	};*/

	CONFIG_NUM_ALUS[0] = 127;
	CONFIG_NUM_ALUS[1] = 2;
	CONFIG_NUM_ALUS[2] = 1;
	CONFIG_NUM_ALUS[3] = 2;
	CONFIG_NUM_ALUS[4] = 1;
	CONFIG_NUM_ALUS[5] = 1;
	CONFIG_NUM_ALUS[6] = 1;
	CONFIG_NUM_ALUS[7] = 2;
	CONFIG_NUM_ALUS[8] = 2;
	CONFIG_NUM_ALUS[9] = 0;
	CONFIG_NUM_ALUS[10] = 0;
	CONFIG_NUM_ALUS[11] = 0;
	CONFIG_NUM_ALUS[12] = 0;
	/*{
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
	};*/

	CONFIG_ALU_LATENCY[0] = 1;
	CONFIG_ALU_LATENCY[1] = 1;
	CONFIG_ALU_LATENCY[2] = 4;
	CONFIG_ALU_LATENCY[3] = 32;
	CONFIG_ALU_LATENCY[4] = 1;
	CONFIG_ALU_LATENCY[5] = 2;
	CONFIG_ALU_LATENCY[6] = 2;
	CONFIG_ALU_LATENCY[7] = 2;
	CONFIG_ALU_LATENCY[8] = 6;
	CONFIG_ALU_LATENCY[9] = 32;
	CONFIG_ALU_LATENCY[10] = 32;
	CONFIG_ALU_LATENCY[11] = 2;
	CONFIG_ALU_LATENCY[12] = 1;
	/*{
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
	};*/

	#ifdef TRANSFORMER_INORDER
	//minqh_flag_buffer_index = 0;
	//minqh_mem_flag_buffer_index = 0;
	correct_timing_ptr = 0;
	correct_timing_fetch_ptr = 0;
	correct_mem_fetch_ptr = 0;
	//correct_tb_fetch_ptr = 0;
	correct_timing_release_ptr = 0;
	//printf("init 1 core id: %d\n", id);
#ifdef ANALYTICAL_MODEL_TIMING_SHIFT
	correct_qemu_tb_instr_buf = (ana_tb_struct_t *)trifs_trace_shared_tb_inst_buf_head(id);
	correct_qemu_instr_info_buf = (ana_instr_struct_t *)trifs_trace_shared_buf_head(id);
	correct_qemu_mem_info_buf = (ana_mem_struct_t *)trifs_mem_shared_buf_head(id);

	//minqh_flag_buffer = (instr_info_buffer_flag_t *)get_minqh_flag_buf_head(id);
	printf("timing tb struct size: %d %llx\n", sizeof(ana_tb_struct_t), correct_qemu_tb_instr_buf);
	printf("timing struct size: %d %llx\n", sizeof(ana_instr_struct_t), correct_qemu_instr_info_buf);
	printf("timing mem struct size: %d %llx\n", sizeof(ana_mem_struct_t), correct_qemu_mem_info_buf);
	//pseq_trifs_trace_uops_buf = trifs_trace_uops_global_buf;
	//pseq_trifs_trace_uops_ptr = trifs_trace_uops_global_buf;
#endif
	//code_cache_found = 0;
	//tb_start = 1;
	//tb_end = 0;
	tb_num_uops = 0;
	uops_index = 0;
	same_logical_pc = 0;
	same_access_size = 0;
	same_physical_addr = 0;
	mem_zero_num = 0;
	not_cached_num = 0;
	tb_mem_num = 0;
	tb_mem_num_index = 0;
	x86_eip = 0;
	trifs_trace_uops_global_index = 0;
	//printf("init 1.5 core id: %d\n", id);
	memset(trifs_trace_uops_global_buf, 0, sizeof(trifs_trace_uops_global_buf));
	//memset(trifs_trace_tb_mem_num, 0, sizeof(int));
	//printf("init 1.6 core id: %d\n", id);
	//trifs_trace_uops_tb_fetch_ptr = -1;
	//temp_cache = 0;
	//mem_tb_start = 1;
	//tb_has_mem = 0;
	//print_flag = 0;
	//printf("init 1.7 core id: %d\n", id);
	tb_uop_end_return_index = 0;
	
	m_inorder_fetch_rob_count = 0;
	for (int k = 0; k < QEMU_NUM_REGS; k++){
		m_inorder_register_finish_cycle[k] = 0;
	}
	for (int k = 0; k < INORDER_MAX_ESTIMATE_INST_NUM; k++){
		m_inorder_execute_inst_width[k]=0;
		for (int kk = 0; kk < FU_NUM_FU_TYPES; kk++){
			m_inorder_function_unit_width[k][kk] = 0;
		}
	}
	//printf("init 2 core id: %d\n", id);
	m_inorder_retire_inst_cycle = (uint64_t *)malloc(CORRECT_RETIRE_BUF_SIZE * sizeof(uint64_t));
	m_next_pc = (int *)malloc(CORRECT_RETIRE_BUF_SIZE * sizeof(int));
	m_next_fu_type = (int *)malloc(CORRECT_RETIRE_BUF_SIZE * sizeof(int));
	for (int k = 0; k < CORRECT_RETIRE_BUF_SIZE; k++){
		m_inorder_retire_inst_cycle[k] = 0;
		m_next_pc[k]=0;
		m_next_fu_type[k]=0;
	}
	memset(trifs_trace_uops_buf, 0, sizeof(trifs_trace_uops_buf));
       trifs_trace_num_uops = 0;
	memset(trifs_trace_mem_global_buf, 0, sizeof(trifs_trace_mem_global_buf));
	trifs_trace_mem_buf = trifs_trace_mem_global_buf;
	trifs_trace_mem_ptr = correct_qemu_mem_info_buf;
	trifs_trace_mem_uop_fetch = trifs_trace_mem_buf;

	m_inorder_x86_retire_info = (struct inorder_retire_info *)malloc(sizeof(struct inorder_retire_info));
	m_inorder_x86_retire_info->inorder_uop_sum = 0;
	m_inorder_x86_retire_info->inorder_last_pc = -1;
	m_inorder_x86_retire_info->inorder_last_fu_type = -1;
	m_inorder_x86_retire_info->inorder_skip_cycle = 0;
//printf("init 3 core id: %d\n", id);
	cal_lsq_store_queue_ptr = 0;
	cal_lsq_store_queue_0.store_address = 0;
	cal_lsq_store_queue_0.store_finish_cycle = 0;
	cal_lsq_store_queue_1.store_address = 0;
	cal_lsq_store_queue_1.store_finish_cycle = 0;

	#endif

	#ifdef TRANSFORMER_USE_CACHE_COHERENCE_buffer
	m_coherence_buffer = (volatile coherence_buffer_t *)malloc(Coherence_buffer_size * sizeof(volatile coherence_buffer_t));
	memset(m_coherence_buffer, 0, Coherence_buffer_size * sizeof(volatile coherence_buffer_t));
	m_coherence_buffer_write_ptr = 0;
	L2_hit_increase_back = (volatile long long *)malloc(sizeof(volatile long long));
	#endif

#ifdef TRANSFORMER_INORDER_BRANCH
	m_predictor = new yags_t( BRANCHPRED_PHT_BITS, BRANCHPRED_EXCEPTION_BITS, BRANCHPRED_TAG_BITS );
	m_spec_bpred = (predictor_state_t *)malloc(sizeof(predictor_state_t));
	m_spec_bpred->cond_state = 0;
	m_spec_bpred->indirect_state = 0;
	m_spec_bpred->ras_state.TOS=0;
	m_spec_bpred->ras_state.next_free=0;
#endif

	// private cache
#ifdef TRANSFORMER_INORDER_CACHE
	CAT_set_bit = DL1_SET_BITS;
	CAT_assoc = DL1_ASSOC;
	CAT_set_size= (1 << CAT_set_bit);
	CAT_set_mask = CAT_set_size - 1;
	CAT_block_bit = DL1_BLOCK_BITS;
	CAT_block_size = (1 << CAT_block_bit);
	CAT_block_mask = CAT_block_size - 1;	 
 
	CAT_pseudo_core_cache = (CAT_pseudo_cache_set_t*)malloc(CAT_set_size * sizeof(CAT_pseudo_cache_set_t*));
	for(int j = 0; j < CAT_set_size; j++){
		CAT_pseudo_core_cache[j].cache_line_list = (CAT_pseudo_cache_line_t*)malloc(CAT_assoc * sizeof(CAT_pseudo_cache_line_t));
		for(int k = 0; k < CAT_assoc; k++){
			CAT_pseudo_core_cache[j].cache_line_list[k].flag = CAT_CACHE_EMPTY;
		}
	}

    
	CAT_L2_set_bit = L2_SET_BITS;
	CAT_L2_assoc = L2_ASSOC;
	CAT_L2_set_size = (1 << CAT_L2_set_bit);
	CAT_L2_set_mask = CAT_L2_set_size - 1;

	CAT_pseudo_core_L2_cache = (CAT_pseudo_cache_set_t*)malloc(CAT_L2_set_size * sizeof(CAT_pseudo_cache_set_t*));
	for(int j = 0; j < CAT_L2_set_size; j++){
		CAT_pseudo_core_L2_cache[j].cache_line_list = (CAT_pseudo_cache_line_t*)malloc(CAT_L2_assoc * sizeof(CAT_pseudo_cache_line_t));
		for(int k = 0; k < CAT_L2_assoc; k++){
			CAT_pseudo_core_L2_cache[j].cache_line_list[k].flag = CAT_CACHE_EMPTY;
		}
	}

	CAT_L3_set_bit = L3_SET_BITS;
	CAT_L3_assoc = L3_ASSOC;
	CAT_L3_set_size = (1 << CAT_L3_set_bit);
	CAT_L3_set_mask = CAT_L3_set_size - 1;
 
	CAT_pseudo_L3_cache = (CAT_pseudo_cache_set_t*)malloc(CAT_L3_set_size * sizeof(CAT_pseudo_cache_set_t));
	for(int i = 0; i < CAT_L3_set_size; i++){
		CAT_pseudo_L3_cache[i].cache_line_list = (CAT_pseudo_cache_line_t*)malloc(CAT_L3_assoc * sizeof(CAT_pseudo_cache_line_t));
		for(int j = 0; j < CAT_L3_assoc; j++){
			CAT_pseudo_L3_cache[i].cache_line_list[j].flag = CAT_CACHE_EMPTY;
		}
	}
#endif

	//analysis count parameter
	inorder_wait_num = 0;
	inorder_l1_icache_hit_count = 0;
	inorder_l1_icache_miss_count = 0;
	inorder_l1_dcache_hit_count = 0;
	inorder_l1_dcache_miss_count = 0;
	inorder_l2_cache_hit_count = 0;
	inorder_l2_cache_miss_count = 0;
	inorder_l3_cache_hit_count = 0;
	inorder_l3_cache_miss_count = 0;
	inorder_branch_count = 0;
	inorder_branch_misprediction_count = 0;
	ttrifs_trace_log("init core: %d end\n", id);
}

pseq_t::~pseq_t() {
	free(m_inorder_retire_inst_cycle);
	//free(cal_lsq_store_queue);
#ifdef TRANSFORMER_INORDER_BRANCH
	free(m_spec_bpred);
	delete(m_predictor);
#endif
	//delete(correct_qemu_instr_info_buf);
#ifdef ANALYTICAL_MODEL_TIMING_SHIFT
	delete(correct_qemu_tb_instr_buf);
#endif
	//delete(minqh_flag_buffer);
}

#ifdef TRANSFORMER_INORDER_CACHE
//false means alredy in cache
bool pseq_t::CAT_pseudo_line_insert(long long address, long long cycle){
	unsigned int index = Set(address);
	long long ba = BlockAddress(address);

	CAT_pseudo_cache_set_t *set = &CAT_pseudo_core_cache[index];

	for(int i = 0; i < CAT_assoc; i++){
		if(set->cache_line_list[i].flag == CAT_CACHE_FULL && set->cache_line_list[i].block_addr == ba){
			for(unsigned int j = i; j > 0; j--){
				set->cache_line_list[j].block_addr = set->cache_line_list[j - 1].block_addr;
				set->cache_line_list[j].flag = set->cache_line_list[j - 1].flag;
			}
			set->cache_line_list[0].block_addr = ba;
			set->cache_line_list[0].flag = CAT_CACHE_FULL;
			return false;
		}
	}

	//if the set is full, we need to put the replaced line into L3 cache
	if(set->cache_line_list[CAT_assoc - 1].flag == CAT_CACHE_FULL){
		CAT_pseudo_L2_line_insert(set->cache_line_list[CAT_assoc - 1].block_addr, cycle);
	}
	
	for(unsigned int i = CAT_assoc - 1; i > 0; i--){
		set->cache_line_list[i].block_addr = set->cache_line_list[i - 1].block_addr;
		set->cache_line_list[i].flag = set->cache_line_list[i - 1].flag;
	}
	set->cache_line_list[0].block_addr = ba;
	set->cache_line_list[0].flag = CAT_CACHE_FULL;
	
	return true;
}

//false means alredy in cache
bool pseq_t::CAT_pseudo_L2_line_insert(long long address, long long cycle){
	unsigned int index = Set(address);
	long long ba = BlockAddress(address);

	CAT_pseudo_cache_set_t *set = &CAT_pseudo_core_L2_cache[index];

	for(int i = 0; i < CAT_L2_assoc; i++){
		if(set->cache_line_list[i].flag == CAT_CACHE_FULL && set->cache_line_list[i].block_addr == ba){
			for(unsigned int j = i; j > 0; j--){
				set->cache_line_list[j].block_addr = set->cache_line_list[j - 1].block_addr;
				set->cache_line_list[j].flag = set->cache_line_list[j - 1].flag;
			}
			set->cache_line_list[0].block_addr = ba;
			set->cache_line_list[0].flag = CAT_CACHE_FULL;
			return false;
		}
	}

	//if the set is full, we need to put the replaced line into L3 cache
	if(set->cache_line_list[CAT_L2_assoc - 1].flag == CAT_CACHE_FULL){
		#ifdef TRANSFORMER_USE_CACHE_COHERENCE
			#ifdef TRANSFORMER_USE_CACHE_COHERENCE_buffer
				m_write_coherence_buffer(set->cache_line_list[CAT_L2_assoc - 1].block_addr, Coherence_replace, cycle, 0);
			#else
				//CAT_pseudo_access_directory(address, core, Coherence_replace, cycle);
			#endif
		#endif
		
		CAT_pseudo_L3_line_insert(set->cache_line_list[CAT_L2_assoc - 1].block_addr, cycle);
	}
	
	for(unsigned int i = CAT_L2_assoc - 1; i > 0; i--){
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
**2. L1 miss && not in other cores' L1 cache && in L3 cache
*/
bool pseq_t::CAT_pseudo_L3_line_insert(long long address, long long cycle){
	unsigned int index = L3_Set(address);
	long long ba = BlockAddress(address);

	CAT_pseudo_cache_set_t *set = &CAT_pseudo_L3_cache[index];

	for(int i = 0; i < CAT_L3_assoc; i++){
		if(set->cache_line_list[i].flag == CAT_CACHE_FULL && set->cache_line_list[i].block_addr == ba){
			for(unsigned int j = i; j > 0; j--){
				set->cache_line_list[j].block_addr = set->cache_line_list[j - 1].block_addr;
				set->cache_line_list[j].flag = set->cache_line_list[j - 1].flag;
			}
			set->cache_line_list[0].block_addr = ba;
			set->cache_line_list[0].flag = CAT_CACHE_FULL;
			return false;
		}
	}

	for(unsigned int i = CAT_L3_assoc - 1; i > 0; i--){
		set->cache_line_list[i].block_addr = set->cache_line_list[i - 1].block_addr;
		set->cache_line_list[i].flag = set->cache_line_list[i - 1].flag;
	}
	set->cache_line_list[0].block_addr = ba;
	set->cache_line_list[0].flag = CAT_CACHE_FULL;
	
	return true;
}

//check if L3 hit or miss, return false if not in L3
bool pseq_t::CAT_pseudo_L3_tag_search(long long address){
	unsigned int index = L3_Set(address);
	long long   ba = BlockAddress(address);
	bool   cachehit = false;

	/* search all sets until we find a match */
	CAT_pseudo_cache_set_t *set = &CAT_pseudo_L3_cache[index];
	for (int i = 0 ; i < CAT_L3_assoc ; i ++) {
		// if it is found in the cache ...
		if ( (set->cache_line_list[i].flag == CAT_CACHE_FULL) && BlockAddress(set->cache_line_list[i].block_addr) == ba ) {
			cachehit = true;
			break;
		}
	}
	return cachehit;
}
#endif

#ifdef TRANSFORMER_USE_CACHE_COHERENCE_buffer

// Write coherence buffer
bool pseq_t::m_write_coherence_buffer(long long address, int type, long long cycle, int in_L3){
	while (m_coherence_buffer[m_coherence_buffer_write_ptr].flag == FULL){
		;
	}
	m_coherence_buffer[m_coherence_buffer_write_ptr].address = address;
	m_coherence_buffer[m_coherence_buffer_write_ptr].cycle = cycle;
	m_coherence_buffer[m_coherence_buffer_write_ptr].type = type;
	m_coherence_buffer[m_coherence_buffer_write_ptr].in_L2 = in_L3;
	m_coherence_buffer[m_coherence_buffer_write_ptr].flag = FULL;
	m_coherence_buffer_write_ptr = (m_coherence_buffer_write_ptr+1)%Coherence_buffer_size;

	return true;
}
#endif

bool pseq_t::trifs_trace_translate_uops(struct TransOp* uop, instr_info_buffer_t *instr_info, uint64_t trifs_trace_virt_pc)
{
	uint32_t inst;
	inst = uop->opcode;

	instr_info->logical_pc = trifs_trace_virt_pc;

	instr_info->m_futype = uops_gems_map_table[inst].gems_futype;

	instr_info->m_dest_reg[0] = uop->rd + 1;
	instr_info->m_source_reg[0] = uop->ra + 1;
	instr_info->m_source_reg[1] = uop->rb + 1;
	instr_info->m_source_reg[2] = uop->rc + 1;

#ifdef TRANSFORMER_MISPREDICTION
	if(instr_info->m_futype == FU_BRANCH){
		instr_info->branch_taken_npc_pc = uop->riptaken;
		instr_info->branch_untaken_npc_pc = uop->ripseq;
	}
#endif
}

bool pseq_t::trifs_trace_translate_x86_inst_reset()
{
    memset(trifs_trace_uops_buf, 0, sizeof(trifs_trace_uops_buf));
    trifs_trace_num_uops = 0;
}

void pseq_t::code_cache_insert(int inst_num, uint64_t tb_addr){
	int tb_fetch_inst_num = inst_num;
	vector<instr_info_buffer_t> uops;
	correct_timing_fetch_ptr = 0;
	
	if(!bb_cache_map.count(tb_addr)){
		temp_cache = new code_cache();

		while(tb_fetch_inst_num){

				ana_instr_struct_t *x86_info_buf_entry = &(correct_qemu_instr_info_buf[correct_timing_fetch_ptr]);

				unsigned char trifs_trace_x86_buf[16];
				int x86_index = 0;
				for(; x86_index < 16; x86_index++){
					trifs_trace_x86_buf[x86_index] = (unsigned char)x86_info_buf_entry->ana_x86_inst[x86_index];
				}// */
				uint64_t virt_pc = x86_info_buf_entry->ana_x86_virt_pc;
				
#ifdef ANASIM_DEBUG
				assert(virt_pc != 0);
#endif
				trifs_trace_translate_x86_inst_reset();
				
				pthread_mutex_lock(&mutex);
				int num_uops = trifs_trace_ptlsim_decoder(
	                            trifs_trace_x86_buf,  16, //trifs_trace_num_bytes
	                            trifs_trace_uops_buf, &trifs_trace_num_uops, 
	                            virt_pc, true);
				pthread_mutex_unlock(&mutex);

#ifdef ANASIM_DEBUG
				if(trifs_trace_num_uops >= 64){
					assert(0);
				}
#endif
	                    	for(int j = 0; j < trifs_trace_num_uops; j++){
					instr_info_buffer_t *cur_uop = new instr_info_buffer_t();
	                     	trifs_trace_translate_uops(&(trifs_trace_uops_buf[j]), cur_uop, virt_pc);
					temp_cache->uops.push_back(*cur_uop);
	                    	}//*/
	                    	
				
				tb_fetch_inst_num--;
				
				correct_timing_fetch_ptr++;
			}
		bb_cache_map[tb_addr] = temp_cache;
		//uops = temp_cache->uops;
		//tb_num_uops = uops.size();
		//uops_index = tb_num_uops;
		correct_timing_fetch_ptr = 0;
	}
}

bool pseq_t::TRANS_previous_code_cache_lookup(int inst_num){
		int phy_proc = m_id;
		vector<instr_info_buffer_t> uops;
	
		int tb_fetch_inst_num = inst_num;
		uint64_t tb_addr;
#ifdef ANALYTICAL_MODEL_TIMING_SHIFT
		tb_addr = correct_qemu_tb_instr_buf->ana_tb_addr;
		//x86_eip = correct_qemu_tb_instr_buf->ana_eip;
#endif
		if(!bb_cache_map.count(tb_addr)){
			if(tb_fetch_inst_num == 0){
				not_cached_num++;
			}
			temp_cache = new code_cache();
			while(tb_fetch_inst_num){

				ana_instr_struct_t *x86_info_buf_entry = &(correct_qemu_instr_info_buf[correct_timing_fetch_ptr]);

				unsigned char trifs_trace_x86_buf[16];
				int x86_index = 0;
				for(; x86_index < 16; x86_index++){
					trifs_trace_x86_buf[x86_index] = (unsigned char)x86_info_buf_entry->ana_x86_inst[x86_index];
				}// */
				uint64_t virt_pc = x86_info_buf_entry->ana_x86_virt_pc;
				
#ifdef ANASIM_DEBUG
				assert(virt_pc != 0);
#endif
				trifs_trace_translate_x86_inst_reset();
				
				pthread_mutex_lock(&mutex);
				int num_uops = trifs_trace_ptlsim_decoder(
	                            trifs_trace_x86_buf,  16, //trifs_trace_num_bytes
	                            trifs_trace_uops_buf, &trifs_trace_num_uops, 
	                            virt_pc, true);
				pthread_mutex_unlock(&mutex);

#ifdef ANASIM_DEBUG
				if(trifs_trace_num_uops >= 64){
					assert(0);
				}
#endif
	                    	for(int j = 0; j < trifs_trace_num_uops; j++){
					instr_info_buffer_t *cur_uop = new instr_info_buffer_t();
	                     	trifs_trace_translate_uops(&(trifs_trace_uops_buf[j]), cur_uop, virt_pc);
					temp_cache->uops.push_back(*cur_uop);
	                    	}//*/
	                    	
				
				tb_fetch_inst_num--;
				
				correct_timing_fetch_ptr++;
			}
			bb_cache_map[tb_addr] = temp_cache;
			uops = temp_cache->uops;
			tb_num_uops = uops.size();
			uops_index = tb_num_uops;
			correct_timing_fetch_ptr = 0;
		}
}

bool pseq_t::TRANS_inorder_simpleLookupInstruction(instr_info_buffer_t * entry, int inst_num){
	int phy_proc = m_id;
	vector<instr_info_buffer_t> uops;

	if(uops_index <= 0){
		
		//trifs_trace_uops_tb_fetch_ptr = -1;
		
		if(fetch_return_ready){
			tb_fetch_end = 1;
			fetch_return_ready = 0;
			return false;
		}//*/

		correct_timing_fetch_ptr = 0;
		int tb_fetch_inst_num = inst_num;
		uint64_t tb_addr;
#ifdef ANALYTICAL_MODEL_TIMING_SHIFT
		tb_addr = correct_qemu_tb_instr_buf->ana_tb_addr;
		x86_eip = correct_qemu_tb_instr_buf->ana_eip;
#ifdef ANALYTICAL_MODEL_TRACE_LOG
		ttrifs_trace_log("%d %x ", phy_proc, tb_addr);
#endif
#endif
#ifdef ANALYTICAL_MODEL_TRACE_LOG
		ttrifs_trace_log("%d %x ", phy_proc, tb_addr);
#endif
		//assert(bb_cache_map.count(tb_addr));
		//timing_call_times_core++;
		/* if(!bb_cache_map.count(tb_addr)){
#ifdef ANALYTICAL_MODEL_TRACE_LOG
			ttrifs_trace_log("!cached ");
#endif
			if(tb_fetch_inst_num == 0){
				not_cached_num++;
			}
			temp_cache = new code_cache();
			while(tb_fetch_inst_num){

				ana_instr_struct_t *x86_info_buf_entry = &(correct_qemu_instr_info_buf[correct_timing_fetch_ptr]);

				unsigned char trifs_trace_x86_buf[16];
				int x86_index = 0;
				for(; x86_index < 16; x86_index++){
					trifs_trace_x86_buf[x86_index] = (unsigned char)x86_info_buf_entry->ana_x86_inst[x86_index];
				}/
				uint64_t virt_pc = x86_info_buf_entry->ana_x86_virt_pc;
				
				//printf("timing %d: %d %d %llx %llx %llx\n", phy_proc, tb_fetch_inst_num, correct_timing_fetch_ptr, &(correct_qemu_instr_info_buf[correct_timing_fetch_ptr]), virt_pc, x86_info_buf_entry->x86_inst[0]);
#ifdef ANASIM_DEBUG
				assert(virt_pc != 0);
#endif
				trifs_trace_translate_x86_inst_reset();
				
				pthread_mutex_lock(&mutex);
				int num_uops = trifs_trace_ptlsim_decoder(
	                            trifs_trace_x86_buf,  16, //trifs_trace_num_bytes
	                            trifs_trace_uops_buf, &trifs_trace_num_uops, 
	                            virt_pc, true);
				pthread_mutex_unlock(&mutex);

#ifdef ANASIM_DEBUG
				if(trifs_trace_num_uops >= 64){
					assert(0);
				}
#endif
	                    	for(int j = 0; j < trifs_trace_num_uops; j++){
					instr_info_buffer_t *cur_uop = new instr_info_buffer_t();
	                     	trifs_trace_translate_uops(&(trifs_trace_uops_buf[j]), cur_uop, virt_pc);
					temp_cache->uops.push_back(*cur_uop);
	                    	}
	                    	
				
				tb_fetch_inst_num--;
				
				correct_timing_fetch_ptr++;
			}
			bb_cache_map[tb_addr] = temp_cache;
			uops = temp_cache->uops;
			tb_num_uops = uops.size();
			uops_index = tb_num_uops;
#ifdef ANALYTICAL_MODEL_TRACE_LOG
			ttrifs_trace_log("%x %d ", bb_cache_map[tb_addr], tb_num_uops);
#endif
			//if(timing_call_times_core < 10000){
				//printf("%d %d !cached %x %x %d\n", phy_proc, timing_call_times_core, tb_addr,  bb_cache_map[tb_addr], tb_num_uops);
			//}

		}
		else{ */
#ifdef ANALYTICAL_MODEL_TRACE_LOG
			ttrifs_trace_log("cached ");
#endif
			temp_cache = bb_cache_map[tb_addr];
			uops = temp_cache->uops;
			tb_num_uops = uops.size();
			uops_index = tb_num_uops;
#ifdef ANALYTICAL_MODEL_TRACE_LOG
			ttrifs_trace_log("%x %d ", bb_cache_map[tb_addr], tb_num_uops);
#endif
			//if(timing_call_times_core < 10000){
				//printf("%d %d cached %x %x %d\n", phy_proc, timing_call_times_core, tb_addr,  bb_cache_map[tb_addr], tb_num_uops);
			//}
		//}
		//delete cur_uop;
		 //*/
#ifdef ANALYTICAL_MODEL_TRACE_LOG
		 ttrifs_trace_log("\n");
#endif
		
		//trifs_trace_uops_tb_fetch_ptr = 0;
		trifs_trace_mem_ptr = correct_qemu_mem_info_buf;
		tb_mem_num = correct_qemu_tb_instr_buf->ana_tb_mem_num;
		tb_mem_num_index = 0;
		same_logical_pc = 0;
		same_physical_addr = 0;
#ifdef ACCURACY_TEST
		printf("new tb: %d %d\n", tb_num_uops, uops_index);
#endif
	}

	if(tb_num_uops <= 0){
#ifdef ACCURACY_TEST
		printf("empty tb\n");
#endif
		tb_fetch_end = 1;
		fetch_return_ready = 0;
		correct_timing_fetch_ptr = 0;
		correct_mem_fetch_ptr = 0;
		trifs_trace_mem_ptr = correct_qemu_mem_info_buf;
		tb_mem_num_index = 0;
		return 0;
	}
	
	int cache_index = tb_num_uops - uops_index;

	entry->logical_pc = (temp_cache->uops)[cache_index].logical_pc;
	entry->m_futype = (temp_cache->uops)[cache_index].m_futype;
	
	//printf("timing %d fetch uop futype: %d pc: %llx ", phy_proc, entry->m_futype, entry->logical_pc);
	if((entry->m_futype == FU_RDPORT) || (entry->m_futype == FU_WRPORT)){
		entry->m_physical_addr = 0;
		//printf("tb_mem_num: %d\n", tb_mem_num);
#ifdef TRANSFORMER_GLOBAL_CACHE
		if(tb_mem_num_index < tb_mem_num){
			entry->m_physical_addr = trifs_trace_mem_ptr->ana_physical_addr;
					
			trifs_trace_mem_ptr++;
			tb_mem_num_index++;
			/*if(entry->logical_pc == same_logical_pc){
				entry->m_physical_addr = same_physical_addr;
			}
			else{
				while(trifs_trace_mem_ptr->ana_x86_virt_pc < entry->logical_pc){
					trifs_trace_mem_ptr++;
					tb_mem_num_index++;
					if(tb_mem_num_index == tb_mem_num){
						printf("while mem not found error! :%d\n", tb_mem_num);
					}
				}
				entry->m_physical_addr = trifs_trace_mem_ptr->ana_physical_addr;
			}*/
			
			/*if(entry->logical_pc == same_logical_pc){
				entry->m_physical_addr = same_physical_addr;
			}
			else{
				if(tb_mem_num_index < tb_mem_num){
					while(tb_mem_num_index < tb_mem_num){
						if((trifs_trace_mem_ptr->ana_x86_virt_pc == entry->logical_pc) | (trifs_trace_mem_ptr->ana_x86_virt_pc != 0)){
							break;
						}
						else{
							trifs_trace_mem_ptr++;
							tb_mem_num_index++;
						}
					}
					if(tb_mem_num_index == tb_mem_num){
						printf("while mem not found error! :%d\n", tb_mem_num);
					} 
					entry->m_physical_addr = trifs_trace_mem_ptr->ana_physical_addr;
					
					trifs_trace_mem_ptr++;
					tb_mem_num_index++;
				}
				else{
					printf("second mem not found error!\n");
					assert(0);
				}
			}// */

			same_logical_pc = entry->logical_pc;
			same_physical_addr = entry->m_physical_addr; //
		}
		else{
			//printf("first mem not found error!\n");
			//assert(0);
			mem_zero_num++;
		}//*/
#endif
	}
	else{
		entry->m_physical_addr = 0;
	}
	
	#ifdef TRANSFORMER_MISPREDICTION
	if(entry->m_futype == FU_BRANCH){
		entry->m_branch_result = 0;
		if(x86_eip == (temp_cache->uops)[cache_index].branch_taken_npc_pc){
			entry->m_branch_result += 1;
		}
		if(x86_eip == (temp_cache->uops)[cache_index].branch_untaken_npc_pc){
			entry->m_branch_result += 2;
		}
	}
	#endif
	
	for (int i = 0; i < QEMU_SI_MAX_SOURCE; i++){			
		entry->m_source_reg[i] = ((temp_cache->uops)[cache_index].m_source_reg[i])%16 + 8;
	}
	for (int i = 0; i < QEMU_SI_MAX_DEST; i++){
		entry->m_dest_reg[i] = ((temp_cache->uops)[cache_index].m_dest_reg[i])%16 + 8;
	}//*/

#ifdef ANASIM_DEBUG
	if(entry->logical_pc == 0){
		printf("pc: %llx\n", entry->logical_pc);
	}
	assert(entry->logical_pc != 0);
#endif
	m_next_pc[correct_timing_release_ptr] = (int)entry->logical_pc;
	m_next_fu_type[correct_timing_release_ptr] = entry->m_futype;
	correct_timing_release_ptr = (correct_timing_release_ptr + 1) % CORRECT_RETIRE_BUF_SIZE;
	uops_index--;//*/
	//trifs_trace_uops_tb_fetch_ptr++;

	if(uops_index <= 0){
#ifdef ACCURACY_TEST
		printf("last inst: %d\n", entry->m_futype);
#endif
		fetch_return_ready = 1;
		correct_timing_fetch_ptr = 0;
		correct_mem_fetch_ptr = 0;
		trifs_trace_mem_ptr = correct_qemu_mem_info_buf;
		tb_mem_num_index = 0;
	}
	return true;
}

void pseq_t::log_print_to_file(int inst_num)
{
	ttrifs_trace_log("tb_start*************************%d\n", inst_num);
}

bool pseq_t::m_calculate(int inst_num)
{

	
	int phy_proc = m_id;
	int proc = 0;
//printf("timing before %d\n", phy_proc);
		// If pipeline is stalled, we do not fetch and just return
		if(!tb_last_inst){
			if (m_inorder_x86_retire_info->inorder_skip_cycle > 0){

				for(int tt = 0; tt < m_inorder_x86_retire_info->inorder_skip_cycle; tt++){
					uint64_t cycle_offset = m_local_cycles % INORDER_MAX_ESTIMATE_INST_NUM;
					m_inorder_execute_inst_width[cycle_offset] = 0;
					for (int mm = 0; mm < FU_NUM_FU_TYPES; mm++){
						m_inorder_function_unit_width[cycle_offset][mm] = 0;
					}
					m_local_cycles++;
				}
			} //*/
		}
//printf("aaa: %d\n", phy_proc);
		//if(fetch_return_ready){
		//	inorder_dispatch_inst_count = dispatch_reserve;
		//}
#ifdef ACCURACY_TEST
		printf("inner before %d inorder_dispatch_inst_count: %d rob: %d skip_cycle: %d local_cycle: %d\n", tb_last_inst, inorder_dispatch_inst_count, m_inorder_fetch_rob_count, m_inorder_x86_retire_info->inorder_skip_cycle, m_local_cycles);
		if(tb_last_inst){
			tb_last_inst = 0;
			inorder_dispatch_inst_count = last_tb_fetch_num;
		}
		else
#endif
		{
			inorder_dispatch_inst_count = MAX_FETCH;
			m_inorder_x86_retire_info->inorder_skip_cycle = 1;
		}
		//printf("m_inorder_fetch_rob_count: %d\n", m_inorder_fetch_rob_count);
		while (inorder_dispatch_inst_count > 0) {
#ifdef ANALYTICAL_MODEL_TRACE_LOG
		ttrifs_trace_log("%d inorder_dispatch_inst_count: %d, m_inorder_fetch_rob_count: %d\n", phy_proc, inorder_dispatch_inst_count, m_inorder_fetch_rob_count);
#endif
			if (m_inorder_fetch_rob_count < INORDER_MAX_STALL_INST_NUM) {
					
				// fetch
				
				instr_info_buffer_t entry;
				//printf("aaa\n");
				//pthread_mutex_lock(&mutex);
				bool fetch_ok = TRANS_inorder_simpleLookupInstruction(&entry, inst_num);
#ifdef ANALYTICAL_MODEL_TRACE_LOG
				ttrifs_trace_log("pc: %x, fu_type: %d\n", entry.logical_pc, entry.m_futype);
#endif
#ifdef DUMP_INSTRUCTION_FLOW
				//assert(entry.m_futype < 13);
				if(dump_ins_num > 0){
					if((entry.m_futype < 13) && (entry.m_futype >= 0)){
						//printf("%d %d %d %d %d %d %d %d %d %d\n", (int)entry.logical_pc, (int)entry.m_futype, (int)entry.m_source_reg[0], (int)entry.m_source_reg[1], (int)entry.m_source_reg[2], (int)entry.m_dest_reg[0], 
						//(int)entry.m_physical_addr, (int)entry.branch_taken_npc_pc, (int)entry.branch_untaken_npc_pc, (int)entry.m_branch_result);
						dumptrifs_trace_log("%d %d %d %d %d %d %d %d %d %d\n", (int)entry.logical_pc, (int)entry.m_futype, (int)entry.m_source_reg[0], (int)entry.m_source_reg[1], (int)entry.m_source_reg[2], (int)entry.m_dest_reg[0], 
						(int)entry.m_physical_addr, (int)entry.branch_taken_npc_pc, (int)entry.branch_untaken_npc_pc, (int)entry.m_branch_result);
						//ins_log_file << to_string(entry.logical_pc) << " " << entry.m_futype << " " << (int)entry.m_source_reg[0] << " " << (int)entry.m_source_reg[1] << " " << (int)entry.m_source_reg[2] << " " << (int)entry.m_dest_reg[0] << " " << 
						//(int)entry.m_physical_addr << " " << (int)entry.branch_taken_npc_pc << " " << (int)entry.branch_untaken_npc_pc << " " << (int)entry.m_branch_result << endl;
						dump_ins_num--;
					}
				}
				else{
					exit(0);
				}
#endif
				//pthread_mutex_unlock(&mutex);
				//printf("bbb\n");
				//direct continue test by whj
				/*if(zmf_finished == TRUE){
					printf("Return inside waiting loop.\n");
					return;
				}
				continue;
				//break;//*/
				//if(fetch_return_ready){
				//	break;
				//}

				if(tb_fetch_end){
#ifdef ACCURACY_TEST
					printf("tb end: %d\n", entry.m_futype);
#endif
					last_tb_fetch_num = inorder_dispatch_inst_count;
					tb_last_inst = 1;
					tb_fetch_end = 0;
					return true;
				}
				//continue;
				
				/*if (fetch_ok == false){
					inorder_wait_num++;
					//printf("bbb\n");
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
				uint64_t fetch_cycle = m_local_cycles;

				// schedule
				uint64_t max_value = fetch_cycle;
				
				 for (int i = 0; i < QEMU_SI_MAX_SOURCE; i++) {
#ifdef ANASIM_DEBUG
				 	assert(entry.m_source_reg[i] > 0);
					assert(entry.m_source_reg[i] < QEMU_NUM_REGS);
#endif
					if (entry.m_source_reg[i] !=0) {
						uint64_t finish_cycle = m_inorder_register_finish_cycle[entry.m_source_reg[i]] + 1;
						if (max_value < finish_cycle) {
							max_value = finish_cycle;
						}
					}
				}
				// WHJ lsq
				#ifdef TRANSFORMER_GLOBAL_CACHE
				 if(entry.m_futype== FU_RDPORT){
					uint64_t temp_addr = entry.m_physical_addr;
					if(cal_lsq_store_queue_ptr){
						if(temp_addr == cal_lsq_store_queue_0.store_address){
							if(max_value < cal_lsq_store_queue_0.store_finish_cycle){
								max_value = cal_lsq_store_queue_0.store_finish_cycle;
							}
						}
						else if(temp_addr == cal_lsq_store_queue_1.store_address){
							if(max_value < cal_lsq_store_queue_1.store_finish_cycle){
								max_value = cal_lsq_store_queue_1.store_finish_cycle;
							}
						}
				 	}
					else{
						if(temp_addr == cal_lsq_store_queue_1.store_address){
							if(max_value < cal_lsq_store_queue_1.store_finish_cycle){
								max_value = cal_lsq_store_queue_1.store_finish_cycle;
							}
						}
						else if(temp_addr == cal_lsq_store_queue_0.store_address){
							if(max_value < cal_lsq_store_queue_0.store_finish_cycle){
								max_value = cal_lsq_store_queue_0.store_finish_cycle;
							}
						}
					}
				} // */
				#endif
				uint64_t issue_cycle = max_value;

				// Execute stage
		                // We do two things. 
		                // 1. If current cycle has executed 4 instructions, we execute this instr. in next cycle.
		                uint64_t execute_offset = issue_cycle % INORDER_MAX_ESTIMATE_INST_NUM;
		                 while (m_inorder_execute_inst_width[execute_offset] >= MAX_EXECUTE) {
		                    execute_offset++;
		                    if (execute_offset >= INORDER_MAX_ESTIMATE_INST_NUM) {
		                        execute_offset = 0; 
		                    }
#ifdef ANASIM_DEBUG
		                    assert(execute_offset != (issue_cycle % INORDER_MAX_ESTIMATE_INST_NUM));
#endif
		                }    
		                m_inorder_execute_inst_width[execute_offset]++;
		                // 2. If FunctionUnit is used in current cycle, we execute this instr. in next cycle.
		                while (m_inorder_function_unit_width[execute_offset][fu_mapped] >= CONFIG_NUM_ALUS[fu_mapped]) {
		                    execute_offset++;
		                    if (execute_offset >= INORDER_MAX_ESTIMATE_INST_NUM) {
		                        execute_offset = 0; 
		                    }
#ifdef ANASIM_DEBUG
		                    assert(execute_offset != (issue_cycle % INORDER_MAX_ESTIMATE_INST_NUM));
#endif
					m_inorder_execute_inst_width[execute_offset]++;
		                }
#ifdef ANASIM_DEBUG
				assert(fu_mapped < 9);
				assert(fu_mapped >= 0);
#endif
				  //m_inorder_execute_inst_width[execute_offset]++;
		                m_inorder_function_unit_width[execute_offset][fu_mapped]++; //*/
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
					if ((fu_type == FU_RDPORT) || (fu_type == FU_WRPORT)) {
						
						uint64_t addr = entry.m_physical_addr;
						int thread_id = phy_proc;
						//printf("aaa\n");
						if (CAT_pseudo_line_insert(addr, execute_cycle))
						{
		    				inorder_l1_dcache_miss_count++;
				    	    if (CAT_pseudo_L2_line_insert(addr, execute_cycle)){
                                inorder_l2_cache_miss_count++;
                                int in_L3 = CAT_pseudo_L3_tag_search(addr);
						    	if(in_L3) 
							    {
								    inorder_l3_cache_hit_count++;
    								CAT_pseudo_L3_line_insert(addr, execute_cycle);
	    						}else{
		    						//l2 cache miss
			    					inorder_l3_cache_miss_count++;
				    				cache_cycle += INORDER_MEMORY_LATENCY;
					    		}
    
	    						// l1 cache miss
			    				cache_cycle += INORDER_L3_CACHE_LATENCY;
							
				    			#ifdef TRANSFORMER_USE_CACHE_COHERENCE
					    			#ifdef TRANSFORMER_USE_CACHE_COHERENCE_buffer
						    			if (fu_type == FU_WRPORT){
							    			m_write_coherence_buffer(addr, Coherence_store_miss, execute_cycle, in_L3);
								    	}else if (fu_type == FU_RDPORT){
									    	m_write_coherence_buffer(addr, Coherence_load_miss, execute_cycle, in_L3);
    									}
	    							#endif
		    					#endif
						    }else{
    							inorder_l2_cache_hit_count++;
	    						
		    					#ifdef TRANSFORMER_USE_CACHE_COHERENCE
			    					#ifdef TRANSFORMER_USE_CACHE_COHERENCE_buffer
				    					if (fu_type == FU_WRPORT){
					    					m_write_coherence_buffer(addr, Coherence_store_hit, execute_cycle, 0);
						    			}
							    	#endif
    							#endif
                            }
						    cache_cycle += INORDER_L2_CACHE_LATENCY;
                        }else {
							inorder_l1_dcache_hit_count++;
							
							#ifdef TRANSFORMER_USE_CACHE_COHERENCE
								#ifdef TRANSFORMER_USE_CACHE_COHERENCE_buffer
									if (fu_type == FU_WRPORT){
										m_write_coherence_buffer(addr, Coherence_store_hit, execute_cycle, 0);
									}
								#endif
							#endif
						} 
						//printf("fff\n");
						cache_cycle += INORDER_L1_CACHE_LATENCY;
					}
					penalty_cycle += cache_cycle;
				#endif //*/

				#ifdef TRANSFORMER_GLOBAL_CACHE
					uint64_t cache_cycle = 0;
					if ((fu_type == FU_RDPORT) || (fu_type == FU_WRPORT)) {
						//uint64_t addr = s_instr->m_physical_addr;
						pthread_mutex_lock(&mutex);
						uint64_t addr = entry.m_physical_addr;
						int thread_id = phy_proc;
						//uint64_t l1_target_cycle = CAT_pseudo_line_get_target_cycle(addr, thread_id);
						//if (CAT_pseudo_line_insert(addr, thread_id) || (l1_target_cycle > execute_cycle))
						if (CAT_pseudo_line_insert(addr, thread_id, execute_cycle))
						{
							//uint64_t l2_target_cycle = CAT_pseudo_L2_get_target_cycle(addr);
							//if (/*(!CAT_in_L1_cache(addr)) &&*/ !CAT_pseudo_L2_tag_search(addr) || (l2_target_cycle > execute_cycle)) 
							if ( !CAT_pseudo_L2_tag_search(addr)) 
							{
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
									if (fu_type == FU_WRPORT){
										CAT_pseudo_access_coherence_buffer(addr, thread_id, Coherence_store_miss, execute_cycle);
									}else if (fu_type == FU_RDPORT){
										CAT_pseudo_access_coherence_buffer(addr, thread_id, Coherence_load_miss, execute_cycle);
									}
								#else
									if (fu_type == FU_WRPORT){
										CAT_pseudo_access_directory(addr, thread_id, Coherence_store_miss, execute_cycle);
									}else if (fu_type == FU_RDPORT){
										CAT_pseudo_access_directory(addr, thread_id, Coherence_load_miss, execute_cycle);
									}
								#endif
							#endif
						} else {
							inorder_l1_dcache_hit_count++;
							//cache_cycle += INORDER_L1_CACHE_LATENCY;

							#ifdef TRANSFORMER_USE_CACHE_COHERENCE
								#ifdef TRANSFORMER_USE_CACHE_COHERENCE_buffer
									if (fu_type == FU_WRPORT){
										CAT_pseudo_access_coherence_buffer(addr, thread_id, Coherence_store_hit, execute_cycle);
									}
								#else
									if (fu_type == FU_WRPORT){
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
				if (entry.m_dest_reg[0] != 0) {
					m_inorder_register_finish_cycle[entry.m_dest_reg[0]] = finish_cycle;
				} //*/

				// Added by WHJ 2013.06.25 LSQ
				//WHJ lsq
				#ifdef TRANSFORMER_GLOBAL_CACHE
				 if(fu_type == FU_WRPORT){
				 	if(cal_lsq_store_queue_ptr){
						cal_lsq_store_queue_ptr = 0;
						cal_lsq_store_queue_1.store_address = entry.m_physical_addr;
						cal_lsq_store_queue_1.store_finish_cycle = finish_cycle;
				 	}
					else{
						cal_lsq_store_queue_ptr = 1;
						cal_lsq_store_queue_0.store_address = entry.m_physical_addr;
						cal_lsq_store_queue_0.store_finish_cycle = finish_cycle;
				 	}
				}// */
				#endif

				// retire
				uint64_t retire_cycle = finish_cycle + 1;
				int index = (correct_timing_release_ptr - 1 + CORRECT_RETIRE_BUF_SIZE) % CORRECT_RETIRE_BUF_SIZE;
#ifdef ANASIM_DEBUG
				assert(index >= 0);
				assert(index < CORRECT_RETIRE_BUF_SIZE);
				assert(correct_timing_release_ptr < CORRECT_RETIRE_BUF_SIZE);
#endif
				m_inorder_retire_inst_cycle[index] = retire_cycle;

#ifdef WHJ_TEST
				total_uops_num++;
				if(total_uops_num < TEST_UOPS_THREDSHOLD){
					ttrifs_trace_log("%d,	\tpc: %llx, \tfu: %d,  \tsource: %d : %d : %d,  \tdest: %d, \tfetch: %lld, \tissue: %lld, \texecute: %lld, \tfinish: %lld, \tretire: %lld, \tlocal: %lld, \tmem_add: %llx\n", 
						total_uops_num, entry.logical_pc, entry.m_futype,
						entry.m_source_reg[0], entry.m_source_reg[1], entry.m_source_reg[2], entry.m_dest_reg[0], fetch_cycle, issue_cycle, execute_cycle, finish_cycle, retire_cycle, m_local_cycles, entry.m_physical_addr);
				}
#endif

				#ifdef TRANSFORMER_INORDER_BRANCH
				if (fu_type == FU_BRANCH) {
					inorder_branch_count++;
					uint64_t fetch_pc = entry.logical_pc;
					bool taken = m_predictor->Predict(fetch_pc, m_spec_bpred->cond_state, 0, phy_proc);
#ifdef ACCURACY_TEST
					printf("branch: %d\n", fu_type);
#endif
					m_spec_bpred->cond_state = ((m_spec_bpred->cond_state << 1) | (taken & 0x1));
                         		if (!((taken && (entry.m_branch_result & 0x1)) || ((!taken) && (entry.m_branch_result & 0x2)))) 
					{
#ifdef ACCURACY_TEST
						printf("branch break: %d\n", fu_type);
#endif
						inorder_branch_misprediction_count++;
						m_inorder_x86_retire_info->inorder_skip_cycle += finish_cycle - fetch_cycle + INORDER_MISPREDICTION_LATENCY;
						break;
					}
				}
				#endif

			}
			
			inorder_dispatch_inst_count--;

			//if(fetch_return_ready){
					//fetch_return_ready = 0;
				//break;
			//}
		}

#ifdef ACCURACY_TEST
		printf("inner after %d inorder_dispatch_inst_count: %d rob: %d skip_cycle: %d local_cycle: %d\n", tb_last_inst, inorder_dispatch_inst_count, m_inorder_fetch_rob_count, m_inorder_x86_retire_info->inorder_skip_cycle, m_local_cycles);
#endif
		//if(fetch_return_ready){
					//fetch_return_ready = 0;
		//	return true;
		//}

#ifdef ANALYTICAL_MODEL_TRACE_LOG
		ttrifs_trace_log("correct_timing_ptr: %d, m_local_cycles: %d\n", correct_timing_ptr, m_local_cycles);
#endif
//printf("ccc\n");
//printf("bbb: %d\n", phy_proc);
		// Added by minqh 2013.06.20
		// Do not use loop here. If Branch (or other reason) stalls pipeline, we do not process the stall here.
		//while (inorder_x86_retire_info[phy_proc][proc]->inorder_skip_cycle > 0) 
		if (m_inorder_x86_retire_info->inorder_skip_cycle > 0)
	        {
	            // commit a whole X86 instruction
	          
	            while ((m_inorder_retire_inst_cycle[correct_timing_ptr] > 0) &&
	                (m_inorder_retire_inst_cycle[correct_timing_ptr] != (uint64_t)-1) &&
	            (m_inorder_retire_inst_cycle[correct_timing_ptr] <= m_local_cycles)) {
	     
	                m_inorder_retire_inst_cycle[correct_timing_ptr] = (uint64_t)-1;
	                int next_pc = m_next_pc[correct_timing_ptr];
	                int next_fu_type = m_next_fu_type[correct_timing_ptr];
	 
	                correct_timing_ptr = (correct_timing_ptr + 1) % CORRECT_RETIRE_BUF_SIZE;
	                m_local_instr_count++;

#ifdef ANASIM_DEBUG
	 		assert(m_inorder_x86_retire_info->inorder_last_pc != 0);
#endif
	                if ((next_pc == m_inorder_x86_retire_info->inorder_last_pc) 
	                && (FU_BRANCH != m_inorder_x86_retire_info->inorder_last_fu_type)) {
	                    m_inorder_x86_retire_info->inorder_uop_sum++;
	                } else {
	                    m_inorder_fetch_rob_count -= m_inorder_x86_retire_info->inorder_uop_sum;
#ifdef ANASIM_DEBUG
	                    assert(m_inorder_fetch_rob_count >= 0);
#endif
	                    m_inorder_x86_retire_info->inorder_uop_sum = 1;
	                }
	                m_inorder_x86_retire_info->inorder_last_pc = next_pc;
	                m_inorder_x86_retire_info->inorder_last_fu_type = next_fu_type;
	            }// */
	 
	             uint64_t cycle_offset = m_local_cycles % INORDER_MAX_ESTIMATE_INST_NUM;
#ifdef ANASIM_DEBUG
				assert(cycle_offset >= 0);
				assert(cycle_offset < INORDER_MAX_ESTIMATE_INST_NUM);
#endif
	            m_inorder_execute_inst_width[cycle_offset] = 0;
	            for (int mm = 0; mm < FU_NUM_FU_TYPES; mm++){
	                m_inorder_function_unit_width[cycle_offset][mm] = 0;
	            } 
 
	            //// global information
	            m_local_cycles++;
	 	//printf("m_local_cycles: %d\n", m_local_cycles);
	            m_inorder_x86_retire_info->inorder_skip_cycle--;
		} //*/
#ifdef ANALYTICAL_MODEL_TRACE_LOG
		ttrifs_trace_log("end: correct_timing_ptr: %d, m_local_cycles: %d\n", correct_timing_ptr, m_local_cycles);
#endif
	//#endif
		//inorder_dispatch_inst_count = MAX_FETCH;
//printf("timing end %d\n", phy_proc);
//printf("ddd\n");
		return false;
}

/*void transformer_inorder_timing(){
	
	while (true) {
	  for ( int i = 0; i < global_cpu_count; i++ ) {
		  
		m_seq[i]->m_calculate();
	
		if (zmf_finished == TRUE){
		  break;
		}
	  }
	  if (zmf_finished == TRUE){
		  break;
	  }
	}

}*/


/* int transformer_inorder_qemu_function(int first_phy_proc){

	// Step qemu
	//int timing_fetch_ptr = correct_timing_fetch_ptr[first_phy_proc][0];

	int loop_count = 0;
	while (1){
		int ret = trifs_trace_cpu_exec(first_phy_proc);
		
		if (ret == -1){
			printf(" Step qemu failes!\n");
			assert(0);
		}else if (ret == 1){
			printf(" Qemu terminates!\n");
			fflush(stdout);
			zmf_finished = TRUE;
			return QEMU_ADVANCE_TERMINATE;
		}

		return 0;

	}

} */

/*extern int max_tb_per_exec;
int total_max_tb_per_exec; */

/* void * transformer_inorder_qemu_function_thread (void * arg){
	#ifdef PARALLELIZED_TIMING_BY_CORE
	set_cpu(0);
	#endif

	#ifdef PARALLELIZED_FUNCTIONAL_AND_TIMING
	
	while(1){
		if (zmf_finished==TRUE){
			printf("Functional break out of execution loop. ZMF_FINISHED = TRUE\n");
			break;
		}
		
		// Check all cores
		for (int i = 0; i < global_cpu_count; i++){
			// If instr. buffer is empty, call qemu_function
			int phy_proc = i;
			
			transformer_inorder_qemu_function(phy_proc);
			
		}
	}
	#else
		//sequential version functional only
		while(1){
			if (zmf_finished==TRUE){
				printf("Functional break out of execution loop. ZMF_FINISHED = TRUE\n");
				break;
			}
			
			// Check all cores
			for (int i = 0; i < global_cpu_count; i++){
				// If instr. buffer is empty, call qemu_function
				int phy_proc = i;
				int proc = 0;
				transformer_inorder_qemu_function(phy_proc);
			}
		}
	#endif
} */

/*void * transformer_inorder_timing_thread (void * arg){
	// For convenience, we use zmf_timing directly.
	transformer_inorder_timing();
}

void * transformer_inorder_cache_thread (void * arg){

	printf("Simulating shared cache, pid = %u, thread id = %u\n", (unsigned int)getpid(), (int)pthread_self());
	
	#ifndef PARALLIZED_SHARED_CACHE
	printf(" Init cache thread needs to open PARALLIZED_SHARED_CACHE macro.\n");
	fflush(stdout);
	assert(0);
	#endif

	while (1){
		#ifdef PARALLIZED_SHARED_CACHE
		CAT_read_coherence_buffer();
		#endif
		
		if (zmf_finished == TRUE){
			break;
		}
	}

	printf(" Shared cache thread terminates.\n");
	fflush(stdout);
} */

/*#ifdef PARALLELIZED_TIMING_BY_CORE
void * transformer_inorder_timing_core_thread_simple(void * arg){
	//printf("start\n");
	int thread_id = ((timing_core_thread_arg *)arg)->core_id;
	int base_core_id = thread_id * trans_core_per_thread;
	
	set_cpu(thread_id + 1);
	printf("Simulating thread %d, base_core_id=%d, pid = %u, thread id = %u\n", thread_id, base_core_id, (unsigned int)getpid(), (int)pthread_self());

	while (true) {
		//printf("timing start\n");
		m_seq[base_core_id]->m_calculate();
		//printf("timing end\n");
		if (zmf_finished == TRUE){
			printf("thread: %d , calculation wait num: %lld\n", thread_id, m_seq[base_core_id]->calculation_wait_num);
		  break;
		}
	}

}
#endif*/
	// endif for by-core parallelization



/*void   stepInorder()
{
	
	printf(" Init correct instr buffer ok.\n");

	pthread_mutex_init(&mutex, NULL);

	
	#ifdef TRANSFORMER_INORDER_CACHE
		init_cache();
	#endif


	// Version control
	printf("\nVersion: Calculation-based simulator (inorder).\n");
	printf("----------Configuration---------------\n");

	printf("\t[CBS] m_numSMTProcs : %d | ", global_cpu_count);
	printf("QEMU_NUM_REGS : %d \n", QEMU_NUM_REGS);
	printf("FU_NUM_FU_TYPES : %d \n", FU_NUM_FU_TYPES);
	for (int m = 0; m < FU_NUM_FU_TYPES; m++){
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
			printf("[P] Parallelized Timing by core.\n\tcore_per_thread=%d\n", trans_core_per_thread);
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
					printf("\tUnbounded core synchronization.\n");
				#endif
			#endif
		#endif
	#else
		printf("-----------Sequential Version---------------\n");
	#endif

	if (global_cpu_count== 1){
		printf("---------Single Core Version-----------\n");
	}else{
		printf("---------Multi Core Version------------\n");
		printf("\t[M] Simulating %d cores.\n", global_cpu_count);
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
		
		printf("\t[C] L1 assoc_bits %d set_bits %d block_bits %d latency: %d\n", DL1_ASSOC, DL1_SET_BITS, DL1_BLOCK_BITS, INORDER_L1_CACHE_LATENCY);
		printf("\t[C] L2 assoc_bits %d set_bits %d block_bits %d latency: %d\n", L2_ASSOC, L2_SET_BITS, L2_BLOCK_BITS, INORDER_L2_CACHE_LATENCY);
		printf("\t[C] MEM cache latency : %d \n", INORDER_MEMORY_LATENCY);
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

		int num_sim_threads = global_cpu_count;
	  
			  // Qemu version
	  
			  // 1. Initialize parallelized models, like cache ...
			#ifdef PARALLIZED_SHARED_CACHE
				  // Parallel cache
				  pthread_t cache_tid;
				  // Start up the cache_thread
				  pthread_create(&cache_tid, NULL, transformer_inorder_cache_thread, NULL);//*/
			/*#endif
	  
			  // 2. Initialze parallelized functional and timing
			#ifdef PARALLELIZED_FUNCTIONAL_AND_TIMING
	  
				  
				#ifdef PARALLELIZED_TIMING_BY_CORE
				  assert(global_cpu_count%(trans_core_per_thread) == 0);
				  num_sim_threads = global_cpu_count/(trans_core_per_thread);
				  timing_tid = (pthread_t *)malloc(num_sim_threads * sizeof(pthread_t));
				  timing_core_thread_arg *timing_arg = (timing_core_thread_arg *)malloc(num_sim_threads * sizeof(timing_core_thread_arg));
				  for (int i = 0; i < num_sim_threads; i++){
					  timing_arg[i].core_id = i;
					  pthread_create(&(timing_tid[i]), NULL, transformer_inorder_timing_core_thread_simple, (void *)&(timing_arg[i])); 	
				  }//*/
	  
				/*#else
			  
				  //pthread_t timing_tid;
				  pthread_create(&timing_tid, NULL, transformer_inorder_timing_thread, NULL);	
				#endif
	  
				  //transformer_inorder_qemu_function_thread(NULL);
			#else
				  transformer_inorder_timing();
			#endif
	  
	  
	  
			  // 3. Simulation finished, check status.
			  if (zmf_finished !=TRUE){
				  printf("Invalid termination!!!!\n");
				  assert(0);
			  }
	  
			  // 4. Cleanup jobs: Join created threads
			#ifdef PARALLELIZED_FUNCTIONAL_AND_TIMING
				  // Join timing thread
				#ifdef PARALLELIZED_TIMING_BY_CORE
					  for (int i = 0; i < num_sim_threads; i++){
						  pthread_join((timing_tid[i]),NULL);
					  }
				#else
					  pthread_join(timing_tid,NULL);
				#endif//*/
			/*#endif
			  
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

	
  
	assert(zmf_finished == TRUE);
	if (zmf_finished == TRUE) { 
		#ifdef TRANSFORMER_USE_CACHE_COHERENCE
		CAT_pseudo_dump_coherence_stat();
		#endif
		
		printf("\n[Calculation-based simulator]-Wait_Number: ");
		for(int j = 0; j < global_cpu_count; j++){
			printf("\t%lld", m_seq[j]->inorder_wait_num);
		}
		printf("\n[Calculation-based simulator]-Instructions: ");
		uint64_t minqh_total_instr = 0;
		for (int j = 0; j < global_cpu_count; j++) {
			printf("\t%lld", m_seq[j]->m_local_instr_count);
			minqh_total_instr += m_seq[j]->m_local_instr_count;
		}
		printf("\n[Calculation-based simulator]-Total instructions %lld\n", minqh_total_instr);
		printf("[Calculation-based simulator]-Cycles: ");
		for (int j = 0; j < global_cpu_count; j++) {
			#ifdef PARALLIZED_SHARED_CACHE
			printf("\t%lld", *(m_seq[j]->L2_hit_increase_back));
			printf("\t%lld",(m_seq[j]->m_local_cycles - *(m_seq[j]->L2_hit_increase_back) * INORDER_MEMORY_LATENCY));
			#else
			printf("\t%lld",m_seq[j]->m_local_cycles);
			#endif
		}
		printf("\n");
		
		#ifdef TRANSFORMER_INORDER_BRANCH 
		uint64_t total_inorder_branch_count = 0;
		uint64_t total_inorder_branch_misprediction_count = 0;
		for (int j = 0; j < global_cpu_count; j++) {
			total_inorder_branch_count += m_seq[j]->inorder_branch_count;
			total_inorder_branch_misprediction_count += m_seq[j]->inorder_branch_misprediction_count;
		}
		printf("[Calculation-based simulator]-Branches: Total %lld  Mispred %lld  Mispred-Percentage %0.2f\%  \n", 
			total_inorder_branch_count, total_inorder_branch_misprediction_count, total_inorder_branch_misprediction_count/(total_inorder_branch_count+0.0)*100);
		#endif
		
		#ifdef TRANSFORMER_INORDER_CACHE 
		uint64_t total_inorder_l1_icache_hit_count = 0;
		uint64_t total_inorder_l1_icache_miss_count = 0;
		uint64_t total_l1_dcache_hit_count = 0;
		uint64_t total_l1_dcache_miss_count = 0;
		uint64_t total_l2_dcache_hit_count = 0;
		uint64_t total_l2_dcache_miss_count = 0;
		for (int j = 0; j < global_cpu_count; j++) {
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
		printf("[Calculation-based simulator]-Caches: L1-I hit %lld miss %lld L1-D hit %lld miss %lld L2 hit %lld miss %lld \n", 
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
*/


extern "C"
{
	void ana_timing_init(int cpu_count){
		printf("ZMF_init_direct: cpu_count=%d. Currently, we simulate only %d core(s).\n", cpu_count, cpu_count);
		global_cpu_count = cpu_count;
		pthread_mutex_init(&mutex, NULL);
		qemu_instr_buffer_size = MAX_TRIFS_TRACE_BUF_SIZE;
		pthread_mutex_init(&mutex, NULL);

	
	#ifdef TRANSFORMER_GLOBAL_CACHE
		init_cache();
	#endif
		//sleep(10);
		m_seq = (pseq_t **)malloc(global_cpu_count * sizeof(pseq_t *));

		printf(" Init OK. qemu_instr_buffer_size=%d, entry_size=%d\n", qemu_instr_buffer_size, sizeof(x86_instr_info_buffer_t));
	}

	void ana_timing_init_core(int cpu_index){
		m_seq[cpu_index] = new pseq_t(cpu_index);
#ifdef DUMP_INSTRUCTION_FLOW
		//ins_log_file.open("dump_instruction.txt", ofstream::out);
#endif
	}
	
	/*void zmf_init(int cpu_count){

		printf("ZMF_init20: cpu_count=%d. Currently, we simulate only %d core(s).\n", cpu_count, cpu_count);
		global_cpu_count = cpu_count;
		printf("global_cpu_count: %d\n", global_cpu_count);

		//sleep(10);
		pthread_mutex_init(&mutex, NULL);

		// Init buffer
		//qemu_instr_buffer_size = trifs_trace_shared_buf_size();
		//printf("global_cpu_count: %d\n", global_cpu_count);
		//system_t::inst = new system_t(cpu_count);
		m_seq = (pseq_t **)malloc(global_cpu_count * sizeof(pseq_t *));
		for (int i = 0; i < global_cpu_count; i++){
			m_seq[i] = new pseq_t(i);
		}
		
		//stepInorder();
		
		printf(" Init OK. qemu_instr_buffer_size=%d, entry_size=%d\n", qemu_instr_buffer_size, sizeof(x86_instr_info_buffer_t));
	}

	void zmf_init_total(int cpu_count){
		printf("ZMF_init_total: cpu_count=%d. Currently, we simulate only %d core(s).\n", cpu_count, cpu_count);
		global_cpu_count = cpu_count;
		pthread_mutex_init(&mutex, NULL);
		qemu_instr_buffer_size = MAX_TRIFS_TRACE_BUF_SIZE;

		m_seq = (pseq_t **)malloc(global_cpu_count * sizeof(pseq_t *));

		printf(" Init OK. qemu_instr_buffer_size=%d, entry_size=%d\n", qemu_instr_buffer_size, sizeof(x86_instr_info_buffer_t));
	}
	
	void zmf_init_core(int cpu_index){

		printf("ZMF_init_core: cpu_index=%d. \n", cpu_index);
		
		m_seq[cpu_index] = new pseq_t(cpu_index);
		//stepInorder();
		
		printf(" Init %d OK\n", cpu_index);
	}*/

	void timing_end(){
		//if (zmf_finished == TRUE) { 

		//ins_log_file.close();
		
		#ifdef TRANSFORMER_USE_CACHE_COHERENCE
		CAT_pseudo_dump_coherence_stat();
		#endif
		/*printf("\n[Calculation-based simulator]-total_tb_mem_num_dias: ");
		for(int j = 0; j < global_cpu_count; j++){
			printf("\ntotal_memory_lack_dias: %d\n", m_seq[j]->total_tb_mem_num_dias);
		}
		printf("\n[Calculation-based simulator]-total_tb_mem_num_plus_dias: ");
		for(int j = 0; j < global_cpu_count; j++){
			printf("\ntotal_tb_mem_num_plus_dias: %d\n", m_seq[j]->total_tb_mem_num_plus_dias);
		}
		printf("\n[Calculation-based simulator]-cormu_memory_empty_conflict_num: ");
		for(int j = 0; j < global_cpu_count; j++){
			printf("\t%lld", m_seq[j]->cormu_memory_empty_conflict_num);
		}// */

		printf("\n[Calculation-based simulator]-not cached num: ");
		for(int j = 0; j < global_cpu_count; j++){
			printf("\t%lld", m_seq[j]->not_cached_num);
		}
		printf("\n[Calculation-based simulator]-mem_zero_num: ");
		for(int j = 0; j < global_cpu_count; j++){
			printf("\t%lld", m_seq[j]->mem_zero_num);
		}
		printf("\n[Calculation-based simulator]-Wait_Number: ");
		for(int j = 0; j < global_cpu_count; j++){
			printf("\t%lld", m_seq[j]->inorder_wait_num);
		}
		printf("\n[Calculation-based simulator]-Instructions: ");
		uint64_t minqh_total_instr = 0;
		for (int j = 0; j < global_cpu_count; j++) {
			printf("\t%lld", m_seq[j]->m_local_instr_count);
			minqh_total_instr += m_seq[j]->m_local_instr_count;
		}
		printf("\n[Calculation-based simulator]-Total instructions %lld\n", minqh_total_instr);
		printf("[Calculation-based simulator]-Cycles: ");
		for (int j = 0; j < global_cpu_count; j++) {
			#ifdef PARALLIZED_SHARED_CACHE
			printf("\t%lld", *(m_seq[j]->L2_hit_increase_back));
			printf("\t%lld",(m_seq[j]->m_local_cycles - *(m_seq[j]->L2_hit_increase_back) * INORDER_MEMORY_LATENCY));
			#else
			printf("\t%lld",m_seq[j]->m_local_cycles);
			#endif
		}
		printf("\n");
		
		#ifdef TRANSFORMER_INORDER_BRANCH 
		uint64_t total_inorder_branch_count = 0;
		uint64_t total_inorder_branch_misprediction_count = 0;
		for (int j = 0; j < global_cpu_count; j++) {
			total_inorder_branch_count += m_seq[j]->inorder_branch_count;
			total_inorder_branch_misprediction_count += m_seq[j]->inorder_branch_misprediction_count;
		}
		printf("[Calculation-based simulator]-Branches: Total %lld  Mispred %lld  Mispred-Percentage %0.2f\%  \n", 
			total_inorder_branch_count, total_inorder_branch_misprediction_count, total_inorder_branch_misprediction_count/(total_inorder_branch_count+0.0)*100);
		#endif
		
		#ifdef TRANSFORMER_INORDER_CACHE 
		uint64_t total_inorder_l1_icache_hit_count = 0;
		uint64_t total_inorder_l1_icache_miss_count = 0;
		uint64_t total_l1_dcache_hit_count = 0;
		uint64_t total_l1_dcache_miss_count = 0;
		uint64_t total_l2_dcache_hit_count = 0;
		uint64_t total_l2_dcache_miss_count = 0;
		uint64_t total_l3_dcache_hit_count = 0;
		uint64_t total_l3_dcache_miss_count = 0;
		for (int j = 0; j < global_cpu_count; j++) {
			total_inorder_l1_icache_hit_count += m_seq[j]->inorder_l1_icache_hit_count;
			total_inorder_l1_icache_miss_count += m_seq[j]->inorder_l1_icache_miss_count;
			total_l1_dcache_hit_count += m_seq[j]->inorder_l1_dcache_hit_count;
			total_l1_dcache_miss_count += m_seq[j]->inorder_l1_dcache_miss_count;
			total_l2_dcache_hit_count += m_seq[j]->inorder_l2_cache_hit_count;
			total_l2_dcache_miss_count += m_seq[j]->inorder_l2_cache_miss_count;
			total_l3_dcache_hit_count += m_seq[j]->inorder_l3_cache_hit_count;
			total_l3_dcache_miss_count += m_seq[j]->inorder_l3_cache_miss_count;
			#ifdef PARALLIZED_SHARED_CACHE
			total_l2_dcache_hit_count += *(m_seq[j]->L2_hit_increase_back);
			total_l2_dcache_miss_count -= *(m_seq[j]->L2_hit_increase_back);
			#endif
		}
		printf("[Calculation-based simulator]-Caches: L1-I hit %lld miss %lld L1-D hit %lld miss %lld L2 hit %lld miss %lld L3 hit %lld miss %lld\n", 
			total_inorder_l1_icache_hit_count, total_inorder_l1_icache_miss_count, total_l1_dcache_hit_count, total_l1_dcache_miss_count,
			total_l2_dcache_hit_count, total_l2_dcache_miss_count,
            total_l3_dcache_hit_count, total_l3_dcache_miss_count);
		#endif


		printf("[Calculation-based simulator] Simulation terminates.\n");
		printf("[Calculation-based simulator] Goodbye.\n");
		
		fflush(stdout);
		fflush(stderr);
      //}
	}


	void ana_seq_direct_timing_exec(int cpu_index, int inst_num, int is_formal){
#ifdef WHJ_TEST
		m_seq[cpu_index]->log_print_to_file(inst_num);
#endif		
			while(true){
#ifdef ACCURACY_TEST
				printf("outer loop\n");
#endif
				bool exit_flag = m_seq[cpu_index]->m_calculate(inst_num);
				if(exit_flag){
					break;
				}
			} //*/
	}

	void ana_seq_tb_translation_cache_call(int cpu_index, uint64_t tb_addr, int inst_num){
		//printf("aaa: %d %llx %d\n", cpu_index, tb_addr, inst_num);
		m_seq[cpu_index]->code_cache_insert(inst_num, tb_addr);
	}
}




