#include "RHI.h"
#include "DynamicRHI.h"
//GDynamicRHI 的唯一定义（声明在 DynamicRHI.h）；启动时由 D3D12 侧赋值
RHIMODULE FDynamicRHI* GDynamicRHI = nullptr;
