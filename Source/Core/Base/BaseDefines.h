#pragma once
#include "GenericPlatform.h"
extern uint32		GGameThreadId;
extern uint32		GRenderThreadId;

#define NONCOPYABLE(TypeName) \
	TypeName(TypeName&&) = delete; \
	TypeName(const TypeName&) = delete; \
	TypeName& operator=(const TypeName&) = delete; \
	TypeName& operator=(TypeName&&) = delete;


#include <iostream>
#include <cstdlib>

#if defined(_DEBUG) || !defined(NDEBUG)
#define verify(expr) \
    do { \
      if (!(expr)) { \
        std::cerr << "Verification failed: " #expr \
                  << "\nFile: " << __FILE__ \
                  << "\nLine: " << __LINE__ << std::endl; \
        std::abort(); \
      } \
    } while(0)
#else
#define verify(expr) \
    do { \
      if (!(expr)) { \
        std::cerr << "Verification failed (Release): " #expr \
                  << "\nFile: " << __FILE__ \
                  << "\nLine: " << __LINE__ << std::endl; \
      } \
    } while(0)
#endif

// TaskGraph 中s 是否分配高优先级任务的线程以及后台任务的线程
#define CREATE_HIPRI_TASK_THREADS (1)
#define CREATE_BACKGROUND_TASK_THREADS (1)


class FNoncopyable
{
protected:
	// ensure the class cannot be constructed directly
	FNoncopyable() {}
	// the class should not be used polymorphically
	~FNoncopyable() {}
private:
	FNoncopyable(const FNoncopyable&);
	FNoncopyable& operator=(const FNoncopyable&);
};
