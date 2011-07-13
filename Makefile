all: s2tc s2tc_decompress libtxc_dxtn.so

CXXFLAGS = -O3 -Wall -Wextra -fPIC
CXXFLAGS += -MMD
LDFLAGS += -lm

include $(wildcard *.d)

s2tc: s2tc.o libtxc_dxtn.so
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $+

s2tc_decompress: s2tc_decompress.o libtxc_dxtn.so
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $+

libtxc_dxtn.so: s2tc_libtxc_dxtn.o s2tc_compressor.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -shared -Wl,-soname=./libtxc_dxtn.so -o $@ $+

clean:
	$(RM) *.o s2tc s2tc_decompress libtxc_dxtn.so
