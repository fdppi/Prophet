#ifndef __YAGS_H_
#define __YAGS_H_

#include <sys/types.h>
#include "config.h"

/** The conditional branch predictor's state: e.g. 32 history bits */
typedef uint32_t cond_state_t;


typedef word_t indirect_state_t;

/// pointer to an element in the return address stack
typedef half_t ras_pointer_t;

/// state in the return address stack
typedef struct _ras_state_t {
  ras_pointer_t TOS;
  ras_pointer_t next_free;
} ras_state_t;


/** universal predictor state: saves a copy of historical values
 *  so sequencers can roll back on mispredicts.
 **/
typedef struct _predictor_state_t {
  cond_state_t     cond_state;
  indirect_state_t indirect_state;
  ras_state_t      ras_state;
} predictor_state_t;

typedef byte_t tag_t;


/***************************************************/
/********   Class definition  *****************************/
/***************************************************/

class yags_t{


private:
	
  
	/** number of entries in the choice table */
	uint32_t  m_pht_size;
	/** mask for selecting bits which index in the pht table */
	uint32_t  m_pht_mask;
	/** number of entries in the exception tables */
	uint32_t  m_exception_size;
	/** mask for selecting bits which index in the exception table */
	uint32_t  m_exception_mask;
	/** number of tag bits */
	uint32_t  m_tag_bits;
	/** mask for selecting the tag bits */
	uint32_t  m_tag_mask;

	/** pattern history table -- stores bias for PC */
	byte_t *m_pht;

	/** "direction caches" -- stores tagged execeptions:
	*  Taken exception table */
	tag_t  *tkn_tag;
	/** Taken predictor: 2 bit saturating counter */
	byte_t *tkn_pred;
	/** Not Taken exception table */
	tag_t  *n_tkn_tag;
	/** Not Taken exception predictor */
	byte_t *n_tkn_pred;

	
	/// Hash the program counter into the finite branch prediction tables
	word_t MungePC(uint64_t branch_PC, uint32_t proc) const {
		return (word_t) ((branch_PC >> 2) & 0xffffffff);
		//  return (word_t) (( (branch_PC ^ (proc << 16)) >> 2) & 0xffffffff);
	}

	/// Generate the branch prediction
	word_t GenerateChoice(word_t munged_PC, uint32_t proc) {
		//return (munged_PC & m_pht_mask);
		return ( (munged_PC | (proc << 27)) & m_pht_mask);
	}

	/// Generate the index
	word_t GenerateIndex(word_t munged_PC, word_t history) {
		return ((munged_PC ^ history) & m_exception_mask);
	}

	/// Generate the tag
	tag_t GenerateTag(word_t munged_PC, word_t history) {
		return (tag_t) (munged_PC & m_tag_mask);
	}

public:

	/**
	* Constructor: creates object
	* @param pht_entries The log-base-2 number of entries in the PHT.
	* @param exeception_entries The log-base-2 number of entries in the exception table
	* @param tag_bits The number of tag bits to use in the exception table.
	*/
	yags_t( unsigned int pht_entries, unsigned int exception_entries, unsigned int tag_bits );

	~yags_t();

	/**  Branch predict on the branch_PC 
	*   @return bool true or false branch prediction
	*/
	bool Predict(uint64_t branch_PC, cond_state_t history, bool staticPred, unsigned int proc);

};


#endif

