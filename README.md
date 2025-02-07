# kovid-obfustaion-passes
A set of LLVM based plugins that perform code obfustaion.


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
clang-19 test.c -O2 -fpass-plugin=/path/to/build/lib/KoviDRenameCodePlugin.so
```
