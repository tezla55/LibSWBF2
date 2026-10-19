#include <LibSWBF2/pch.h>
#include <LibSWBF2/Chunks/LVL/modl/shdw.h>
#include "InternalHelpers.h"
#include <LibSWBF2/IO/FileReader.h>
#include <LibSWBF2/Chunks/LVL/modl/IBUF.h>
#include <LibSWBF2/Chunks/LVL/modl/BMAP.h>
#include <LibSWBF2/Chunks/LVL/modl/modl.VBUF.h>

namespace LibSWBF2::Chunks::LVL::modl
{
    void GSHD::RefreshSize() { THROW("Not implemented!"); }
    void GSHD::WriteToStream(FileWriter& stream) { THROW("Not implemented!"); }
    void GSHD::ReadFromStream(FileReader& stream)
    {
        BaseChunk::ReadFromStream(stream);
        Check(stream);
        while (ThereIsAnother(stream))
        {
            ChunkHeader next = stream.ReadChunkHeader(true);
            if (next == "INFO"_h)
            {
                // 5 plain u32s - read in place, no chunk class of their own
                ChunkHeader h = stream.ReadChunkHeader(false);
                ChunkSize sz = stream.ReadChunkSize();
                (void)h;
                const size_t start = stream.GetPosition();
                for (int i = 0; i < 5 && (ChunkSize)(i * 4 + 4) <= sz; ++i) m_Info[i] = stream.ReadUInt32();
                stream.SetPosition(start + sz + ((4 - sz % 4) % 4));
            }
            else if (next == "IBUF"_h) { READ_CHILD(stream, p_IndexBuffer); }
            else if (next == "VBUF"_h) { READ_CHILD(stream, p_VertexBuffer); }
            else { READ_CHILD_GENERIC(stream); }
        }
        BaseChunk::EnsureEnd(stream);
    }

    void shdw::RefreshSize() { THROW("Not implemented!"); }
    void shdw::WriteToStream(FileWriter& stream) { THROW("Not implemented!"); }
    void shdw::ReadFromStream(FileReader& stream)
    {
        BaseChunk::ReadFromStream(stream);
        Check(stream);
        while (ThereIsAnother(stream))
        {
            ChunkHeader next = stream.ReadChunkHeader(true);
            if (next == "BNAM"_h) { READ_CHILD(stream, p_Bone); }
            else if (next == "BMAP"_h) { READ_CHILD(stream, p_BoneMap); }
            else if (next == "GSHD"_h) { READ_CHILD(stream, p_Geometry); }
            else { READ_CHILD_GENERIC(stream); }
        }
        BaseChunk::EnsureEnd(stream);
    }
}
