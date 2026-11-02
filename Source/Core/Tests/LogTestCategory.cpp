#include "LogTestCategory.h"

CORE_DEFINE_LOG_CATEGORY(LogSharedTest, Log);

void LogFromOtherTranslationUnit()
{
    CORE_LOG(LogSharedTest, Log, "other translation unit");
}
