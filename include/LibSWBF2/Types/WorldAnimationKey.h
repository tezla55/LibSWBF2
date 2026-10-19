#pragma once
#include <LibSWBF2/req.h>


namespace LibSWBF2
{
	class FileWriter;
	class FileReader;
}


namespace LibSWBF2::Types
{
	struct Vector3;
	struct String;

	// M479 (swbf2render): NOT packed. pack(1) put m_EaseOut/m_EaseIn at odd
	// offsets after the 1-byte transition type, and the Vita's VFP stores
	// data-abort on them - myg1.lvl (world animations) crashed in
	// wrld::anim::ReadFromStream. ReadFromStream reads field by field, so
	// the layout never mattered.
	struct LIBSWBF2_API WorldAnimationKey
	{
		float m_Time;
		Vector3 m_Value;
		EWorldAnimKeyTransitionType m_TransitionType;
		Vector3 m_EaseOut;
		Vector3 m_EaseIn;

		void WriteToStream(FileWriter& stream);
		void ReadFromStream(FileReader& stream);

		String ToString() const;

	};
}