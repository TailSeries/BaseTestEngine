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
#include "Algorithm/ModulePublic/Base4/Sort.h"

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



int main(int argc, const char** argv)
{
	std::vector<int> testarr{1, 2, 3, 3, 4, 2, 1, 5};
	//SelectSort(testarr);
	InsertSort(testarr);
	return 0;
}
