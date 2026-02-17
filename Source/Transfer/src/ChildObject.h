#pragma once
#include "TransferBase.h"
#include "Module.h"
#include "Objects/BaseObject.h"
class  TRANSFERMODULE ChildObject:public BaseObject
{
public:
	ChildObject()
	{
	};
	virtual void redirectTransfer(TransferBase& transfer);
	template<class TransferFunction> void Transfer(TransferFunction& transfer);

};


