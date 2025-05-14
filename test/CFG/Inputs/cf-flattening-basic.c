// Test that the CFFlattening pass successfully flattens the control flow graph

int flattening_target(int x, int y) {
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