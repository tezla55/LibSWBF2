#pragma once
#include <LibSWBF2/Logging/LoggerEntry.h>
#include <LibSWBF2/Types/LibString.h>
#include <LibSWBF2/Chunks/HeaderNames.h>
#include <LibSWBF2/Chunks/BaseChunk.h>
#include <memory>
#include <queue>
#include <mutex>


namespace LibSWBF2
{
	class FileWriter;
}

namespace LibSWBF2::Logging
{
	// Singleton
	class Logger
	{
	public:
		Logger(); // do not call anywhere else
		~Logger();

		static std::unique_ptr<Logger>& GetInstance();

		LIBSWBF2_API static void SetLogfileLevel(const ELogType LogfileLevel);
		LIBSWBF2_API static bool GetNextLog(LoggerEntry& nextLogEntry);

		void Log(const std::string &message, const ELogType level, const unsigned long line, const char* file);

	private:
		// VITA FIX 2026-08-24: this was a bare relative path ("LibSWBF2.log",
		// no device prefix) on every platform. Works by accident on PC (just
		// resolves relative to the process's cwd) but is not a valid Vita
		// path at all - sceIoOpen has no concept of a relative cwd the way
		// POSIX does, every path needs an explicit device prefix
		// (ux0:/app0:/etc). The constructor's `m_Writer->Open(LOG_FILE, true)`
		// silently fails on Vita and leaves m_Writer unopened - harmless by
		// itself, since Log() only actually tries to write for Warning/Error-
		// level messages (m_LogfileLevel defaults to Warning) - but the FIRST
		// time ANY chunk anywhere in a parse hits a LOG_WARN or LOG_ERROR
		// (e.g. SKIN.cpp's "Ignore yet unsupported SKIN type" warning, which
		// a real character's skin chunk can genuinely hit), FileWriter::
		// WriteLine() -> CheckGood() throws "File '' is not open!" - an
		// uncaught exception with NO connection to whatever content actually
		// triggered the warning, that looked for a long time like a mysterious,
		// content-specific parsing bug (it isn't - it's this). Confirmed via a
		// real crash dump pulled off Vita hardware, several hours of tracing
		// every FileWriter/WriteToStream call site in the codebase (found
		// none reachable from a normal load), and a PC-side reproduction
		// attempt that (correctly) never reproduced it, since PC's relative-
		// path Open() actually succeeds. Root cause only became clear on
		// reading Logger's OWN constructor, not any of the chunk-parsing code
		// that was actually throwing.
#ifdef __vita__
		const String LOG_FILE = "ux0:data/swbf2/LibSWBF2.log";
#else
		const String LOG_FILE = "LibSWBF2.log";
#endif
		const int MAX_ENTRIES = 10000;

	public:
		// M392 (swbf2render load time): true when a message of this level
		// would be written - the LOG_* macros ask BEFORE formatting, so a
		// discarded message costs nothing. Device: 6315 warning lines per
		// load, each formatted, written and flushed to the memory card.
		static bool Wants(const ELogType level) { return level >= GetInstance()->m_LogfileLevel; }
	private:
		static std::unique_ptr<Logger> m_Instance;
		FileWriter* m_Writer;
		ELogType m_LogfileLevel = ELogType::Warning;
		std::queue<LoggerEntry> m_Logs;
		std::mutex m_Lock;
	};
}
