#pragma once
#include <LibSWBF2/Chunks/GenericChunk.h>
#include <LibSWBF2/Chunks/STR.h>

// MILESTONE 459 (swbf2render): the munged model's SHADOW VOLUME.
// Layout found by walking geo1.lvl / rep.lvl chunk trees (modl children):
//   shdw
//     INFO (0 bytes)
//     BNAM  - the bone a static shadow hangs off (e.g. "geo_bldg_bunker"), or
//     BMAP  - the bone map of a skinned one (units)
//     GSHD  - the closed shadow mesh:
//       INFO (5 x u32: skinned?, ?, 5, vertexCount, indexCount)
//       IBUF - triangle STRIP indices (same format as a segment's IBUF)
//       VBUF - positions (+ normals / weights), same format as a segment's
//     CSHD  - precomputed edge connectivity (not parsed: rebuilt from GSHD)
namespace LibSWBF2::Chunks::LVL::modl
{
	struct IBUF;
	struct VBUF;
	struct BMAP;

	struct LIBSWBF2_API GSHD : public GenericChunk<"GSHD"_m>
	{
	public:
		uint32_t m_Info[5] = { 0, 0, 0, 0, 0 };
		IBUF* p_IndexBuffer = nullptr;
		VBUF* p_VertexBuffer = nullptr;

	public:
		void RefreshSize() override;
		void WriteToStream(FileWriter& stream) override;
		void ReadFromStream(FileReader& stream) override;
	};

	struct LIBSWBF2_API shdw : public GenericChunk<"shdw"_m>
	{
	public:
		STR<"BNAM"_m>* p_Bone = nullptr;
		BMAP* p_BoneMap = nullptr;
		GSHD* p_Geometry = nullptr;

	public:
		void RefreshSize() override;
		void WriteToStream(FileWriter& stream) override;
		void ReadFromStream(FileReader& stream) override;
	};
}
