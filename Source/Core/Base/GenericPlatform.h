#pragma once
#include <chrono>
#include <functional>
#include <memory>
#include <string>

#ifndef PLATFORM_CPU_X86_FAMILY
#if (defined(_M_IX86) || defined(__i386__) || defined(_M_X64) || defined(__amd64__) || defined(__x86_64__))
#define PLATFORM_CPU_X86_FAMILY	1
#else
#define PLATFORM_CPU_X86_FAMILY	0
#endif
#endif
/*
 * 编译期筛选出当前是32位还是64位系统
 */
template<typename T32, typename  T64, int pointersize>
struct SelectIntPointerType
{
	
};

template<typename T32, typename  T64>
struct SelectIntPointerType<T32, T64, 8>
{
	using TIntPointer = T64;
};

template<typename T32, typename  T64>
struct SelectIntPointerType<T32, T64, 4>
{
	using TIntPointer = T32;
};

struct FGenericPlatformTypes
{
	// 8-bit unsigned integer
	typedef unsigned char 		uint8;

	// 16-bit unsigned integer
	typedef unsigned short int	uint16;

	// 32-bit unsigned integer
	typedef unsigned int		uint32;

	// 64-bit unsigned integer
	typedef unsigned long long	uint64;

	//~ Signed base types.

	// 8-bit signed integer
	typedef	signed char			int8;

	// 16-bit signed integer
	typedef signed short int	int16;

	// 32-bit signed integer
	typedef signed int	 		int32;

	// 64-bit signed integer
	typedef signed long long	int64;

	//~ Character types

	// An ANSI character. 8-bit fixed-width representation of 7-bit characters.
	typedef char				ANSICHAR;

	// A wide character. In-memory only. ?-bit fixed-width representation of the platform's natural wide character set. Could be different sizes on different platforms.
	typedef wchar_t				WIDECHAR;

	// An 8-bit character type. In-memory only. 8-bit representation. Should really be char8_t but making this the generic option is easier for compilers which don't fully support C++11 yet (i.e. MSVC).
	typedef uint8				CHAR8;

	// A 16-bit character type. In-memory only.  16-bit representation. Should really be char16_t but making this the generic option is easier for compilers which don't fully support C++11 yet (i.e. MSVC).
	typedef uint16				CHAR16;

	// A 32-bit character type. In-memory only. 32-bit representation. Should really be char32_t but making this the generic option is easier for compilers which don't fully support C++11 yet (i.e. MSVC).
	typedef uint32				CHAR32;

	// A switchable character. In-memory only. Either ANSICHAR or WIDECHAR, depending on a licensee's requirements.



	using UPTRINT = SelectIntPointerType<uint32, uint64, sizeof(void*)>::TIntPointer;
	using PTRINT = SelectIntPointerType<int32, int64, sizeof(void*)>::TIntPointer;

	typedef UPTRINT SIZE_T;

	typedef PTRINT SSIZE_T;

	typedef int32					TYPE_OF_NULL;
	typedef decltype(nullptr)		TYPE_OF_NULLPTR;
};

//我们现在只有windows平台
#if defined(_WIN64) || defined(_WIN32)
struct FWindowsPlatformTypes : public FGenericPlatformTypes
{
#ifdef _WIN64
	typedef unsigned __int64	SIZE_T;
	typedef __int64				SSIZE_T;
#else
	typedef unsigned long		SIZE_T;
	typedef long				SSIZE_T;
#endif
};
using FPlatformTypes = FWindowsPlatformTypes;


#define VARARGS     __cdecl											/* Functions with variable arguments */
#define CDECL	    __cdecl											/* Standard C function */
#define STDCALL		__stdcall										/* Standard calling convention */
#define FORCEINLINE __forceinline									/* Force code to be inline */
#define FORCENOINLINE __declspec(noinline)							/* Force code to NOT be inline */

#endif


//跨平台定义在这里

//~ Unsigned base types.
/// An 8-bit unsigned integer.
typedef FPlatformTypes::uint8		uint8;
/// A 16-bit unsigned integer.
typedef FPlatformTypes::uint16		uint16;
/// A 32-bit unsigned integer.
typedef FPlatformTypes::uint32		uint32;
/// A 64-bit unsigned integer.
typedef FPlatformTypes::uint64		uint64;

//~ Signed base types.
/// An 8-bit signed integer.
typedef	FPlatformTypes::int8		int8;
/// A 16-bit signed integer.
typedef FPlatformTypes::int16		int16;
/// A 32-bit signed integer.
typedef FPlatformTypes::int32		int32;
/// A 64-bit signed integer.
typedef FPlatformTypes::int64		int64;

//~ Character types.
/// An ANSI character. Normally a signed type.
typedef FPlatformTypes::ANSICHAR	ANSICHAR;
/// A wide character. Normally a signed type.
typedef FPlatformTypes::WIDECHAR	WIDECHAR;
/// Either ANSICHAR or WIDECHAR, depending on whether the platform supports wide characters or the requirements of the licensee.

/// An 8-bit character containing a UTF8 (Unicode, 8-bit, variable-width) code unit.
typedef FPlatformTypes::CHAR8		UTF8CHAR;
/// A 16-bit character containing a UCS2 (Unicode, 16-bit, fixed-width) code unit, used for compatibility with 'Windows TCHAR' across multiple platforms.
typedef FPlatformTypes::CHAR16		UCS2CHAR;
/// A 16-bit character containing a UTF16 (Unicode, 16-bit, variable-width) code unit.
typedef FPlatformTypes::CHAR16		UTF16CHAR;
/// A 32-bit character containing a UTF32 (Unicode, 32-bit, fixed-width) code unit.
typedef FPlatformTypes::CHAR32		UTF32CHAR;

/// An unsigned integer the same size as a pointer
typedef FPlatformTypes::UPTRINT UPTRINT;
/// A signed integer the same size as a pointer
typedef FPlatformTypes::PTRINT PTRINT;
/// An unsigned integer the same size as a pointer, the same as UPTRINT
typedef FPlatformTypes::SIZE_T SIZE_T;
/// An integer the same size as a pointer, the same as PTRINT
typedef FPlatformTypes::SSIZE_T SSIZE_T;

/// The type of the NULL constant.
typedef FPlatformTypes::TYPE_OF_NULL	TYPE_OF_NULL;
/// The type of the C++ nullptr keyword.
typedef FPlatformTypes::TYPE_OF_NULLPTR	TYPE_OF_NULLPTR;

//我们直接用std::string 代理FString
using FString = std::string;

//我们直接用std::chrono::duration 来表示FTimeSpan  Nime/Dime表示间隔多少秒
template<int64 Nime = 1, int64 Dime = 10'000'000>
using FTimespan = std::chrono::duration<int64, std::ratio<Nime, Dime>>;
template<int64 Nime = 1, int64 Dime = 10'000'000, typename Rep, typename Period>
constexpr auto FTimespanCast(const std::chrono::duration<Rep, Period>& d)
{
	return std::chrono::duration_cast<std::chrono::duration<Rep, std::ratio<Nime, Dime>>>(d);
}

template<typename T>
using TFunction = std::function<T>;



/*
 * std::shared_ptr的引用计数本身也是atomic的，这点和TRefCountPtr一致。
 * 不过TRefCountPtr 要求的引用计数是侵入式的，同时它还支持序列化,
 * 我们后面再实现序列化，所以这里先用  TRefCountPtr = std::shared_ptr, 做完序列化再用自己的sharedptr
 */
template<typename T>
using TRefCountPtr = std::shared_ptr<T>;

#if PLATFORM_WIN
#define PLATFORM_CACHE_LINE_SIZE 64
#else
#define PLATFORM_CACHE_LINE_SIZE 64
#endif



