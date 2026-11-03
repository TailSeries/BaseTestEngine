#include "TestShaders.h"
#include <filesystem>
#include <fstream>
#include <sstream>

IMPLEMENT_GLOBAL_SHADER(FTestVS, "/Demo/TestShaders.usf", "VSMain", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FTestPS, "/Demo/TestShaders.usf", "PSMain", SF_Pixel);

void RegisterTestShaderTypes()
{
    // 显式初始化，避免静态库链接裁剪导致注册对象未被引用；调用多次不重复注册。
    (void)FTestVS::GetStaticType();
    (void)FTestPS::GetStaticType();
}

std::string LoadTestShaderSource(const std::string& VirtualPath)
{
    // 最小虚拟路径映射；暂不支持任意目录、include 或热重载。
    if (VirtualPath != "/Demo/TestShaders.usf") throw std::invalid_argument("Unknown shader source: " + VirtualPath);
    // CMake 提供绝对路径，因此验证脚本切换工作目录不会影响源码加载。
    const std::filesystem::path Path = std::filesystem::path(RHITEST_SHADER_DIR) / "TestShaders.usf";
    std::ifstream File(Path, std::ios::binary);
    if (!File) throw std::runtime_error("Cannot open shader source: " + Path.string());
    std::ostringstream Text;
    Text << File.rdbuf();
    if (File.bad()) throw std::runtime_error("Cannot read shader source: " + Path.string());
    return Text.str();
}
