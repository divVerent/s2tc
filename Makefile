all: s2tc

CXXFLAGS = -O3 -Wall -Wextra

include $(wildcard *.d)
CXXFLAGS += -MMD

s2tc: s2tc.o s2tc_compressor.o
	$(CXX) $(LDFLAGS) -o $@ $+

clean:
	$(RM) *.o s2tc
