#include <LibSWBF2/pch.h>
#include <LibSWBF2/Wrappers/Level.h>
#include <LibSWBF2/Wrappers/Model.h>
#include "InternalHelpers.h"

#include <LibSWBF2/Chunks/LVL/modl/LVL.modl.h>
#include <LibSWBF2/Chunks/LVL/skel/skel.h>
#include <LibSWBF2/Chunks/LVL/modl/shdw.h>
#include <LibSWBF2/Chunks/LVL/modl/IBUF.h>
#include <LibSWBF2/Chunks/LVL/modl/BMAP.h>
#include <LibSWBF2/Chunks/LVL/modl/modl.VBUF.h>

static LibSWBF2::String empty = "";

namespace LibSWBF2::Wrappers
{
	bool Model::FromChunk(Level* mainContainer, modl* modelChunk, Model& out)
	{
		if (mainContainer == nullptr)
		{
			LOG_ERROR("Given mainContainer was NULL!");
			return false;
		}
		if (modelChunk == nullptr)
		{
			LOG_ERROR("Given modelChunk was NULL!");
			return false;
		}

		out.p_Model = modelChunk;
		out.p_Skeleton = mainContainer->FindSkeleton(out.p_Model->p_Name->m_Text);

		List<segm*>& segments = modelChunk->m_Segments;
		for (size_t i = 0; i < segments.Size(); ++i)
		{
			Segment segment;
			if (Segment::FromChunk(mainContainer, segments[i], segment))
			{
				out.m_Segments.Add(segment);
			}
		}
		return true;
	}

	const String& Model::GetName() const
	{
		return p_Model->p_Name->m_Text;
	}

	const List<Segment>& Model::GetSegments() const
	{
		return m_Segments;
	}

	bool Model::IsSkeletalMesh() const
	{
		if (p_Skeleton == nullptr)
			return false;

		for (size_t i = 0; i < m_Segments.Size(); ++i)
		{
			if (m_Segments[i].ContainsWeights())
			{
				return true;
			}
		}
		return false;
	}


	bool Model::IsSkeletonBroken() const
	{
		if (p_Skeleton == nullptr)
			return true;

		uint32_t count = p_Skeleton->p_Info->m_BoneCount;
		List<String>& names = p_Skeleton->p_BoneNames->m_Texts;
		List<String>& parents = p_Skeleton->p_BoneParents->m_Texts;
		List<Matrix3x3>& rotations = p_Skeleton->p_BoneTransforms->m_BoneRotations;
		List<Vector3>& positions = p_Skeleton->p_BoneTransforms->m_BonePositions;

		// there's always one less parent, because root (DummyRoot) doesn't have one =P
		size_t equalParents = parents.Size() + 1;
		if (count != names.Size() || count != equalParents || count != rotations.Size() || count != positions.Size())
		{
			LOG_ERROR(
				"Broken Skeleton! Sizes don't match up!\n"
				"\tCount: {}\n"
				"\tNames: {}\n"
				"\tParents (-1): {}\n"
				"\tRotations: {}\n"
				"\tPositions: {}",
				count,
				names.Size(),
				parents.Size(),
				rotations.Size(),
				positions.Size()
			);

			return true;
		}

		return false;
	}




	bool Model::GetSkeleton(List<Bone>& bones) const
	{
		if (p_Skeleton == nullptr)
			return false;

		uint32_t count = p_Skeleton->p_Info->m_BoneCount;
		List<String>& names = p_Skeleton->p_BoneNames->m_Texts;
		List<String>& parents = p_Skeleton->p_BoneParents->m_Texts;
		List<Matrix3x3>& rotations = p_Skeleton->p_BoneTransforms->m_BoneRotations;
		List<Vector3>& positions = p_Skeleton->p_BoneTransforms->m_BonePositions;


		bones.Clear();
		int offset = IsSkeletonBroken() ? 1 : 0;

		// MILESTONE 447 (port): the parent list has one entry per bone EXCEPT
		// the root, and the root is not always first. rep_walk_atte's
		// skeleton has no "dummyroot"; its root is the node named after the
		// model ("rep_walk_atte", index 31 of 44), so reading parents[i - 1]
		// for every i > 0 shifted bones 1..31 onto their predecessor's
		// parent - the "exploded" AT-TE. Verified against the source
		// basepose.msh MODL/PRNT chunks. Root = "dummyroot" if present, else
		// the model-named node, else the first entry (the old behaviour).
		const size_t n = names.Size();
		size_t root = 0;
		if (parents.Size() + 1 == n)
		{
			bool found = false;
			for (size_t i = 0; i < n && !found; ++i) if (names[i] == "dummyroot") { root = i; found = true; }
			if (!found && p_Model != nullptr && p_Model->p_Name != nullptr)
				for (size_t i = 0; i < n && !found; ++i) if (names[i] == p_Model->p_Name->m_Text) { root = i; found = true; }
		}
		for (size_t i = 0; i < n; ++i)
		{
			const String* parent = &empty;
			if (parents.Size() >= n)      parent = &parents[i];
			else if (i < root)            parent = &parents[i];
			else if (i > root)            parent = &parents[i - 1];
			bones.Add({ names[i], *parent, positions[i + offset], MatrixToQuaternion(rotations[i + offset]) });
		}
		return true;
	}


	const CollisionMesh& Model::GetCollisionMesh() const
	{
		return m_CollisionMesh;
	}


	List<CollisionPrimitive> Model::GetCollisionPrimitives(ECollisionMaskFlags mask) const
	{
		List<CollisionPrimitive> maskedPrimitives;

		for (int i = 0; i < m_CollisionPrimitives.Size(); i++)
		{
			const CollisionPrimitive& curPrimitive = m_CollisionPrimitives[i];

			if ((curPrimitive.GetMaskFlags() & mask) != 0)
			{
				maskedPrimitives.Add(curPrimitive);
			}
		}

		return maskedPrimitives;
	}


	// M22/M242: real per-model visibility bounds, straight from the modl
	// chunk's own INFO sub-chunk (m_VisibilityBox) - a real field the
	// format has always carried, separate from m_VertexBox (used only to
	// dequantize compressed vertex positions, see modl/VBUF's own reader).
	// Declared on this wrapper since M22, used by swbf2render's frustum
	// culling and (M242) HUD weapon-model geometry ever since - but never
	// actually implemented here. The checked-in source and the binary
	// that has been deploying successfully had drifted apart; caught while
	// trying to build a native PC-side tool against this exact function.
	bool Model::GetVisibilityBox(Vector3& mn, Vector3& mx) const
	{
		if (p_Model == nullptr || p_Model->p_Info == nullptr)
		{
			return false;
		}
		mn = p_Model->p_Info->m_VisibilityBox[0];
		mx = p_Model->p_Info->m_VisibilityBox[1];
		return true;
	}

	uint32_t Model::GetShadowVolumeCount() const
	{
		return (p_Model == nullptr) ? 0 : (uint32_t)p_Model->m_Shadows.Size();
	}

	bool Model::GetShadowVolume(uint32_t part, ShadowVolumePart& out) const
	{
		out = ShadowVolumePart();
		if (p_Model == nullptr || part >= p_Model->m_Shadows.Size()) return false;
		const LibSWBF2::Chunks::LVL::modl::shdw* sh = p_Model->m_Shadows[part];
		if (sh == nullptr || sh->p_Geometry == nullptr) return false;
		const auto* vb = sh->p_Geometry->p_VertexBuffer;
		const auto* ib = sh->p_Geometry->p_IndexBuffer;
		if (vb == nullptr || ib == nullptr) return false;
		out.vertexCount = (uint32_t)vb->m_Positions.Size();
		out.positions = vb->m_Positions.GetArrayPtr();
		out.indexCount = (uint32_t)ib->m_Indices.Size();
		out.stripIndices = ib->m_Indices.GetArrayPtr();
		if (vb->m_Normals.Size() == out.vertexCount) out.normals = vb->m_Normals.GetArrayPtr();
		if (vb->m_ShadowCornerB.Size() == out.vertexCount)
		{
			out.cornerB = vb->m_ShadowCornerB.GetArrayPtr();
			out.cornerC = vb->m_ShadowCornerC.GetArrayPtr();
			out.cornerBones = vb->m_BoneIndicies.GetArrayPtr();
		}
		if (sh->p_BoneMap != nullptr)
		{
			out.boneMapCount = (uint32_t)sh->p_BoneMap->m_IndexMap.Size();
			out.boneMap = sh->p_BoneMap->m_IndexMap.GetArrayPtr();
		}
		if (sh->p_Bone != nullptr) out.boneName = sh->p_Bone->m_Text;
		return out.vertexCount > 0 && out.indexCount > 2;
	}
}