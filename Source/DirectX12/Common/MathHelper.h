#pragma once
#include <Windows.h>
#include <DirectXMath.h>
#include "ModulePublic/DXModule.h"

#include <cstdint>

class DXMODULE MathHelper
{
public:

	// 获取[a, b)之间的浮点数
	static float RandF(float a, float b)
	{
		return  a + RandF() * (b - a);
	}

	//获取[0, 1)之间的浮点数
	static float RandF()
	{
		return (float)(rand()) / (float)RAND_MAX;
	}

	// 获取[a, b] 之间的随机整数
	static int Rand(int a, int b)
	{
		return a + rand() % (b - a + 1);
	}

	template<typename T>
	static T Min(const T& a, const T& b)
	{
		return  a < b ? a : b;
	}
	template<typename T>
	static T Max(const T& a, const T& b)
	{
		return a > b ? a : b;
	}

	template<typename T>
	static T Lerp(const T& a, const  T& b, float t)
	{
		return  a + (b - a) * t;
	}
	template<typename T>
	static T Clamp(const T& x, const T& low, const T& high)
	{
		return x < low ? low : (x > high ? high : x);
	}


	//将球坐标系（Spherical Coordinates）下的点 (radius, theta, phi) 转换为笛卡尔坐标系（Cartesian Coordinates）下的 XMVECTOR。
	/*y轴朝上的坐标系，
	 *theta，与x轴夹角
	 *phi，与y轴夹角
	 */
	static DirectX::XMVECTOR SphericalToCartesian(float redius, float theta, float phi)
	{
		return DirectX::XMVectorSet(
			redius * sinf(phi) * cosf(theta), // x
			redius * cosf(phi), // y
			redius * sinf(phi) * sinf(theta),//z
			1.0f//w
		);
	}

	//求去掉平移影响之后的矩阵的逆转置矩阵
	/*
	 * 通常用在求法线逆转置的过程中，法线只要方向和位移没关系
	 */
	static DirectX::XMMATRIX InverseTranspose(const DirectX::XMMATRIX& M)
	{
		DirectX::XMMATRIX A = M;
		A.r[3] = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);//去掉最后一行位移的影响
		DirectX::XMVECTOR det = DirectX::XMMatrixDeterminant(A);// det的值实际上只存在于x分量上，为了照顾SIMD用了vector
		return DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(&det, A));
	}

	//获取一个单位4x4矩阵
	/*为什么返回值不是 XMMATRIX ？
	 *XMFLOAT4X4 是一个普通的 C++ 结构体，用于存储数据；而 XMMATRIX 是一个高性能的 SIMD 类型，仅用于计算，不推荐这个东西的使用过程中出现拷贝。
	  从 XMMATRIX 转换为 XMFLOAT4X4
		XMMATRIX mat = XMMatrixIdentity();
		XMFLOAT4X4 matOut;
		XMStoreFloat4x4(&matOut, mat);
	  从 XMFLOAT4X4 转换为 XMMATRIX
		XMMATRIX mat2 = XMLoadFloat4x4(&matOut);
	 */
	static DirectX::XMFLOAT4X4 Identity4x4()
	{
		static DirectX::XMFLOAT4X4 I(
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		);
		return I;
	}


	//、、快速开根，但是小于1的时候误差很大
	static float FastCbrtMagic(float x) {
		float xthird = 0.333f * x;
		union { float f; int i; } u = { x };
		u.i = u.i / 3 + 709983559; // magic number 709921077
		u.f = 0.667f * u.f + xthird / (u.f * u.f);
		return u.f;
	}



	// 将一个二维向量转成乘它与x轴的夹角 [0, 2PI)
	static float AngleFromXY(const float x, const float y);

	// 生成一个在单位球面上随机分布的单位向量
	static DirectX::XMVECTOR RandUnitVec3();
	//生成一个在单位球面上、并且位于指定法线 n 所指方向的半球中的随机单位向量。
	/*
	 * 常用于 GI，Lambert， 路径追踪，局部方向采样
	 */
	static DirectX::XMVECTOR RandHemisphereUnitVec3(DirectX::XMVECTOR n);

	static const float Infinity;
	static const float Pi;
};