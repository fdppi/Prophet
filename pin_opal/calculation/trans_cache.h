#ifndef _TRANS_CACHE_H
#define _TRANS_CACHE_H

#include "stdlib.h"
#include "stdio.h"
#include "config.h"


/*------------------------------------------------------------------------*/
/* Data type and variable declaration(s)                                                   */
/*------------------------------------------------------------------------*/

typedef struct CAT_pseudo_cache_line{
	long long block_addr;
	int flag;
    long long target_cycle; 
}CAT_pseudo_cache_line_t;

typedef struct CAT_pseudo_cache_set{
	CAT_pseudo_cache_line_t *cache_line_list;
}CAT_pseudo_cache_set_t;


#ifdef TRANSFORMER_USE_CACHE_COHERENCE
typedef struct CAT_pseudo_directory_status_s{
	unsigned int sharer;

	#ifndef TRANSFORMER_USE_CACHE_COHERENCE_history
	long long req_cycle;
	#endif
}CAT_pseudo_directory_status_t;


typedef struct CAT_pseudo_directory_history_s{
	int index;
	int core;
	unsigned int req_cycle[TRANS_directory_history_size];
	char req_type[TRANS_directory_history_size];
	char valid[TRANS_directory_history_size];
}CAT_pseudo_directory_history_t;


typedef struct CAT_pseudo_coherence_stat_s{
	long long *inv;
	long long *inv_fake;
	long long *access_load_miss;
	long long *access_load_miss_fake;
	long long *access_store_miss;
	long long *access_store_miss_fake;
	long long *access_store_hit;
	long long *access_store_hit_fake;

	long long *access_replace;
	long long *access_replace_fake;
	long long *increase_cycle;
	long long *L2_hit_increase;
	long long *L2_miss_increase;
}CAT_pseudo_coherence_stat_t;


	#ifdef TRANSFORMER_USE_CACHE_COHERENCE_buffer
	typedef struct coherence_buffer_s{
		long long address;
		long long cycle;
		//int core;
		int type;
		int in_L2;
		volatile int flag;
	}coherence_buffer_t;

	#endif
#endif


/*------------------------------------------------------------------------*/
/* Function declaration(s)                                                   */
/*------------------------------------------------------------------------*/

void init_cache();

void CAT_pseudo_dump_coherence_stat();
void CAT_read_coherence_buffer();

void trans_cache_hello();
// End of file
#endif
