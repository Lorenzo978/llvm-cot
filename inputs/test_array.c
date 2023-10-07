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

	int array[8];
	int res = 0;
	
	if(a>10)  {
		//res = a + 8;
		printf("ciao");
	}
	else {
		
			for(int i = 0; i <= a; i++) {
				array[i] = i * 2;
			}
		
	
	}
	
	
	for(int i = a; i > 2; i--) {
		res += array[i];
	} 
	
	return res;
}
