// Test that the BreakCFG pass successfully breaks the control flow graph

int simple_function(int x) {
  int result = 0;
  
  // This if statement should be transformed
  if (x > 10) {
    result = x * 2;
  } else {
    result = x / 2;
  }
  
  return result;
}