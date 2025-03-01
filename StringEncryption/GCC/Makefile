# If GCCDIR is set, use that; otherwise, default to g++
CXX = g++-12

# Compiler flags: enable C++11, debugging, all warnings, and disable RTTI.
CXXFLAGS = -std=c++11 -g -Wall -fno-rtti -Wno-error
# Workaround for some literal suffix warnings with the current GCC plugin headers.
CXXFLAGS += -Wno-literal-suffix
CXXFLAGS += -I/usr/lib/gcc/x86_64-linux-gnu/12/plugin/include/

# Top-level goal: build our GCC plugin as a shared library.
# (Assumes your GCC plugin source is in StringEncryptionPlugin.cpp)
all: libKoviDStringEncryptionGCCPlugin.so

libKoviDStringEncryptionGCCPlugin.so: StringEncryptionPlugin.cpp
	$(CXX) $(CXXFLAGS) -DSTR_GCC_CRYPTO_KEY="\"$(STR_GCC_CRYPTO_KEY)\"" -fPIC -shared -o $@ $<

clean:
	rm -f libKoviDStringEncryptionGCCPlugin.so

# A simple check: try to compile an empty C++ file using the plugin.
check: libKoviDStringEncryptionGCCPlugin.so
	$(CXX) -fplugin=./libKoviDStringEncryptionGCCPlugin.so -c -x c++ /dev/null -o /dev/null

.PHONY: all clean check
