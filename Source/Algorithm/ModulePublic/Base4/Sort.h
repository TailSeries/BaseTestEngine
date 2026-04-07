#pragma once
#include <cstdio>

#include "Module.h"
class ALGOMODULE TestClass
{
	int a = 23;
public:
	void testfunc();
};


void Test45486896()
{
	int a = 23;
	std::printf("TestClass::testfunc() a = %d", a);
}
