
#pragma once
#include <LibSWBF2/Chunks/GenericChunk.h>
#include <LibSWBF2/req.h>
#include <unordered_set>

namespace LibSWBF2::Chunks::LVL::sound
{
	struct LIBSWBF2_API SampleBankData : public GenericChunk<"Data"_fnv>
	{
		void RefreshSize() override;
		void WriteToStream(FileWriter& stream) override;
		void ReadFromStream(FileReader& stream) override;

		// M249: real, narrowly-scoped memory fix. A real sample bank
		// (BF2's own geo.lvl: 573 real clips, ~15MB of PCM) is read
		// UNCONDITIONALLY, every clip's data allocated+copied, whether
		// the caller wants it or not - fine on PC, but this Vita port's
		// "user" heap was already down to ~12-15MB free by the time
		// sound loading runs (confirmed via this project's own logMemory
		// calls), so loading the whole real bank here left every single
		// clip with no data at all (an allocation/read that silently
		// came up empty under real memory pressure, not a crash).
		//
		// When non-null, ONLY clips whose real name hash is IN this set
		// get their data actually read; every other clip is skipped via
		// SkipBytes (the stream position still advances correctly - a
		// real, cheap seek, not a broken read). nullptr (the default)
		// preserves the exact original behavior - every clip loaded,
		// unconditionally - so this is opt-in and changes nothing for
		// any existing caller that never sets it.
		static std::unordered_set<FNVHash>* s_LoadFilter;
	};
}
