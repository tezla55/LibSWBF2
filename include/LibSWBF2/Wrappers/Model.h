#pragma once
#include <LibSWBF2/req.h>
#include <LibSWBF2/Wrappers/Segment.h>
#include <LibSWBF2/Wrappers/CollisionPrimitive.h>
#include <LibSWBF2/Wrappers/CollisionMesh.h>


namespace LibSWBF2
{
	namespace Chunks
	{
		namespace LVL::modl
		{
			struct modl;
			struct segm;
			struct modl;
		}

		namespace skel
		{
			struct skel;
		}
	}
}


namespace LibSWBF2::Wrappers
{
	using Types::List;
	using Types::Vector3;
	using Types::Vector4;
	using Chunks::LVL::modl::modl;
	using Chunks::LVL::modl::segm;
	using Chunks::LVL::skel::skel;
	
	class Level;

	struct LIBSWBF2_API Bone
	{
		String m_BoneName;
		String m_Parent;
		Vector3 m_Position;
		Vector4 m_Rotation;
	};

	class LIBSWBF2_API Model
	{

	private:
		friend Level;
		friend List<Model>;

		Model() = default;
		~Model() = default;

	private:
		modl* p_Model;
		skel* p_Skeleton;
		List<Segment> m_Segments;

		CollisionMesh m_CollisionMesh;
		List<CollisionPrimitive> m_CollisionPrimitives;

	public:
		static bool FromChunk(Level* mainContainer, modl* modelChunk, Model& out);

		const String& GetName() const;
		const List<Segment>& GetSegments() const;

		// MILESTONE 22 (2026-08-25, Vita port): real per-model visibility
		// bounding box, straight from the file's own modl/INFO chunk
		// (m_VisibilityBox) - a SEPARATE box from m_VertexBox (which is
		// only used internally to dequantize compressed vertex
		// positions, see modl.VBUF.cpp). Read from every model in every
		// .lvl since the format's introduction, but never exposed on
		// this wrapper or used anywhere in this codebase (confirmed via
		// SWBF2Phoenix's own copy of the same chunk code - same story
		// there, Unity's automatic culling means they never needed it
		// either). Model-local space, unrotated - callers combine with
		// the instance's own rotation/position for a world-space test.
		bool GetVisibilityBox(Vector3& mn, Vector3& mx) const;

		// M459: the model's real shadow volume PARTS (munged "shdw" chunks).
		// Positions in model space; stripIndices a triangle STRIP. A STATIC
		// part (boneName set) carries per-vertex FACE normals; a SKINNED part
		// (boneMap set) carries, per vertex, its triangle's other two corners
		// and all three corners' bone indices (into boneMap).
		struct ShadowVolumePart
		{
			uint32_t vertexCount = 0, indexCount = 0, boneMapCount = 0;
			const Vector3* positions = nullptr;
			const Vector3* normals = nullptr;      // static parts
			const Vector3* cornerB = nullptr;      // skinned parts
			const Vector3* cornerC = nullptr;      // skinned parts
			const Vector3u8* cornerBones = nullptr; // skinned parts: this, B, C
			const uint16_t* stripIndices = nullptr;
			const uint8_t* boneMap = nullptr;
			String boneName;
		};
		uint32_t GetShadowVolumeCount() const;
		bool GetShadowVolume(uint32_t part, ShadowVolumePart& out) const;

		bool IsSkeletalMesh() const;
		bool GetSkeleton(List<Bone>& bones) const;
		bool IsSkeletonBroken() const;


		const CollisionMesh& GetCollisionMesh() const;
		List<CollisionPrimitive> GetCollisionPrimitives(ECollisionMaskFlags mask =
															ECollisionMaskFlags::All) const;
	};
}