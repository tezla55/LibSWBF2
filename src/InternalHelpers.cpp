#include "InternalHelpers.h"
#include <LibSWBF2/pch.h>
#include <LibSWBF2/Logging/Logger.h>

namespace LibSWBF2
{
	Vector2 ToLib(const glm::vec2 vector)
	{
		return Vector2(vector.x, vector.y);
	}

	glm::vec3 ToGLM(const Vector3& vector)
	{
		return glm::vec3(vector.m_X, vector.m_Y, vector.m_Z);
	}

	Vector3 ToLib(const glm::vec3 vector)
	{
		return Vector3(vector.x, vector.y, vector.z);
	}

	glm::mat3 ToGLM(const Matrix3x3& matrix)
	{
		return glm::mat3(
			matrix.matrix[0][0], matrix.matrix[0][1], matrix.matrix[0][2],
			matrix.matrix[1][0], matrix.matrix[1][1], matrix.matrix[1][2],
			matrix.matrix[2][0], matrix.matrix[2][1], matrix.matrix[2][2]
		);
		//return glm::mat3(
		//	matrix.matrix[0][0], matrix.matrix[1][0], matrix.matrix[2][0],
		//	matrix.matrix[0][1], matrix.matrix[1][1], matrix.matrix[2][1],
		//	matrix.matrix[0][2], matrix.matrix[1][2], matrix.matrix[2][2]
		//);
	}

	Vector4 ToLib(const glm::quat& quaternion)
	{
		return Vector4(quaternion.x, quaternion.y, quaternion.z, quaternion.w);
	}

	template <typename V, typename T>
	List<V> TriangleStripToTriangleList(List<T>& indexBuffer, uint32_t offset)
	{
		List<V> result;
		V a,b,c;

		for (int i = 0; i < indexBuffer.Size() - 2; i++)
		{
			a = indexBuffer[i]   + (V) offset;
			b = indexBuffer[i+1] + (V) offset;
			c = indexBuffer[i+2] + (V) offset; 

			if (a != b && b != c && a != c)	//Catch degenerate 
			{
				if (i % 2 != 0) //swap clockwiseness 
				{
					std::swap(a,b);
				}

				result.Add(a);
				result.Add(b);
				result.Add(c);
			}
		}

		return result;
	}

	template List<uint32_t> TriangleStripToTriangleList<uint32_t,uint32_t>(List<uint32_t>& indexBuffer, uint32_t offset);
	template List<uint32_t> TriangleStripToTriangleList<uint32_t,uint16_t>(List<uint16_t>& indexBuffer, uint32_t offset);
	template List<uint16_t> TriangleStripToTriangleList<uint16_t,uint16_t>(List<uint16_t>& indexBuffer, uint32_t offset);


	template <typename V, typename T>
	List<V> TriangleFanToTriangleList(List<T>& indexBuffer, uint32_t offset)
	{
		List<V> result;
		V a,b,c;

		if (indexBuffer.Size() < 3)
		{
			return result;
		}

		for (int i = 2; i < indexBuffer.Size(); i++)
		{
			a = indexBuffer[i-1] + (V) offset;
			b = indexBuffer[i]   + (V) offset;
			c = indexBuffer[0]   + (V) offset; 

			if (a != b && b != c && a != c)	//Catch degenerate 
			{
				result.Add(a);
				result.Add(b);
				result.Add(c);
			}
		}

		return result;
	}

	template List<uint32_t> TriangleFanToTriangleList<uint32_t,uint32_t>(List<uint32_t>& indexBuffer, uint32_t offset);
	template List<uint32_t> TriangleFanToTriangleList<uint32_t,uint16_t>(List<uint16_t>& indexBuffer, uint32_t offset);
	template List<uint16_t> TriangleFanToTriangleList<uint16_t,uint16_t>(List<uint16_t>& indexBuffer, uint32_t offset);



	Vector4 MatrixToQuaternion(const Matrix3x3& matrix)
	{
		glm::mat3 mat = ToGLM(matrix);

		// Some real shipped chunks store an IMPROPER orthogonal matrix -
		// determinant -1, i.e. a rotation composed with an axis flip from
		// the source coordinate convention - rather than a pure rotation.
		// Every single BARR on Geonosis is like this. Decoded straight
		// out of geo1.lvl, "barrier18" carries
		//     [ 0.58912  0  0.80804 ]
		//     [ 0        1  0       ]
		//     [ 0.80804  0 -0.58912 ]
		// whose determinant is -1, alongside a position (-9.328, 0,
		// -160.341) that matches the authored geo1.bar corners exactly.
		//
		// glm::quat_cast is only defined for a proper rotation. Handed a
		// reflection it does not fail, it returns a DEGENERATE, non-unit
		// quaternion - all 398 Geonosis barriers come back as the very
		// same (0, 0.7071, 0, 0), magnitude 0.7071. Rotating a vector by
		// a quaternion that short does not rotate it, it shrinks it; for
		// this particular value it collapses the vector to exactly zero,
		// so any "is this point inside my local box" test built on it
		// answers YES for every point in the world.
		//
		// WHICH axis gets flipped is not free, and picking wrong is not
		// obvious - it still yields a unit quaternion and a plausible
		// box, just the MIRROR IMAGE of the real one, covering a
		// different patch of ground. Negating a COLUMN flips one of the
		// box's own local axes, which leaves the same box (an axis is
		// sign-agnostic) and so cannot undo a reflection at all.
		// Negating the X ROW mirrors the axes in world X, which is
		// exactly the stored convention's flip.
		//
		// Checked against the authored data rather than reasoned about:
		// reconstructing barrier18's four corners from the recovered
		// position/rotation/size reproduces the four Corner() lines in
		// geo1.bar, and the same holds for barrier19. The column-negating
		// version reproduced the same X and Z VALUES paired the wrong way
		// round - the giveaway that it was a mirror.
		//
		// Matrices that are already proper (every Instance, Region and
		// HintNode on this map - all measured at exactly |q| = 1.0) are
		// untouched by this.
		if (glm::determinant(mat) < 0.0f)
		{
			mat[0][0] = -mat[0][0];
			mat[1][0] = -mat[1][0];
			mat[2][0] = -mat[2][0];
		}

		glm::quat quat = glm::quat(mat);
		return ToLib(quat);
	}

	std::string ToLower(String name)
	{
		std::string result = name.Buffer();
		std::transform(result.begin(), result.end(), result.begin(),
			[](unsigned char c) { return std::tolower(c); });
		return result;
	}
}
