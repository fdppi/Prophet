
#include "config.h"
#include "trans_cache.h"
#include "calculation.h"

/*------------------------------------------------------------------------*/
/* Macro declarations                                                     */
/*------------------------------------------------------------------------*/


extern int global_cpu_count;
#define NUM_PROCS (global_cpu_count)

extern pseq_t **m_seq;

/*------------------------------------------------------------------------*/
/* variable declaration(s)                                                   */
/*------------------------------------------------------------------------*/

int CAT_L2_shared_block_bit;
int CAT_L2_shared_block_size;
int CAT_L2_shared_block_mask;
int CAT_L2_shared_set_bit;
int CAT_L2_shared_set_size;
long long CAT_L2_shared_set_mask;
int CAT_L2_shared_assoc; 

unsigned int g_DIRECTORY_SIZE_BIT; // (29-L2_BLOCK_BITS)
unsigned int TRANS_directory_size;
long long TRANS_directory_index_mask;

#ifdef TRANSFORMER_USE_CACHE_COHERENCE

//  Directory status for all addresses
CAT_pseudo_directory_status_t *CAT_pseudo_directory_status;
// Detail directory access history entries, size = directory_history_max_size
CAT_pseudo_directory_history_t  *CAT_pseudo_directory_history;
// Currently used history entries
unsigned int TRANS_directory_history_count;
// History entry array index for all addresses, -1 means NULL
int  **CAT_pseudo_directory_history_index;
// Max history entry array size
unsigned int TRANS_directory_history_max_size; // = 1024*

#ifdef TRANSFORMER_USE_CACHE_COHERENCE_buffer
volatile coherence_buffer_t ** CAT_pseudo_coherence_buffer;
int *coherence_buffer_read_ptr;
volatile long long ** L2_hit_increase_return;
#endif

CAT_pseudo_coherence_stat_t *CAT_pseudo_coherence_stat;

#endif



//CAT_pseudo_cache_set_t **CAT_pseudo_core_cache;

CAT_pseudo_cache_set_t* CAT_pseudo_L2_shared_cache;

 
 /*------------------------------------------------------------------------*/
 /* Function definitions													*/
 /*------------------------------------------------------------------------*/

 
 
 void init_cache(){

	 CAT_L2_shared_block_bit = DL1_BLOCK_BITS;
	 CAT_L2_shared_block_size = (1 << CAT_L2_shared_block_bit);
	 CAT_L2_shared_block_mask = CAT_L2_shared_block_size - 1;
 
	 CAT_L2_shared_set_bit = L2_SET_BITS;
	 CAT_L2_shared_assoc = L2_ASSOC;
	 CAT_L2_shared_set_size = (1 << CAT_L2_shared_set_bit);
	 CAT_L2_shared_set_mask = CAT_L2_shared_set_size - 1;
 
	 CAT_pseudo_L2_shared_cache = (CAT_pseudo_cache_set_t*)malloc(CAT_L2_shared_set_size * sizeof(CAT_pseudo_cache_set_t));
	 for(int i = 0; i < CAT_L2_shared_set_size; i++){
		 CAT_pseudo_L2_shared_cache[i].cache_line_list = (CAT_pseudo_cache_line_t*)malloc(CAT_L2_shared_assoc * sizeof(CAT_pseudo_cache_line_t));
		 for(int j = 0; j < CAT_L2_shared_assoc; j++){
			 CAT_pseudo_L2_shared_cache[i].cache_line_list[j].flag = CAT_CACHE_EMPTY;
		 }
	 }
 
	#ifdef TRANSFORMER_USE_CACHE_COHERENCE

	printf(" Coherence initialization: \n");
	#ifdef TRANSFORMER_USE_CACHE_COHERENCE_history
		printf("     Version: detailed coherence, using access histories.\n");
		#ifdef TRANSFORMER_USE_CACHE_COHERENCE_buffer
		printf("     Version: paralel coherence, using request buffers.\n     Error: Currently not supported.\n");
		assert(0);
		#endif
	#endif
	#ifdef TRANSFORMER_USE_CACHE_COHERENCE_buffer
	printf("     Version: paralel coherence, using request buffers.\n");
	#endif

	 g_DIRECTORY_SIZE_BIT = (29-L2_BLOCK_BITS); // 29 means 512 M
	 TRANS_directory_size = 1 << g_DIRECTORY_SIZE_BIT;
	 long long TRANS_directory_index_size = ((long long)1) << (64-L2_BLOCK_BITS);
	 TRANS_directory_index_mask = TRANS_directory_index_size - 1;
 
	 CAT_pseudo_directory_status = (CAT_pseudo_directory_status_t *)malloc(TRANS_directory_size * sizeof(CAT_pseudo_directory_status_t));
	 memset(CAT_pseudo_directory_status, 0, TRANS_directory_size * sizeof(CAT_pseudo_directory_status_t));
 
	#ifdef TRANSFORMER_USE_CACHE_COHERENCE_history
	 TRANS_directory_history_count = 0;
	 TRANS_directory_history_max_size = 1024 * 40 * NUM_PROCS; // 40 K entries * NUM_PROCS
	 printf(" Coherence initialization: dir_size_bit = %d, dir_size = %d\n", g_DIRECTORY_SIZE_BIT, TRANS_directory_size);
 
 
	 CAT_pseudo_directory_history = (CAT_pseudo_directory_history_t *)malloc(TRANS_directory_history_max_size * sizeof(CAT_pseudo_directory_history_t));
	 memset(CAT_pseudo_directory_history, 0, TRANS_directory_history_max_size * sizeof(CAT_pseudo_directory_history_t));
	 for (int i = 0; i < TRANS_directory_history_max_size; i++){
		 CAT_pseudo_directory_history[i].index = -1;
		 CAT_pseudo_directory_history[i].core = -1;
	 }
 
	 CAT_pseudo_directory_history_index = (int **)malloc(NUM_PROCS * sizeof(int *));
	#endif

	 CAT_pseudo_coherence_stat = (CAT_pseudo_coherence_stat_t *)malloc(sizeof(CAT_pseudo_coherence_stat_t));
	 CAT_pseudo_coherence_stat->inv = (long long *)malloc(NUM_PROCS * sizeof(long long));
	 CAT_pseudo_coherence_stat->inv_fake= (long long *)malloc(NUM_PROCS * sizeof(long long));
	 CAT_pseudo_coherence_stat->access_load_miss= (long long *)malloc(NUM_PROCS * sizeof(long long));
	 CAT_pseudo_coherence_stat->access_load_miss_fake= (long long *)malloc(NUM_PROCS * sizeof(long long));
	 CAT_pseudo_coherence_stat->access_store_miss= (long long *)malloc(NUM_PROCS * sizeof(long long));
	 CAT_pseudo_coherence_stat->access_store_miss_fake= (long long *)malloc(NUM_PROCS * sizeof(long long));
	 CAT_pseudo_coherence_stat->access_store_hit= (long long *)malloc(NUM_PROCS * sizeof(long long));
	 CAT_pseudo_coherence_stat->access_store_hit_fake= (long long *)malloc(NUM_PROCS * sizeof(long long));
	 CAT_pseudo_coherence_stat->access_replace= (long long *)malloc(NUM_PROCS * sizeof(long long));
	 CAT_pseudo_coherence_stat->increase_cycle= (long long *)malloc(NUM_PROCS * sizeof(long long));
	 CAT_pseudo_coherence_stat->L2_hit_increase= (long long *)malloc(NUM_PROCS * sizeof(long long));
	 CAT_pseudo_coherence_stat->L2_miss_increase= (long long *)malloc(NUM_PROCS * sizeof(long long));
	 CAT_pseudo_coherence_stat->access_replace_fake = (long long *)malloc(NUM_PROCS * sizeof(long long));
	 
	for(int i = 0; i < NUM_PROCS; i++){
		 CAT_pseudo_coherence_stat->inv[i] = 0;
		 CAT_pseudo_coherence_stat->inv_fake[i] = 0;
		 CAT_pseudo_coherence_stat->access_load_miss[i] = 0;
		 CAT_pseudo_coherence_stat->access_load_miss_fake[i] = 0;
		 CAT_pseudo_coherence_stat->access_store_miss[i] = 0;
		 CAT_pseudo_coherence_stat->access_store_miss_fake[i] = 0;
		 CAT_pseudo_coherence_stat->access_store_hit[i] = 0;
		 CAT_pseudo_coherence_stat->access_store_hit_fake[i] = 0;
		 CAT_pseudo_coherence_stat->access_replace[i] = 0;
		 CAT_pseudo_coherence_stat->access_replace_fake[i] = 0;
		 CAT_pseudo_coherence_stat->increase_cycle[i] = 0;
		 CAT_pseudo_coherence_stat->L2_hit_increase[i] = 0;
		 CAT_pseudo_coherence_stat->L2_miss_increase[i] = 0;
		 

 		#ifdef TRANSFORMER_USE_CACHE_COHERENCE_history
		 CAT_pseudo_directory_history_index[i] = (int *)malloc(TRANS_directory_size * sizeof(int));
		 //memset(CAT_pseudo_directory_history_index[i], 0, TRANS_directory_size * sizeof(int) );
		 for (int j = 0; j < TRANS_directory_size; j++){
			 CAT_pseudo_directory_history_index[i][j] = -1;
		 }
		 #endif
	 }
	 
		#ifdef TRANSFORMER_USE_CACHE_COHERENCE_buffer
		 // Do not use global variables, initialize coherence buffer and read_ptr in each pseq
		 // Here we just assign the pointers
		printf("num procs: %d\n", NUM_PROCS);
		 CAT_pseudo_coherence_buffer = (volatile coherence_buffer_t **)malloc(NUM_PROCS * sizeof(volatile coherence_buffer_t *));
		 coherence_buffer_read_ptr = (int *)malloc(NUM_PROCS * sizeof(int));
		 L2_hit_increase_return = (volatile long long **)malloc(NUM_PROCS * sizeof(volatile long long *));
		 for (int i = 0; i < NUM_PROCS; i++){
			 //CAT_pseudo_coherence_buffer[i] = (coherence_buffer_t *)malloc(Coherence_buffer_size * sizeof(coherence_buffer_t));
			 //memset(CAT_pseudo_coherence_buffer[i], 0, Coherence_buffer_size * sizeof(coherence_buffer_t));
 
			 CAT_pseudo_coherence_buffer[i] = m_seq[i]->m_coherence_buffer;
			 coherence_buffer_read_ptr[i] = 0;
			 L2_hit_increase_return[i] = m_seq[i]->L2_hit_increase_back;
		 }
		printf("num procs: %d\n", NUM_PROCS);
		#endif
		 
	 printf(" Initialize coherence OK.\n");
	 fflush(stdout);
	 
	#endif
 }
  

#ifdef TRANSFORMER_USE_CACHE_COHERENCE


// Added by minqh 2013.05.29 for cache coherence simulation

inline bool isSharer(int core, unsigned int sharers){
	return (1 & (sharers>>core));
}
inline unsigned int setSharer(int core, unsigned int sharers){
	return (sharers | (1<<core));
}
inline unsigned int clearSharer(int core, unsigned int sharers){
	return (sharers & (~(1<<core)));
}

#ifdef TRANSFORMER_USE_CACHE_COHERENCE_history
inline void CAT_clean_up_history_entries(){
	// First, get current global_cycle
	uint64 global_cycle = system_t::inst->m_seq[0]->getLocalCycle();
	for (int i = 1; i < NUM_PROCS; i++){
		uint64 core_cycle = system_t::inst->m_seq[i]->getLocalCycle();
		if (global_cycle > core_cycle){
			global_cycle = core_cycle;
		}
	}
	
	// Check and clean all history entries
	int new_history_count = 0;
	int ori_history_count = TRANS_directory_history_count;
	for (int i = 0; i < TRANS_directory_history_max_size; i++){
		if (CAT_pseudo_directory_history[i].index == -1){
			// Empty entry, do nothing
			
		}else{
			// Check whether this entry needs to be cleaned 
			assert(CAT_pseudo_directory_history[i].req_cycle[0] != 0);
			assert(CAT_pseudo_directory_history[i].core >= 0);
			if (CAT_pseudo_directory_history[i].req_cycle[0] < global_cycle){
				// This entry can be cleaned
				int lastest_type = CAT_pseudo_directory_history[i].req_type[0];
				int core = CAT_pseudo_directory_history[i].core;
				int index = CAT_pseudo_directory_history[i].index;
				CAT_pseudo_directory_status_t *status = &(CAT_pseudo_directory_status[index]);

				// Update directory status
				if ( (lastest_type == Coherence_replace) || (lastest_type == Coherence_inv)){
					status->sharer = clearSharer(core, status->sharer);
				}else if ((lastest_type == Coherence_load_miss) || (lastest_type == Coherence_store_hit) || (lastest_type == Coherence_store_miss)){
					status->sharer = setSharer(core, status->sharer);
				}

				// Update directory index
				CAT_pseudo_directory_history_index[core][index] = -1;

				// Update history entries
				CAT_pseudo_directory_history[i].core = -1;
				CAT_pseudo_directory_history[i].index = -1;
				for (int j = 0; j < TRANS_directory_history_size; j++){
					CAT_pseudo_directory_history[i].req_cycle[j] = 0;
					CAT_pseudo_directory_history[i].req_type[j] = 0;
					CAT_pseudo_directory_history[i].valid[j] = 0;
				}

				ori_history_count--;
			}else{
				// Do not clean this entry
				new_history_count++;	
			}
		}
	}

	assert(ori_history_count == new_history_count);
	TRANS_directory_history_count = ori_history_count;

	// Re-calculate TRANS_directory_history_count
}
inline int CAT_get_history_index(int core, int index){
	int h_index = CAT_pseudo_directory_history_index[core][index];
	if (h_index != -1){
		return h_index;
	}else{
		// We need to allocate a history entry for this address
		if (TRANS_directory_history_count < (TRANS_directory_history_max_size-10)){
			CAT_clean_up_history_entries();
		}
		assert(TRANS_directory_history_count < (TRANS_directory_history_max_size-10));

		// Find an empty history entry
		int empty_index = 0;
		for (empty_index = 0; empty_index < TRANS_directory_history_max_size; empty_index++){
			if (CAT_pseudo_directory_history[empty_index].index == -1){
				break;
			}
		}
		assert(empty_index < TRANS_directory_history_max_size ); // Assert enough TRANS_directory_history_max_size

		CAT_pseudo_directory_history[empty_index].index = index;
		CAT_pseudo_directory_history[empty_index].core = core;

		CAT_pseudo_directory_history_index[core][index] = empty_index;
		TRANS_directory_history_count++;
		return empty_index;
	}
}
inline void CAT_pseudo_insert_history(CAT_pseudo_directory_history_t *history, int core, int type, unsigned int cycle, CAT_pseudo_directory_status_t *status){

	int insert_index = 0;
	for (insert_index = 0; insert_index < TRANS_directory_history_size; insert_index++){
		if (history->req_cycle[insert_index] < cycle){
			break;
		}
	}
	// 2.1 if all history entries is cycle bigger, insert at the last and update isSharer
	int ori_type = history->req_type[TRANS_directory_history_size-1];
	if (insert_index == TRANS_directory_history_size){
		history->req_type[TRANS_directory_history_size-1] = type;
		history->req_cycle[TRANS_directory_history_size-1] = cycle;

		history->valid[TRANS_directory_history_size-1] = 1;
	}else{
		// 2.2 Find the insert index, Insert this access into the history entries
		//	  Notion: Remove the history tail needs to update status->sharer information !!!
		//	  From the history tail, 
		int tail_index = TRANS_directory_history_size-1;
		//int ori_type = CAT_pseudo_directory_history[core][index].req_type[TRANS_directory_history_size-1];
		for (; tail_index > insert_index; tail_index--){
			// Move the entries one entry later, if they are cycle-less than current request
			// Notion: insert_index is at least 0, so tail_index is at least 1, that is, we need not care about ArrayIndexOutOfBound
			history->req_type[tail_index] = history->req_type[tail_index-1];
			history->req_cycle[tail_index] = history->req_cycle[tail_index-1];

			history->valid[tail_index] = history->valid[tail_index-1];
		}
		history->req_type[insert_index] = type;
		history->req_cycle[insert_index] = cycle;

		history->valid[insert_index] = 1;
	}
	// 2.3 Update status->sharer when removing the earliest entry
	if ( (ori_type == Coherence_replace) || (ori_type == Coherence_inv)){
		status->sharer = clearSharer(core, status->sharer);
	}else if ((ori_type == Coherence_load_miss) || (ori_type == Coherence_store_hit) || (ori_type == Coherence_store_miss)){
		status->sharer = setSharer(core, status->sharer);
	}

	
}

inline bool checkSharer(CAT_pseudo_directory_history_t *history, unsigned int cycle, int core, unsigned int sharers){
	// 1. First check history entries
	int insert_index = 0;
	for (insert_index = 0; insert_index < TRANS_directory_history_size; insert_index++){
		if (history->req_cycle[insert_index] < cycle){
			break;
		}
	}
	if (insert_index == TRANS_directory_history_size){
		// All recorded history entries are later than current cycle, check directory_status
		return isSharer(core,sharers);
	}else if (history->req_cycle[insert_index] == 0){
		// All recorded history entries are later than current cycle, check directory_status
		return isSharer(core,sharers);
	}else{
		int ori_type = history->req_type[insert_index];
		if ( (ori_type == Coherence_replace) || (ori_type == Coherence_inv)){
			return false;
		}else if ((ori_type == Coherence_load_miss) || (ori_type == Coherence_store_hit) || (ori_type == Coherence_store_miss)){
			return true;
		}
	}

}

// We just access the directory and statistics for invalidations. In fact, we just count them and do not really send them.
// Not accurate? No, it is not.
inline void CAT_pseudo_access_directory_his(long long address, int core, int type, long long cycle){

	int index = (int)(TRANS_directory_index_mask & (long long) (address >> CAT_L2_shared_block_bit));

	// Notion: For currently 512M cache, we define directory size for just 512M.
	assert(index < TRANS_directory_size);

	// 0. Find the directory
	CAT_pseudo_directory_status_t *status = &(CAT_pseudo_directory_status[index]);
	
	bool need_inv = false;
	
	// 0. Statistics	
	if (type==Coherence_load_miss ){
		CAT_pseudo_coherence_stat->access_load_miss[core]++;
	}else if( type==Coherence_store_miss){
		CAT_pseudo_coherence_stat->access_store_miss[core]++;
		need_inv = true;
	}else if (type == Coherence_store_hit){
		CAT_pseudo_coherence_stat->access_store_hit[core]++;
		need_inv = true;
	}else if (type == Coherence_replace){
		CAT_pseudo_coherence_stat->access_replace[core]++;
	}

	// 1. Compare this request with its current history
	// Currently nothing

	// 2. Check request type and insert history
	// For replacement and load_miss, insert history
	// For store_hit or store_miss, insert history and send INV
	unsigned int addr = (unsigned int)address;
	int history_index = CAT_get_history_index(core, index);
	if (history_index >= TRANS_directory_history_max_size){
		printf(" history_index = %d, count=%d, max=%d\n",history_index, TRANS_directory_history_count, TRANS_directory_history_max_size);
		fflush(stdout);
	}
	assert(history_index < TRANS_directory_history_max_size);
	CAT_pseudo_directory_history_t *history = &(CAT_pseudo_directory_history[history_index]);
	CAT_pseudo_insert_history(history, core, type, (unsigned int)cycle, status);

	// If no need for sending invalidations, return now
	if (!need_inv){
		return;
	}

	// 3. Send inv if necessary
	for (int i = 0; i < NUM_PROCS; i++){
		if (i == core){
			continue;
		}

		bool isasharer = false;
		if (CAT_pseudo_directory_history_index[i][index] != -1){
			// Has a history
			CAT_pseudo_directory_history_t *i_history = &(CAT_pseudo_directory_history[CAT_pseudo_directory_history_index[i][index]]);
			if (checkSharer(i_history, (unsigned int)cycle, i, status->sharer)){
				isasharer = true;
			}
		}else{
			// Have no history
			if (isSharer(i,status->sharer)){
				// Is a sharer, insert inv for this core
				int new_index = CAT_get_history_index(i, index);

				isasharer = true;
			}else{
				// Not a sharer, do nothing
			}
		}
		//if (isSharer(i,status->sharer))
		if (isasharer){
			CAT_pseudo_directory_history_t *i_history = &(CAT_pseudo_directory_history[CAT_pseudo_directory_history_index[i][index]]);
			CAT_pseudo_insert_history(i_history, i, Coherence_inv, (unsigned int)cycle, status);
			
			CAT_pseudo_coherence_stat->inv[i]++;
		}
		
	}

	
}

#endif



  /** convert the address 'a' to the block address, not shifted */
inline long long   Shared_BlockAddress(long long a) {
	return (a & ~(long long)CAT_L2_shared_block_mask); 
}

 /** convert the address 'a' to a location (set) in L2 cache */
inline unsigned int Shared_L2_Set(long long a) { 
 	return (CAT_L2_shared_set_mask & (long long) (a >> CAT_L2_shared_block_bit));	
}

inline bool CAT_pseudo_shared_L2_line_insert(long long address, long long cycle){
	unsigned int index = Shared_L2_Set(address);
	long long ba = Shared_BlockAddress(address);

	CAT_pseudo_cache_set_t *set = &CAT_pseudo_L2_shared_cache[index];

	for(unsigned int i = 0; i < CAT_L2_shared_assoc; i++){
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

	for(unsigned int i = CAT_L2_shared_assoc - 1; i > 0; i--){
		set->cache_line_list[i].block_addr = set->cache_line_list[i - 1].block_addr;
		set->cache_line_list[i].flag = set->cache_line_list[i - 1].flag;
	}
	set->cache_line_list[0].block_addr = ba;
	set->cache_line_list[0].flag = CAT_CACHE_FULL;
	
	return true;
}

inline bool CAT_pseudo_shared_L2_tag_search(long long address){
	unsigned int index = Shared_L2_Set(address);
	long long   ba = Shared_BlockAddress(address);
	bool   cachehit = false;

	/* search all sets until we find a match */
	CAT_pseudo_cache_set_t *set = &CAT_pseudo_L2_shared_cache[index];
	for (unsigned int i = 0 ; i < CAT_L2_shared_assoc ; i ++) {
		// if it is found in the cache ...
		if ( (set->cache_line_list[i].flag == CAT_CACHE_FULL) && Shared_BlockAddress(set->cache_line_list[i].block_addr) == ba ) {
			cachehit = true;
			break;
		}
	}
	return cachehit;
}

inline void CAT_pseudo_access_directory_ori(long long address, int core, int type, long long cycle, int in_L2){

	long long index = (TRANS_directory_index_mask & (long long) (address >> CAT_L2_shared_block_bit));

	// Notion: For currently 512M cache, we define directory size for just 512M.
	assert(index < TRANS_directory_size);

	// 0. Find the directory
	CAT_pseudo_directory_status_t *status = &(CAT_pseudo_directory_status[index]);

	// 1. Compare cycle, deciding whether it's a valid access
	bool valid_access = (status->req_cycle <= cycle)?true:false;


	if (type == Coherence_replace){
		CAT_pseudo_shared_L2_line_insert(address, cycle);
	}
	else if (type==Coherence_load_miss){
		bool real_in_L2 = CAT_pseudo_shared_L2_tag_search(address);
		if(real_in_L2 & !in_L2){
			CAT_pseudo_coherence_stat->increase_cycle[core] -= INORDER_MEMORY_LATENCY;
			CAT_pseudo_coherence_stat->L2_hit_increase[core]++;
			CAT_pseudo_coherence_stat->L2_miss_increase[core]--;
		}
		else if(!real_in_L2 & in_L2){
			CAT_pseudo_coherence_stat->increase_cycle[core] += INORDER_MEMORY_LATENCY;
			CAT_pseudo_coherence_stat->L2_hit_increase[core]--;
			CAT_pseudo_coherence_stat->L2_miss_increase[core]++;
		}
	}else if( type==Coherence_store_miss){
		bool real_in_L2 = CAT_pseudo_shared_L2_tag_search(address);
		if(real_in_L2 & !in_L2){
			CAT_pseudo_coherence_stat->increase_cycle[core] -= INORDER_MEMORY_LATENCY;
			CAT_pseudo_coherence_stat->L2_hit_increase[core]++;
			CAT_pseudo_coherence_stat->L2_miss_increase[core]--;
		}
		else if(!real_in_L2 & in_L2){
			CAT_pseudo_coherence_stat->increase_cycle[core] += INORDER_MEMORY_LATENCY;
			CAT_pseudo_coherence_stat->L2_hit_increase[core]--;
			CAT_pseudo_coherence_stat->L2_miss_increase[core]++;
		}
	}
	
	if (valid_access){
		if (type==Coherence_load_miss ){
			CAT_pseudo_coherence_stat->access_load_miss[core]++;
		}else if( type==Coherence_store_miss){
			CAT_pseudo_coherence_stat->access_store_miss[core]++;
		}else if (type == Coherence_store_hit){
			CAT_pseudo_coherence_stat->access_store_hit[core]++;
		}else if (type == Coherence_replace){
			CAT_pseudo_coherence_stat->access_replace[core]++;
		}
	}else{
		if (type==Coherence_load_miss){
			CAT_pseudo_coherence_stat->access_load_miss_fake[core]++;
		}else if (type==Coherence_store_miss){
			CAT_pseudo_coherence_stat->access_store_miss_fake[core]++;
		}else if (type == Coherence_store_hit){
			CAT_pseudo_coherence_stat->access_store_hit_fake[core]++;
		}else if (type == Coherence_replace){
			CAT_pseudo_coherence_stat->access_replace_fake[core]++;
		}
	}

	// 2. Send invalidation ?? Well, I mean count the invalidation
	if (valid_access){
		if (type == Coherence_store_hit || type==Coherence_store_miss){
			for (int i = 0; i < NUM_PROCS; i++){
				if (i == core){
					continue;
				}
				if (isSharer(i, status->sharer)){
					CAT_pseudo_coherence_stat->inv[i]++;
				}
			}
		}
	}
	else{
		if (type == Coherence_store_hit || type==Coherence_store_miss){
			for (int i = 0; i < NUM_PROCS; i++){
				if (i == core){
					continue;
				}
				if (isSharer(i, status->sharer)){
					CAT_pseudo_coherence_stat->inv_fake[i]++;
				}
			}
		}
	}

	// 3. Update status
	if (valid_access){
		if (type == Coherence_store_hit || type==Coherence_store_miss){
			//status->sharer = 0;
			status->sharer = setSharer(core, 0);
		}else if (type == Coherence_load_miss){
			status->sharer = setSharer(core, status->sharer);
		}else if (type == Coherence_replace){
			status->sharer = clearSharer(core, status->sharer);
		}

		status->req_cycle = cycle;
	}
	// */
}


// Read coherence buffer and calculate
// @para: expected calculated core
inline bool CAT_calculate_coherence(int core){
	// Compare the four cores' requests and get the earliest one
}

#ifdef TRANSFORMER_USE_CACHE_COHERENCE_buffer
void CAT_read_coherence_buffer(){
	long long address = 0;
	int type = 0;
	long long cycle = 0;
	int core = 0;

	// Pick up a coherence request from coherence buffer
	// 1. Simple selecting, round-robin mechanism
	for (int i = 0; i < NUM_PROCS; i++){
		if (CAT_pseudo_coherence_buffer[i][coherence_buffer_read_ptr[i]].flag == FULL){
			CAT_pseudo_access_directory_ori(CAT_pseudo_coherence_buffer[i][coherence_buffer_read_ptr[i]].address,
				i,
				CAT_pseudo_coherence_buffer[i][coherence_buffer_read_ptr[i]].type,
				CAT_pseudo_coherence_buffer[i][coherence_buffer_read_ptr[i]].cycle,
				CAT_pseudo_coherence_buffer[i][coherence_buffer_read_ptr[i]].in_L2);
			CAT_pseudo_coherence_buffer[i][coherence_buffer_read_ptr[i]].flag = EMPTY;

			coherence_buffer_read_ptr[i] = (coherence_buffer_read_ptr[i]+1)%Coherence_buffer_size;
		}
	}

	// 2. Detailed accurate selecting, comparing cycles
	// To be implemented
}
#endif


void CAT_pseudo_dump_coherence_stat(){
	long long total_inv = 0;
	long long total_inv_fake = 0;
	long long total_access_lm = 0;
	long long total_access_lm_fake = 0;
	long long total_access_sm = 0;
	long long total_access_sm_fake = 0;
	long long total_access_sh = 0;
	long long total_access_sh_fake = 0;
	long long total_access_replace = 0;
	long long total_access_replace_fake = 0;
	long long total_L2_hit_increase = 0;
	long long total_L2_miss_increase = 0;
	
	printf("\n Dump coherence statistics:\n Increase_cycle: Invalidations: Load_miss: Store_miss: Store_hit: Replace\n");
	for (int i = 0; i < NUM_PROCS; i++){
		
		printf("\t%lld", CAT_pseudo_coherence_stat->increase_cycle[i]);
		printf("\t%lld", CAT_pseudo_coherence_stat->inv[i]);
		printf("\t%lld", CAT_pseudo_coherence_stat->access_load_miss[i]);
		printf("\t%lld", CAT_pseudo_coherence_stat->access_store_miss[i]);
		printf("\t%lld", CAT_pseudo_coherence_stat->access_store_hit[i]);
		printf("\t%lld\n", CAT_pseudo_coherence_stat->access_replace[i]);
		
		total_inv += CAT_pseudo_coherence_stat->inv[i];
		total_access_lm += CAT_pseudo_coherence_stat->access_load_miss[i];
		total_access_sm += CAT_pseudo_coherence_stat->access_store_miss[i];
		total_access_sh += CAT_pseudo_coherence_stat->access_store_hit[i];
		total_access_replace += CAT_pseudo_coherence_stat->access_replace[i];
		total_access_replace_fake += CAT_pseudo_coherence_stat->access_replace_fake[i];
		total_L2_hit_increase += CAT_pseudo_coherence_stat->L2_hit_increase[i];
		total_L2_miss_increase += CAT_pseudo_coherence_stat->L2_miss_increase[i];
		
		#ifdef PARALLIZED_SHARED_CACHE
		*(L2_hit_increase_return[i]) = CAT_pseudo_coherence_stat->L2_hit_increase[i];
		#endif
	}
	
	printf("Total inv %lld load_miss %lld store_miss %lld store_hit %lld Replace %lld Replace_fake %lld L2_hit_increase %lld L2_miss_increase %lld\n", 
		total_inv, total_access_lm, total_access_sm, total_access_sh, total_access_replace, total_access_replace_fake, total_L2_hit_increase, total_L2_miss_increase);

	fflush(stdout);
}

#endif



 
