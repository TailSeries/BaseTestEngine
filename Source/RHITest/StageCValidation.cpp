#include "D3DShaderCompiler.h"
#include "TestShaders.h"
#include <fstream>

// C1 编译元数据不依赖 GPU；C2 资源容器测试需要 RHI 已初始化。
// 用异常而非 assert，确保 Release 也执行检查。
void ValidateStageCCompiler()
{
    const auto Require = [](bool Condition, const char* Message)
    {
        if (!Condition) throw std::runtime_error(Message);
    };
    const auto VS = CompileD3DShader({
        "cbuffer CB : register(b1) { float4x4 WVP; }; "
        "float4 Main(float4 P : POSITION) : SV_POSITION { return mul(P, WVP); }",
        "Main", SF_Vertex});
    Require(VS.bSucceeded, "C1: b1 compilation failed");
    const auto CB = VS.ParameterMap.FindParameterAllocation("CB");
    Require(CB && CB->Type == EShaderParameterType::UniformBuffer &&
        CB->BufferIndex == 1 && CB->BaseIndex == 0 && CB->Size == 64,
        "C1: cbuffer binding or size incorrect");
    Require(VS.ResourceCounts.NumCBs == 2 && VS.ResourceCounts.NumSRVs == 0,
        "C1: sparse b1 must produce register extent 2");
    Require(!VS.ParameterMap.FindParameterAllocation("Missing"), "C1: missing parameter found");

    const auto PS = CompileD3DShader({
        "Texture2D Images[2] : register(t3); SamplerState S : register(s2); "
        "float4 Main(float2 UV : TEXCOORD) : SV_Target { "
        "return Images[0].Sample(S, UV) + Images[1].Sample(S, UV); }",
        "Main", SF_Pixel});
    Require(PS.bSucceeded, "C1: texture array compilation failed");
    const auto Images = PS.ParameterMap.FindParameterAllocation("Images[0]");
    const auto Image1 = PS.ParameterMap.FindParameterAllocation("Images[1]");
    const auto Sampler = PS.ParameterMap.FindParameterAllocation("S");
    Require(Images && Images->Type == EShaderParameterType::SRV &&
        Images->BaseIndex == 3 && Images->Size == 1 && Image1 &&
        Image1->BaseIndex == 4 && Image1->Size == 1, "C1: texture array allocation incorrect");
    Require(Sampler && Sampler->Type == EShaderParameterType::Sampler && Sampler->BaseIndex == 2,
        "C1: sampler allocation incorrect");
    Require(PS.ResourceCounts.NumSRVs == 5 && PS.ResourceCounts.NumSamplers == 3,
        "C1: resource extents incorrect");
    const auto Desc = PS.MakeRHICreateDesc();
    Require(Desc.Code.size() == PS.ShaderCode.size() && Desc.ResourceCounts.NumSRVs == 5,
        "C1: output to RHI bridge lost metadata");

    const auto Invalid = CompileD3DShader({"invalid hlsl", "Main", SF_Pixel});
    Require(!Invalid.bSucceeded && !Invalid.Diagnostics.empty(), "C1: compiler diagnostics missing");
    bool Rejected = false;
    try { (void)Invalid.MakeRHICreateDesc(); }
    catch (const std::logic_error&) { Rejected = true; }
    Require(Rejected, "C1: failed output accepted by RHI bridge");
    const auto Unsupported = CompileD3DShader({
        "StructuredBuffer<float4> B : register(t0); "
        "float4 Main() : SV_Target { return B[0]; }", "Main", SF_Pixel});
    Require(!Unsupported.bSucceeded && !Unsupported.Diagnostics.empty(),
        "C1: unsupported resource silently accepted");
    // C2：Bind 解析位置与真正提交分离。非零寄存器不能被误认为根参数槽位。
    FShaderUniformBufferParameter Uniform;
    Uniform.Bind(VS.ParameterMap, "CB", SPF_Mandatory);
    Require(Uniform.GetBaseIndex() == 1 && Uniform.GetNumBytes() == 64, "C2: UB binding lost reflection data");
    FShaderResourceParameter Texture;
    Texture.Bind(PS.ParameterMap, "Images[1]", SPF_Mandatory);
    Require(Texture.GetBaseIndex() == 4 && Texture.GetNumResources() == 1, "C2: SRV binding index incorrect");
    const auto ExpectFailure = [&Require](auto&& Operation)
    {
        bool Failed = false;
        try { Operation(); } catch (const std::exception&) { Failed = true; }
        Require(Failed, "C2: invalid parameter operation was accepted");
    };
    ExpectFailure([&] { Uniform.Bind(PS.ParameterMap, "S", SPF_Mandatory); });
    Require(!Uniform.IsBound(), "C2: failed rebind retained old slot");
    ExpectFailure([&] { Texture.Bind(VS.ParameterMap, "CB", SPF_Mandatory); });
    ExpectFailure([&] { Uniform.Bind(VS.ParameterMap, "Missing", SPF_Mandatory); });
    Uniform.Bind(VS.ParameterMap, "Missing", SPF_Optional);
    Require(!Uniform.IsBound(), "C2: optional missing parameter was bound");

    // 不调用底层 Context：这些情况必须在 RenderCore 参数检查阶段就返回或抛错。
    FRHICommandList NoContext(nullptr);
    SetShaderConstantBufferData(NoContext, SF_Vertex, Uniform, nullptr, 0);
    Uniform.Bind(VS.ParameterMap, "CB", SPF_Mandatory);
    float Values[16]{};
    ExpectFailure([&] { SetShaderConstantBufferData(NoContext, SF_Vertex, Uniform, Values, 4); });
    ExpectFailure([&] { SetShaderConstantBufferData(NoContext, SF_Pixel, Uniform, Values, sizeof(Values)); });
    FShaderResourceParameter SamplerParameter;
    SamplerParameter.Bind(PS.ParameterMap, "S", SPF_Mandatory);
    ExpectFailure([&] { SetSRVParameter(NoContext, SF_Pixel, SamplerParameter, nullptr); });

    // RHI 已初始化。资源容器独立拥有 RHI Shader；类型对象只保存参数信息。
    const TShaderRef<FTestVS> Shader = CreateShader<FTestVS>(VS);
    Require(Shader.GetVertexShader()->GetFrequency() == SF_Vertex, "C2: wrong RHI stage");
    ExpectFailure([&] { (void)Shader.GetPixelShader(); });
    ExpectFailure([&] { (void)CreateShader<FTestVS>(PS); });
    ExpectFailure([&] { (void)CreateShader<FTestVS>(Invalid); });
    // C3：类型注册、Map 复用，以及整批编译失败不能留下半份 Map。
    RegisterTestShaderTypes();
    const size_t TypeCount = FGlobalShaderType::GetTypes().size();
    RegisterTestShaderTypes();
    Require(TypeCount == 2 && FGlobalShaderType::GetTypes().size() == TypeCount,
        "C3: type registration is not idempotent");
    static const FGlobalShaderType Duplicate("FTestVS", "/Duplicate.usf", "Main", SF_Vertex,
        [](const FShaderCompilerOutput& Output) -> TRefCountPtr<FShader> { return std::make_shared<FTestVS>(Output); });
    ExpectFailure([&] { FGlobalShaderType::Register(Duplicate); });
    FGlobalShaderMap Map;
    ExpectFailure([&] { (void)Map.GetShader<FTestVS>(); });
    uint32 CompileCount = 0;
    const auto CountedCompiler = [&](const FShaderCompilerInput& Input)
    {
        ++CompileCount;
        return CompileD3DShader(Input);
    };
    Map.CompileMissing(CountedCompiler, LoadTestShaderSource);
    Require(CompileCount == 2 && Map.GetNumShaders() == 2, "C3: missing shader map entries");
    const TRefCountPtr<FRHIVertexShader> OriginalVS = Map.GetShader<FTestVS>().GetVertexShader();
    Map.CompileMissing(CountedCompiler, LoadTestShaderSource);
    Require(CompileCount == 2 && Map.GetShader<FTestVS>().GetVertexShader() == OriginalVS,
        "C3: repeated map lookup recompiled or replaced shader");

    FGlobalShaderMap FailedMap;
    uint32 Attempt = 0;
    ExpectFailure([&]
    {
        FailedMap.CompileMissing([&](const FShaderCompilerInput& Input)
        {
            if (++Attempt == 2) return FShaderCompilerOutput{};
            return CompileD3DShader(Input);
        }, LoadTestShaderSource);
    });
    Require(Attempt == 2 && FailedMap.GetNumShaders() == 0, "C3: failed batch polluted shader map");
    ExpectFailure([&]
    {
        FailedMap.CompileMissing(CompileD3DShader, [](const std::string&) -> std::string
        { throw std::runtime_error("Missing source test"); });
    });
    ExpectFailure([&]
    {
        FailedMap.CompileMissing([](const FShaderCompilerInput& Input)
        {
            FShaderCompilerOutput Result = CompileD3DShader(Input);
            Result.Frequency = Input.Frequency == SF_Vertex ? SF_Pixel : SF_Vertex;
            return Result;
        }, LoadTestShaderSource);
    });
    Require(FailedMap.GetNumShaders() == 0, "C3: rejected source/stage mutated map");
    FailedMap.CompileMissing(CompileD3DShader, LoadTestShaderSource);
    Require(FailedMap.GetNumShaders() == 2, "C3: retry after failure did not recover");
    TShaderRef<FTestPS> Retained;
    {
        FGlobalShaderMap Temporary;
        Temporary.CompileMissing(CompileD3DShader, LoadTestShaderSource);
        Retained = Temporary.GetShader<FTestPS>();
    }
    Require(Retained.GetPixelShader() != nullptr, "C3: shader ref did not retain resource");
    std::ofstream MapReport("stage-c-map-checks.txt");
    MapReport << "PASS: idempotent/duplicate registration, missing lookup, typed map reuse, "
        "atomic failed compilation, source/stage failures, retry, retained shader reference.\n";
    if (!MapReport) throw std::runtime_error("Cannot write Stage C shader map report");

    // C4：生命周期入口、初始化失败回滚、重入拒绝和可重复初始化。
    class FProbeResource : public FRenderResource
    {
    public:
        int InitCalls = 0, ReleaseCalls = 0;
        bool bFail = false, bReenter = false;
        std::unique_ptr<int> Partial;
        ~FProbeResource() override { ReleaseResource(); }
    protected:
        void InitRHI(FRHICommandListBase& CmdList) override
        {
            ++InitCalls;
            Partial = std::make_unique<int>(1);
            if (bReenter) InitResource(CmdList);
            if (bFail) throw std::runtime_error("Injected InitRHI failure");
        }
        void ReleaseRHI() noexcept override { ++ReleaseCalls; Partial.reset(); }
    };
    FProbeResource Probe;
    BeginReleaseResource(&Probe);
    Require(Probe.ReleaseCalls == 0, "C4: uninitialized release called hook");
    BeginInitResource(&Probe);
    BeginInitResource(&Probe);
    Require(Probe.IsInitialized() && Probe.InitCalls == 1, "C4: repeated initialization recreated resource");
    BeginReleaseResource(&Probe);
    BeginReleaseResource(&Probe);
    Require(!Probe.IsInitialized() && Probe.ReleaseCalls == 1 && !Probe.Partial,
        "C4: repeated release or incorrect state");
    Probe.bFail = true;
    ExpectFailure([&] { BeginInitResource(&Probe); });
    Require(!Probe.IsInitialized() && !Probe.Partial && Probe.ReleaseCalls == 2,
        "C4: failed InitRHI did not roll back");
    Probe.bFail = false;
    Probe.bReenter = true;
    ExpectFailure([&] { BeginInitResource(&Probe); });
    Require(!Probe.IsInitialized() && !Probe.Partial, "C4: reentrant init did not roll back");
    Probe.bReenter = false;
    BeginInitResource(&Probe);
    Require(Probe.IsInitialized(), "C4: retry after rollback failed");
    BeginReleaseResource(&Probe);
    ExpectFailure([] { BeginInitResource(nullptr); });
    ExpectFailure([] { BeginReleaseResource(nullptr); });

    auto OwnedResource = [&]
    {
        FShaderCompilerOutput TemporaryOutput = VS;
        return std::make_unique<FShaderMapResource>(TemporaryOutput);
    }(); // 编译输出已销毁，资源容器必须拥有独立的创建数据。
    Require(!OwnedResource->IsInitialized(), "C4: constructor unexpectedly initialized RHI");
    ExpectFailure([&] { (void)OwnedResource->GetShader(); });
    BeginInitResource(OwnedResource.get());
    const TRefCountPtr<FRHIShader> FirstRHI = OwnedResource->GetShader();
    BeginInitResource(OwnedResource.get());
    Require(OwnedResource->GetShader() == FirstRHI, "C4: duplicate init replaced shader");
    BeginReleaseResource(OwnedResource.get());
    ExpectFailure([&] { (void)OwnedResource->GetShader(); });
    Require(FirstRHI->GetFrequency() == SF_Vertex, "C4: release invalidated external owner");
    BeginInitResource(OwnedResource.get());
    Require(OwnedResource->GetShader() != FirstRHI, "C4: reinitialization did not recreate shader");
    const TRefCountPtr<FRHIShader> SecondRHI = OwnedResource->GetShader();
    OwnedResource.reset(); // 派生析构先调用 ReleaseResource，外部引用仍有效。
    Require(SecondRHI.use_count() == 1, "C4: destructor did not release shader reference");
    std::ofstream ResourceReport("stage-c-resource-checks.txt");
    ResourceReport << "PASS: explicit initialization, duplicate init/release, rollback/retry/reentrancy, "
        "owned compiler data, guarded access, recreated RHI shader and retained external reference.\n";
    if (!ResourceReport) throw std::runtime_error("Cannot write Stage C resource report");

    std::ofstream ParameterReport("stage-c-parameter-checks.txt");
    ParameterReport << "PASS: named parameter binding, nonzero indices, mandatory/optional lookup, "
        "type/size/stage checks, failed rebind reset, shader resource creation and typed access.\n";
    if (!ParameterReport) throw std::runtime_error("Cannot write Stage C parameter report");

    std::ofstream Report("stage-c-compiler-checks.txt");
    Report << "PASS: b1 binding/64-byte cbuffer, register extents, t3 array, s2, "
        "parameter lookup, RHI bridge, compilation diagnostics, unsupported resource rejection.\n";
    if (!Report) throw std::runtime_error("Cannot write Stage C compiler report");
}
