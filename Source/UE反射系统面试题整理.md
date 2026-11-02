# UE 反射系统面试题整理

> 适用方向：UE 中高级引擎开发、底层框架、工具链及 Gameplay 框架。重点是理解 **UHT → 类型注册 → 属性布局 → GC / 序列化 / 函数调用** 的完整链路。

## 一、基础架构

- [ ] **UE 为什么需要反射系统？C++ RTTI 为什么不够？** 解释元数据、属性枚举、序列化、GC、蓝图、RPC，以及 UE 自己的类型系统。
- [ ] **UObject、UField、UStruct、UClass、UFunction、FProperty 的继承关系是什么？** 画出 UObject / UField 和 FField / FProperty 两套体系，解释 UProperty 向 FProperty 的迁移。
- [ ] **UClass 和普通 C++ class 是什么关系？** 区分 C++ 对象类型、运行时类型描述对象、CDO 与 StaticClass。
- [ ] **UStruct::Children 和 ChildProperties 有什么区别？** 解释 UFunction 和 FProperty 分别存储在哪里，以及 Next 链表。
- [ ] **StaticClass、GetClass、IsA、Cast 和 CastField 有什么区别？** 区分原生类、蓝图生成类、对象的实际运行时类型，以及 UObject 和 FField 两套类型体系；解释 FFieldClass 描述什么。
- [ ] **PropertyFlags 与 meta=(...) 有什么区别？** 区分运行时行为标志和编辑器元数据，研究 WITH_METADATA、WITH_EDITORONLY_DATA 与打包裁剪；业务逻辑不能假定编辑器元数据在运行时存在。

## 二、UHT 与注册流程

- [ ] **UHT 的工作原理是什么？** UBT 调用 UHT、解析头文件、生成 `.generated.h` 和 `.gen.cpp`。
- [ ] **GENERATED_BODY() 宏展开后包含什么？** 重点解释 `DECLARE_CLASS`、`StaticRegisterNatives`、构造函数辅助代码和友元声明（具体随类型与 UE 版本变化）。
- [ ] **StaticClass() 怎样进入类型构造入口？与 GetPrivateStaticClass() 有什么关系？** 类对象的获取、构造与静态注册机制；旧版本常经 GetPrivateStaticClass，本地 ATestActor 的 DECLARE_CLASS2 直接使用生成的 Z_Construct_UClass_* 入口。
- [ ] **Z_Construct_UClass_XXX() 做了什么？** 构造元数据、依赖类型、函数与属性描述及构造注册。
- [ ] **FClassParams、FPropertyParamsBase 和编译期注册辅助机制有什么作用？** 生成代码如何把静态描述数据转换成运行时反射对象；注意不同 UE 版本中注册辅助类型不同。
- [ ] **类型依赖为什么需要分阶段构造？** 区分类对象骨架、完整属性/函数信息和 CDO；以本地 UE5.8 生成代码中的 ETypeConstructPhase::Inner / Outer 为例。注意类与结构体的两个阶段不能简单套用同一套实现。

## 三、反射数据结构

- [ ] **FProperty 如何描述成员变量？** `PropertyFlags`、`Offset_Internal`、`ElementSize`、`ArrayDim`、类型派生类。
- [ ] **如何通过 FProperty 读取或修改对象成员？** `ContainerPtrToValuePtr`、类型检查、对象内存布局。
- [ ] **UClass、UStruct、UScriptStruct 有什么区别？** 对象类型、普通结构体反射描述和函数反射描述。
- [ ] **PropertyLink、RefLink、DestructorLink 等链表有什么作用？** 不同操作场景下的预处理链表，以及与 `ChildProperties` 的区别。
- [ ] **为什么 FProperty 不再继承 UObject？** 内存开销、GC 管理成本、对象生命周期和轻量化设计。

## 四、运行时核心机制

- [ ] **UE 的 GC 如何利用反射系统？** 对象引用属性、引用遍历描述、`AddReferencedObjects`、非反射引用；对比旧 Token Stream 与本地 UE5.8 的 Reference Schema，避免只按旧类型名理解当前实现。
- [ ] **UE 序列化如何利用 FProperty？** `SerializeItem`、容器属性、反射属性遍历、带标签与无版本序列化、默认值比较、字段增删/改名/类型变化、Core Redirects 和自定义版本。
- [ ] **Transient、SaveGame 等标志怎样影响序列化？** 区分资产保存和存档场景；SaveGame 标志需结合 Archive 的 IsSaveGame 行为，不会自动创建存档，也不应把任意内存布局直接写入文件。
- [ ] **ProcessEvent 如何调用 UFunction？** 函数查找、函数标志、原生 thunk、Native 函数和蓝图函数调用；普通 C++ 直接调用原生函数与 ProcessEvent 路径有何区别。
- [ ] **动态函数调用的参数内存如何管理？** CPF_Parm / CPF_ReturnParm / CPF_OutParm / CPF_ReferenceParm、ParmsSize、对齐、初始化、返回值读取和销毁；研究 FStructOnScope。参数缓冲区是反射调用使用的数据布局，不能等同于 C++ ABI 调用栈。
- [ ] **RPC 为什么依赖 UFunction？** `FUNC_Net` 标志、网络调用分发、参数序列化。
- [ ] **CDO 是什么？如何与反射属性及对象构造关联？** 默认值初始化、对象创建和蓝图类默认属性。
- [ ] **UScriptStruct 如何构造、复制、比较和销毁未知类型的实例？** ICppStructOps、TCppStructOps、结构体 traits 与原生操作；为什么 FString、容器等非平凡类型不能统一 memcpy。
- [ ] **强引用、弱引用和软引用有什么区别？** TObjectPtr、TWeakObjectPtr、TSoftObjectPtr、TStrongObjectPtr、FGCObject 与 AddReferencedObjects；区分 GC 可达性、对象生命周期和资产加载依赖。研究增量 GC 写屏障；对象持有的强引用要保持目标可达，持有者自身也必须可达。

## 五、深入源码与设计

- [ ] **UHT 为什么不直接使用标准 C++ 编译器生成反射信息？** 独立解析、构建流程、宏约束、生成代码与维护成本。
- [ ] **为什么 UStruct 需要 Link 阶段？** 属性布局信息、属性链表、引用处理和初始化；区分原生 C++ 编译器确定的布局与动态生成类型的布局，研究 LinkWithoutChangingOffset，不能概括成重新排布所有原生 C++ 成员。
- [ ] **热重载或 Live Coding 对反射系统有什么影响？** 类重建、实例迁移、旧类型引用及重实例化。
- [ ] **如果让你设计一个 C++ 反射系统，你会怎么做？** 类型描述、代码生成、注册、类型擦除和成员访问。
- [ ] **UE 反射系统有哪些限制？** 并非任意 C++ 类型都能反射，UHT 支持的声明语法有限。
- [ ] **类型/属性/函数描述缓存什么时候失效？** 模块卸载、蓝图重编译、类型重实例化和 Live Coding；如何识别旧描述对象并刷新缓存。普通只读查询与注册/修改操作的线程要求也需要分别研究。
- [ ] **代码生成怎样接入可靠的增量构建？** 扫描参数与真实编译一致、头文件依赖、生成顺序、稳定输出、无变化时不重写、类型删除后的旧产物处理和错误诊断；结合 Python + libclang / CMake 方案设计。
- [ ] **生成代码如何合法访问私有字段并处理原生布局？** 友元、类内生成入口、成员指针/访问器；考虑继承、非标准布局类型、位域，不能把标准 offsetof 当成任意 C++ 类的通用解法。

---

## 六、重点深入的五道题

### 1. FProperty 为什么不再继承 UObject？

不能只回答“减少内存开销”，还应覆盖：

- UObject 对象管理、注册和 GC 的成本。
- `FField` 独立的所有权和生命周期管理。
- 属性描述实例数量多，轻量化收益明显。
- `FProperty` 仍有多态类型描述和访问能力。

**追问：** `FProperty` 的生命周期由谁管理？它描述的 UObject 引用如何被 GC 识别？

### 2. UHT 生成的反射信息如何注册？

典型流程：

```text
UCLASS / UPROPERTY 声明
    ↓
UBT 调用 UHT
    ↓
生成 .generated.h / .gen.cpp
    ↓
编译并链接生成代码
    ↓
模块加载、编译期类型注册信息可用
    ↓
按需构造或初始化 UClass / FProperty 等运行时元数据
```

**追问：** `StaticClass()` 为什么能返回对应的 `UClass*`？首次调用和后续调用有什么区别？

### 3. FProperty 如何访问 C++ 对象成员？

```cpp
UCLASS()
class UMyObject : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY()
    int32 Health = 100;
};
```

```cpp
FIntProperty* Prop = FindFProperty<FIntProperty>(
    UMyObject::StaticClass(), TEXT("Health"));

// 假设 Object 是仍然有效的 UObject*；类型判断不能检测已经悬空的裸指针。
if (Prop && Object && Object->IsA(UMyObject::StaticClass()))
{
    int32* Value = Prop->ContainerPtrToValuePtr<int32>(Object);
    UE_LOG(LogTemp, Log, TEXT("%d"), *Value);
}
```

**追问：** `FBoolProperty`、`FArrayProperty` 和 `FStructProperty` 为什么不能统一当成普通标量处理？

### 4. UE 的 GC 为什么需要反射？

- GC 如何知道 UObject 内有哪些 UObject 引用。
- `UPROPERTY` 和 `TObjectPtr` 各自的职责。
- 由属性生成引用遍历描述，避免每次 GC 都重新解释完整属性信息。
- 本地 UE5.8 的 UClass::ReferenceSchema 与 FProperty::EmitReferenceInfo；部分接口仍保留 AssembleReferenceTokenStream 名称，不能仅凭接口名判断底层数据结构。
- 为什么普通未追踪裸指针成员不会自动参与 GC。
- `AddReferencedObjects` 如何补充特殊引用。
- 仅使用 TObjectPtr 包装未追踪的成员并不能替代 GC 引用报告；启用增量可达性分析时还要理解引用写入的屏障。

### 5. 如果重新设计 UE 的反射系统？

| 设计维度 | 关键问题 |
| --- | --- |
| 类型系统 | TypeInfo、PropertyInfo、FunctionInfo 如何设计 |
| 元数据生成 | 宏、代码生成、Clang AST 如何取舍 |
| 类型注册 | 静态注册、按需初始化、跨模块注册 |
| 属性访问 | Offset、Getter/Setter、类型擦除 |
| 函数调用 | 参数打包、函数指针、动态分发 |
| 生命周期 | 元数据所有权、模块卸载、热重载 |
| 性能 | 元数据内存、查找缓存、运行时访问开销 |

---

## 七、建议学习顺序

1. **UHT 生成与注册**：从已有 ATestActor 出发，跟踪 GENERATED_BODY、StaticClass、Z_Construct_UClass_*、FRegisterCompiledInInfo、ProcessNewlyLoadedUObjects；区分静态生成描述与运行时类型对象。
2. **字段访问与类型判断**：研究 UStruct、FField、FProperty、FFieldClass、StaticClass / GetClass、Cast / CastField、ContainerPtrToValuePtr，以及原生布局与 Link 阶段。
3. **结构体生命周期**：研究 UScriptStruct、ICppStructOps、非平凡类型的构造/复制/销毁，再开始 Demo 中手写类型描述和字段描述。
4. **函数调用**：研究 UFunction、exec thunk、ProcessEvent、参数属性及缓冲区生命周期。
5. **序列化**：字段遍历、属性标志、Archive、默认值、字段演进与兼容性。
6. **GC**：引用语义、引用遍历 Schema、手动引用报告、嵌套容器/结构体及增量 GC 写屏障。
7. **热重载与工具链设计**：元数据缓存失效、重实例化，以及 Python + libclang / CMake 的可靠生成流程。

每一步都按“能解释设计目的 → 找到本地源码入口 → 运行一个最小实验 → 总结对应 Demo 设计”推进。开始 Demo 代码生成前，先手写并验证生成器要产出的最小运行时结构。

对于渲染引擎岗位，还可以关联研究 `FShaderParametersMetadata`、Shader 参数布局、RDG 参数绑定等元数据驱动机制。它们不等于 UObject 反射，但具有可比较的设计思想。

## 八、实验验收清单

实验工程：`F:\workspace\UnrealEngine58\Games\RenderLab\ContentExample`。现有 ATestActor 已包含 FReflectionStats、ProbeStats 和 AddHealth，可作为起点。以下复选框表示实际验证情况，不表示仅阅读过相关代码。

- [ ] **生成与注册**：添加属性后，在 `.generated.h` / `.gen.cpp` 中找到对应描述、类型入口与注册信息；解释静态描述数据和运行时 FProperty 的区别。
- [ ] **字段访问**：遍历继承字段，按名称读取和修改标量、嵌套结构体及动态数组；确认容器地址与字段所属类型匹配。
- [ ] **结构体生命周期**：通过类型描述构造、复制和销毁含 FString 的结构体，验证正确值和资源释放。
- [ ] **函数调用**：通过 ProcessEvent 调用带参数和返回值的函数；再扩展非平凡参数，验证初始化和销毁。
- [ ] **序列化演进**：保存数据后分别增加、删除、改名和改变字段类型，观察加载与默认值行为，记录哪些变化需要迁移逻辑。
- [ ] **GC 引用**：比较来自可达对象的强引用、弱引用和未追踪引用，并验证嵌套结构体/容器的引用遍历；实验中避免解引用已回收对象。
- [ ] **蓝图与 CDO**：创建 ATestActor 的蓝图派生类，比较原生 StaticClass、实例 GetClass、两种类的 CDO 以及实例默认值。
- [ ] **缓存与构建**：修改/删除反射声明后验证生成文件更新；观察蓝图重编译或类型重实例化时，缓存的描述对象是否仍然有效。

每个实验记录：输入声明、生成代码位置、关键调用链、预期与实际结果，以及一个可解释的失败案例。

## 九、本地 UE5.8 源码核对要点

- **当前工程的生成形态**：`DECLARE_CLASS2` 使用生成的类型构造函数作为访问入口；ATestActor::StaticClass() 经该宏调用 Z_Construct_UClass_ATestActor(ETypeConstructPhase::Inner)。不要要求它一定经过旧资料中的 GetPrivateStaticClass()。
- **类的两阶段构造**：在当前 ATestActor 生成代码中，Inner 经 GetPrivateStaticClassBody 获取/创建类对象，Outer 经 ConstructUClass 填充完整类型信息；研究构造依赖时区分这两个阶段。
- **GC 数据结构**：`UObject/Class.h` 的 UClass::ReferenceSchema 是 UE::GC::FSchemaOwner；`Private/UObject/GarbageCollection.cpp` 中的 EmitReferenceInfo 生成引用描述，AssembleReferenceTokenStream 接口名仍保留。
- **原生布局**：`UObject/UnrealType.h` 的 LinkWithoutChangingOffset 用于保留偏移；原生字段偏移由生成代码中的 STRUCT_OFFSET 等表达式交给 C++ 编译器求值，不能理解为 UHT 独立模拟完整 C++ ABI。
- **存档标志**：`Private/UObject/Property.cpp` 的 FProperty::ShouldSerializeValue 检查 Archive::IsSaveGame 与 CPF_SaveGame；是否保存某字段还受其他属性标志和 Archive 上下文影响。
- **参数与结构体内存**：`UObject/StructOnScope.h`、UScriptStruct::ICppStructOps、UFunction::ParmsSize；缓冲区大小正确只是条件之一，还需要正确对齐和对象生命周期。

当前工程生成代码目录：`Intermediate\Build\Win64\UnrealEditor\Inc\ContentExample\UHT\`，重点阅读 TestActor.generated.h 和 TestActor.gen.cpp。生成宏中的行号会随源文件修改而变化，不应硬编码。

> 提示：本文为面试准备清单，不是源码逐行结论。具体符号、注册路径、GC 实现细节应以目标 UE 版本源码为准。
