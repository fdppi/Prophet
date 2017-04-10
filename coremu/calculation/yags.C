
#include <stdlib.h>
#include "yags.h"


/*------------------------------------------------------------------------*/
/* Constructor(s) / destructor                                            */
/*------------------------------------------------------------------------*/

//**************************************************************************

yags_t::yags_t(uint32_t pht_entries, uint32_t exception_entries, uint32_t tag_bits ){
  m_pht_size = 1 << pht_entries;
  m_pht_mask = m_pht_size - 1;

  /* initialize to weakly not-taken */
  m_pht = (byte_t *) malloc( m_pht_size * sizeof(byte_t) );
  memset( (void *) m_pht, 1, m_pht_size * sizeof(byte_t) );

  m_exception_size = 1 << exception_entries;
  m_exception_mask = m_exception_size - 1;
  m_tag_bits = tag_bits;
  m_tag_mask = (1 << m_tag_bits) - 1;

  /* initialize tags to all zeros (a basically random value) */
  tkn_tag   = (tag_t *) malloc( m_exception_size * sizeof(tag_t) );
  n_tkn_tag = (tag_t *) malloc( m_exception_size * sizeof(tag_t) );
  memset((void *)tkn_tag,  0, m_exception_size * sizeof(tag_t));
  memset((void *)n_tkn_tag,0, m_exception_size * sizeof(tag_t));

  /* initialize predictions to agree to bias */
  tkn_pred   = (byte_t *) malloc( m_exception_size * sizeof(byte_t) );
  n_tkn_pred = (byte_t *) malloc( m_exception_size * sizeof(byte_t) );
  memset((void *)tkn_pred, 3, m_exception_size * sizeof(tag_t));
  memset((void *)n_tkn_pred, 0, m_exception_size * sizeof(tag_t));
}

//**************************************************************************
yags_t::~yags_t()
{
  if (m_pht) {
    free( m_pht );
    m_pht = NULL;
  }
  if (tkn_tag) {
    free( tkn_tag );
    tkn_tag = NULL;
  }
  if (n_tkn_tag) {
    free( n_tkn_tag );
    n_tkn_tag = NULL;
  }
  if (tkn_pred) {
    free( tkn_pred );
    tkn_pred = NULL;
  }
  if (n_tkn_pred) {
    free( n_tkn_pred );
    n_tkn_pred = NULL;
  }
}

bool yags_t::Predict(uint64_t branch_PC, word_t history, bool staticPred, uint32_t proc)
{

  word_t shifted_PC = MungePC(branch_PC, proc);
  word_t choice_index = GenerateChoice(shifted_PC, proc);
  //ASSERT(choice_index < m_pht_size);

  bool taken_bias, predict;

  taken_bias = (m_pht[choice_index] >= 2);
  predict = taken_bias;

  word_t except_index = GenerateIndex(shifted_PC, history);  
  assert(except_index < m_exception_size);
  tag_t tag = GenerateTag(shifted_PC, history);
  
  if (taken_bias) {
	if (tkn_tag[except_index] == tag) {
	  // hit in the taken exception table
	  predict = (tkn_pred[except_index] >= 2);
	}
  } else { 
	if (n_tkn_tag[except_index] == tag) {
	  // hit in the not-taken exception table
	  predict = (n_tkn_pred[except_index] >= 2);
	}
  }
  
  return predict;
}


