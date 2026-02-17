#include "ChildObject.h"


template <class TransferFunction>
void ChildObject::Transfer(TransferFunction& transfer)
{
	std::cout << "\ChildObject::Transfer TransferFunction\n";
}

void ChildObject::redirectTransfer(TransferBase& transfer)
{
	transfer.TransferDispatch(*this);
}
