# kovid-obfusctaion-passes
kovid-obfusctaion-passes is a collection of LLVM and GCC based plugins designed to perform code obfuscation through various transformation techniques. The repository showcases how compiler passes can be utilized to obscure code, making it more challenging for reverse engineering and tampering. Our aim is to provide open source obfuscation passes that empower security researchers and developers to better understand these techniques and prepare defenses against potential malicious attacks. As part of the broader OpenSecurity projects, this initiative promotes transparency and collaboration in the security community, enabling researchers to study, experiment with, and ultimately counteract obfuscation strategies employed by adversaries.

<img width="1187" alt="Screenshot 2025-02-15 at 16 52 22" src="https://github.com/user-attachments/assets/24782832-5a84-4e51-af4e-84246fe6e5b2" />


## Implemented plugins

1. ***Code renaming***

Renames functions using a reversible encryption technique. This obscures the original symbol names, making it harder for an attacker to understand the program's structure or intent by simply reading symbol tables.

2. ***Dummy Code Insertion***

Inserts non-functional “dummy” instructions into the code. These extraneous operations increase the complexity and size of the binary, thereby distracting both human reverse engineers and automated analysis tools from the real logic.

3. ***Metadata and Unused Code Removal***

Removes debug metadata, unused functions, and other non-essential information. This removal limits the amount of contextual information available to an attacker, reducing insights into program structure, variable types, and execution flow.

4. ***Instruction Pattern Transformation***
  - ***Arithmetic code obfuscation***

Transforms common arithmetic operations into equivalent but more complex sequences. By altering familiar instruction patterns, the pass complicates static analysis and reverse engineering efforts without changing the program’s behavior. We will cover more instructions.

5. ***String Encryption Obfuscation***

Encrypts plaintext string literals in the binary so that sensitive or informative strings are hidden. This prevents attackers from easily gleaning information by simply reading the binary’s embedded strings.

6. ***Control Flow Taint***

Implements advanced control flow obfuscation by combining multiple techniques: control flow breaking with opaque predicates, control flow flattening with dispatcher blocks, and state variable injection. This comprehensive approach creates layered protection that is significantly more resistant to automated deobfuscation tools.

## Install deps

```
# LLVM
echo "deb http://apt.llvm.org/$(lsb_release -cs)/ llvm-toolchain-$(lsb_release -cs)-19 main" | sudo tee /etc/apt/sources.list.d/llvm.list
wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | sudo apt-key add -
sudo apt-get update
sudo apt-get install -y llvm-19-dev clang-19 libclang-19-dev lld-19 pkg-config libgc-dev libssl-dev zlib1g-dev libcjson-dev libunwind-dev liblldb-19-dev
sudo apt-get install -y python3.12-dev
sudo apt-get install -y ninja-build

# GCC Plugin Dev Package
$ sudo apt-get install gcc-12-plugin-dev
$ sudo apt-get install g++-12

# lit
$ pip3 install lit
$ sudo ln -s ~/.local/bin/lit /usr/bin/llvm-lit
$ which llvm-lit
/usr/bin/llvm-lit
```

## Build

```
mkdir build_plugin && cd build_plugin
cmake ../kovid-obfusctaion-passes/ -DCMAKE_BUILD_TYPE=Relase -DLLVM_DIR=/usr/lib/llvm-19/lib/cmake/llvm -GNinja
ninja && sudo ninja install
```

It should be available here:

```
/usr/local/lib/libKoviDRenameCodeGCCPlugin.so
/usr/local/lib/libKoviDRenameCodeLLVMPlugin.so
```

If you want to build with LLDB plugins, taht can be used for de-obfuscation on the fly, use `-DKOP_BUILD_LLDB_PLUGINS=1`, so:

```
cmake ../kovid-obfustaion-passes/ -DCMAKE_BUILD_TYPE=Relase -DLLVM_DIR=/usr/lib/llvm-19/lib/cmake/llvm -DKOP_BUILD_LLDB_PLUGINS=1 -GNinja
```

### MacOS

On MacOS, disable GCC plugins as follows:

```
$ cmake ../kovid-obfustaion-passes -GNinja -DCMAKE_BUILD_TYPE=Relase -DLLVM_DIR=/opt/homebrew/opt/llvm@19/lib/cmake/llvm -DKOP_BUILD_GCC_PLUGINS=0
```

### Windows

On Windows, GCC plugins are automatically disabled. You can build using Visual Studio or command line with CMake:

#### Using Visual Studio

1. Open the folder in Visual Studio (File → Open → Folder)
2. Visual Studio will automatically detect `CMakeSettings.json`
3. Select the desired configuration (x64-Release or x64-Debug)
4. Build → Build All

#### Using Command Line

```powershell
# Install LLVM 19 for Windows
# Download from https://github.com/llvm/llvm-project/releases

# Create build directory
mkdir build
cd build

# Configure
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release -DLLVM_DIR="C:\Program Files\LLVM\lib\cmake\llvm"

# Or with Ninja
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DLLVM_DIR="C:\Program Files\LLVM\lib\cmake\llvm"

# Build
cmake --build . --config Release
```

The plugins will be built in `build/lib/` directory.

## Run

Here are some examples.

### Code renaming Plugin

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

Or with GCC (Linux only):

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

#### Windows Example

On Windows, use the `opt` tool with the plugin. The workflow involves three steps:

```powershell
# Step 1: Compile to LLVM IR
clang-19 -O2 -emit-llvm -c test.c -o test.bc

# Step 2: Apply the obfuscation pass using opt
opt-19 -load-pass-plugin=C:\path\to\build\lib\libKoviDRenameCodeLLVMPlugin.dll -passes="kovid-rename-code" test.bc -o test_obf.bc

# Step 3: Compile the obfuscated IR to object file
clang-19 -O2 -c test_obf.bc -o test.o
```

Note: On Windows, LLVM plugins are DLL files and must be used with the `opt` tool, not directly with clang.

### String Encryption Obfuscation Plugin

NOTE: Module Passes such as `libKoviDStringEncryptionLLVMPlugin.so` and `libKoviDRemoveMetadataAndUnusedCodeLLVMPlugin.so` use this way for now.

We can go to LLVM IR and then use `opt` tool as:

```
$ clang-19 -O2 -emit-llvm -c test.c -o test.bc
$ opt-19 -load-pass-plugin=libKoviDStringEncryptionLLVMPlugin.so -passes="string-encryption" test.bc -o test_obf.bc
$ clang-19 -O2 test_obf.bc
$ ./a.out 
2 4
 Z[V
```

With no obfusctaion it would look as:
```
$ clang-19 -O2 test.c
$ ./a.out 
2 4
All good 4
```
So, the `All good 4` is tainted with this plugin...

## Debugging obfuscated code

There will be LLDB plugins that will do deobfuscation of the tainted code. But some things won't need any plugin for debugging. For example, the `RenameCode` plugin does not drop debugging information, so when renaming function `bar` into function `5fgafx`, you will still be able to set a breakpoint to `bar`. In general, debugging information should be `strip`ped from binary and used only during debugging sessions (or you can use `Split DWARF`, which is supported by most of modern compilers and debuggers).

But, for example, if you want to debug code that was processed with `libKoviDStringEncryptionLLVMPlugin.so`, you will need to use LLDB plugin for it:

![deobf-lldb](https://github.com/user-attachments/assets/e774af5b-ea88-49b7-a9a2-07a7f878f0c6)

## Deobfuscation Tools

1. kovid-deobfuscator

<img width="611" alt="Screenshot 2025-02-09 at 16 31 35" src="https://github.com/user-attachments/assets/04ddc5e3-838f-4549-b3d2-ca5cf748d98d" />

## Platform Support

- **Linux**: Full support (LLVM and GCC plugins)
- **macOS**: LLVM plugins only (GCC plugins not supported)
- **Windows**: LLVM plugins only (GCC plugins not supported)

## TODO

1. Support more obfusctaion techniques


## Users

Some of the plugins are being used for obfuscated build of an awesome, open-source rootkit - [KoviD](https://github.com/carloslack/KoviD).

## Run tests

Please note that we need `lit` and `FileCheck` in `PATH`, also add `-DENABLE_TESTS=ON` during build configuration.

```bash
export PATH=/path/to/llvm/bin/:$PATH
ninja check-passes
```

## Contact

New ideas? Drop me a message at djolertrk@gmail.com
