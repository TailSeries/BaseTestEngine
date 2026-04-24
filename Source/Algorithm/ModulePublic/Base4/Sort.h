#pragma once
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <stack>

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
*	1.当前节点 i
		left  = 2*i + 1
		right = 2*i + 2
		parent = (i - 1) / 2
	2.堆的特点:以一个小顶堆为例，每个子树的顶点都是最小值。
	3.不限于二叉堆，甚至也可以三叉，四叉
	4.二叉堆的情况下，我们只需要遍历一半的元素。因为对于二叉堆，那一半的元素其实看做叶子结点，单节点其实是天然的ok的堆。

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



void Sink(std::vector<int>& arr, int k)
{
	int leftChild = k * 2 + 1;
	int rightChild = k * 2 + 2;
	while (leftChild < arr.size() || rightChild < arr.size())
	{
		int toBeSwapIndex = leftChild;
		if (rightChild < arr.size()) 
		{
			// 右子节点也在, 好处谁更小
			if (arr[rightChild] < arr[leftChild])
			{
				toBeSwapIndex = rightChild;
			}
			else
			{
				toBeSwapIndex = leftChild;
			}
		}

		if (arr[k] > arr[toBeSwapIndex])
		{
			std::swap(arr[k], arr[toBeSwapIndex]);
		}
		k = toBeSwapIndex;
		leftChild = k * 2 + 1;
		rightChild = k * 2 + 2;
	}
}
/*
* 每次往堆里构建一个新节点的前提是堆里已有的节点，已经组成一个有序堆
* 因此，我们顺序遍历用上浮 或者 逆序遍历用下沉，都可以原地构建一个有序堆。其中顺序遍历上浮需要遍历所有节点，但是逆序遍历只要一半就可以了
* 上浮的方式构建堆，复杂度为nlogn 但是下沉的方式构建堆，工作量每层只有上一层的一半所以复杂度反而是n
*/
void BuildHeap(std::vector<int>& arr)
{

	int N = arr.size() / 2;
	for (int i = N; i >= 0; i--)
	{
		Sink(arr, i);
	}
}

/*
* heapsort：
*/

void Sink(std::vector<int>& arr, int k, int tail)
{
	int leftChild = k * 2 + 1;
	int rightChild = k * 2 + 2;
	while (leftChild <= tail || rightChild <= tail)
	{
		int toBeSwapIndex = leftChild;
		if (rightChild <= tail)
		{
			// 右子节点也在, 好处谁更小
			if (arr[rightChild] < arr[leftChild])
			{
				toBeSwapIndex = rightChild;
			}
			else
			{
				toBeSwapIndex = leftChild;
			}
		}

		if (arr[k] > arr[toBeSwapIndex])
		{
			std::swap(arr[k], arr[toBeSwapIndex]);
		}
		k = toBeSwapIndex;
		leftChild = k * 2 + 1;
		rightChild = k * 2 + 2;
	}
}
void HeapSort(std::vector<int>& arr)
{
	int N = arr.size() / 2;
	int Tail = arr.size() - 1;
	for (int i = N; i >= 0; i--)
	{
		Sink(arr, i, Tail);
	}

	for (int i = 0; i <= Tail; )
	{
		std::swap(arr[0], arr[Tail]);
		Tail--;
		Sink(arr, i, Tail);
	}

	std::reverse(arr.begin(), arr.end());
}

/*
* 普通队列  FIFO：Map + 固定size的数组实现，支持dq[i]的随机访问
* 优先队列 其实就是大顶堆
*/

/*
* 二分查找
*/
int Rank(int value, std::vector<int>& arr, int lo, int hi)
{
	if (lo > hi) return -1;
	
	int mid = (lo + hi) / 2;
	if (arr[mid] == value) return mid;
	if (arr[mid] < value)
	{
		return Rank(value, arr, mid + 1, hi);
	}
	if (arr[mid] > value)
	{
		return Rank(value, arr, lo, mid - 1);
	}
	return -1;
}
/*
* 二叉查找树，左 < 中 < 右。注意这跟上面的小顶堆有本质的不同，小顶堆的父节点用于大于子节点，左右子节点的大小则无所谓。
* :事实上不太可能直接把一个数组弄成二叉查找树，数组一旦有序，BST直接退化成一条单链结构，这没意义。
* ：缺点对于顺序结构这是不平衡的
*/

struct BSTNode
{
	BSTNode* Left;
	BSTNode* Right;
	int key;
	int value;
	int N;
};

BSTNode* GetByKey(BSTNode* root, int inKey)
{
	if (root == nullptr) return nullptr;

	if (inKey == root->key)
	{
		return root;
	}
	if (inKey < root->key)
	{
		return GetByKey(root->Left, inKey);
	}
	else
	{
		return GetByKey(root->Right, inKey);
	}

}

BSTNode* PutBykey(BSTNode* root, int key, int value)
{
	if (root == nullptr)
	{
		return new BSTNode(nullptr, nullptr, key, value, 1);
	}
	if (key == root->key)
	{
		root->value = value;
		return root;
	}
	else if (key < root->key)
	{
		BSTNode* leaf = PutBykey(root->Left, key, value);
		if (root->Left == nullptr)
		{
			root->Left = leaf;
		}
		root->N++;
		return leaf;
	}
	else
	{
		BSTNode* leaf = PutBykey(root->Right, key, value);
		if (root->Right == nullptr)
		{
			root->Right = leaf;
		}
		root->N++;
		return leaf;
	}

	
}

void BST()
{



}

inline void PrintBSTImpl(BSTNode* node, const std::string& prefix, bool isLeft)
{
    if (!node) return;
    std::cout << prefix << (isLeft ? "\xe2\x94\x9c\xe2\x94\x80\xe2\x94\x80 " : "\xe2\x94\x94\xe2\x94\x80\xe2\x94\x80 ") << node->key << "\n";
    PrintBSTImpl(node->Left,  prefix + (isLeft ? "\xe2\x94\x82   " : "    "), true);
    PrintBSTImpl(node->Right, prefix + (isLeft ? "\xe2\x94\x82   " : "    "), false);
}

inline void PrintBST(BSTNode* root)
{
    if (!root) { std::cout << "(empty)\n"; return; }
    std::cout << root->key << "\n";
    PrintBSTImpl(root->Left,  "", true);
    PrintBSTImpl(root->Right, "", false);
}



struct AVLBSTNode
{
	AVLBSTNode* Left;
	AVLBSTNode* Right;
	int key;
	int value;
	int height;
};
/*
* 操作AVL的平衡：
* 关键点： 
	每次有新的节点进入就要重新平衡，而单次平衡的树高度，最大刚好就是2
	左旋与右旋的本质，就是先把三个节点弄成递增/递减的单链，然后把中间节点抽出来作为个根节点。
	这种做法改变树的平衡性，但不会破坏中序遍历的遍历结果。这一点很重要，对于红黑树也是这样的。反过来思考：如果我不得不将一颗平衡树改的不平衡的情况下，我中序遍历的结果仍然是一样的。
* 
*/

/*
* hash表，拉链法
*/


/*
* 图
* 连通图：任意节点互联互通，树可以认为是一副无环连通图。
*/

using Bag = std::vector<int>;
struct Graph
{
	int V;
	int E;
	std::vector<Bag> adj;
	Graph(int numv)
		:V(numv)
	{
		adj.resize(numv);
	}

	void AddEdge(int v, int w)
	{
		adj[v].push_back(w); 
		adj[w].push_back(v);
		E++;
	}

	Bag GetAdj(int v)
	{
		return adj[v];
	}


};

/*
* 所有点的度数之和 = 边数 × 2
* 度数，有多少边就是多少度
*/
int GetDegree(Graph& G, int v)
{
	return G.adj[v].size();
}

int MaxDegree(Graph& G)
{
	int maxDegress = 0;
	for (int i = 0; i < G.adj.size(); i++)
	{
		int curDegree = GetDegree(G, i);
		maxDegress = curDegree > maxDegress ? curDegree : maxDegress;
	}
	return maxDegress;
}

int AvgDegree(Graph& G)
{
	return 2*G.E / G.V;
}

int NumSelfLoops(Graph& G)
{
	int count = 0;
	for (int i = 0; i < G.adj.size(); i++)
	{
		auto& bag = G.adj[i];
		for (int& j : bag)
		{
			if (j == i)
			{
				count++;
			}
		}
	}
	return count / 2;
}


/*
* DFS
* 关键：连通图里面，DFS一定能访问到所有的节点，但不一定能访问到所有的边。 
*/
struct DFSTree
{
	std::vector<bool> marked; // 记录所有和v连通的点，包括自己，避免走重复节点
	int count; // 和s连通的点的数量，包括自己。
	DFSTree(Graph& G, int S)
	{
		s = S;
		marked.resize(G.V);
		DFS(G, s);
	}

	void DFS(Graph& G, int v)
	{
		marked[v] = true;
		count++;
		std::vector<int>& bag = G.adj[v];
		for (int next : bag)
		{
			if (!marked[next])
			{
				edgeTo[next] = v;
				DFS(G, next);
			}
		}
	}

	/*
		*单点路径问题：从s 到 p是否存在一条指定路径
		*marked 保证了可达性
		* edgeTo 在所有已知搜索树中，记录当前顶点最后可由谁到达
	*/
	std::vector<int> edgeTo;// 已知搜索树中，记录当前顶点最后可由谁到达
	int s;
	bool HasPathTo(int v)
	{
		return marked[v];
	}

	std::vector<int> GetPath(int v)
	{
		if (!HasPathTo(v)) return {};
		std::vector<int> result;
		int start = v;
		result.push_back(start);
		while (edgeTo[start] != s)
		{
			start = edgeTo[start];
			result.push_back(start);
		}

		result.push_back(s);
		std::reverse(result.begin(), result.end());
		return result;
	}
};






