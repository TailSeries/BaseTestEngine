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

## 十、UHT 之后的完整学习清单

本节将运行时机制与面试追问分为 15 个专题。P0 要能结合源码与实验解释；P1 要理解调用链、设计取舍和故障场景；P2 根据岗位方向深入。已阅读 UHT 流程不等于完成运行时验证，各项待实际验证后再勾选。

### 1. 运行时类型体系与类型判断〔P0〕

- [ ] UObject / UField / UStruct / UClass / UScriptStruct / UFunction / UEnum 的职责和继承关系。
- [ ] FField / FFieldClass / FProperty 与 UObject 类型体系的区别，UProperty 迁移的成本与收益。
- [ ] StaticClass 与 GetClass，原生类、蓝图生成类及类型描述对象自身的类型。
- [ ] IsA、IsChildOf、Cast、CastChecked、CastField 的实现、失败行为和性能。
- [ ] ClassCastFlags 的适用范围，为什么不能给每个业务类分配一个独立 bit。

### 2. FProperty 与属性访问〔P0，当前开始〕

- [ ] 字段名称、Owner、类型、偏移、GetElementSize、ArrayDim 和 PropertyFlags。
- [ ] Children、ChildProperties、PropertyLink 等字段组织结构及继承遍历。
- [ ] ContainerPtrToValuePtr 的 UObject* / void* 重载及 Container 所指层级。
- [ ] 逐层访问 Actor.ProbeStats.Health，区分描述对象地址、容器地址和字段值地址。
- [ ] FBoolProperty 的位域/掩码，为什么不能把任意布尔属性都当 bool*。
- [ ] 静态数组 ArrayDim 与动态 TArray 的数量、内存布局和访问方式。
- [ ] FArrayProperty / FMapProperty / FSetProperty 与相应 Script Helper。
- [ ] 枚举、对象引用、类引用、接口和委托的属性描述。
- [ ] 原生布局、动态类型布局与 Link；属性 C++ 类型检查和实际对象类型检查。

验收：不为具体业务类写字段访问代码，也能递归枚举、打印和修改受支持的属性。

### 3. 未知类型的构造、复制与销毁〔P0〕

- [ ] UScriptStruct::InitializeStruct / CopyScriptStruct / DestroyStruct。
- [ ] ICppStructOps、TCppStructOps、TStructOpsTypeTraits 与原生操作选择。
- [ ] 原生构造/析构与逐属性初始化/销毁之间的关系。
- [ ] 清零、默认构造、复制构造、赋值、浅拷贝与 memcpy 的区别。
- [ ] 含 FString / TArray 的非平凡类型，大小、对齐、类型擦除和内存所有权。
- [ ] 自定义序列化、比较及网络序列化如何通过结构体操作接入。

验收：仅取得 UScriptStruct*，安全创建、复制并销毁一个包含 FString 的实例。

### 4. UObject 创建、身份与默认值〔P0〕

- [ ] NewObject 到分配、身份初始化、构造和属性初始化的调用顺序。
- [ ] Class、Name、Outer、对象标志、GUObjectArray 分别负责什么。
- [ ] Outer 与 Actor::Owner、组件附着和 GC 所有权语义的区别。
- [ ] FObjectInitializer、CreateDefaultSubobject 与运行时 NewObject。
- [ ] CDO、Archetype、对象模板和实例；原生默认值、蓝图默认值、关卡覆盖值。
- [ ] DuplicateObject 如何处理子对象和引用，为什么不能直接拷贝 UObject 内存。

验收：解释一个实例的值来自哪里，能排查修改构造默认值后旧实例值未变化的原因。

### 5. 反射函数调用〔P0〕

- [ ] FuncMap、NativeFunctionLookupTable、UFunction::Func 的职责。
- [ ] Bind、Invoke、ProcessEvent、exec thunk 与原生业务函数的关系。
- [ ] FFrame、原生参数缓冲区与脚本执行上下文。
- [ ] CPF_Parm / ReturnParm / OutParm / ReferenceParm，参数大小、偏移和对齐。
- [ ] 非平凡参数初始化/销毁、返回值与引用/输出参数读取。
- [ ] 普通调用、虚函数调用与反射调用的成本来源。

验收：按名称调用函数，正确处理 FString 参数和返回值。

### 6. 反射序列化与兼容〔P0〕

- [ ] 属性描述的序列化与实例字段值的序列化；FArchive 的多种用途。
- [ ] SerializeBin、带标签和无版本属性序列化，默认值比较与差量保存。
- [ ] Transient、SaveGame、Config、DuplicateTransient 和 Archive 上下文。
- [ ] 对象引用编码/恢复与对象内容保存的区别，避免持久化进程内地址。
- [ ] 字段新增、删除、重排、改名和类型变化；Core Redirects 与自定义版本。
- [ ] 自定义 Serialize 与父类及默认属性序列化的配合。

验收：保存数据后修改字段定义，验证加载结果并解释需要迁移的情况。

### 7. GC 与反射〔P0〕

- [ ] 根集合、引用图、循环引用、可达性分析与清扫。
- [ ] ReferenceSchema 的生成和执行，容器/嵌套结构体/自定义回调。
- [ ] AddReferencedObjects、FGCObject 与非自动引用报告。
- [ ] TObjectPtr / TWeakObjectPtr / TSoftObjectPtr / TStrongObjectPtr 的引用、加载和生命周期语义。
- [ ] 包装指针与报告引用的区别；强引用边要从可达对象出发才保持目标可达。
- [ ] 弱引用如何识别已销毁对象，以及索引复用问题。
- [ ] BeginDestroy、IsReadyForFinishDestroy、FinishDestroy 与实际释放。
- [ ] 并行 GC、增量可达性分析、写屏障和 Cluster 分别优化什么。

### 8. 蓝图与原生反射衔接〔P0〕

- [ ] UBlueprint 资产、Skeleton Class、UBlueprintGeneratedClass 与 CDO。
- [ ] 蓝图新增字段和函数怎样产生属性、布局及脚本信息。
- [ ] BlueprintCallable / Pure / ImplementableEvent / NativeEvent 的行为。
- [ ] 事件入口、_Implementation、蓝图覆盖和父类实现调用。
- [ ] 蓝图字节码、ProcessInternal、VM 执行与原生 thunk。
- [ ] 蓝图重编译对实例、类型描述和引用的影响。

验收：追踪一次 C++ 进入蓝图和一次蓝图进入 C++ 的调用。

### 9. 接口与委托〔P1〕

- [ ] UInterface / IInterface 与 FImplementedInterface 的类型、偏移和实现来源标志。
- [ ] ImplementsInterface 为真与能否取得原生接口指针的区别。
- [ ] Cast<IInterface>、Execute_XXX 与仅蓝图实现的接口。
- [ ] 原生委托与动态委托；反射函数名、绑定对象和序列化。
- [ ] 绑定的生命周期与 GC 语义；委托执行并不自动负责线程切换。

### 10. 属性标志、元数据与编辑器〔P1，工具岗位重点〕

- [ ] PropertyFlags / ClassFlags / FunctionFlags 与 meta=(...)。
- [ ] EditAnywhere / VisibleAnywhere / BlueprintReadWrite 的不同控制维度。
- [ ] WITH_EDITOR / WITH_EDITORONLY_DATA / WITH_METADATA 和 Cook 裁剪。
- [ ] Details 面板、IPropertyHandle 和属性定制。
- [ ] PreEditChange、PostEditChangeProperty、Modify、Undo/Redo 和包脏标记。
- [ ] 直接修改字段内存与编辑器属性修改流程的区别。

### 11. 反射与复制、RPC〔P1，网络岗位深入〕

- [ ] Replicated、属性注册和复制条件；反射提供什么，网络系统还负责什么。
- [ ] 属性复制描述、RepNotify、对象引用网络标识。
- [ ] RPC 函数标志、参数序列化、调用空间、权限和所属连接。
- [ ] NetSerialize / NetDeltaSerialize 与结构体操作。
- [ ] 传统复制路径与 Iris 的描述/协议构建思路。

### 12. 类型变更、模块与重实例化〔P1〕

- [ ] 模块加载/卸载、类型注册与对应原生代码的生命周期。
- [ ] Hot Reload、Live Coding、代码补丁和对象重实例化的区别。
- [ ] 字段、对象尺寸和虚表变化分别需要什么处理。
- [ ] 新旧 CDO、默认值、实例覆盖值和引用迁移。
- [ ] UClass / UFunction / FProperty / 属性路径缓存失效及刷新。

### 13. 性能、并发与故障排查〔P1〕

- [ ] 构建耗时、元数据内存、查找、参数转换和动态调用成本。
- [ ] 预处理链表、查找缓存、布局/遍历描述及缓存生命周期。
- [ ] FName 的用途；进程内名称索引不能直接当作跨版本持久化 ID。
- [ ] 只读查询、注册、对象修改、GC 与工作线程访问的边界。
- [ ] 泛型属性修改如何维护容器、对象引用及增量 GC 不变量。
- [ ] 排查查找失败、错误容器、参数越界、GC 悬空引用和重载缓存失效。

### 14. 自研系统设计〔P0，项目经历重点〕

- [ ] TypeInfo / PropertyInfo / FunctionInfo 的最小设计及静态/运行时数据划分。
- [ ] 类型 ID、跨模块查找、重复注册、卸载和循环依赖。
- [ ] 原生布局、继承、私有字段、位域；偏移、成员指针和访问器取舍。
- [ ] 非平凡对象生命周期、函数调用、类型擦除及错误处理。
- [ ] Python + libclang 的标记识别、编译参数、增量生成和 CMake 依赖。
- [ ] 生成失败诊断、旧产物清理、确定性输出和版本兼容。
- [ ] 代码生成能替代哪些工作，哪些运行时机制仍需实现。
- [ ] 通过功能实验与基准验证正确性、性能和设计边界。

### 15. 按岗位扩展〔P2〕

- [ ] 渲染：FShaderParametersMetadata、Shader 参数布局、RDG 参数遍历与 UObject 反射的异同。
- [ ] 编辑器：通用 Inspector、属性路径、事务、批量资产修改。
- [ ] 资产：包依赖、硬软引用、异步加载、Cook 与加载兼容。
- [ ] 脚本：类型桥接、函数绑定、参数转换、生命周期和异常边界。

P0 验收统一要求：能解释输入输出与设计目的，能找到本地源码入口，能运行最小实验，能说明 Demo 将保留哪些机制及如何简化。

## 十一、第一节运行时实践：FProperty 与属性访问

本节以当前 ContentExample 的 ATestActor::ProbeStats.Health / Speed 为例。先理解标量与内嵌结构体，数组、bool 位域和对象引用后续分别展开。以下示例供放入工程验证；文档更新不表示已执行 UE 编译或 PIE 测试。

### 1. 描述对象与字段值是两块不同的内存

```text
ATestActor 的 UClass
  └─ FStructProperty("ProbeStats")
       ├─ 偏移：相对 Actor 实例起始地址
       └─ Struct → 描述 FReflectionStats 的 UScriptStruct
                      ├─ FIntProperty("Health")，偏移相对 Stats 实例
                      └─ FFloatProperty("Speed")，偏移相对 Stats 实例

某个 ATestActor 实例
  └─ ProbeStats 的实际内存
       ├─ Health 的实际值
       └─ Speed 的实际值
```

FProperty 不保存每个 Actor 的 Health 值；同一字段描述供该类型的多个实例使用。访问必须同时提供字段描述和具体实例地址。

### 2. 本地源码阅读入口

引擎根目录：`F:\workspace\UnrealEngine58\Engine\Source\Runtime\CoreUObject`。

| 文件/符号 | 本节观察点 |
| --- | --- |
| Public/UObject/UnrealType.h：FProperty | ArrayDim、PropertyFlags、Offset_Internal 和属性链表 |
| Public/UObject/UnrealType.h：FindFProperty | 用 TFieldIterator 遍历，以 FName 比较字段名；模板指定属性类型 |
| Public/UObject/UnrealType.h：ContainerPtrToValuePtr | UObject* 与 void* 重载，类型/边界断言与地址计算 |
| Public/UObject/UnrealType.h：TProperty 的 Get/SetPropertyValue_InContainer | 容器转值地址后读取/赋值 |
| Private/UObject/Property.cpp：FProperty 的生成参数构造函数 | 从 Prop.Offset 接收生成代码提供的原生偏移 |
| TestActor.gen.cpp：NewProp_ProbeStats / Health / Speed | STRUCT_OFFSET 的所属类型分别是 ATestActor 与 FReflectionStats |

本地 UE5.8 的 ElementSize 成员已标注弃用；访问时使用 GetElementSize()，静态数组维度读取 ArrayDim。

### 3. 地址计算与类型检查

对本节的标量/内嵌结构体，核心关系是：

```text
字段值地址 = 当前容器地址 + 字段相对偏移 + 静态数组索引 × 元素尺寸

StatsData   = ActorAddress + Offset(ATestActor::ProbeStats)
HealthData  = StatsData    + Offset(FReflectionStats::Health)
```

ContainerPtrToValuePtr 的模板参数只决定返回指针类型，不会自动确认字段就是该 C++ 类型。应先 FindFProperty<FIntProperty> 或 CastField<FIntProperty>，再取 int32 地址。

顶层 Actor 属性传入 UObject 派生对象指针，使用 UObject* 重载；结构体内字段传入该结构体的内存，使用 void* 重载。不要把 Health 属性的容器传成 Actor，也不要为了统一接口随意把顶层对象先转成 void*，这样会改变检查路径。

ArrayIndex 对应属性声明的静态数组维度，不是动态 TArray 的元素下标；动态数组需使用 FScriptArrayHelper 等访问元素。

### 4. 可操作示例

在已有 TestActor.cpp 的 include 区域补充 `#include "UObject/UnrealType.h"`，然后在 BeginPlay 定义之前放入以下辅助函数。在 BeginPlay 的 Super::BeginPlay() 后调用 `RunPropertyAccessLesson(this);`，原有示例可以保留。

```cpp
static void RunPropertyAccessLesson(ATestActor* Actor)
{
    if (!IsValid(Actor))
        return;

    // 先观察原生类直接声明的属性；None 表示不包含父类字段。
    for (TFieldIterator<FProperty> It(
             ATestActor::StaticClass(), EFieldIterationFlags::None); It; ++It)
    {
        UE_LOG(LogTemp, Display,
            TEXT("[PropertyLesson] %s : %s, Offset=%d, ElementSize=%d, ArrayDim=%d"),
            *It->GetName(), *It->GetCPPType(), It->GetOffset_ForInternal(),
            It->GetElementSize(), It->ArrayDim);
    }

    // 默认查找包含父类，所以 Actor 是蓝图派生实例也能找到原生 ProbeStats。
    FStructProperty* StatsProp = FindFProperty<FStructProperty>(
        Actor->GetClass(), GET_MEMBER_NAME_CHECKED(ATestActor, ProbeStats));
    if (!StatsProp)
        return;

    // 第一层：从 Actor 起点定位内嵌结构体。
    void* StatsData = StatsProp->ContainerPtrToValuePtr<void>(Actor);
    check(StatsData == static_cast<void*>(&Actor->ProbeStats));

    // 查找是在“类型描述”上进行，地址计算是在“具体实例内存”上进行。
    FIntProperty* HealthProp = FindFProperty<FIntProperty>(
        StatsProp->Struct, GET_MEMBER_NAME_CHECKED(FReflectionStats, Health));
    FFloatProperty* SpeedProp = FindFProperty<FFloatProperty>(
        StatsProp->Struct, GET_MEMBER_NAME_CHECKED(FReflectionStats, Speed));
    if (!HealthProp || !SpeedProp)
        return;

    // 第二层：容器已变为 StatsData，而不是 Actor。
    int32* HealthData = HealthProp->ContainerPtrToValuePtr<int32>(StatsData);
    check(HealthData == &Actor->ProbeStats.Health);

    const int32 Before = HealthProp->GetPropertyValue_InContainer(StatsData);
    const float Speed = SpeedProp->GetPropertyValue_InContainer(StatsData);
    HealthProp->SetPropertyValue_InContainer(StatsData, 321);

    UE_LOG(LogTemp, Display,
        TEXT("[PropertyLesson] Health=%d -> %d, Speed=%.2f, DirectHealth=%d"),
        Before, *HealthData, Speed, Actor->ProbeStats.Health);
    check(Actor->ProbeStats.Health == 321);

    // 本实验恢复值，便于继续观察已有 AddHealth / CDO 示例。
    HealthProp->SetPropertyValue_InContainer(StatsData, Before);
}
```

GetOffset_ForInternal 仅用于本实验观察；业务访问优先走属性 API。直接赋值标量不会自动触发编辑器事务、属性通知或业务 setter，也不能推导成任意属性均可通过裸指针写入。

### 5. 实验和面试追问

- [ ] 用原生 TestActor 和蓝图派生实例分别运行，观察当前值及包含继承的字段查找。
- [ ] 观察 ProbeStats 与 Health 的偏移分别相对于谁；验证两处地址相等断言。
- [ ] 将 Health 查找模板改为 FFloatProperty，确认类型不匹配时返回空，而不是得到一个合法的 float 字段。
- [ ] 两个 Actor 设置不同 Health，解释为什么属性描述可以共享，而读取结果不同。
- [ ] 解释 FindFProperty 不是“按名称查 C++ 内存”，它先查描述，再依据描述访问实例。
- [ ] 思考通用递归打印器需要携带哪两个状态：当前 UStruct* 与当前容器地址；进入 FStructProperty 时两者都要更新。顶层 UObject* 检查路径也应保留。

本节暂不把复选框标为完成：需要实际编译/PIE 验证后记录结果。下一节扩展字段遍历、继承和递归打印，再分别处理 bool、动态数组和对象引用。

> 提示：本文为面试准备清单，不是源码逐行结论。具体符号、注册路径、GC 实现细节应以目标 UE 版本源码为准。
