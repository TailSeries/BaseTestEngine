#pragma once
#include "CoreModule.h"
/*
 * 一套机制，用于在线程内部自动清理
 */

class COREMODULE FTlsAutoCleanup
{
public:
	virtual ~FTlsAutoCleanup()
	{
	}
	void Register();
};


//需要配合Funnable接口使用，它里面实际存储的是T的一份副本，生命周期和整个线程等同
template< class T >
class TTlsAutoCleanupValue
	: public FTlsAutoCleanup
{
public:

	/** Constructor. */
	TTlsAutoCleanupValue(const T& InValue)
		: Value(InValue)
	{
	}

	/** Gets the value. */
	T Get() const
	{
		return Value;
	}

	/* Sets the value. */
	void Set(const T& InValue)
	{
		Value = InValue;
	}

	/* Sets the value. */
	void Set(T&& InValue)
	{
		Value = MoveTemp(InValue);
	}

private:

	/** The value. */
	T Value;
};
