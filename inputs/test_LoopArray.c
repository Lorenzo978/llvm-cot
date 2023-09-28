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
	
	if(a>10)  {
		res = a + 8;
		printf("ciao");
	}
	else {
		for(int i = 0; i < 7; i++) {
			for(int j=0; j < a;j++)
				res = res * a;
		}
	
	}
 
  return res;
}
