#pragma once
#include <iostream>
#include "TransferUtility.h"
#include "Module.h"
// 获取子类型的类型特性
template<typename T>
struct DerivedType {
	using type = T;
};

class TestTransfer;
class TransferBase;
template<>
struct DerivedType<TransferBase>
{
	using type = TransferBase;
};

template<>
struct DerivedType<TestTransfer>
{
	using type = TestTransfer;
};





class BaseObject;
class TransferBase
{
public:
	
	void init()
	{
		
	};

	void Test001()
	{
	
		std::cout << "TransferBase Test001 " << std::endl;
	};

	virtual TransferBase& self() {
		return *this;
	}

	template<typename T>
	void TransferDispatch(T& data)
	{
		TraitsType<T> dataTraits;
		TransferDown(data, dataTraits);
	}

	//有个向下的虚函数来委托处理这个
	virtual void TransferDown(BaseObject& obj, TraitsBase& dataTraits) {};

};

template<typename T>
class ChildTranser:public TransferBase
{
public:
	virtual T& self() {
			return *this;
	}
};


#include "../../Main/Test/Test.h"
TRANSFERMODULE void TestTransfer(MainTest* mainPtr);



class TRANSFERMODULE TestTransfer001
{
public:
	TestTransfer001(int a){};
};

