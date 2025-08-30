# If GCCDIR is set, use that; otherwise, default to g++
CXX = g++-12

# Compiler flags: enable C++11, debugging, all warnings, and disable RTTI.
CXXFLAGS = -std=c++17 -g -Wall -fno-rtti -Wno-error
# Workaround for some literal suffix warnings with the current GCC plugin headers.
CXXFLAGS += -Wno-literal-suffix
CXXFLAGS += -I/usr/lib/gcc/x86_64-linux-gnu/12/plugin/include/
all: libKoviDControlFlowTaintGCCPlugin.so

libKoviDControlFlowTaintGCCPlugin.so: ControlFlowTaint.cpp
	$(CXX) $(CXXFLAGS) -DSTR_GCC_CRYPTO_KEY="\"$(STR_GCC_CRYPTO_KEY)\"" -fPIC -shared -o $@ $<

clean:
	rm -f libKoviDControlFlowTaintGCCPlugin.so

# A simple check: try to compile an empty C++ file using the plugin.
check: libKoviDControlFlowTaintGCCPlugin.so
	$(CXX) -fplugin=./libKoviDControlFlowTaintGCCPlugin.so -c -x c++ /dev/null -o /dev/null

.PHONY: all clean check