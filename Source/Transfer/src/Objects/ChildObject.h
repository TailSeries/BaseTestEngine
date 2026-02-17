#pragma once
#include "../Module.h"
#include "../TransferBase.h"
class TestTransfer;
class  TRANSFERMODULE ChildObject
{
public:
	ChildObject()
	{
		TransferBase t;
		redirectTransfer(t);
	};
	virtual void redirectTransfer(TransferBase& transfer);


	template<class TransferFunction> void Transfer(TransferFunction& transfer);



};


