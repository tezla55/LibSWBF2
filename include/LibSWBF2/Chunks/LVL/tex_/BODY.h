#pragma once
#include <LibSWBF2/Chunks/GenericChunk.h>
#include <LibSWBF2/Types/Enums.h>
#include <LibSWBF2/DirectX/DXTexCrossPlat.h>

#ifdef _WIN32
namespace DirectX { class ScratchImage; }
#else
namespace DXTexCrossPlat { class CrossPlatImage; }
#endif


namespace LibSWBF2::Chunks::LVL::LVL_texture
{
	struct LIBSWBF2_API BODY : public GenericChunk<"BODY"_m>
	{
	private:
		mutable DXTexCrossPlat::CrossPlatImage* p_Image = nullptr;

	public:
		void RefreshSize() override;
		void WriteToStream(FileWriter& stream) override;
		void ReadFromStream(FileReader& stream) override;

		bool GetImageData(ETextureFormat format, uint16_t& width, uint16_t& height, const uint8_t*& data) const;
		// M488 (swbf2render): the stored blocks, unconverted - false once converted or when not DXT1/3/5
		bool GetRawData(uint16_t& width, uint16_t& height, uint32_t& d3dFormat, const uint8_t*& data, size_t& size) const;

		~BODY();
	};
}