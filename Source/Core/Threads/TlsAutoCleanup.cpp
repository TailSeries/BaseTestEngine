#include "TlsAutoCleanup.h"
#include "RunnableThread.h"

void FTlsAutoCleanup::Register()
{
	FRunnableThread* RunnableThread = FRunnableThread::GetRunnableThread();
	if (RunnableThread)
	{
		RunnableThread->TlsInstances.push_back(this);
	}
}
