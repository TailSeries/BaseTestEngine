#pragma once
#include "Logging/Log.h"

// 跨翻译单元分类：声明在头文件、定义在另一个 .cpp。
CORE_DECLARE_LOG_CATEGORY_EXTERN(LogSharedTest);
void LogFromOtherTranslationUnit();
