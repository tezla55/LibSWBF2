#pragma once
#include <LibSWBF2/Chunks/GenericChunk.h>
#include <LibSWBF2/Chunks/STR.h>
#include <LibSWBF2/Types/Enums.h>

namespace LibSWBF2::Chunks::LVL::modl
{
	struct LIBSWBF2_API VBUF : public GenericChunk<"VBUF"_m>
	{
	public:
		uint32_t m_Count;
		uint32_t m_Stride;	// bytes per vertex
		EVBUFFlags m_Flags;
		
		List<Vector3> m_Positions;
		List<Vector3> m_Normals;
		List<Vector3> m_Tangents;
		List<Vector3> m_BiTangents;

		List<Color4u8> m_Colors;
		List<Vector2> m_TexCoords;

		List<Vector3u8> m_BoneIndicies;
		List<Vector3> m_Weights;

		// M459: a SKINNED shadow volume's vertex (flag 0x800, found in
		// rep.lvl/cis.lvl, stride 28): its own position (m_Positions), the
		// three corner bone indices (m_BoneIndicies: this, B, C) plus one
		// unknown byte, and the positions of its triangle's other two
		// corners - so the face normal can be rebuilt after skinning.
		List<Vector3> m_ShadowCornerB;
		List<Vector3> m_ShadowCornerC;
		List<uint8_t> m_ShadowCornerFlag;

	public:
		void RefreshSize() override;
		void WriteToStream(FileWriter& stream) override;
		void ReadFromStream(FileReader& stream) override;

		String ToString() const override;
	};
}