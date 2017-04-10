
SOURCES=$(wildcard *.cpp)
OBJECTS=$(patsubst %.cpp,%.o,$(SOURCES))
LIBS=libdecode.a

#FLAGS=-march=pentium4 -O3 -g
FLAGS=-march=x86-64 -O3 -g -lstdc++ -static
INCLUDES=-I.

.PHONY: all

#all: $(OBJECTS)
all: $(LIBS)

$(LIBS): $(OBJECTS)
	@ar rcus $(LIBS) $^

#%.o: %.cpp
#	g++ $(FLAGS) $(INCLUDES) -c $< -o ../obj/$@
%.o: %.cpp
	g++ $(FLAGS) $(INCLUDES) -c $<

#test-decoder: test/driver.cpp
#	g++ $(FLAGS) $(INCLUDES) $< ../obj/*.o -o $@

#%.cpp: %.spasm
#	$(FES2_HOME)/tools/bin/spasm.py $< > $@
%.cpp: %.spasm
	./spasm.py $< > $@

clean:
	-rm -f *.a *.o

