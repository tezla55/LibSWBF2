#ifdef __vita__
#include <psp2/io/fcntl.h> // M389: native sceIo reads
#endif
#include <LibSWBF2/pch.h>
//#ifdef MEMORY_MAPPED_READER
#include <LibSWBF2/IO/MemoryMappedReader.h>
#include "InternalHelpers.h"

#if !defined(WIN32) && !defined(__vita__)

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <errno.h>

#endif

#include <limits>
#include <memory>
#include <filesystem>
#include <iostream>
#include <cstdio>


namespace fs = std::filesystem;

namespace LibSWBF2
{
	using LibSWBF2::Logging::Logger;

	MemoryMappedReader::MemoryMappedReader()
	{
		m_LatestChunkPos = 0;
	    p_ReaderHead = nullptr;
	    p_MappingBase = nullptr;
	}

	MemoryMappedReader::~MemoryMappedReader()
	{
		try { Close(); } catch (...) {/*Do nothing if no mapping is set...*/}
	}

	bool MemoryMappedReader::Open(const Types::String& File)
	{
		if (p_MappingBase != nullptr)
		{
			LOG_ERROR("Already reading file: {}!", m_FileName);
			return false;
		}

		fs::path path = File.Buffer();
		if (!fs::exists(path) || fs::is_directory(path))
		{
			LOG_ERROR("File '{}' could not be found!", File);
			return false;
		}
		   
		m_FileSize = fs::file_size(path);
		m_FileName = path.string().c_str();

#ifdef __vita__
		// MILESTONE 24 (2026-08-25): real seek-based streaming instead of
		// reading the whole file into one `new uint8_t[m_FileSize]`
		// allocation (the old approach here, kept working fine for small/
		// medium files but hard-failed with std::bad_alloc on the real
		// army data files, rep.lvl/147MB and cis.lvl/101MB, on real
		// hardware - see MemoryMappedReader.h's comment on p_VitaFile).
		// Real mmap() isn't practically available for arbitrary ux0:
		// files on Vita (no POSIX mmap backing sceIo), so this keeps the
		// file open and every read below goes through VitaRead()'s small
		// buffered window instead of a raw pointer - genuinely only
		// touches the bytes actually needed (plus a little read-ahead),
		// which is what makes Level::FromFile's real subLVLsToLoad
		// feature (see LVL.cpp) actually effective here: skipped sub
		// LVLs get SkipBytes()'d (a real fseek, not a pointer bump) and
		// their content is never read off the SD card at all.
		m_VitaFd = sceIoOpen(m_FileName.Buffer(), SCE_O_RDONLY, 0);
		if (m_VitaFd < 0)
		{
			LOG_ERROR("Failed to open file: {}!", m_FileName);
			return false;
		}
		m_VitaPos = 0;
		p_VitaBuf = new uint8_t[VITA_BUF_SIZE];
		m_VitaBufStart = 0;
		m_VitaBufLen = 0;
#elif !defined(WIN32)
		int fd = open(m_FileName.Buffer(), O_RDONLY);
		if (fd < 0)
		{
			LOG_ERROR("Failed to open file: {}!", m_FileName);
			return false;
		}

		p_MappingBase = (uint8_t *) mmap(0,m_FileSize,PROT_READ,MAP_FILE|MAP_PRIVATE,fd,0);
		close(fd);

	    if (p_MappingBase == MAP_FAILED)
		{
			LOG_ERROR("Memory map failed for file: {}!", m_FileName);
			return false;
	    }	
#else
		HANDLE fileHandle = INVALID_HANDLE_VALUE, fileMappingHandle = NULL;

		try {
			fileHandle = CreateFileA(m_FileName.Buffer(), GENERIC_READ, FILE_SHARE_READ,
				NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			if (fileHandle == INVALID_HANDLE_VALUE)
			{
				THROW("Failed to open file: {}!", m_FileName);
			}

			fileMappingHandle = CreateFileMapping(fileHandle, NULL, PAGE_READONLY, 0, 0, NULL);
			if (fileMappingHandle == NULL)
			{
				THROW("Failed to create file mapping object for file: {}!", m_FileName);
			}

			p_MappingBase = static_cast<uint8_t*>(MapViewOfFile(fileMappingHandle, FILE_MAP_READ, 0, 0, 0));
			if (p_MappingBase == nullptr)
			{
				THROW("Memory map failed for file: {}!", m_FileName);
			}
		}
		catch (LibException e)
		{
			LOG_ERROR("{}", e.what());
			if (fileMappingHandle != NULL) CloseHandle(fileMappingHandle);
			if (fileHandle != INVALID_HANDLE_VALUE) CloseHandle(fileHandle);

			return false;
		}

		CloseHandle(fileMappingHandle);
		CloseHandle(fileHandle);
#endif

		p_ReaderHead = p_MappingBase;

		LOG_INFO("File '{}' ({} bytes) successfully opened.", m_FileName, m_FileSize);
		return true;
	}


	// MILESTONE 24 follow-up (2026-08-25): buffered read primitive, Vita
	// only - see the comment on p_VitaBuf in the header for why. Serves
	// `len` bytes starting at the CURRENT m_VitaPos out of p_VitaBuf,
	// refilling (one real fread(), up to VITA_BUF_SIZE bytes starting at
	// the requested position) only when the request isn't already fully
	// covered by what's buffered. `len` bigger than the whole buffer
	// (large blobs - texture pixels, big string blocks) bypasses
	// buffering entirely and reads straight into `dst`, then invalidates
	// the buffer so the next small read starts a fresh window rather
	// than trusting stale/partial buffered bytes.
	void MemoryMappedReader::VitaRead(void* dst, size_t len)
	{
#ifdef __vita__ // M389: sceIo calls - never used on PC, but must compile there
		if (len > VITA_BUF_SIZE)
		{
			size_t got = 0;
			while (got < len)
			{
				const int r = sceIoPread(m_VitaFd, (uint8_t*)dst + got, (SceSize)(len - got), (SceOff)(m_VitaPos + got));
				if (r <= 0) break;
				got += (size_t)r;
			}
			(void)got; // best-effort - CheckGood() already validated len is within the file
			m_VitaPos += len;
			m_VitaBufLen = 0;
			return;
		}

		bool covered = m_VitaBufLen > 0
			&& m_VitaPos >= m_VitaBufStart
			&& (m_VitaPos + len) <= (m_VitaBufStart + m_VitaBufLen);

		if (!covered)
		{
			// The raw stdio position doesn't need to already match
			// m_VitaPos here - always seek explicitly before refilling,
			// since SkipBytes()/SetPosition() (both real fseeks against
			// m_VitaPos) can move m_VitaPos independently of wherever
			// p_VitaFile's own cursor last ended up.
			// M383: adaptive refill - see m_VitaRefill.
			const bool sequential = m_VitaBufLen > 0 && m_VitaPos == m_VitaBufStart + m_VitaBufLen;
			m_VitaRefill = sequential ? (m_VitaRefill * 2 > VITA_BUF_SIZE ? VITA_BUF_SIZE : m_VitaRefill * 2)
			                          : VITA_REFILL_MIN;
			size_t want = m_VitaRefill < len ? len : m_VitaRefill;
			if (want > VITA_BUF_SIZE) want = VITA_BUF_SIZE;
			m_VitaBufStart = m_VitaPos;
			m_VitaBufLen = 0;
			while (m_VitaBufLen < want)
			{
				const int r = sceIoPread(m_VitaFd, p_VitaBuf + m_VitaBufLen, (SceSize)(want - m_VitaBufLen), (SceOff)(m_VitaPos + m_VitaBufLen));
				if (r <= 0) break;
				m_VitaBufLen += (size_t)r;
			}
		}

		memcpy(dst, p_VitaBuf + (m_VitaPos - m_VitaBufStart), len);
		m_VitaPos += len;
#else
		(void)dst; (void)len;
#endif
	}

	ChunkHeader MemoryMappedReader::ReadChunkHeader(bool peek)
	{
		ChunkHeader value{};
		if (CheckGood(sizeof(ChunkHeader)))
		{
#ifdef __vita__
			size_t pos = m_VitaPos;
			VitaRead(&value, sizeof(value));

			if (peek)
			{
				m_VitaPos = pos;
			}
#else
			auto pos = p_ReaderHead - p_MappingBase;

			memcpy(&value, p_ReaderHead, sizeof(value));
			p_ReaderHead += sizeof(value);

			if (peek)
			{
				p_ReaderHead = p_MappingBase + pos;
			}
#endif
			m_LatestChunkPos = pos;
		}
		return value;
	}

	ChunkSize MemoryMappedReader::ReadChunkSize()
	{
		ChunkSize value = 0;
		if (CheckGood(sizeof(ChunkSize)))
		{
#ifdef __vita__
			VitaRead(&value, sizeof(value));
#else
			value = *((uint32_t *) p_ReaderHead);
			p_ReaderHead += sizeof(value);
#endif
		}
		return value;
	}

	uint8_t MemoryMappedReader::ReadByte()
	{
		uint8_t value = 0;
		if (CheckGood(sizeof(uint8_t)))
		{
#ifdef __vita__
			VitaRead(&value, sizeof(value));
#else
			value = *(p_ReaderHead++);
#endif
		}
		return value;
	}

	bool MemoryMappedReader::ReadBytes(uint8_t* data, size_t length)
	{
		if (CheckGood(length))
		{
#ifdef __vita__
			VitaRead(data, length);
#else
			memcpy(data, p_ReaderHead, length);
			p_ReaderHead += length;
#endif
			return true;
		}
		return false;
	}

	int32_t MemoryMappedReader::ReadInt32()
	{
		int32_t value = 0;
		if (CheckGood(sizeof(int32_t)))
		{
#ifdef __vita__
			VitaRead(&value, sizeof(value));
#else
			value = *((int32_t *) p_ReaderHead);
			p_ReaderHead+=sizeof(int32_t);
#endif
		}
		return value;
	}

	int16_t MemoryMappedReader::ReadInt16()
	{
		int16_t value = 0;
		if (CheckGood(sizeof(int16_t)))
		{
#ifdef __vita__
			VitaRead(&value, sizeof(value));
#else
			value = *((int16_t *) p_ReaderHead);
			p_ReaderHead+=sizeof(int16_t);
#endif
		}
		return value;
	}

	uint32_t MemoryMappedReader::ReadUInt32()
	{
		uint32_t value = 0;
		if (CheckGood(sizeof(uint32_t)))
		{
#ifdef __vita__
			VitaRead(&value, sizeof(value));
#else
			value = *((uint32_t *) p_ReaderHead);
			p_ReaderHead+=sizeof(uint32_t);
#endif
		}
		return value;
	}

	uint16_t MemoryMappedReader::ReadUInt16()
	{
		uint16_t value = 0;
		if (CheckGood(sizeof(uint16_t)))
		{
#ifdef __vita__
			VitaRead(&value, sizeof(value));
#else
			value = *((uint16_t *) p_ReaderHead);
			p_ReaderHead+=sizeof(uint16_t);
#endif
		}
		return value;
	}

	float MemoryMappedReader::ReadFloat()
	{
		float value = 0.0f;
		if (CheckGood(sizeof(float)))
		{
#ifdef __vita__
			VitaRead(&value, sizeof(value));
#else
			value = *((float *) p_ReaderHead);
			p_ReaderHead+=sizeof(float);
#endif
		}
		return value;
	}

	Types::String MemoryMappedReader::ReadString(size_t length)
	{
		Types::String value;
		if (CheckGood(length))
		{
			char* str = new char[length+1];
#ifdef __vita__
			VitaRead(str, length);
#else
			memcpy((char *) str, (char *) p_ReaderHead, length);
			p_ReaderHead += length;
#endif
			str[length] = 0;
			value = str;
			delete[] str;
		}
		return value;
	}

	Types::String MemoryMappedReader::ReadString()
	{
		char str[1024];
		uint8_t current = 1;
		for (uint16_t i = 0; CheckGood(1) && current != 0; ++i)
		{
			if (i >= 1024)
			{
				LOG_WARN("Reading null terminated string exceeded buffer size!");
				break;
			}
			current = ReadByte();
			str[i] = current;
		}
		return str;
	}

	void MemoryMappedReader::Close()
	{
#ifdef __vita__
		if (m_VitaFd < 0)
		{
			THROW("Nothing has been opened yet!");
		}
		sceIoClose(m_VitaFd);
		m_VitaFd = -1;
		m_VitaPos = 0;
		delete[] p_VitaBuf;
		p_VitaBuf = nullptr;
		m_VitaBufStart = 0;
		m_VitaBufLen = 0;
#else
		if (p_MappingBase == nullptr)
		{
			THROW("Nothing has been opened yet!");
		}
#if !defined(WIN32)
		munmap(p_MappingBase, m_FileSize);
#else
		UnmapViewOfFile(p_MappingBase);
#endif
#endif

		m_FileName = "";
		m_FileSize = 0;
		p_MappingBase = nullptr;
		p_ReaderHead = nullptr;
	}

	size_t MemoryMappedReader::GetPosition()
	{
#ifdef __vita__
		return m_VitaPos;
#else
		return (size_t) (p_ReaderHead - p_MappingBase);
#endif
	}

	void MemoryMappedReader::SetPosition(size_t NewPosition)
	{
		if (NewPosition < 0 || NewPosition > m_FileSize)
		{
			LOG_WARN("Cannot set read position to {:#x} because it is out of range! Range: 0x00 - {:#x}", NewPosition, m_FileSize);
			return;
		}

#ifdef __vita__
		// MILESTONE 55 (2026-08-26): removed the eager fseek() here - see
		// SkipBytes()'s own real comment below for the full reasoning.
		// VitaRead() (the ONLY place that ever actually reads bytes)
		// already unconditionally reseeks p_VitaFile to m_VitaPos before
		// any refill (its own comment: "SkipBytes()/SetPosition() ... can
		// move m_VitaPos independently of wherever p_VitaFile's own
		// cursor last ended up" - it was ALREADY written defensively
		// assuming this) - so this fseek() was 100% redundant work on
		// every single call, never relied on by anything else that reads
		// p_VitaFile's raw cursor directly.
		m_VitaPos = NewPosition;
#else
		p_ReaderHead = p_MappingBase + NewPosition;
#endif
	}

	size_t MemoryMappedReader::GetFileSize()
	{
		return m_FileSize;
	}

	bool MemoryMappedReader::CheckGood(size_t ReadSize)
	{
#ifdef __vita__
		if (m_VitaFd < 0)
		{
			THROW("Cant read {:#x} bytes, file not opened!", ReadSize);
		}

		if (m_VitaPos + ReadSize > m_FileSize)
		{
			THROW("Reading {:#x} bytes will end up out of file!  Current position: {:#x}  FileSize: {:#x}", ReadSize, m_VitaPos, m_FileSize);
		}
#else
		if (p_MappingBase == nullptr)
		{
			THROW("Cant read {:#x} bytes, memory mapping uninitialized!", ReadSize);
		}

		size_t current = (size_t) (p_ReaderHead - p_MappingBase);
		if (current + ReadSize > m_FileSize)
		{
			THROW("Reading {:#x} bytes will end up out of file!  Current position: {:#x}  FileSize: {:#x}", ReadSize, current, m_FileSize);
		}
#endif

		return true;
	}

	bool MemoryMappedReader::SkipBytes(const size_t& Amount)
	{
		if (CheckGood(Amount))
		{
#ifdef __vita__
			// MILESTONE 55 (2026-08-26): removed the eager fseek() here -
			// real, measured motivation (see VITA_BUF_SIZE's own comment
			// in the header for the full real numbers: ~17ms on PC vs.
			// ~17-20s on real Vita hardware for the SAME restricted load).
			// This is the single hottest call in exactly that traversal
			// pattern (one SkipBytes() per unwanted sub-LVL/chunk body,
			// and a multi-hundred-MB side file has a LOT of them) - and
			// VitaRead() (the only place that actually reads bytes)
			// ALREADY unconditionally reseeks p_VitaFile to m_VitaPos
			// before every refill regardless of what SkipBytes() did, so
			// this fseek() was pure redundant syscall overhead, doubling
			// the real seek count for every single skip. Just advancing
			// m_VitaPos (the logical position VitaRead() actually trusts)
			// is sufficient and correct.
			m_VitaPos += Amount;
#else
			p_ReaderHead += Amount;
#endif
			return true;
		}
		return false;
	}
}

//#endif
