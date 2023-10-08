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
			if(a> 5) {
					printf("ciao");
					res = res * a;
					while(res > 0) {
						res--;
						printf("quaaa");
					}
			}
			else {
					printf("ciaooooooooo");
					res = res + a;
			}
		}
	
	}
 
  return res;
}
