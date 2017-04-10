
#CC = /home/minqh/gcc-3.4.6/bin/g++
CC = icpc
#OPT_FLAGS = -march=i686
OPT_FLAGS = -march=x86-64
 
OPT_FLAGS += -O3 -g -ggdb -g3 -mmic
 
#OPT_FLAGS += -O2 -finline-functions -DNO_VECTOR_BOUNDS_CHECKS -DMULTIFACET_NO_OPT_WARN
 
OPT_FLAGS += -fpic -finline-functions

OPT_FLAGS += 

#LIBS=-Ldecoder -ldecode

#
# SOURCE
#
# 
SRC_CPP := \
        yags.c \
        trans_cache.c \
        calculation.c

OBJ_DIR = m_generated
OBJ = $(addprefix $(OBJ_DIR)/,$(SRC_CPP:.c=.o) )

#
# BUILD RULES
#
# 
# The default rule is make the opal module
default_rule: module


module: 
	@echo "Opal: building module" 
	mkdir -p $(OBJ_DIR)
	$(CC) $(OPT_FLAGS) -g -c yags.c -o $(OBJ_DIR)/yags.o
	$(CC) $(OPT_FLAGS) -g -c calculation.c -o $(OBJ_DIR)/calculation.o -lpthread -lscif
	$(CC) $(OPT_FLAGS) -g -c trans_cache.c -o $(OBJ_DIR)/trans_cache.o
	$(CC) $(OPT_FLAGS) -g -o cal_server $(OBJ) -lpthread -lscif

	@echo "Finished."


clean: 
	rm -rf *.so *.o
	rm -rf $(OBJ_DIR)
