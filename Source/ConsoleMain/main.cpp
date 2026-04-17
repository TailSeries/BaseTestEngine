#include <Windows.h>
#include <iostream>
#include <map>
#include <timeapi.h>
#include <vector>
#include "Base4/Sort.h"

template<typename OutT, typename InT>
FORCEINLINE OutT value_as(InT InValue)
{
	if constexpr (std::is_same_v<OutT, InT>)
	{
		return InValue;
	}
	else if constexpr (sizeof(InT) < sizeof(OutT))
	{
		OutT ExtendedValue = {};
		memcpy(&ExtendedValue, &InValue, sizeof(InT));
		return ExtendedValue;
	}
	else
	{
		OutT OutValue;
		memcpy(&OutValue, &InValue, sizeof(OutT));
		return OutValue;
	}
}


int TestFunc002(const void* Data, const int& TypeId)
{
	return TypeId + 2;
};


void func_by_value(int typeId)
{
	int a = typeId * 2;
	// 编译器生成: mov eax, edx   (直接用寄存器值)
}

void func_by_ref(const int& typeId)
{
	int a = typeId * 2;
	// 编译器生成: mov eax, [rdx] (把 rdx 当指针解引用)
}



int main()
{
	std::vector<int> arr{ 0, 2, 1, 0, 3, 5, 6, 7, 7, 6, 8, 5 ,6 };
	//QuickSort(arr, 0, arr.size() - 1);
	int left = 0;
	int right = arr.size() - 1;
	//TribleQuickSort(arr, left, right);
	HeapSort(arr);

	return 0;
}
