#pragma once
#include <LibSWBF2/Chunks/HeaderNames.h>
#include <LibSWBF2/Types/LibString.h>
#include <fstream>
#include <atomic>
#include <cstdio>


#include <LibSWBF2/IO/FileReader.h>


namespace LibSWBF2
{
	class MemoryMappedReader : public FileReader
	{
	public:
		MemoryMappedReader();
		~MemoryMappedReader();

		bool Open(const Types::String& File);
		ChunkHeader ReadChunkHeader(bool peek);
		ChunkSize ReadChunkSize();
		uint8_t ReadByte();
		bool ReadBytes(uint8_t* data, size_t length);
		int32_t ReadInt32();
		int16_t ReadInt16();
		uint32_t ReadUInt32();
		uint16_t ReadUInt16();
		float ReadFloat();
		Types::String ReadString(size_t length);
		Types::String ReadString(); // will read until the first null terminator
		size_t GetPosition();
		void SetPosition(size_t NewPosition);
		size_t GetFileSize();
		bool SkipBytes(const size_t& Amount);
		void Close();

		bool CheckGood(size_t ReadSize);

	private:

		uint8_t* p_ReaderHead = nullptr;
		uint8_t* p_MappingBase = nullptr;

		// MILESTONE 24 (2026-08-25, Vita port): real seek-based streaming
		// I/O, Vita-only (see MemoryMappedReader.cpp::Open() for why -
		// the previous Vita path read the ENTIRE file into a
		// `new uint8_t[m_FileSize]` buffer up front, defeating
		// Level::FromFile's real subLVLsToLoad selective-loading feature
		// for any file too big to fit whole in memory, which is exactly
		// the situation for rep.lvl/cis.lvl - confirmed via std::bad_alloc
		// on real hardware even when restricted to load 2-3 tiny models).
		// p_MappingBase/p_ReaderHead above are unused on Vita; every
		// method has a parallel #ifdef __vita__ branch using these
		// instead, scoped to Vita only - no other platform's behavior
		// changes.
		// M389 (swbf2render load time): a native sceIo descriptor, not a
		// stdio FILE*. Device probe (M388): the same 18.8MB file read in 2MB
		// chunks - fread 3.8 MB/s, sceIoRead 12.3 MB/s. Every read is one
		// sceIoPread at the wanted offset (no separate seek).
		int m_VitaFd = -1;
		size_t m_VitaPos = 0;

		// MILESTONE 24 follow-up (2026-08-25): a small fixed-size
		// read-ahead buffer - the first version of the Vita path above
		// issued one fread() PER PRIMITIVE (often just 1-4 bytes), which
		// worked and fixed the memory problem, but real hardware showed
		// it made every .lvl load dramatically slower (a 6.3MB file that
		// used to take ~4s took ~58s) - each fread() is a real sceIo
		// syscall against the memory card, and a chunk-heavy file issues
		// a LOT of tiny reads walking the hierarchy. Standard buffered-
		// I/O fix: read a VITA_BUF_SIZE chunk at a time into p_VitaBuf,
		// serve small reads out of it, only refill (another real
		// fread()) when a request falls outside what's currently
		// buffered. Bounded, small, constant-size memory (not
		// proportional to file size, so it doesn't reopen the original
		// bad_alloc problem) - see VitaRead() in the .cpp.
		//
		// MILESTONE 55 (2026-08-26): bumped 64KB -> 2MB. Real, measured
		// motivation: a restricted (subLVLsToLoad) load of rep.lvl
		// (147MB) took ~17ms on PC (fast, effectively free - confirmed
		// via a native timing tool) but ~17-20 REAL SECONDS on real Vita
		// hardware for the exact same operation - a ~1000x gap that
		// isn't explained by genuine parsing cost (the PC number proves
		// the actual wanted data is tiny), so it's near-certainly real
		// I/O overhead: SD2Vita-class adapters have real, meaningfully
		// higher PER-SEEK/PER-READ latency than typical flash storage,
		// and scanning a huge multi-hundred-sub-LVL container even just
		// to read each one's small header before skipping its (possibly
		// huge) body means a LOT of small refills at 64KB. A much bigger
		// read-ahead window means far fewer real fread() syscalls to
		// walk the same header-heavy structure - a real, bounded
		// (constant, not proportional to file size) memory cost, still
		// trivially small against this project's own 200MB heap ceiling
		// (M33) even with a couple of Levels open in sequence.
		static const size_t VITA_BUF_SIZE = 2 * 1024 * 1024;
		uint8_t* p_VitaBuf = nullptr;
		size_t m_VitaBufStart = 0; // file offset the buffer starts at
		size_t m_VitaBufLen = 0;   // valid bytes currently in the buffer
		// M383 (swbf2render load time): the size of the NEXT refill. A
		// refill that continues exactly where the buffer ended doubles it
		// (sequential reading ramps back to VITA_BUF_SIZE in a few steps);
		// a refill after a jump (a skipped sub-LVL) starts again at
		// VITA_REFILL_MIN. Device log M382: a restricted rep.lvl open for
		// ONE class took 22s - every skipped sub-LVL was followed by a 2MB
		// refill just to read the next 8-byte chunk header, i.e. most of
		// the 141MB file was read to skip it.
		static const size_t VITA_REFILL_MIN = 64 * 1024;
		size_t m_VitaRefill = VITA_REFILL_MIN;

		void VitaRead(void* dst, size_t len);
	};
}
