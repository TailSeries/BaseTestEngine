#include "Base4/Sort.h"

#include <cstdio>

void TestClass::testfunc()
{
	a += 456874;
	std::printf("TestClass::testfunc() a = %d", a);
}

void SelectSort(std::vector<int>& arr) 
{
	if (arr.size() == 0)
	{
		return;
	}
	int minvalueIndex = 0;
	int startindex = 0;
	while (startindex < arr.size())
	{
		minvalueIndex = startindex;
		for (int i = startindex + 1; i < arr.size(); i++)
		{
			if (arr[i] < arr[minvalueIndex])
			{
				minvalueIndex = i;
			}
		}
		std::swap(arr[minvalueIndex], arr[startindex]);
		startindex++;
	}
}
