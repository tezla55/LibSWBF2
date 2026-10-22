#include <LibSWBF2/pch.h>
#include <LibSWBF2/Chunks/LVL/modl/modl.VBUF.h>
#include <LibSWBF2/Chunks/LVL/modl/LVL.modl.h>
#include "InternalHelpers.h"
#include <LibSWBF2/IO/FileReader.h>
#include <LibSWBF2/Logging/Logger.h>
#include "InternalHelpers.h"
#include <LibSWBF2/IO/FileReader.h>
#include <limits>
#include <cstring>
#include <string>
#include <vector>


#undef min
#undef max

namespace
{
    // swbf2-vita M532: a vertex block read from the file in ONE call, then parsed from memory. Positions are
    // file offsets (the parser's per-vertex SetPosition(vertexStart + stride) works unchanged). `final`, with
    // inline bodies: the parser's own stream.ReadX() calls become plain loads - on the Vita each used to be a
    // virtual call + CheckGood + a buffered file read (VBUF: ~2.5 M of them on one cor1 load). Reads past the
    // block give 0 (the file reader threw) - the vertex data itself is pinned by tests/native/check_lvl_digest.
    class SpanReader final : public LibSWBF2::FileReader
    {
        const uint8_t* m_Data; size_t m_Base, m_Pos, m_End;
        template <class T> T Get() { T v{}; if (m_Pos >= m_Base && m_Pos + sizeof(T) <= m_End) memcpy(&v, m_Data + (m_Pos - m_Base), sizeof(T)); m_Pos += sizeof(T); return v; }
    public:
        SpanReader(const uint8_t* data, size_t base, size_t len) : m_Data(data), m_Base(base), m_Pos(base), m_End(base + len) {}
        bool Open(const LibSWBF2::Types::String&) override { return true; }
        LibSWBF2::ChunkHeader ReadChunkHeader(bool peek) override { LibSWBF2::ChunkHeader h; h.m_Magic = Get<uint32_t>(); if (peek) m_Pos -= 4; return h; }
        LibSWBF2::ChunkSize ReadChunkSize() override { return Get<uint32_t>(); }
        uint8_t ReadByte() override { return Get<uint8_t>(); }
        bool ReadBytes(uint8_t* d, size_t n) override { if (m_Pos < m_Base || m_Pos + n > m_End) return false; memcpy(d, m_Data + (m_Pos - m_Base), n); m_Pos += n; return true; }
        int32_t ReadInt32() override { return Get<int32_t>(); }
        int16_t ReadInt16() override { return Get<int16_t>(); }
        uint32_t ReadUInt32() override { return Get<uint32_t>(); }
        uint16_t ReadUInt16() override { return Get<uint16_t>(); }
        float ReadFloat() override { return Get<float>(); }
        LibSWBF2::Types::String ReadString(size_t length) override { std::string t(length, '\0'); ReadBytes((uint8_t*)&t[0], length); return t.c_str(); }
        LibSWBF2::Types::String ReadString() override { std::string t; while (m_Pos < m_End) { const uint8_t c = Get<uint8_t>(); if (c == 0) break; t += (char)c; } return t.c_str(); }
        size_t GetPosition() override { return m_Pos; }
        void SetPosition(size_t p) override { m_Pos = p; }
        size_t GetFileSize() override { return m_End; }
        bool SkipBytes(const size_t& n) override { m_Pos += n; return true; }
        void Close() override {}
        bool CheckGood(size_t n) override { return m_Pos >= m_Base && m_Pos + n <= m_End; }
    };
}

namespace LibSWBF2::Chunks::LVL::modl
{
    void VBUF::RefreshSize()
    {
        THROW("Not implemented!");
    }

    void VBUF::WriteToStream(FileWriter& stream)
    {
        THROW("Not implemented!");
    }

    void VBUF::ReadFromStream(FileReader& fileStream)
    {
        BaseChunk::ReadFromStream(fileStream);
        Check(fileStream);

        m_Count = fileStream.ReadUInt32();
        m_Stride = fileStream.ReadUInt32();
        m_Flags = (EVBUFFlags)fileStream.ReadUInt32();

        // M459: walk up to the model - a segment's VBUF sits at segm/modl,
        // a shadow volume's at GSHD/shdw/modl.
        modl* model = nullptr;
        for (GenericBaseChunk* p = GetParent(); p != nullptr && model == nullptr; p = p->GetParent())
            model = dynamic_cast<modl*>(p);
        if (model == nullptr)
        {
            // should never happen
            LOG_ERROR("Parent modl is NULL!");
            BaseChunk::EnsureEnd(fileStream);
            return;
        }

        // swbf2-vita M532: the rest of the chunk (the vertices) in one read; parsed from memory below
        const size_t blockStart = fileStream.GetPosition();
        const size_t chunkEnd = GetDataPosition() + GetDataSize();
        const size_t blockLen = chunkEnd > blockStart ? chunkEnd - blockStart : 0;
        std::vector<uint8_t> block(blockLen);
        if (blockLen > 0) fileStream.ReadBytes(block.data(), blockLen);
        SpanReader stream(block.data(), blockStart, blockLen);

        // M459: compressed position decode, shared by the shadow-corner layout.
        auto readPos = [&](Vector3& out) {
            if ((m_Flags & EVBUFFlags::PositionCompressed) != 0)
            {
                glm::vec3 low = ToGLM(model->p_Info->m_VertexBox[0]);
                glm::vec3 mul = (ToGLM(model->p_Info->m_VertexBox[1]) - ToGLM(model->p_Info->m_VertexBox[0]));
                int16_t data[4];
                data[0] = stream.ReadInt16(); data[1] = stream.ReadInt16(); data[2] = stream.ReadInt16(); data[3] = stream.ReadInt16();
                glm::vec3 c(data[0], data[1], data[2]);
                constexpr float i16min = std::numeric_limits<int16_t>::min();
                constexpr float i16max = std::numeric_limits<int16_t>::max();
                out = ToLib(low + (c - i16min) * mul / (i16max - i16min));
            }
            else out.ReadFromStream(stream);
        };

        for (uint32_t i = 0; i < m_Count; ++i)
        {
            // M459: every vertex advances by exactly m_Stride - a field this
            // reader does not know no longer shifts every vertex after it.
            const size_t vertexStart = stream.GetPosition();
            if ((m_Flags & EVBUFFlags::ShadowFaceCorners) != 0)
            {
                Vector3 a, b, c;
                readPos(a);
                const uint8_t b0 = stream.ReadByte(), b1 = stream.ReadByte(), b2 = stream.ReadByte(), fl = stream.ReadByte();
                readPos(b);
                readPos(c);
                m_Positions.Add(a); m_ShadowCornerB.Add(b); m_ShadowCornerC.Add(c);
                m_BoneIndicies.Add({ b0, b1, b2 }); m_ShadowCornerFlag.Add(fl);
                stream.SetPosition(vertexStart + m_Stride);
                continue;
            }
            if ((m_Flags & EVBUFFlags::Position) != 0)
            {
                if ((m_Flags & EVBUFFlags::PositionCompressed) != 0)
                {
                    glm::vec3 low = ToGLM(model->p_Info->m_VertexBox[0]);
                    glm::vec3 mul = (ToGLM(model->p_Info->m_VertexBox[1]) - ToGLM(model->p_Info->m_VertexBox[0]));

                    int16_t data[4];
                    data[0] = stream.ReadInt16();
                    data[1] = stream.ReadInt16();
                    data[2] = stream.ReadInt16();
                    data[3] = stream.ReadInt16();
                    glm::vec3 c(data[0], data[1], data[2]);

                    constexpr float i16min = std::numeric_limits<int16_t>::min();
                    constexpr float i16max = std::numeric_limits<int16_t>::max();

                    m_Positions.Add(ToLib(low + (c - i16min) * mul / (i16max - i16min)));
                }
                else
                {
                    m_Positions.Emplace().ReadFromStream(stream);
                }
            }

            if ((m_Flags & EVBUFFlags::BlendWeight) != 0)
            {
                if ((m_Flags & EVBUFFlags::BlendWeightCompressed) != 0)
                {
                    int8_t data[4];
                    data[0] = stream.ReadByte();
                    data[1] = stream.ReadByte();
                    data[2] = stream.ReadByte();
                    data[3] = stream.ReadByte();

                    float one = (float) data[1];
                    float two = (float) data[2];
                    m_Weights.Add({ two, one, 1.0f - two - one});
                }
                else
                {
                    float x = stream.ReadFloat();
                    float y = stream.ReadFloat();
                    m_Weights.Add({ x, y, 1.0f - x - y});
                }
            }

            if ((m_Flags & EVBUFFlags::Unknown1) != 0)
            {

                uint32_t inds = stream.ReadUInt32();

                uint8_t x = (uint8_t) (inds & 0xffu);
                uint8_t y = (uint8_t) ((inds >> 8u) & 0xffu);
                uint8_t z = (uint8_t) ((inds >> 16u) & 0xffu);


                m_BoneIndicies.Add({x, y, z});


                /*
                uint8_t data[4];
                data[0] = stream.ReadByte();
                data[1] = stream.ReadByte();
                data[2] = stream.ReadByte();
                data[3] = stream.ReadByte();

                if ((m_Flags & EVBUFFlags::PositionCompressed) != 0)
                {
                    m_Bones.Emplace().ReadFromStream(stream);
                    LOG_WARN("Weight?: {}", stream.ReadFloat());

                    uint16_t data2[2];
                    data2[0] = *(uint16_t*)&data[0];
                    data2[1] = *(uint16_t*)&data[2];
                    uint32_t data3 = *(uint32_t*)&data[0];
                    LOG_WARN("[{}] = {}-{}-{}-{} / {} - {} / {}", i, data[0], data[1], data[2], data[3], data2[0], data2[1], data3);

                    std::string hash = lookup_fnv_hash(data3);
                    LOG_WARN("Hash: {}", hash);
                }
                */
            }

            if ((m_Flags & EVBUFFlags::Normal) != 0)
            {
                if ((m_Flags & EVBUFFlags::NormalCompressed) != 0)
                {
                    // M459: UNSIGNED bytes in B,G,R order (D3DCOLOR packing), 0..255 -> -1..1.
                    // Proven natively: geo_bldg_bunker's shadow volume, 312/312 decoded
                    // normals parallel to their face's true normal this way, 90/312 with
                    // the old signed x,y,z decode (which scrambled every lit mesh too).
                    uint8_t data[4];
                    data[0] = (uint8_t)stream.ReadByte();
                    data[1] = (uint8_t)stream.ReadByte();
                    data[2] = (uint8_t)stream.ReadByte();
                    data[3] = (uint8_t)stream.ReadByte();
                    glm::vec3 normal((float)data[2], (float)data[1], (float)data[0]);
                    normal = (normal / 255.0f) * 2.0f - 1.0f;
                    m_Normals.Add(ToLib(normal));
                }
                else
                {
                    m_Normals.Emplace().ReadFromStream(stream);
                }
            }

            if ((m_Flags & EVBUFFlags::Tangents) != 0)
            {
                if ((m_Flags & EVBUFFlags::NormalCompressed) != 0)
                {
                    uint8_t data[4]; // M459: same packing as the normal (see there)
                    data[0] = (uint8_t)stream.ReadByte();
                    data[1] = (uint8_t)stream.ReadByte();
                    data[2] = (uint8_t)stream.ReadByte();
                    data[3] = (uint8_t)stream.ReadByte();
                    glm::vec3 tangent((float)data[2], (float)data[1], (float)data[0]);
                    tangent = (tangent / 255.0f) * 2.0f - 1.0f;
                    m_Tangents.Add(ToLib(tangent));

                    data[0] = (uint8_t)stream.ReadByte();
                    data[1] = (uint8_t)stream.ReadByte();
                    data[2] = (uint8_t)stream.ReadByte();
                    data[3] = (uint8_t)stream.ReadByte();
                    glm::vec3 biTangent((float)data[2], (float)data[1], (float)data[0]);
                    biTangent = (biTangent / 255.0f) * 2.0f - 1.0f;
                    m_BiTangents.Add(ToLib(biTangent));
                }
                else
                {
                    m_Tangents.Emplace().ReadFromStream(stream);
                    m_BiTangents.Emplace().ReadFromStream(stream);
                }
            }

            if ((m_Flags & EVBUFFlags::Color) != 0)
            {
                m_Colors.Emplace().ReadFromStream(stream);
            }

            if ((m_Flags & EVBUFFlags::StaticLighting) != 0)
            {
                m_Colors.Emplace().ReadFromStream(stream);
            }

            if ((m_Flags & EVBUFFlags::TexCoord) != 0)
            {
                if ((m_Flags & EVBUFFlags::TexCoordCompressed) != 0)
                {
                    uint16_t data[2];
                    data[0] = stream.ReadByte();
                    data[1] = stream.ReadByte();
                    glm::vec2 uv((float)data[0], (float)data[1]);
                    uv = uv / 2048.0f;
                    m_TexCoords.Add(ToLib(uv));
                }
                else
                {
                    m_TexCoords.Emplace().ReadFromStream(stream);
                }
            }
            if (m_Stride > 0) stream.SetPosition(vertexStart + m_Stride); // M459
        }

        fileStream.SetPosition(chunkEnd);
        BaseChunk::EnsureEnd(fileStream);
    }

    String VBUF::ToString() const
    {
        std::string result = "Count = " + std::to_string(m_Count) + "\n";
        result += "Stride = " + std::to_string(m_Stride) + "\n";
        result += "Flags = " + std::string(VBUFFlagsToString(m_Flags).Buffer()) + "\n";
        result += "Positions[" + std::to_string(m_Positions.Size()) + "]\n";
        result += "Normals[" + std::to_string(m_Normals.Size()) + "]\n";
        result += "Tangents[" + std::to_string(m_Tangents.Size()) + "]\n";
        result += "BiTangents[" + std::to_string(m_BiTangents.Size()) + "]\n";
        result += "Colors[" + std::to_string(m_Colors.Size()) + "]\n";
        result += "Tex Coords[" + std::to_string(m_TexCoords.Size()) + "]\n";
        result += "\n";

        //result += "Positions["+std::to_string(m_Positions.Size())+"] = [";
        //for (uint32_t i = 0; i < m_Positions.Size(); ++i)
        //{
        //    result += m_Positions[i].ToString().Buffer() + std::string(", ");
        //    if ((i + 1) % 3 == 0)
        //    {
        //        result += "\n";
        //    }
        //}
        //result.resize(result.size() - 2);
        //result += "]";

        return result.c_str();
    }
}
