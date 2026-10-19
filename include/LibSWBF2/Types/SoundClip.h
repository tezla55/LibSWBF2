#pragma once
#include <LibSWBF2/Types/LibString.h>

namespace LibSWBF2
{
	class FileReader;
	class FileWriter;
}

namespace LibSWBF2::Types
{
	struct LIBSWBF2_API SoundClip
	{
		SoundClip();
		~SoundClip();

		// M249: real bug, found while testing this project's own sound
		// system against the actual retail global.lvl (739MB, thousands of
		// real SoundClip headers) - crashed with a double free that never
		// showed up against the small test files used so far (core.lvl,
		// 7 clips; geo.lvl, 573, both under one List<T> growth step).
		//
		// SoundClip owns a raw buffer (m_Data) and frees it in its
		// destructor, but declared neither a copy constructor nor a copy
		// assignment operator - so the compiler-generated ones did a
		// SHALLOW copy of m_Data. List<T>::Resize() (src/Types/List.cpp)
		// copy-assigns every old element into a freshly allocated array
		// then deletes the old array - which, for SoundClip, left the new
		// array's m_Data pointing at a buffer the just-deleted old element
		// had ALSO freed. The next time anything freed that same clip
		// (a later Resize, or simple destruction), it was a double free on
		// an already-dangling pointer. Only surfaces once a List<SoundClip>
		// grows enough to actually reallocate - which real weapon/footstep/
		// VO-scale sound banks do and small test banks don't, hence never
		// caught before.
		//
		// Fixed with a real Rule-of-5: deep-copy on copy, ownership
		// transfer (and null the source) on move, so List<T>'s
		// copy-assignment resize path is correct and its Add(T&&) path
		// stays a cheap pointer transfer rather than an unnecessary deep
		// copy.
		SoundClip(const SoundClip& other);
		SoundClip& operator=(const SoundClip& other);
		SoundClip(SoundClip&& other) noexcept;
		SoundClip& operator=(SoundClip&& other) noexcept;

		size_t m_HeaderPosition;
		size_t m_DataPosition;

		// Header
		FNVHash m_NameHash;
		uint32_t m_SampleRate;
		uint32_t m_SampleCount;
		uint32_t m_DataLength;
		uint32_t m_Padding;
		uint32_t m_Alias;

		const uint8_t* GetSampleData() const;
		uint8_t GetBytesPerSample() const;

		// returns size of read header
		uint8_t ReadHeaderFromStream(FileReader& stream);
		void ReadDataFromStream(FileReader& stream);

		bool TryLookupName(String& result);
		String ToString() const;

	private:
		uint8_t* m_Data = nullptr;
	};
}