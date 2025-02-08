# kovid-obfustaion-passes
kovid-obfustaion-passes is a collection of LLVM-based plugins designed to perform code obfuscation through various transformation techniques. The repository showcases how LLVM passes can be utilized to obscure code, making it more challenging for reverse engineering and tampering. Our aim is to provide open source obfuscation passes that empower security researchers and developers to better understand these techniques and prepare defenses against potential malicious attacks. As part of the broader OpenSecurity projects, this initiative promotes transparency and collaboration in the security community, enabling researchers to study, experiment with, and ultimately counteract obfuscation strategies employed by adversaries.

## Install deps

```
echo "deb http://apt.llvm.org/$(lsb_release -cs)/ llvm-toolchain-$(lsb_release -cs)-19 main" | sudo tee /etc/apt/sources.list.d/llvm.list
wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | sudo apt-key add -
sudo apt-get update
sudo apt-get install -y llvm-19-dev clang-19 libclang-19-dev lld-19 pkg-config libgc-dev libssl-dev zlib1g-dev libcjson-dev libunwind-dev
sudo apt-get install -y python3.12-dev
```

## Build

```
mkdir build_plugin && cd build_plugin
cmake ../kovid-obfustaion-passes/ -DCMAKE_BUILD_TYPE=Relase -DLLVM_DIR=/usr/lib/llvm-19/lib/cmake/llvm -GNinja
ninja
```

## Run

```
$ cat test.c
#include <stdlib.h>
#include <stdio.h>

int f2();

__attribute__((noinline))
static void bar() {
	f2();
}

__attribute__((noinline))
static void foo() {
	bar();
}

__attribute__((noinline))
static void f() {
  
  foo();
}

__attribute__((noinline))
static void g() {
	f();
}

__attribute__((noinline))
int k() {
	g();
	int a;
	scanf("%d", &a);
	return a;
}

int main() {
	int l = k();
	return l;
}
```

```
clang-19 test.c -O2 -fpass-plugin=/path/to/build/lib/KoviDRenameCodePlugin.so -c
```

NOTE: Make sure you use the same LLVM version as the one used for plugin build.
