//=============================================================================
// FILE:
//      input_for_hello.c
//
// DESCRIPTION:
//      Sample input file for HelloWorld and InjectFuncCall
//
// License: MIT
//=============================================================================
#include <stdio.h>
int foo(int a) {
 int res=0;
 if(a>10)
  res=5;
 else
{
  res=3;
  printf("ciao");
}
if(res > 0) {
	res = 5;
	printf("ciao");
}
else {
	res = a * 2;
	printf("hello");
}
  return res;
}
