#include <LibSWBF2/pch.h>
#include <LibSWBF2/Chunks/LVL/modl/IBUF.h>
#include "InternalHelpers.h"
#include <LibSWBF2/IO/FileReader.h>

namespace LibSWBF2::Chunks::LVL::modl
{
    void IBUF::RefreshSize()
    {
        THROW("Not implemented!");
    }

    void IBUF::WriteToStream(FileWriter& stream)
    {
        THROW("Not implemented!");
    }

    void IBUF::ReadFromStream(FileReader& stream)
    {
        BaseChunk::ReadFromStream(stream);
        Check(stream);

        m_IndicesCount = stream.ReadUInt32();
        m_Indices.Resize(m_IndicesCount);
        // swbf2-vita M532: one bulk read - was one ReadUInt16 + Add per index
        if (m_IndicesCount > 0)
            stream.ReadBytes((uint8_t*)m_Indices.Grow(m_IndicesCount), (size_t)m_IndicesCount * sizeof(uint16_t));

        BaseChunk::EnsureEnd(stream);
    }

    String IBUF::ToString() const
    {
        std::string result = fmt::format("Number of Inidces = {}\nIndices = [", m_IndicesCount);
        for (uint32_t i = 0; i < m_IndicesCount; ++i)
        {
            result += fmt::format("{}, ", m_Indices[i]);
        }

        result.resize(result.size() - 2);
        result += "]";
        return result.c_str();
    }
}