// Test that the ControlFlowTaint pass successfully obfuscates the control flow graph
// This includes breaking CFG with opaque predicates and adding state tracking

int taint_target(int x, int y) {
  int result = 0;
  
  if (x > 10) {
    result = x + y;
  } else {
    result = x - y;
  }
  
  if (y > 5) {
    result = result * 2;
  }
  
  return result;
}