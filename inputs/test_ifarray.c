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
		//res = a + 8;
		printf("ciao");
		for(int i = 0; i < 5; i++) {
			printf("ciaooooooooo");
			res = res + a;
		
		}
	}
	else {
		for(int i = 0; i < 7; i++) {
					printf("ciao");
					res = res * a;
			
		}
	
	}
 
  return res;
}
