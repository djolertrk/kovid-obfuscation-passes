# If GCCDIR is set, use that; otherwise, default to g++
CXX = g++-12

# Compiler flags: enable C++11, debugging, all warnings, and disable RTTI.
CXXFLAGS = -std=c++11 -g -Wall -fno-rtti -Wno-error
# Workaround for some literal suffix warnings with the current GCC plugin headers.
CXXFLAGS += -Wno-literal-suffix
CXXFLAGS += -I/usr/lib/gcc/x86_64-linux-gnu/12/plugin/include/
all: libKoviDDummyCodeInsertionGCCPlugin.so

libKoviDDummyCodeInsertionGCCPlugin.so: DummyCodeInsertion.cpp
	$(CXX) $(CXXFLAGS) -DSTR_GCC_CRYPTO_KEY="\"$(STR_GCC_CRYPTO_KEY)\"" -fPIC -shared -o $@ $<

clean:
	rm -f libKoviDDummyCodeInsertionGCCPlugin.so

# A simple check: try to compile an empty C++ file using the plugin.
check: libKoviDDummyCodeInsertionGCCPlugin.so
	$(CXX) -fplugin=./libKoviDDummyCodeInsertionGCCPlugin.so -c -x c++ /dev/null -o /dev/null

.PHONY: all clean check
