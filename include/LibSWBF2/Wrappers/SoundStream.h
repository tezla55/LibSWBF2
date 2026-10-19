#pragma once
#include <LibSWBF2/Types/SoundClip.h>
#include <LibSWBF2/Wrappers/Sound.h>
#include <LibSWBF2/Audio/SoundDecoder.h>
#include <LibSWBF2/Types/LibString.h>
#include <LibSWBF2/Types/List.h>


namespace LibSWBF2
{
	class Container;

	namespace Chunks::LVL::sound
	{
		struct Stream;
	}	
}



namespace LibSWBF2::Wrappers
{
	using LibSWBF2::Types::String;
	using LibSWBF2::Types::List;
	using LibSWBF2::Chunks::LVL::sound::Stream;

	class Level;

	class LIBSWBF2_API SoundStream
	{
	private:
		friend Level;
		friend List<SoundStream>;

		SoundStream() = default;
		~SoundStream();

		// M249: same real bug class as SoundClip (see its own header
		// comment for the full citation) - SoundStream owns `p_Decoder`
		// and frees it in its destructor, but declared no copy
		// constructor/assignment, so List<SoundStream>::Resize() (which
		// every real level with enough streams to outgrow the list's
		// initial capacity - global.lvl, thousands of real VO/music
		// streams - actually exercises) shallow-copied the pointer and
		// then double-freed it. `p_Decoder` is safely reconstructible
		// from `p_StreamChunk`'s own real format/channel/interleave data
		// (the exact same real construction FromChunk already does), so
		// the copy just re-derives it rather than needing to clone
		// SoundDecoder's abstract, stateful internals. `p_Reader`/
		// `p_StreamBuffer` are caller-owned (SetFileReader/
		// SetFileStreamBuffer's own doc comments), never freed by this
		// class, so copying the raw pointer value for those is correct,
		// not an oversight.
		SoundStream(const SoundStream& other);
		SoundStream& operator=(const SoundStream& other);
		SoundStream(SoundStream&& other) noexcept;
		SoundStream& operator=(SoundStream&& other) noexcept;

		Stream* p_StreamChunk;

		List<Sound> m_Sounds;

		/*
		File stream reader and buffer fields
		*/

		FileReader * p_Reader = nullptr;
		uint8_t * p_StreamBuffer = nullptr;
		size_t m_SizeStreamBuffer;

		/*
		Fields relating stream/buffer to current segment properties
		*/

		Sound * p_CurrentSegment = nullptr;
		size_t m_StreamBufferReadOffset;
		size_t m_StreamBufferDataSize;
		size_t m_SegmentOffset;

		/*
		Decoder, there was originally one for each substream,
		but soundmunge exe says substream interleave will never interrupt a
		IMAADPCM block, so one will do. 
		*/

		SoundDecoder * p_Decoder = nullptr;

		class SoundMapsWrapper* p_NameToIndexMaps;



	public:

		// Caller must provide stream (any subclass of FileReader) and pre-allocate/provide...
		bool SetFileReader(FileReader * reader);

		// the stream buffer and...
		//
		// MILESTONE 255: real, load-bearing constraint, found from a real
		// user report of streamed audio going from clean to totally
		// garbled partway through playback, root-caused natively
		// (LibSWBF2/tools/check_chunked.cpp) - `numBytes` MUST be an
		// exact multiple of the real IMA-ADPCM block size for this
		// stream (channels * 36 bytes; e.g. 72 for stereo). This class
		// always refills exactly `numBytes` bytes on each internal
		// ReadBytesFromStream() call with no awareness of where a real
		// block boundary falls - a refill that lands mid-block permanently
		// desyncs IMAADPCMDecoder's block-header detection from the real
		// bitstream from that point on (clean audio right up to the
		// first misaligned refill, garbage after). Round the caller's
		// desired buffer size DOWN to a multiple of the block size
		// before calling this - e.g. `(64*1024 / blockSize) * blockSize`.
		// Not enforced here (no LOG_WARN/assert) - a real gap, flagged
		// rather than silently fixed, since a genuinely PCM16 stream has
		// no such constraint at all and this function has no format
		// context of its own to tell the difference.
		bool SetFileStreamBuffer(uint8_t * buffer, size_t numBytes);

		// identify the specific segment to be read before calling...
		bool SetSegment(FNVHash segmentNameHash);

		// ReadBytesFromStream, which puts the encoded bytes in the stream buffer, after which
		// the final call to ReadSamples can proceed.
		int32_t ReadBytesFromStream(int32_t NumBytes);

		// Complicated, but I figured these params would need to be experimented with a lot
		// from C# so I wanted to keep all this exposed until we nail down exactly what 
		// buffering behavior is best in Unity.
		// Plus, we know from the Lua API that only one stream can be played from a given file
		// at any time, so it makes sense to give the option of sharing one stream and buffer
		// between multiple SoundStreams of the same file... 
		int32_t ReadSamples(void * samplesBuffer, size_t samplesBufferLength, 
		size_t numSamplesToRead, ESoundFormat format);

		int32_t BytesLeftInSegment();

		/*
		// This method might not be needed, it depends on whether or not we
		// encounter other formats or if IMAADPCM block boundaries can be violated...
		int32_t ReadSamplesFromBytes(void * samplesBuffer, size_t samplesBufferLength, 
									size_t numBytesToRead, ESoundFormat format, int32_t &bytesRead);
		*/

		int32_t GetNumSamplesInBytes(int32_t NumBytes);
		static bool FromChunk(Stream* stream, SoundStream& out);

		FNVHash GetHashedName() const;
		ESoundFormat GetFormat() const;
		uint32_t GetNumChannels() const;
		uint32_t GetNumSubstreams() const;
		uint32_t GetSubstreamInterleave() const;
		uint32_t GetChannelInterleave() const;

		bool HasData() const;
		bool HasSegment(FNVHash segmentName) const;

		const List<Sound>& GetSounds() const;
		const Sound* GetSound(const String& soundName) const;
		const Sound* GetSound(FNVHash soundHash) const;
	};
}
