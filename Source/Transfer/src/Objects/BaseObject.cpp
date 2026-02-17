#include "BaseObject.h"
template TRANSFERMODULE void BaseObject::Transfer(TransferBase& transfer);

void BaseObject::redirectTransfer(TransferBase& transfer)
{
	//如果进来的是父类型
	transfer.self();//这已经了子transer了
	Transfer(transfer);
	transfer.TransferDispatch(*this);
}



//只有通用实现
template<class TransferFunc>
void BaseObject::Transfer(TransferFunc& transfer)
{
	//这儿实际相当于只提供了TransferBase的实现
	/*
	 *我们在外面依次重写
	 *
	 */
	std::cout << "\nBaseObject Original Transferfunc" << std::endl;
	//auto& value = transfer.GetRealType();
	transfer.Test001();
}
