#pragma once
#include <LibSWBF2/Chunks/GenericChunk.h>
#include <LibSWBF2/Chunks/STR.h>
#include <LibSWBF2/Chunks/LVL/tern/tern.INFO.h>
#include <LibSWBF2/Chunks/LVL/tern/PCHS.h>
#include <LibSWBF2/Chunks/LVL/tern/LTEX.h>
#include <LibSWBF2/Chunks/LVL/tern/DTEX.h>

namespace LibSWBF2::Chunks::LVL::terrain
{
	struct LIBSWBF2_API tern : public GenericChunk<"tern"_m>
	{
	public:
		STR<"NAME"_m>* p_Name;
		STR<"DTLX"_m>* p_DetailTexture;
		INFO* p_Info;
		LTEX* p_LayerTextures;
		DTEX* p_LayerDetailTextures;
		PCHS* p_Patches;

		// M530 (swbf2render): the munged water, WATR > INFO (grid cells per side, ?, patch size,
		// extent), LAYR (name, int, HEIGHT, 2 floats, u/v tile, rgba), WMAP (grid x grid 4-bit layer
		// indices, two cells per byte). Layouts read from the shipped lvls (check_water), not documented.
		uint32_t m_WaterGrid = 0;
		float m_WaterExtent = 0.0f;
		List<float> m_WaterHeights;
		List<uint8_t> m_WaterMap;

	public:
		void RefreshSize() override;
		void WriteToStream(FileWriter& stream) override;
		void ReadFromStream(FileReader& stream) override;

		String ToString() const override;
	};
}