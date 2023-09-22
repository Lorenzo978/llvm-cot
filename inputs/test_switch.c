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
 switch(a) {
 	case 0 : {
 		res = a + 2;
 		break; }
 	case 1 : {
 		res = a + 3;
 		printf("ciao");
 		break; }
 	case 2 : {
 		res = a + 4;
 		break; }
 		}
  return res;
}
