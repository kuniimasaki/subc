int classify(int x) {
  switch (x) {
    case 1:
      break;
    case 2:
    case 3:
      break;
    default:
      break;
  }
  return 0;
}

int fallthrough(int x) {
  int result = 0;
  switch (x) {
    case 0:
      result = result + 1;
      // fall through
    case 1:
      result = result + 10;
      break;
    case 2:
      result = result + 100;
      break;
  }
  return result;
}

int main(void) {
  int total = fallthrough(0) + fallthrough(1) + fallthrough(2); // 11+10+100=121

  int i = 5, sum = 0;
  while (i > 0) {
    switch (i) {
      case 3:
        i = i - 1;
        continue;
      case 1:
        break;
    }
    sum = sum + i;
    i = i - 1;
  }
  // i=5(no match) sum=5,i=4; i=4(no match) sum=9,i=3; i=3(continue) i=2;
  // i=2(no match) sum=11,i=1; i=1(match, break) sum=12,i=0; loop ends.
  return total + sum - 121; // 121+12-121 = 12
}
