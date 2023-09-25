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

	int res = 2;

	for(int i = 0; i < 7; i++) {
			res = res * a;
		}
 
  return res;
}
