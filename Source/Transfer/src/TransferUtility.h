#pragma once
class BaseObject;
struct TraitsBase
{
	virtual BaseObject& DownCastRealType(BaseObject& obj) = 0;
};




template<typename T>
struct TraitsType :TraitsBase
{
	using REAL_TYPE = T;
	virtual REAL_TYPE& DownCastRealType(BaseObject& obj) override {
		return reinterpret_cast<REAL_TYPE&>(obj);
	};
};
