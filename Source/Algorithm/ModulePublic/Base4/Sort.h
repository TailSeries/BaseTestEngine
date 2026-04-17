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
* 非稳定性排序
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

/*
 * 归并排序
 * *两个有序数组，可以从小到大归并到一个数组中.
 * 可以是稳定性排序，判断相等的时候就拿前面的就好，时间负载nlogn
 */
// 这里 考虑arr[lo~mid] arr[mid + 1 ~ hi] 有序，现在归并它们两个。
void Merge(std::vector<int>& arr, int lo, int mid , int hi, std::vector<int>& temp)
{
	for (int i = lo; i <= hi; i++)
	{
		temp[i] = arr[i];
	}

	int a = lo;
	int b = mid + 1;
	for (int i = lo; i <= hi; i++)
	{
		if (a > mid)
		{
			arr[i] = temp[b];
			b++;
		}
		else if (b > hi)
		{
			arr[i] = temp[a];
			a++;
		}
		else if (temp[a] <= temp[b])
		{
			arr[i] = temp[a];
			a++;
		}
		else
		{
			arr[i] = temp[b];
			b++;
		}
	}
}

// 从上向下归并: 所有数组元素在递归中，被分割成归并两个长度为1的数组
void MergeSorUpDown(std::vector<int>& arr, int lo, int hi, std::vector<int>& temp)
{
	if (lo >= hi) return; //只有一个元素没必要归并
	int mid = (lo + hi) / 2;
	MergeSorUpDown(arr, lo, mid, temp);
	MergeSorUpDown(arr, mid + 1, hi, temp);
	Merge(arr, lo, mid, hi, temp);
}

/*
*  从下向上归并：先两两归并，然后四四归并，最后归并成一个大数组.
* 注意这时候mid不该是 (lo + hi) / 2 了，因为数组不一定直接可以2等
*/

void MergeSortDownUp(std::vector<int>& arr)
{
	std::vector<int> temp(arr.size());
	int SN = 1;
	while (SN <= (arr.size() - 1))
	{
		for (int lo = 0; lo < arr.size(); lo += (SN * 2))
		{
			int hi = lo + 2*SN - 1;
			if (hi >= arr.size())
			{
				hi = arr.size() - 1;
			}
			int mid = lo + SN - 1; // mid 不再是简单的 (lo + hi) / 2 了。
			if (mid > hi) { mid = hi; }
			Merge(arr, lo, mid, hi, temp);
		}
		SN = SN + SN;
	}
}


/*
* 选一个“基准值”（pivot），把数组分成“比它小”和“比它大”两部分，然后递归地对两边继续做同样的事。
*	例如：[5, 2, 8, 3, 1]
	选 pivot = 5
	重排
		[小于 pivot] pivot [大于 pivot] 
		[2, 3, 1] 5 [8]
	递归排序左右两边
		左边：[2, 3, 1]
		右边：[8]
	nlogn级别，非稳定。
	排定一个元素，再排定这个元素的左右两边
	边界条件：能交换切分元素和i的前提得是，i <= 切分元素，所以这里是 while(i < j && arr[i] < arr[left]) i++; 而不是 while(i < j && arr[i] <= arr[left]) i++;
*/
int Partition(std::vector<int>& arr, int left, int right)
{
	if (left >= right) return left;

	int i = left;
	int j = right;
	int value = arr[left];
	while (i < j)
	{
		while(i < j && arr[i] < arr[left]) i++; // 第一个比arr[left]大=的元素/ i == j
		while(i < j && arr[j] >= arr[left]) j--; // 第一个比arr[left]小的元素 / i == j
		std::swap(arr[i], arr[j]);
	}
	std::swap(arr[left], arr[i]);
	return i;
}

void QuickSort(std::vector<int>& arr, int left, int right)
{
	if (left >= right) return;
	int Nextindex = Partition(arr, left, right);
	QuickSort(arr, 0, Nextindex - 1);
	QuickSort(arr, Nextindex + 1, right);
}

/*
* 三路切分:分治思想 关键1.一次排序把==的上下限给找到。
* lt - 1 开始往前的元素一定比value小。
* gt + 1 开始往后的元素一定比value大。
* 问题：不必急于一求出来就解决 lt - 1; gt + 1;可能超数组范围的问题。
*		left = lt - 1;//有可能  lt - 1 < 0 但是没关系，这是新loop的右边界
		right = gt + 1; //有可能 gt + 1 > arr.size() - 1, 但是没关系，这是新loop的左边界
		sort的loop 中应该不停记录originalLeft和originaRight，分治思想下，这俩是每次小问题的边界
*/

void TriblePartition(std::vector<int>& arr, int& left, int& right)
{
	if (left >= right) return;

	int lt = left;
	int gt = right;
	int i = lt + 1;
	int& value = arr[lt];

	while (i <= gt)
	{
		if (arr[i] < value)
		{
			// lt - 1 始终就是左边界
			std::swap(arr[i], arr[lt]);
			lt++;
		}
		else if (arr[i] > value)
		{
			// gt + 1 开始的元素一定 >  value。
			std::swap(arr[i], arr[gt]);
			gt--;
		}
		else
		{
			i++;
		}
	}

	// 其上下界在什么位置？
	left = lt - 1;//有可能  lt - 1 < 0 但是没关系，这是新loop的右边界
	right = gt + 1; //有可能 gt + 1 > arr.size() - 1, 但是没关系，这是新loop的左边界
}
/*
* loop 中应该不停记录originalLeft和originaRight，分治思想下，这俩是每次小问题的边界
*/
void TribleQuickSort(std::vector<int>& arr, int& left, int& right)
{
	if (left >= right) return;
	int originalLeft = left;
	int originaRight = right;
	TriblePartition(arr, left, right);
	TribleQuickSort(arr, originalLeft, left);
	TribleQuickSort(arr, right, originaRight);
}


/*
* 小顶堆示例
* 关键：数组下标和堆节点的联系
*	// 当前节点 i
left  = 2*i + 1
right = 2*i + 2
parent = (i - 1) / 2
*/
// 让k上浮到合适位置
void Swim(std::vector<int>& arr, int k)
{
	int parentIndex = (k - 1) / 2;
	while (parentIndex >= 0 && arr[k] < arr[parentIndex])
	{
		std::swap(arr[k], arr[parentIndex]);
		k = parentIndex;
		parentIndex = (k - 1) / 2;
	}
}

//private void sink(int k)
//{
//	while (2 * k <= N
//		{
//		int j = 2 * k;
//		if (j < N && less(j, j + 1)) j++;
//		if (!less(k, j)) break;
//		exch(k, j);
//		k = j;
//		}
//}

void Sink(std::vector<int>& arr, int k)
{
	int leftChild = k * 2 + 1;
	int rightChild = k * 2 + 2;

	while ()
	{
		if (arr[k] > arr[leftChild])
		{
			std::swap(arr[k], arr[leftChild]);
		}

		if (arr[k] > arr[rightChild])
		{
			std::swap(arr[k], arr[rightChild]);
		}
	}



}



