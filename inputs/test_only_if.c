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
 if(a>10) {
 	printf("ciao");
 	res=a+5;
 }
  return res;
}
