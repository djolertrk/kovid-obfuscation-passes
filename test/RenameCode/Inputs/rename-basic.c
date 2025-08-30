// Test that the RenameCode pass successfully renames functions with local linkage

__attribute__((noinline))
static void foo() {
  // This will be renamed
}

__attribute__((noinline))
static void bar() {
  // This will be renamed
  foo();
}

int main() {
  // main should not be renamed because it has external linkage
  bar();
  return 0;
}