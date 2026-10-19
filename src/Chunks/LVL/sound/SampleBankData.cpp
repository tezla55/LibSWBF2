
#include <LibSWBF2/pch.h>
#include <LibSWBF2/Chunks/LVL/sound/SampleBankData.h>
#include <LibSWBF2/Chunks/LVL/sound/SampleBank.h>
#include <LibSWBF2/Chunks/LVL/sound/SampleBankInfo.h>
#include "InternalHelpers.h"
#include <LibSWBF2/IO/FileReader.h>


namespace LibSWBF2::Chunks::LVL::sound
{
	// M249: see this class's own header comment. nullptr by default -
	// every existing caller keeps loading every clip's data exactly as
	// before.
	std::unordered_set<FNVHash>* SampleBankData::s_LoadFilter = nullptr;

	void SampleBankData::RefreshSize()
	{
		THROW("Not implemented!");
	}

	void SampleBankData::WriteToStream(FileWriter& stream)
	{
		THROW("Not implemented!");
	}

	void SampleBankData::ReadFromStream(FileReader& stream)
	{
		BaseChunk::ReadFromStream(stream);
		Check(stream);

		SampleBank *bank = dynamic_cast<SampleBank*>(GetParent());
		if (bank == nullptr)
		{
			LOG_ERROR("Tried to read SampleBankData from a chunk other than SampleBank");
		}
		else
		{
			SampleBankInfo* info = bank -> p_Info;
			if (info == nullptr)
			{
				LOG_ERROR("Tried to read SampleBankData, but parent SampleBank's Info chunk was missing!");
			}
			else
			{
				for (int i = 0; i < info -> m_SoundHeaders.Size(); i++)
				{
					Types::SoundClip& currClip = info -> m_SoundHeaders[i];

					size_t EndPos = stream.GetPosition() + currClip.m_DataLength + currClip.m_Padding;
					if (!PositionInChunk(EndPos))
					{
						LOG_ERROR("Attempt to read SampleBank's SoundClip data would overshoot file,...");
						break;
					}

					// If sound header has alias we skip it
					if (currClip.m_Alias != 0)
					{
						continue;
					}

					// M249: real, opt-in memory fix - see this class's own
					// header comment on s_LoadFilter. A clip not in the
					// filter is skipped (real SkipBytes, correct stream
					// position, no allocation) instead of read.
					if (s_LoadFilter != nullptr && s_LoadFilter->find(currClip.m_NameHash) == s_LoadFilter->end())
					{
						stream.SkipBytes(currClip.m_DataLength + currClip.m_Padding);
						continue;
					}

					currClip.ReadDataFromStream(stream);
				}
			}
		}

		BaseChunk::EnsureEnd(stream);
	}
}
