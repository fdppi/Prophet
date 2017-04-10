#ifndef __TRANS_DEFINE_H_
#define __TRANS_DEFINE_H_


/*------------------------------------------------------------------------*/
/* Macro declarations                                                     */
/*------------------------------------------------------------------------*/



#define TRANSFORMER_USE_QEMU
 
 // Parallel version:	Added by minqh 2012.11.21
#define TRANSFORMER_PARALLEL
 
#ifdef TRANSFORMER_PARALLEL
	 // macro for base version
	 #define PARALLELIZED_FUNCTIONAL_AND_TIMING
 
	 // macro for parallelized cache
	 //#define PARALLIZED_SHARED_CACHE
 
	#ifdef PARALLELIZED_FUNCTIONAL_AND_TIMING
	 // macro for by-core version
	 //   Notion: By-core parallelization needs Functional-Timing parallelization
	#define PARALLELIZED_TIMING_BY_CORE
	#endif
 
#endif
 
 
#define TRANSFORMER_INORDER
#define TRANSFORMER_INORDER_BRANCH
#define TRANSFORMER_INORDER_CACHE
//#define TRANSFORMER_GLOBAL_CACHE
//#define ORDER_VIOLATION_TEST
//easton commented
//#define ANASIM_DEBUG
 
//#define TRANSFORMER_USE_CACHE_COHERENCE

#ifdef  TRANSFORMER_USE_CACHE_COHERENCE
	// Accurate coherence, use request history                always not on
	/*#define TRANSFORMER_USE_CACHE_COHERENCE_history*/

	#ifdef PARALLIZED_SHARED_CACHE
	// Parallelized shared cache, using buffer to send requests
	//#define TRANSFORMER_USE_CACHE_COHERENCE_buffer
	#endif
#endif
 
#ifdef TRANSFORMER_INORDER_BRANCH
#define TRANSFORMER_MISPREDICTION
#endif
 
#ifdef TRANSFORMER_MISPREDICTION
#define TRANSFORMER_MISPREDICTION_NOP
#endif
 

#endif
 
