#include <bit>
#include <cassert>
#include <deque>
#include <iostream>
#include <optional>
#include <stack>
#include <string>
#include "AngelScript/include/angelscript.h"
#include "AngelScript/scriptstdstring/scriptstdstring.h"
#include "AngelScript/scriptbuilder/scriptbuilder.h"
#include "AngelScript/source/as_tokendef.h"
#include "Test/Test.h"

void MessageCallback(const asSMessageInfo* msg, void* param)
{
	const char* type = "ERR ";
	if (msg->type == asMSGTYPE_WARNING)
		type = "WARN";
	else if (msg->type == asMSGTYPE_INFORMATION)
		type = "INFO";
	printf("%s (%d, %d) : %s : %s\n", msg->section, msg->row, msg->col, type, msg->message);
}

void print(const std::string& msg)
{
	printf("%s", msg.c_str());
}

//test angelscript


struct FScriptStructWildcard
{
	FScriptStructWildcard(){};
	~FScriptStructWildcard(){};
};
std::string* GetMemory(std::string* self)
{
	return self;
}


void TestAs()
{
	// 1.注册标准string 以及一个cpp的打印函数
	asIScriptEngine* Engine = asCreateScriptEngine();
	int r = Engine->SetMessageCallback(asFUNCTION(MessageCallback), 0, asCALL_CDECL);
	assert(r >= 0);
	RegisterStdString(Engine);


	{
		// 允许对int这样Primitivetype类型和值类型使用 inout&
		Engine->SetEngineProperty(asEP_ALLOW_UNSAFE_REFERENCES, 1);
		//禁止自动GC
		Engine->SetEngineProperty(asEP_AUTO_GARBAGE_COLLECT, 0);
		//允许脚本中使用=语义绑定参数，但是会给警告
		Engine->SetEngineProperty(asEP_ALTER_SYNTAX_NAMED_ARGS, 1);

		// 禁用掉引用类型的赋值拷贝操作（构造拷贝还是有效的）
		/*
		 * 	FTest001 ab = a; // 成功 调用到拷贝构造
			a = ab;// 报错
			TestOut(a);// 报错
		 */
		Engine->SetEngineProperty(asEP_DISALLOW_VALUE_ASSIGN_FOR_REF_TYPE, 1);

		//默认使用@声明的类型的所有变量将会默认是handle， 立即初始化没用了FTest001 a(100), 对于引用类型必须	FTest001 a = FTest001(10);
		Engine->SetEngineProperty(asEP_ALLOW_IMPLICIT_HANDLE_TYPES, 1);

		// enum 访问，必须使用 enum class:member的方式进行访问
		Engine->SetEngineProperty(asEP_REQUIRE_ENUM_SCOPE, 1);
		// 永远生成默认的构造器，也合理，不然&out的时候错误一堆。
		Engine->SetEngineProperty(asEP_ALWAYS_IMPL_DEFAULT_CONSTRUCT, 1);

		//有get set前缀的默认识别为虚拟属性
		Engine->SetEngineProperty(asEP_PROPERTY_ACCESSOR_MODE, 3);
	}

	r = Engine->RegisterGlobalFunction("void print(const string &in)", asFUNCTION(print), asCALL_CDECL);

	r = Engine->RegisterGlobalFunction("string@ printxs(string@)", asFUNCTION(GetMemory), asCALL_CDECL);
	assert(r >= 0);

	// 2.加载、编译脚本
	CScriptBuilder builder;
	{

		r = builder.StartNewModule(Engine, "MyModule");

		r = builder.AddSectionFromFile("test.as");
		r = builder.AddSectionFromFile("test2.as");


		r = builder.BuildModule();
	}

	/*{
		r = builder.StartNewModule(Engine, "MyModule2");
		r = builder.AddSectionFromFile("test2.as");
		r = builder.BuildModule();
	
	}*/



	//3.创建上下文，查找并执行脚本函数
	asIScriptModule* mod = Engine->GetModule("MyModule");
	asIScriptFunction* func = mod->GetFunctionByDecl("void main(int)");
	if (func == 0)
	{

		printf("The script must have the function 'void main()'. Please add it and try again.\n");
		return;
	}

	asIScriptContext* ctx = Engine->CreateContext();
	ctx->Prepare(func);
	r = ctx->Execute();
	if (r != asEXECUTION_FINISHED)
	{
		if (r == asEXECUTION_EXCEPTION)
		{
			printf("An exception '%s' occurred. Please correct the code and try again.\n", ctx->GetExceptionString());
		}
	}
}

class Test25111
{
public:
	int TestFunc(int a, int b){}
};

typedef int (*DLLFunc)(MainTest* mainPtr);



#define METHODPR(Ret,Cls,Name,Args) ((Ret(Cls::*)Args)&Cls::Name), #Cls, #Name, false

#include <Windows.h>
void TestLoadLibrary()
{

	HMODULE hDll = LoadLibraryA("Transfer.dll");
	if (!hDll)
	{
		DWORD error = GetLastError();
		return ;
	}
	DLLFunc DLLEntryFunc = reinterpret_cast<DLLFunc>(GetProcAddress(hDll, "TestTransfer"));
	if (!DLLEntryFunc)
	{
		FreeLibrary(hDll);
		return;
	}
	MainTest* ptr = new MainTest();
	DLLEntryFunc(ptr);
}




template<typename T>
struct TAutoDereference
{
	T* Ptr;
	
	TAutoDereference(T* InPtr)
		: Ptr(InPtr)
	{
	}

	TAutoDereference(void* InPtr)
		: Ptr((T*)InPtr)
	{
	}
	TAutoDereference(const void* InPtr)
		: Ptr((T*)InPtr)
	{
	}



	TAutoDereference(const T& InPtr)
		: Ptr((T*)&InPtr)
	{
	}

	TAutoDereference(T& InPtr)
		: Ptr(&InPtr)
	{
	}

	TAutoDereference(T&& InPtr)
		: Ptr((T*)&InPtr)
	{
	}

	FORCEINLINE operator T& ()
	{
		return *Ptr;
	}

	FORCEINLINE operator T* ()
	{
		return Ptr;
	}

	FORCEINLINE operator void* ()
	{
		return Ptr;
	}
};



struct Rawtewfu9ias
{
	int a = 0;
	template <typename T>
	void Append(T Str){}
	template <typename T>
	void Append2(T& Str) {}

	void Append3(int& Str) {}
};

struct F1Entisdf
{
	
};


const Rawtewfu9ias* GetTestsdfas222()
{
	static Rawtewfu9ias value;
	return &value;

}


const Rawtewfu9ias& GetTestsdfas333()
{
	static Rawtewfu9ias value;
	return value;

}

F1Entisdf* GetTestsdfas()
{
	return new F1Entisdf();

}


template<typename T>
auto GetGeneralPointer(T&& vlaue)
{
	if constexpr (std::is_reference_v<T>)
	{
		return (void*)(&vlaue);


	}
	else
	{
		return (void*)(vlaue);
	}
}

class TEst0015a
{
	template<typename T>
	void TestFunc(T a)
	{}

};

template<typename T>
void TEsthuidavb(T& value)
{
	
}

template<typename F>
constexpr bool Register(F&& f)
{
	if constexpr (std::is_member_function_pointer_v<std::decay_t<F>>)
	{
		// 成员函数绑定
		return true;
	}
	else if constexpr (std::is_invocable_v<F>)
	{
		// 普通函数 / lambda / 函数对象
		return true;
	}
	else
	{
	
		return false;
	}
}
#define TEXT L

#define F1DATAKEY_CPP_BIND(T, DataSource, DataSourceName, DataType, InModuleName) T+ DataSource+DataSourceName+DataType+ TEXT(InModuleName)

#define F1DATAKEY_CPP_BIND4(T, DataSource, DataSourceName, DataType) F1DATAKEY_CPP_BIND(T, DataSource, DataSourceName, DataType, "")

#define F1DATAKEY_CPP_BIND5(T, DataSource, DataSourceName, DataType, InModuleName) F1DATAKEY_CPP_BIND(T, DataSource, DataSourceName, DataType, InModuleName)


#define GET_F1DATAKEY_ARG(_0, _1, _2, _3, _4, _5, ...) _5

#define F1DATAKEY_ARGS_FILTER(...) GET_F1DATAKEY_ARG(__VA_ARGS__, F1DATAKEY_CPP_BIND5, F1DATAKEY_CPP_BIND4)(__VA_ARGS__)


#define F1DATAKEY(T, DataSource, DataSourceName, DataType, ...) \
F1DATAKEY_ARGS_FILTER(T, DataSource, DataSourceName, DataType, ##__VA_ARGS__)

#define F1DATAKEY2(T, DataSource, DataSourceName, DataType, ...) \
F1DATAKEY_ARGS_FILTER(T, DataSource, DataSourceName, DataType, ##__VA_ARGS__)


template<typename T>
struct BindIndacateModule
{
	static constexpr bool value = true;
};
template<>
struct BindIndacateModule<void>
{
	static constexpr bool value = false;
};
int FF1DataKey_EntityInstance{};
int UDataRegistry {};
int UF1EntityInstanceConfig {};


struct FF1GeneralConditionBaseTypeAS
{


	template<typename T>
	static void AsConstructor_Template(FF1GeneralConditionBaseTypeAS& DestinationPtr)
	{

	}
};

template<typename OutT, typename InT>
FORCEINLINE OutT value_as(InT InValue)
{
	if constexpr (std::is_same_v<OutT, InT>)
	{
		return InValue;
	}
	else if constexpr (sizeof(InT) < sizeof(OutT))
	{
		OutT ExtendedValue = {};
		memcpy(&ExtendedValue, &InValue, sizeof(InT));
		return ExtendedValue;
	}
	else
	{
		OutT OutValue;
		memcpy(&OutValue, &InValue, sizeof(OutT));
		return OutValue;
	}
}


class Test4186897
{
	int a = 10;
	int testcu8sdf(int v)
	{
		return a + v;
	}
};


template <typename C>
asMETHOD_t Store(void (C::* method)())
{
	return reinterpret_cast<asMETHOD_t>(method);
}

template <typename C>
void Call(asMETHOD_t m, C* obj)
{
	auto real = reinterpret_cast<void (C::*)()>(m);
	(obj->*real)();
}

bool GetFinalIndex(std::vector<int>& nums, int index)
{
	if (nums[index] == 0)
	{
		if (index == nums.size() - 1)
		{
			return true;
		}
		else {
			return false;
		}
	}

	if (index + nums[index] >= nums.size() - 1)
	{
		return true;
	}
	else
	{
		for (int i = 1; i <= nums[index]; i++)
		{
			if (GetFinalIndex(nums, index + i))
			{
				return true;
			}
		}
		return false;

	}
}
// 每次能跳跃的最大长度，更新最大长度
bool canJump(std::vector<int>& nums) {
	if (nums.size() == 0) return false;
	int maxLength = 0 + nums[0];
	for (int i = 1; i < nums.size(); i++)
	{
		if (i <= maxLength)
		{
			if (nums[i] + i >= maxLength)
			{
				maxLength = nums[i] + i;
			}
		}
		else
		{
			break;
		}
	}
	if (maxLength < nums.size() - 1)
	{
		return false;
	}
	return true;
}

bool IsKStepup(std::vector<int>& nums, int k)
{
	for (int i = k - 1; i >= 0; i--)
	{
		if (nums[i] + i >= k)
		{
			bool succ = IsKStepup(nums, i);
			if (succ)
			{
				return succ;
			}
		}
	}
	if ( k == 0)
	{
		return true;
	}
	return false;
}


bool canJump2(std::vector<int>& nums) {
	if (nums.size() == 0) return false;
	return IsKStepup(nums, nums.size() - 1);
}


void jumpreslt(std::vector<int>& nums, std::set<int> records, int& jumptimes)
{
	jumptimes++;
	std::set<int> curtargetsIds;
	while (records.size() > 0)
	{
		int curtargerId = *records.begin();
		records.erase(records.begin());
		for (int i = curtargerId - 1; i >= 0; i--)
		{
			if (nums[i] + i >= curtargerId)
			{
				if (i == 0)
				{
					return;
				}
				// 可以一步跳跃到当前位置的所有id
				curtargetsIds.emplace(i);
			}
		}
	}
	jumpreslt(nums, curtargetsIds, jumptimes);
}

int jump(std::vector<int>& nums) {
	if (nums.size() <= 1) return 0;

	int jumptimes = 0;
	std::set<int> curtargetsIds;
	curtargetsIds.emplace(nums.size() - 1);
	jumpreslt(nums, curtargetsIds, jumptimes);
	return jumptimes;
}


struct FTextTest
{
	FTextTest() { std::puts("ctor"); }
	FTextTest(const FTextTest&) { std::puts("copy"); }
	FTextTest(FTextTest&&) noexcept { std::puts("move"); }
	~FTextTest() { std::puts("dtor"); }
};

struct Test01496
{
	FTextTest Text;
	FTextTest TestReturnTest()
	{
		return Text;
	}

	static FTextTest TestReturnTest2()
	{
		//FTextTest re;
		return FTextTest();
	}
};

class FBaseTest
{
public:
	void Test001()
	{
		int a = 0;
		a = a + 1;
	}
};
class FBaseTest2
{
public:
	void Test001()
	{
		int a = 0;
		a = a + 1;
	}
};

void ThisfirstFunc()
{
	int a = 0;
	int  b = 12;
	return;

}

extern void Test45486896();
int main(int argc, const char** argv)
{
	auto it = &FBaseTest::Test001;
	auto it2 = &FBaseTest2::Test001;
	return 0;
}
