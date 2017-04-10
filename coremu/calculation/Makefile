
CC = /home/minqh/gcc-3.4.6/bin/g++
#CC = gcc
#CC = g++
#OPT_FLAGS = -march=i686
OPT_FLAGS = -march=x86-64
 
OPT_FLAGS += -g -ggdb -g3
 
OPT_FLAGS += -O2 -finline-functions -DNO_VECTOR_BOUNDS_CHECKS -DMULTIFACET_NO_OPT_WARN
 
OPT_FLAGS += -fpic -finline-functions

#LIBS=-Ldecoder -ldecode

#
# SOURCE
#
# 
SRC_CPP := \
        yags.C \
        trans_cache.C \
        calculation.C

OBJ_DIR = m_generated
OBJ = $(addprefix $(OBJ_DIR)/,$(SRC_CPP:.C=.o) )





#
# BUILD RULES
#
# 
# The default rule is make the opal module
default_rule: module


module: 
	@echo "Opal: building module" 
	mkdir -p $(OBJ_DIR)
	$(CC) $(OPT_FLAGS) -c yags.C -o $(OBJ_DIR)/yags.o
	$(CC) $(OPT_FLAGS) -c calculation.C -o $(OBJ_DIR)/calculation.o
	$(CC) $(OPT_FLAGS) -c trans_cache.C -o $(OBJ_DIR)/trans_cache.o
	$(CC) $(OPT_FLAGS) -shared -o opal.so $(OBJ)

	@echo "Finished."


clean: 
	rm -rf *.so *.o
	rm -rf $(OBJ_DIR)
