#include "TransferBase.h"
#include <iostream>
#include <string>

void TestTransfer(MainTest* mainPtr)
{
	//mainPtr->MainTestFunc(); // 非虚函数会报链接错误
	mainPtr->VirtualMainTestFunc(); //虚函数 不会报链接错误
}
