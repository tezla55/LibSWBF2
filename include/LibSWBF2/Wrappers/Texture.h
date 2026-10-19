#pragma once
#include <LibSWBF2/Types/Enums.h>
#include <LibSWBF2/Types/List.h>

namespace LibSWBF2::Chunks::LVL::texture
{
	struct tex_;
	struct FMT_;
}

namespace LibSWBF2::Wrappers
{
	using Chunks::LVL::texture::tex_;
	using Chunks::LVL::texture::FMT_;
	using LibSWBF2::Types::String;
	using LibSWBF2::Types::List;

	class Level;

	class LIBSWBF2_API Texture
	{
	private:
		friend Level;
		friend List<Texture>;

		Texture() = default;
		~Texture() = default;

		tex_* p_Texture;
		FMT_* p_FMT;

	public:
		static bool FromChunk(tex_* textureChunk, Texture& out);

		String GetName() const;
		uint32_t GetNumMipMaps() const;

		bool GetImageData(ETextureFormat format, uint8_t mipLevel, uint16_t& width, uint16_t& height, const uint8_t*& data) const;
		// M488 (swbf2render): a mip level's stored DXT1/3/5 blocks; false when the texture is not DXT (or was converted)
		bool GetRawImageData(uint8_t mipLevel, uint16_t& width, uint16_t& height, uint32_t& d3dFormat, const uint8_t*& data, size_t& size) const;
	};
}