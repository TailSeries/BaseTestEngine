#pragma once
#include "../Module.h"
#include "../TransferBase.h"
class  TRANSFERMODULE BaseObject
{
public:
	BaseObject()
	{
	};
	virtual void redirectTransfer(TransferBase& transfer);


	template<class TransferFunction> void Transfer(TransferFunction& transfer);



};


