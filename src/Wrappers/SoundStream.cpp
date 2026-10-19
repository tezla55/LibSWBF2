#include <LibSWBF2/pch.h>
#include <LibSWBF2/Wrappers/SoundStream.h>
#include <LibSWBF2/Chunks/LVL/sound/Stream.h>
#include <LibSWBF2/Chunks/LVL/sound/StreamInfo.h>

#include <LibSWBF2/Audio/IMAADPCMDecoder.h>
#include <LibSWBF2/Audio/PCM16Decoder.h>

#include <LibSWBF2/IO/FileReader.h>

#include "InternalHelpers.h"
#include <LibSWBF2/Types/SoundClip.h>
#include <LibSWBF2/Hashing.h>
#include <unordered_map>
#include <iostream>

namespace LibSWBF2::Wrappers
{
	using Types::SoundClip;


	SoundStream::~SoundStream()
	{
		delete p_Decoder;
	}

	// M249: see this class's own header comment for the real bug these
	// four fixed. Rebuilds p_Decoder from p_StreamChunk's own real format
	// data - the same real construction FromChunk (below) already does -
	// rather than attempting to clone SoundDecoder's abstract internals.
	static SoundDecoder* RebuildDecoderFor(const Stream* streamChunk)
	{
		if (streamChunk == nullptr || streamChunk->p_Info == nullptr) return nullptr;
		const auto format = streamChunk->p_Info->m_Format;
		if (format == ESoundFormat::IMAADPCM)
			return new IMAADPCMDecoder(streamChunk->p_Info->m_NumChannels, streamChunk->p_Info->m_ChannelInterleave);
		if (format == ESoundFormat::PCM16)
			return new PCM16Decoder();
		return nullptr;
	}

	SoundStream::SoundStream(const SoundStream& other)
		: p_StreamChunk(other.p_StreamChunk), m_Sounds(other.m_Sounds),
		  p_Reader(other.p_Reader), p_StreamBuffer(other.p_StreamBuffer),
		  m_SizeStreamBuffer(other.m_SizeStreamBuffer),
		  p_CurrentSegment(other.p_CurrentSegment),
		  m_StreamBufferReadOffset(other.m_StreamBufferReadOffset),
		  m_StreamBufferDataSize(other.m_StreamBufferDataSize),
		  m_SegmentOffset(other.m_SegmentOffset),
		  p_Decoder(RebuildDecoderFor(other.p_StreamChunk)),
		  p_NameToIndexMaps(other.p_NameToIndexMaps)
	{
	}

	SoundStream& SoundStream::operator=(const SoundStream& other)
	{
		if (this == &other) return *this;
		p_StreamChunk = other.p_StreamChunk;
		m_Sounds = other.m_Sounds;
		p_Reader = other.p_Reader;
		p_StreamBuffer = other.p_StreamBuffer;
		m_SizeStreamBuffer = other.m_SizeStreamBuffer;
		p_CurrentSegment = other.p_CurrentSegment;
		m_StreamBufferReadOffset = other.m_StreamBufferReadOffset;
		m_StreamBufferDataSize = other.m_StreamBufferDataSize;
		m_SegmentOffset = other.m_SegmentOffset;

		delete p_Decoder;
		p_Decoder = RebuildDecoderFor(other.p_StreamChunk);
		p_NameToIndexMaps = other.p_NameToIndexMaps;
		return *this;
	}

	SoundStream::SoundStream(SoundStream&& other) noexcept
		: p_StreamChunk(other.p_StreamChunk), m_Sounds(std::move(other.m_Sounds)),
		  p_Reader(other.p_Reader), p_StreamBuffer(other.p_StreamBuffer),
		  m_SizeStreamBuffer(other.m_SizeStreamBuffer),
		  p_CurrentSegment(other.p_CurrentSegment),
		  m_StreamBufferReadOffset(other.m_StreamBufferReadOffset),
		  m_StreamBufferDataSize(other.m_StreamBufferDataSize),
		  m_SegmentOffset(other.m_SegmentOffset),
		  p_Decoder(other.p_Decoder),
		  p_NameToIndexMaps(other.p_NameToIndexMaps)
	{
		other.p_Decoder = nullptr;
		other.p_NameToIndexMaps = nullptr;
	}

	SoundStream& SoundStream::operator=(SoundStream&& other) noexcept
	{
		if (this == &other) return *this;
		p_StreamChunk = other.p_StreamChunk;
		m_Sounds = std::move(other.m_Sounds);
		p_Reader = other.p_Reader;
		p_StreamBuffer = other.p_StreamBuffer;
		m_SizeStreamBuffer = other.m_SizeStreamBuffer;
		p_CurrentSegment = other.p_CurrentSegment;
		m_StreamBufferReadOffset = other.m_StreamBufferReadOffset;
		m_StreamBufferDataSize = other.m_StreamBufferDataSize;
		m_SegmentOffset = other.m_SegmentOffset;

		delete p_Decoder;
		p_Decoder = other.p_Decoder;
		other.p_Decoder = nullptr;
		p_NameToIndexMaps = other.p_NameToIndexMaps;
		other.p_NameToIndexMaps = nullptr;
		return *this;
	}


	bool SoundStream::SetFileReader(FileReader * reader)
	{
		p_Reader = reader;

		if (p_CurrentSegment != nullptr)
		{
			p_Reader -> SetPosition(p_CurrentSegment -> GetDataPosition());
		}

		return true;
	}

	bool SoundStream::SetFileStreamBuffer(uint8_t * buffer, size_t NumBytes)
	{
		p_StreamBuffer = buffer;
		m_SizeStreamBuffer = NumBytes;
		m_StreamBufferReadOffset = 0;
		m_StreamBufferDataSize = 0;

		return true;
	}

	int32_t SoundStream::ReadBytesFromStream(int32_t NumBytesToRead)
	{
		if (p_StreamBuffer == nullptr)
		{
			LOG_ERROR("Cannot read encoded bytes, stream buffer is not set!");
			return -1;
		}

		if (p_Reader == nullptr)
		{
			LOG_ERROR("Cannot read encoded bytes, stream reader is not set!");
			return -1;
		}

		if (p_CurrentSegment == nullptr)
		{
			LOG_ERROR("Cannot read encoded bytes, the segment to be read is not set!");
			return -1;
		}


		if (NumBytesToRead > m_SizeStreamBuffer)
		{
			NumBytesToRead = m_SizeStreamBuffer;
		}

		if (p_Reader != nullptr && p_CurrentSegment != nullptr)
		{
			size_t BytesLeft = p_CurrentSegment -> GetDataLength() - m_SegmentOffset;
			NumBytesToRead = NumBytesToRead > BytesLeft ? BytesLeft : NumBytesToRead;

			p_Reader -> ReadBytes(p_StreamBuffer, NumBytesToRead);

			m_SegmentOffset += NumBytesToRead;
			m_StreamBufferDataSize = NumBytesToRead;
			m_StreamBufferReadOffset = 0;

			return NumBytesToRead;  
		}
		else 
		{
			return 0;
		}
	}


    int32_t SoundStream::GetNumSamplesInBytes(int32_t NumBytes)
    {
    	return p_Decoder -> SamplesInBytes(NumBytes);
    }


    int32_t SoundStream::BytesLeftInSegment()
    {
    	return p_CurrentSegment == nullptr ? -1 : p_CurrentSegment -> GetDataLength() - m_SegmentOffset;
    }


    int32_t SoundStream::ReadSamples(void * samplesBuffer, size_t samplesBufferLength,
        					size_t numSamplesToRead, ESoundFormat format)
    {
    	int32_t NumSamplesRead = 0;
    	int32_t NumBytesReadFromBuffer = 0;
    	int32_t NumBytesLeftInBuffer = 0;

    	size_t BytesRead;


    	// MILESTONE 273 (real bug fix, SWBF2 Vita port): the old condition
    	// - `BytesLeftInSegment() > 0` alone - conflates "more encoded bytes
    	// remain to be FETCHED FROM DISK" with "more data remains to be
    	// DECODED". ReadBytesFromStream() below advances m_SegmentOffset (and
    	// therefore BytesLeftInSegment()) by the FULL amount just fetched
    	// into the buffer the moment a refill happens, not by how much of
    	// that buffered data has actually been consumed by the decoder yet.
    	// So once a refill happens to be the LAST one for a segment (either
    	// because the caller's own stream buffer is smaller than the
    	// segment, so the final refill drains the disk-side remainder in one
    	// go, or because the buffer is bigger than the WHOLE segment, so the
    	// very FIRST refill already does), BytesLeftInSegment() drops to 0
    	// immediately - and this loop's own condition then refuses to keep
    	// decoding the data that refill already fetched into
    	// p_StreamBuffer, even though most of it is still sitting there
    	// un-decoded. Confirmed natively (LibSWBF2/tools/check_vo_decode.cpp)
    	// against a real 51712-sample VO segment: a small stream buffer
    	// stopped at 32768 samples (lost the last ~1s), and a stream buffer
    	// bigger than the whole segment stopped at just 4096 samples (lost
    	// almost the entire real line) - both premature "we're done" reads
    	// of a signal that only ever meant "no more DISK fetches will
    	// happen". The real, correct condition is "there is more to decode
    	// AND/OR more to fetch" - i.e. also keep going while the buffer
    	// itself still holds undecoded bytes from an earlier refill,
    	// regardless of what BytesLeftInSegment() reports.
    	while (NumSamplesRead < numSamplesToRead &&
    	       ((int32_t)m_StreamBufferDataSize - (int32_t)m_StreamBufferReadOffset > 0 || BytesLeftInSegment() > 0))
    	{
    		NumBytesLeftInBuffer = m_StreamBufferDataSize - m_StreamBufferReadOffset;

    		if (NumBytesLeftInBuffer <= 0)
    		{
    			ReadBytesFromStream(m_SizeStreamBuffer);
	    		NumBytesLeftInBuffer = m_StreamBufferDataSize - m_StreamBufferReadOffset;
	    		if (NumBytesLeftInBuffer <= 0) break; // real end of segment - refill genuinely produced nothing more
    		}

    		int32_t NumSamplesDecoded;

	    	if (format == ESoundFormat::Unity)
	    	{
				NumSamplesDecoded = p_Decoder -> DecodeAndFillUnity(p_StreamBuffer + m_StreamBufferReadOffset, NumBytesLeftInBuffer, (float *) samplesBuffer + NumSamplesRead, (size_t) numSamplesToRead - NumSamplesRead, BytesRead);
	    	}
	    	else if (format == ESoundFormat::PCM16)
	    	{
				NumSamplesDecoded = p_Decoder -> DecodeAndFillPCM16(p_StreamBuffer + m_StreamBufferReadOffset, NumBytesLeftInBuffer, (int16_t *) samplesBuffer + NumSamplesRead, (size_t) numSamplesToRead - NumSamplesRead, BytesRead);
	    	}
	    	else 
	    	{
	    		return -1;
	    	}

	    	NumSamplesRead += NumSamplesDecoded;

	    	m_StreamBufferReadOffset += BytesRead;
    	}

    	return NumSamplesRead;
    }




    /*
    int32_t SoundStream::ReadSamplesFromBytes(void * samplesBuffer, size_t samplesBufferLength, 
        							size_t numBytesToRead, ESoundFormat format, int32_t &bytesRead)
    {    	
    	if (format == ESoundFormat::Unity)
    	{
			return DecodeAndFillUnity(p_StreamBuffer, numBytesToRead, (float *) samplesBuffer, samplesBufferLength, bytesRead);
    	}
    	else if (format == ESoundFormat::PCM16)
    	{
			return DecodeAndFillPCM16(p_StreamBuffer, numBytesToRead, (int16_t *) samplesBuffer, samplesBufferLength, bytesRead);
    	}
    	else 
    	{
    		return -1;
    	}
    }
    */




    bool SoundStream::SetSegment(FNVHash segmentNameHash)
    {
    	if (p_Reader == nullptr)
    	{	
    		LOG_ERROR("Cannot set segment, reader is unassigned!");
    		return false;
    	}

    	if (p_StreamBuffer == nullptr)
    	{
    		LOG_ERROR("Cannot set segment, stream buffer is unassigned!");
    		return false;
    	}

    	auto iter = p_NameToIndexMaps -> SoundHashToIndex.find(segmentNameHash);
    	if (iter == p_NameToIndexMaps -> SoundHashToIndex.end())
    	{
    		p_CurrentSegment = nullptr;
    		return false;
    	}
    	else 
    	{
    		p_CurrentSegment = &(m_Sounds[iter -> second]);
    		m_SegmentOffset = 0;
    		m_StreamBufferDataSize = 0;
    		m_StreamBufferReadOffset = 0;

    		p_Decoder -> Reset();

    		p_Reader -> SetPosition(p_CurrentSegment -> GetDataPosition());

    		return true;
    	}
    }



	bool SoundStream::FromChunk(Stream* streamChunk, SoundStream& out)
	{
		if (streamChunk == nullptr)
		{
			LOG_ERROR("Given Stream chunk was NULL!");
			return false;
		}

		out.p_StreamChunk = streamChunk;
		out.p_NameToIndexMaps = new SoundMapsWrapper();

		for (int i = 0; i < streamChunk -> p_Info -> m_NumSubstreams; i++)
		{
			auto format = streamChunk -> p_Info -> m_Format;
			if (format == ESoundFormat::IMAADPCM)
			{
				out.p_Decoder = new IMAADPCMDecoder(streamChunk -> p_Info -> m_NumChannels, streamChunk -> p_Info -> m_ChannelInterleave);
			}
			else if (format == ESoundFormat::PCM16)
			{
				out.p_Decoder = new PCM16Decoder();
			}
		}


		List<SoundClip>& clips = streamChunk -> p_Info -> m_SoundHeaders;
		for (size_t i = 0; i < clips.Size(); ++i)
		{
			Sound sound;
			if (Sound::FromSoundClip(&clips[i], sound))
			{
				sound.m_Format = streamChunk -> p_Info -> m_Format;
				sound.m_NumChannels = streamChunk -> p_Info -> m_NumChannels;
				size_t index = out.m_Sounds.Add(sound);
				out.p_NameToIndexMaps->SoundHashToIndex.emplace(clips[i].m_NameHash, index);
			}
		}

		return true;
	}


	FNVHash SoundStream::GetHashedName() const
	{
		return p_StreamChunk -> p_Info -> m_Name;
	}

	ESoundFormat SoundStream::GetFormat() const
	{
		return p_StreamChunk -> p_Info -> m_Format;
	}

	uint32_t SoundStream::GetNumChannels() const
	{
		return p_StreamChunk -> p_Info -> m_NumChannels;
	}


	bool SoundStream::HasSegment(FNVHash segmentName) const
	{
		// MILESTONE 255: real bug, found while implementing real map
		// ambience playback - this returned true exactly when the
		// segment was NOT found (inverted comparison). Confirmed against
		// this project's own check_sound_data ("geo1cw" - not a real
		// segment name - reported hasSegmentSelf=1) before touching
		// anything, then fixed.
		auto it = p_NameToIndexMaps->SoundHashToIndex.find(segmentName);
		return it != p_NameToIndexMaps->SoundHashToIndex.end();
	}


	bool SoundStream::HasData() const
	{
		return p_StreamChunk -> p_Data != nullptr;
	}

	
	uint32_t SoundStream::GetNumSubstreams() const
	{
		return p_StreamChunk -> p_Info -> m_NumSubstreams;	
	}
	

	uint32_t SoundStream::GetSubstreamInterleave() const
	{
		return p_StreamChunk -> p_Info -> m_SubstreamInterleave;
	}
	

	uint32_t SoundStream::GetChannelInterleave() const
	{
		return p_StreamChunk -> p_Info -> m_ChannelInterleave;
	}


	const List<Sound>& SoundStream::GetSounds() const
	{
		return m_Sounds;
	}

	const Sound* SoundStream::GetSound(const String& soundName) const
	{
		if (soundName.IsEmpty())
		{
			return nullptr;
		}

		return GetSound(FNV::Hash(soundName));
	}

	const Sound* SoundStream::GetSound(FNVHash soundHash) const
	{
		auto it = p_NameToIndexMaps->SoundHashToIndex.find(soundHash);
		if (it != p_NameToIndexMaps->SoundHashToIndex.end())
		{
			return &m_Sounds[it->second];
		}

		//LOG_WARN("Could not find Sound '{}'!", soundHash);
		return nullptr;
	}
}
