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
#include "Foo.hpp"
#include "Global.hpp"

extern Foo GFoo2;

int main()
{
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
		for(int& j :adj[i])
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


	return 0;
}
#include "TestGFoo.hpp"