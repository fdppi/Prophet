
SOURCES=$(wildcard *.cpp)
OBJECTS=$(patsubst %.cpp,%.o,$(SOURCES))
LIBS=libdecode.so

FLAGS=-march=x86-64 -O3 -g 
CFLAGS=-fPIC
INCLUDES=-I.

.PHONY: all

#all: $(OBJECTS)
all: $(LIBS)

#$(LIBS): $(OBJECTS)
#	@ar rcus $(LIBS) $^

$(LIBS): $(OBJECTS)
	g++ -shared $(FLAGS) $(LDFLAGS) $^ -o $(LIBS)

#%.o: %.cpp
#	g++ $(FLAGS) $(INCLUDES) -c $< -o ../obj/$@
%.o: %.cpp
	g++ $(CFLAGS) $(FLAGS) $(INCLUDES) -c $<

#test-decoder: test/driver.cpp
#	g++ $(FLAGS) $(INCLUDES) $< ../obj/*.o -o $@

#%.cpp: %.spasm
#	$(FES2_HOME)/tools/bin/spasm.py $< > $@
%.cpp: %.spasm
	./spasm.py $< > $@

clean:
	-rm -f *.a *.o *.so

