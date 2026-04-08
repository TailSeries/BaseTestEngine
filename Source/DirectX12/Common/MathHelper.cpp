#include "MathHelper.h"
#include <cfloat>
#include <cmath>
const float MathHelper::Infinity = FLT_MAX;
const float MathHelper::Pi = 3.1415926535f;

float MathHelper::AngleFromXY(float x, float y)
{
	float theta = 0.0f;
	if (x >= 0.0f)
	{
		theta = atan2f(y , x);// [-pi / 2, +pi / 2]
		if (theta < 0.0f)
		{
			theta += 2.0f * Pi;
		}
	}
	else
	{
		theta = atan2f(y, x) + Pi;
	}
	return theta;
}

DirectX::XMVECTOR MathHelper::RandUnitVec3()
{
	DirectX::XMVECTOR One = DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f);
	DirectX::XMVECTOR Zero = DirectX::XMVectorZero();
	// 我们不用循环，而是直接球面公式获取随机值
	float y = RandF(-1.0f, 1.0f); // cos(phi)
	float theta = RandF(0.0f, 2 * Pi);

	float l = sqrtf(1.0f - y * y);// xz 平面上的投影长度
	float x = l * cosf(theta);
	float z = l * sinf(theta);

	return DirectX::XMVectorSet(x, y, z, 1.0f);

}

DirectX::XMVECTOR MathHelper::RandHemisphereUnitVec3(DirectX::XMVECTOR n)
{
	//我们不使用循环，而是使用球面坐标系直接采样法线方向的半球，无需循环与判断

	/*
	 * 先在上半球上随机采样一个方向向量，然后将这个半球旋转到法线所指的半球上。
	 */
	float y = RandF(0.0f, 1.0f); // cos(phi)
	float theta = RandF(0.0f, 2 * Pi);

	float l = sqrtf(1.0f - y * y);// xz 平面上的投影长度
	float x = l * cosf(theta);
	float z = l * sinf(theta);

	DirectX::XMVECTOR LocalDir = DirectX::XMVectorSet(x, y, z, 1.0f); // (+y 朝上)

	//构建局部正交基 （a, n, t） 对应原来的(x, y, z)
	DirectX::XMVECTOR forward = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f); 
	if (fabsf(DirectX::XMVectorGetX(DirectX::XMVector3Dot(forward, n))) > 0.999f)      // 防止 up 与 n 平行，退化
	{
		forward = DirectX::XMVectorSet(0.0, -1.0f, 0.0f, 0.0f);
	}

	/*
	 * 左手系下 XxY》》Z  YxZ 》》 X    Z x X 》》 Y
	 * 那么	   a n 》》t n t 》》 a    t   a 》》n
	 */
	DirectX::XMVECTOR a = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(n, forward));
	DirectX::XMVECTOR t = DirectX::XMVector3Cross(a, n);
	//将localdir旋转，旋转矩阵构建相乘之后的结果实际上就是 xa  + yn + zt
	DirectX::XMVECTOR WorldDir = DirectX::XMVectorAdd(
		DirectX::XMVectorAdd(DirectX::XMVectorScale(a, x), DirectX::XMVectorScale(n, y))
		, DirectX::XMVectorScale(t, z));
	return DirectX::XMVector3Normalize(WorldDir);
}

