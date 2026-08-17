#include <Windows.h>
#include <iostream>
#include <map>
#include <timeapi.h>
#include <vector>
#include "Base4/Sort.h"
#include "LeetCode/LeetCode.h"

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
#include "Foo.hpp"
#include "Global.hpp"
#include <unordered_map>
extern Foo GFoo2;

double GetDouble()
{
	int a = 1;
	int b = 3;
	std::vector<int> temp{ b };
	if (0)
	{
		return b / 2.0; // 1.5
	}
	else
	{
		return  temp[0] / 2.0; //1.0
	}
};

template<typename T>
struct TAutoDereference
{
	T* Ptr;

	TAutoDereference(T* InPtr)
		: Ptr(InPtr)
	{}

	TAutoDereference(void* InPtr)
		: Ptr((T*)InPtr)
	{}


	//[juhuangjie] 对应const * 情况
	TAutoDereference(const void* InPtr)
		: Ptr((T*)InPtr)
	{}


	TAutoDereference(const T& InPtr)
		: Ptr((T*)&InPtr)
	{}

	TAutoDereference(T& InPtr)
		: Ptr(&InPtr)
	{}

	FORCEINLINE operator T& ()
	{
		return *Ptr;
	}

	FORCEINLINE operator T* ()
	{
		return Ptr;
	}

	FORCEINLINE operator void* ()
	{
		return Ptr;
	}
};

class Test002569
{
public:
	int a = 10;
};

template<typename T>
void OpAssignValue_Template(void* ValuePtr)
{
	 auto it = *(T*)ValuePtr;
}



template<typename T>
struct TestTemplate001 {};
template<typename T1>
struct TestTemplate001<std::vector<T1>>
{
	
};

struct my_struct
{
	my_struct(bool value)
	{
		std::printf("sjdoifauaosidhf sadf\n");
	}
};

struct FEventReply
{


public:

	FEventReply(bool IsHandled = false)
		: NativeReply(IsHandled ? -1:1)
	{
		std::printf("FEventReply(bool IsHandled = false)");
	}
	int NativeReply = 120;
};


void Testshfiod(bool a = false)
{
	std::printf("void Test564(bool a = false)");
}

void Testshfiod(const FEventReply& av)
{
	std::printf("void Test564(FEventReply* a = nullptr)");
}

int main()
{

	TestTemplate001<int> vsdiou;
	int value = GFoo.a;
	SetConsoleOutputCP(CP_UTF8);

	std::vector<int> arr{ 0, 2, 1, 0, 3, 5, 6, 7, 7, 6, 8, 5 ,6 };

	BSTNode* root = PutBykey(nullptr, 0, 0);
	for (auto& itoa : arr)
	{
		BSTNode* Leaf = PutBykey(root, itoa, itoa);
	}

	std::vector<std::vector<int>> adj
	{
		{2, 1, 5},
		{0, 2},
		{0 ,1, 3, 4},
		{5,4,2},
		{3,2},
		{3, 0}
	};
	Graph G(6);
	for (int i = 0; i < adj.size(); i++)
	{
		for (int& j : adj[i])
			G.AddEdge(i, j);
	}
	G.adj = adj;
	//DFSTree dfsTree(G, 1);
	//bool result =  dfsTree.HasPathTo(0);
	//result = dfsTree.HasPathTo(4);
	//auto resultpath = dfsTree.GetPath(5);

	BFSTree bfsTree(G, 1);
	bool result = bfsTree.HasPathTo(0);
	result = bfsTree.HasPathTo(4);
	auto resultpath = bfsTree.GetPath(5);

	Solution s;
	std::vector<int> nums = { 1000000000,1000000000,1000000000,1000000000 };
	s.fourSum(nums, 0);

	int listNum = 4;
	std::vector<Solution::ListNode> listnodes(listNum);
	std::vector<Solution::ListNode*> listnodePtrs(listNum);
	for (int i = 0; i < listNum; i++)
	{
		listnodes[i].val = i + 1;
		listnodePtrs[i] = &listnodes[i];
	}
	for (int i = 0; i < listNum - 1; i++)
	{
		listnodePtrs[i]->next = listnodePtrs[i + 1];
	}

	//s.removeNthFromEnd(listnodePtrs[0], 2);


	//s.generateParenthesis(3);
	s.strStr("mississippi", "issip");

	std::string testiosayd = "barfoothefoobarman";
	std::vector<std::string> testuiwords = {"foo", "bar"};
	s.findSubstring(testiosayd, testuiwords);

	int wordLen = 4;
	int resudsf = 0;
	for (int i = 0; i < wordLen; i++) {

		for (int j = i; j + wordLen <= 100; j += wordLen)
		{
			resudsf++;
		}
	}

	{
		std::vector<int> ValuesVector = { 5,5,4,3,3,3,3,1 };
		int inddex = s.search(ValuesVector, 1);
		inddex = s.search(ValuesVector, 10);

		ValuesVector.insert(ValuesVector.begin(), 2);
		inddex = 11;
	}

	{
		std::vector<std::vector<char>> board = {
			{'5', '3', '.', '.', '7', '.', '.', '.', '.'},
			{'6', '.', '.', '1', '9', '5', '.', '.', '.'},
			{'.', '9', '8', '.', '.', '.', '.', '6', '.'},
			{'8', '.', '.', '.', '6', '.', '.', '.', '3'},
			{'4', '.', '.', '8', '.', '3', '.', '.', '1'},
			{'7', '.', '.', '.', '2', '.', '.', '.', '6'},
			{'.', '6', '.', '.', '.', '.', '2', '8', '.'},
			{'.', '.', '.', '4', '1', '9', '.', '.', '5'},
			{'.', '.', '.', '.', '8', '.', '.', '7', '9'}
		};

		 s.solveSudoku(board);

		 bool resultsdfsdf = 0;

	}


	{

		char c2 = 65;
		char c = 'A';
		auto resolt = s.countAndSay(4);
		int va = 224;

	}

	{
		std::vector<int> templist = { 2,3,6,7};
		s.combinationSum(templist, 7);
	}


	{
		std::vector<int> templist = {10, 1, 2, 7, 6, 1, 5};
		s.combinationSum2(templist, 8);
	}


	{
		std::vector<int> templist{ 3,4,-1,1};
		s.firstMissingPositive(templist);

	}


	{
		std::vector<int> templist{0,1,0,2,1,0,1,3,2,1,2,1};
		s.trap(templist);
	}

	{
		s.multiply("123", "456");
	}


	return 0;
}
#include "TestGFoo.hpp"