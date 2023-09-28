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
	
	for(int i = 0; i < a; i++) {
		array[i] = i * 2;
	}
	
	for(int i = a; i < 8; i++) {
		res += array[i];
	} 
	
	return res;
}
