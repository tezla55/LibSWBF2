#include <LibSWBF2/pch.h>
#include <LibSWBF2/Chunks/LVL/tern/tern.h>
#include "InternalHelpers.h"
#include <LibSWBF2/IO/FileReader.h>
#include <fmt/format.h>


namespace LibSWBF2::Chunks::LVL::terrain
{
    void tern::RefreshSize()
    {
        THROW("Not implemented!");
    }

    void tern::WriteToStream(FileWriter& stream)
    {
        THROW("Not implemented!");
    }

    void tern::ReadFromStream(FileReader& stream)
    {
        BaseChunk::ReadFromStream(stream);
        Check(stream);

        while (ThereIsAnother(stream))
        {
            ChunkHeader next = stream.ReadChunkHeader(true);
            if (next == "NAME"_h)
            {
                READ_CHILD(stream, p_Name);
            }
            else if (next == "DTLX"_h)
            {
                READ_CHILD(stream, p_DetailTexture);
            }
            else if (next == "INFO"_h)
            {
                READ_CHILD(stream, p_Info);
            }
            else if (next == "LTEX"_h)
            {
                READ_CHILD(stream, p_LayerTextures);
            }
            else if (next == "DTEX"_h)
            {
                READ_CHILD(stream, p_LayerDetailTextures);
            }
            else if (next == "PCHS"_h)
            {
                READ_CHILD(stream, p_Patches);
            }
            else if (next == "WATR"_h)
            {   // M530: read by hand (see tern.h)
                stream.ReadChunkHeader(false);
                const size_t size = stream.ReadChunkSize();
                const size_t end = stream.GetPosition() + size;
                while (stream.GetPosition() + 8 <= end)
                {
                    const ChunkHeader h = stream.ReadChunkHeader(false);
                    const size_t s = stream.ReadChunkSize();
                    const size_t next2 = stream.GetPosition() + ((s + 3) & ~(size_t)3);
                    if (h == "INFO"_h && s >= 16)
                    {
                        m_WaterGrid = stream.ReadUInt32(); stream.ReadUInt32(); stream.ReadFloat(); m_WaterExtent = stream.ReadFloat();
                    }
                    else if (h == "LAYR"_h && s >= 9)
                    {
                        stream.ReadString(); // the layer name (null-terminated)
                        stream.ReadUInt32();
                        m_WaterHeights.Add(stream.ReadFloat());
                    }
                    else if (h == "WMAP"_h)
                    {
                        for (size_t k = 0; k < s; ++k) m_WaterMap.Add(stream.ReadByte());
                    }
                    stream.SetPosition(next2);
                }
                stream.SetPosition(end + ((end & 3) ? 4 - (end & 3) : 0));
            }
            else
            {
                READ_CHILD_GENERIC(stream);
            }
        }

        BaseChunk::EnsureEnd(stream);
    }

    String tern::ToString() const
    {
        return fmt::format("Name: {}", p_Name->m_Text).c_str();
    }
}
