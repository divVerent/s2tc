all: s2tc libtxc_dxtn.so

CFLAGS = -O3 -Wall -Wextra -fPIC
CXXFLAGS = $(CFLAGS)
CFLAGS += -MMD
CXXFLAGS += -MMD
LDFLAGS = -lm

include $(wildcard *.d)

s2tc: s2tc.o s2tc_compressor.o
	$(CXX) $(LDFLAGS) -o $@ $+

libtxc_dxtn.so: s2tc_libtxc_dxtn.o s2tc_compressor.o
	$(CXX) $(LDFLAGS) -shared -o $@ $+

clean:
	$(RM) *.o s2tc libtxc_dxtn.so
