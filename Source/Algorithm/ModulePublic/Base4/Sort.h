#pragma once
#include <cstdio>
#include <vector>
#include <algorithm>

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


//class Comparable
//{
//public:
//	int a;
//	Comparable(int v) :a(v) {};
//	
//	bool operator <(const Comparable& rhs) const
//	{
//		return a < rhs.a;
//	}
//	//隐式转换到 int
//	operator int() const
//	{
//		return a;
//	}
//};
//
//void Exch(std::vector<Comparable>& arr, int a, int b)
//{
//	std::swap(arr[a], arr[b]);
//}
//
//bool Less(std::vector<Comparable>& arr, int a, int b)
//{
//	return arr[a] < arr[b];
//}


/*
* 
* 2.1 选择排序：
	找到数组中最小的元素，和第一个元素交换位置（如果第一个元素和自己一样大，那么可交换可不交换 ）
	剩下的元素中找到第二小的元素，将它和第二个元素交换位置
	如此往复。
	特点：
		无法利用当前输入的状态给下一次的排序提供有用信息。
		不保证稳定 22221 -》 1 2222，第一轮排序完成之后第一个2被挪到了最后。
	时间复杂度 (N - 1) + (N - 2) ... 1 ~ N^2/2
* 
*/
ALGOMODULE void SelectSort(std::vector<int>& arr);

/*
* 2.2 插入排序
*	索引左边已经是有序的，将当前索引插入到左侧合适位置(通过一边移位一边交换的方式)。当索引到了最后一个位置的时候，排定。
* 特点
*	初始的输入状态影响排序最终时长，大致有序的情况下最合适
*	稳定排序
*/
void InsertSort(std::vector<int>& arr)
{
	if (arr.size() == 0) return;

	for (int i = 1; i < arr.size(); i++)
	{
		for (int j = i; j > 0 && arr[j] < arr[j - 1]; j--)
		{
			std::swap(arr[j], arr[j - 1]);
		}
	}
}

/*
* 2.3 希尔排序：使数组中任意间隔为h的元素有序，这样的数组被称为h有序数组。
* 假设 a[i - h], a[i - 2h]...是有序的，那么 a[i]就应该移动到它们之间
*/
void ShellSort(std::vector<int>& arr)
{
	int h = 1;
	while (h < arr.size() / 3)
	{
		h = 3 * h + 1;
	}

	while (h >= 1)
	{
		for (int i = h; i < arr.size(); i++) // 这里i = h，并且i++; arr[h]是第一个需要向左比较的元素，另外是每个元素的h间隔，所以应该是i++；
		{
			for (int j = i; j >= h && arr[j] < arr[j - h]; j -= h)
			{
				std::swap(arr[j], arr[j - h]);
			}
		}
		h = h / 3;
	}

}



