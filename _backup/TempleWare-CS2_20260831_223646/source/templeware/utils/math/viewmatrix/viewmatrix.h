#pragma once

#include "../../../../cs2/datatypes/viewmatrix/viewmatrix.h"
#include "../vector/vector.h"

class ViewMatrix {
public:
	//Vector_t WorldToScreen(const Vector_t& position) const;
	bool WorldToScreen(const Vector_t& position, Vector_t& out) const;
	
	viewmatrix_t* viewMatrix;

};

struct Matrix3x4_t
{
	Matrix3x4_t() = default;

	constexpr Matrix3x4_t(
		const float m00, const float m01, const float m02, const float m03,
		const float m10, const float m11, const float m12, const float m13,
		const float m20, const float m21, const float m22, const float m23)
	{
		arrData[0][0] = m00;
		arrData[0][1] = m01;
		arrData[0][2] = m02;
		arrData[0][3] = m03;
		arrData[1][0] = m10;
		arrData[1][1] = m11;
		arrData[1][2] = m12;
		arrData[1][3] = m13;
		arrData[2][0] = m20;
		arrData[2][1] = m21;
		arrData[2][2] = m22;
		arrData[2][3] = m23;
	}

	constexpr Matrix3x4_t(const Vector_t& vecForward, const Vector_t& vecLeft, const Vector_t& vecUp, const Vector_t& vecOrigin)
	{
		SetForward(vecForward);
		SetLeft(vecLeft);
		SetUp(vecUp);
		SetOrigin(vecOrigin);
	}

	[[nodiscard]] float* operator[](const int nIndex)
	{
		return arrData[nIndex];
	}

	[[nodiscard]] const float* operator[](const int nIndex) const
	{
		return arrData[nIndex];
	}

	constexpr void SetForward(const Vector_t& vecForward)
	{
		arrData[0][0] = vecForward.x;
		arrData[1][0] = vecForward.y;
		arrData[2][0] = vecForward.z;
	}

	constexpr void SetLeft(const Vector_t& vecLeft)
	{
		arrData[0][1] = vecLeft.x;
		arrData[1][1] = vecLeft.y;
		arrData[2][1] = vecLeft.z;
	}

	constexpr void SetUp(const Vector_t& vecUp)
	{
		arrData[0][2] = vecUp.x;
		arrData[1][2] = vecUp.y;
		arrData[2][2] = vecUp.z;
	}

	constexpr void SetOrigin(const Vector_t& vecOrigin)
	{
		arrData[0][3] = vecOrigin.x;
		arrData[1][3] = vecOrigin.y;
		arrData[2][3] = vecOrigin.z;
	}

	[[nodiscard]] constexpr Vector_t GetForward() const
	{
		return { arrData[0][0], arrData[1][0], arrData[2][0] };
	}

	[[nodiscard]] constexpr Vector_t GetLeft() const
	{
		return { arrData[0][1], arrData[1][1], arrData[2][1] };
	}

	[[nodiscard]] constexpr Vector_t GetUp() const
	{
		return { arrData[0][2], arrData[1][2], arrData[2][2] };
	}

	[[nodiscard]] constexpr Vector_t GetOrigin() const
	{
		return { arrData[0][3], arrData[1][3], arrData[2][3] };
	}

	constexpr void Invalidate()
	{
		for (auto& arrSubData : arrData)
		{
			for (auto& flData : arrSubData)
				flData = std::numeric_limits<float>::infinity();
		}
	}

	/// concatenate transformations of two matrices into one
	/// @returns: matrix with concatenated transformations
	[[nodiscard]] constexpr Matrix3x4_t ConcatTransforms(const Matrix3x4_t& matOther) const
	{
		return {
			arrData[0][0] * matOther.arrData[0][0] + arrData[0][1] * matOther.arrData[1][0] + arrData[0][2] * matOther.arrData[2][0],
			arrData[0][0] * matOther.arrData[0][1] + arrData[0][1] * matOther.arrData[1][1] + arrData[0][2] * matOther.arrData[2][1],
			arrData[0][0] * matOther.arrData[0][2] + arrData[0][1] * matOther.arrData[1][2] + arrData[0][2] * matOther.arrData[2][2],
			arrData[0][0] * matOther.arrData[0][3] + arrData[0][1] * matOther.arrData[1][3] + arrData[0][2] * matOther.arrData[2][3] + arrData[0][3],

			arrData[1][0] * matOther.arrData[0][0] + arrData[1][1] * matOther.arrData[1][0] + arrData[1][2] * matOther.arrData[2][0],
			arrData[1][0] * matOther.arrData[0][1] + arrData[1][1] * matOther.arrData[1][1] + arrData[1][2] * matOther.arrData[2][1],
			arrData[1][0] * matOther.arrData[0][2] + arrData[1][1] * matOther.arrData[1][2] + arrData[1][2] * matOther.arrData[2][2],
			arrData[1][0] * matOther.arrData[0][3] + arrData[1][1] * matOther.arrData[1][3] + arrData[1][2] * matOther.arrData[2][3] + arrData[1][3],

			arrData[2][0] * matOther.arrData[0][0] + arrData[2][1] * matOther.arrData[1][0] + arrData[2][2] * matOther.arrData[2][0],
			arrData[2][0] * matOther.arrData[0][1] + arrData[2][1] * matOther.arrData[1][1] + arrData[2][2] * matOther.arrData[2][1],
			arrData[2][0] * matOther.arrData[0][2] + arrData[2][1] * matOther.arrData[1][2] + arrData[2][2] * matOther.arrData[2][2],
			arrData[2][0] * matOther.arrData[0][3] + arrData[2][1] * matOther.arrData[1][3] + arrData[2][2] * matOther.arrData[2][3] + arrData[2][3]
		};
	}

	/// @returns: angles converted from this matrix
	[[nodiscard]] QAngle_t ToAngles() const;

	float arrData[3][4] = {};
};

struct Matrix2x4_t
{
public:
	Matrix3x4_t TranslateToMatrix3x4()
	{
		Matrix3x4_t matrix = Matrix3x4_t();
		Vector4D_t vecRotation = Vector4D_t();
		Vector_t vecPosition = Vector_t();

		vecRotation.x = this->_21; //rot.x
		vecRotation.y = this->_22; //rot.y
		vecRotation.z = this->_23; //rot.z
		vecRotation.w = this->_24; //rot.w

		vecPosition.x = this->_11; //bonepos.x
		vecPosition.y = this->_12; //bonepos.y
		vecPosition.z = this->_13; //bonepos.z

		matrix[0][0] = 1.0f - 2.0f * vecRotation.y * vecRotation.y - 2.0f * vecRotation.z * vecRotation.z;
		matrix[1][0] = 2.0f * vecRotation.x * vecRotation.y + 2.0f * vecRotation.w * vecRotation.z;
		matrix[2][0] = 2.0f * vecRotation.x * vecRotation.z - 2.0f * vecRotation.w * vecRotation.y;

		matrix[0][1] = 2.0f * vecRotation.x * vecRotation.y - 2.0f * vecRotation.w * vecRotation.z;
		matrix[1][1] = 1.0f - 2.0f * vecRotation.x * vecRotation.x - 2.0f * vecRotation.z * vecRotation.z;
		matrix[2][1] = 2.0f * vecRotation.y * vecRotation.z + 2.0f * vecRotation.w * vecRotation.x;

		matrix[0][2] = 2.0f * vecRotation.x * vecRotation.z + 2.0f * vecRotation.w * vecRotation.y;
		matrix[1][2] = 2.0f * vecRotation.y * vecRotation.z - 2.0f * vecRotation.w * vecRotation.x;
		matrix[2][2] = 1.0f - 2.0f * vecRotation.x * vecRotation.x - 2.0f * vecRotation.y * vecRotation.y;

		matrix[0][3] = vecPosition.x;
		matrix[1][3] = vecPosition.y;
		matrix[2][3] = vecPosition.z;

		return matrix;
	}

	[[nodiscard]] const Vector_t GetOrigin(int nIndex)
	{
		return Vector_t(this[nIndex]._11, this[nIndex]._12, this[nIndex]._13);
	}

	const void SetOrigin(int nIndex, Vector_t vecValue)
	{
		this[nIndex]._11 = vecValue.x;
		this[nIndex]._12 = vecValue.y;
		this[nIndex]._13 = vecValue.z;
	}

	[[nodiscard]] const Vector4D_t GetRotation(int nIndex)
	{
		return Vector4D_t(this[nIndex]._21, this[nIndex]._22, this[nIndex]._23, this[nIndex]._24);
	}

	union
	{
		struct
		{
			float _11, _12, _13, _14;
			float _21, _22, _23, _24;
		};
	};
};