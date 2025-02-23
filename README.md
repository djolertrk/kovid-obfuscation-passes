# kovid-obfustaion-passes
kovid-obfustaion-passes is a collection of LLVM and GCC based plugins designed to perform code obfuscation through various transformation techniques. The repository showcases how compiler passes can be utilized to obscure code, making it more challenging for reverse engineering and tampering. Our aim is to provide open source obfuscation passes that empower security researchers and developers to better understand these techniques and prepare defenses against potential malicious attacks. As part of the broader OpenSecurity projects, this initiative promotes transparency and collaboration in the security community, enabling researchers to study, experiment with, and ultimately counteract obfuscation strategies employed by adversaries.

<img width="1187" alt="Screenshot 2025-02-15 at 16 52 22" src="https://github.com/user-attachments/assets/24782832-5a84-4e51-af4e-84246fe6e5b2" />


## Implemented plugins

1. Code renaming
2. Dummy Code Insertion
3. Metadata and Unused Code Removal

## Install deps

```
# LLVM
echo "deb http://apt.llvm.org/$(lsb_release -cs)/ llvm-toolchain-$(lsb_release -cs)-19 main" | sudo tee /etc/apt/sources.list.d/llvm.list
wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | sudo apt-key add -
sudo apt-get update
sudo apt-get install -y llvm-19-dev clang-19 libclang-19-dev lld-19 pkg-config libgc-dev libssl-dev zlib1g-dev libcjson-dev libunwind-dev
sudo apt-get install -y python3.12-dev
sudo apt-get install -y ninja-build

# GCC Plugin Dev Package
$ sudo apt-get install gcc-12-plugin-dev
```

## Build

```
mkdir build_plugin && cd build_plugin
cmake ../kovid-obfustaion-passes/ -DCMAKE_BUILD_TYPE=Relase -DLLVM_DIR=/usr/lib/llvm-19/lib/cmake/llvm -GNinja
ninja && sudo ninja install
```

It should be available here:

```
/usr/local/lib/libKoviDRenameCodeGCCPlugin.so
/usr/local/lib/libKoviDRenameCodeLLVMPlugin.so
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
clang-19 test.c -O2 -fpass-plugin=/path/to/build/lib/libKoviDRenameCodeLLVMPlugin.so -c
```

NOTE: Make sure you use the same LLVM version as the one used for plugin build.

Or with GCC:

```
$ g++-12 test.c -O2 -fplugin=/usr/local/lib/libKoviDRenameCodeGCCPlugin.so test.c -c
KoviD Rename Code GCC Plugin loaded successfully
KoviD Rename: Original function name: bar
KoviD Rename: Using crypto key: default_key
KoviD Rename: Encrypted name: 060414

KoviD Rename: Original function name: foo
KoviD Rename: Using crypto key: default_key
KoviD Rename: Encrypted name: 020a09

KoviD Rename: Original function name: main
KoviD Rename: Using crypto key: default_key
KoviD Rename: Encrypted name: 09040f0f

$ objdump -d test.o

test.o:     file format elf64-x86-64


Disassembly of section .text:

0000000000000000 <_060414>:
   0:	f3 0f 1e fa          	endbr64 
   4:	c3                   	ret    
   5:	66 66 2e 0f 1f 84 00 	data16 cs nopw 0x0(%rax,%rax,1)
   c:	00 00 00 00 

0000000000000010 <_020a09>:
  10:	f3 0f 1e fa          	endbr64 
  14:	c3                   	ret    
  15:	66 66 2e 0f 1f 84 00 	data16 cs nopw 0x0(%rax,%rax,1)
  1c:	00 00 00 00 

0000000000000020 <_09040f0f>:
  20:	f3 0f 1e fa          	endbr64 
  24:	b8 01 00 00 00       	mov    $0x1,%eax
  29:	c3                   	ret
```

# Deobfuscation Tools

1. kovid-deobfuscator

<img width="611" alt="Screenshot 2025-02-09 at 16 31 35" src="https://github.com/user-attachments/assets/04ddc5e3-838f-4549-b3d2-ca5cf748d98d" />

# TODO

1. Support Windows
2. Support more obfustaion techniques
