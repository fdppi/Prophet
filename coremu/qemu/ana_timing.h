/*
 * =====================================================================================
 *
 *       Filename:  ana_timing.h
 *
 *    Description:  timing function call
 *
 *        Version:  1.0
 *        Created:  02/23/2014 08:57:05 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *        Company:  
 *
 * =====================================================================================
 */

ana_tb_struct_t *trifs_trace_shared_tb_inst_buf_head(int core_num){
	printf("core: %d, tb_addr: 0x%llx\n", core_num, ana_tb_temp_buffer);
	return ana_tb_temp_buffer;
}

ana_instr_struct_t *trifs_trace_shared_buf_head(int core_num){
	printf("core: %d, instr_addr: 0x%llx, another_addr: 0x%llx\n", core_num, ana_fetch_instr_temp_buffer, &(ana_fetch_instr_temp_buffer[0].ana_x86_virt_pc));
	return ana_fetch_instr_temp_buffer;
}

ana_mem_struct_t *trifs_mem_shared_buf_head(int core_num){
	printf("core: %d, mem_addr: 0x%llx, another_addr: 0x%llx\n", core_num, ana_fetch_mem_temp_buffer, &(ana_fetch_mem_temp_buffer[0].ana_x86_virt_pc));
	return ana_fetch_mem_temp_buffer;
}

