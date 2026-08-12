/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "Multiplayer/DeterministicHash.h"

#include <cstdlib>
#include <iostream>

namespace
{
	void Require(bool theCondition, const char* theMessage)
	{
		if (!theCondition)
		{
			std::cerr << "FAILED: " << theMessage << '\n';
			std::exit(1);
		}
	}
}

int main()
{
	using namespace PvzMultiplayer;

	DeterministicHash64 aHash;
	aHash.AddU8('a');
	aHash.AddU8('b');
	aHash.AddU8('c');
	Require(aHash.Finish() == 0xE71FA2190541574BULL, "FNV-1a reference hash changed");

	DeterministicHash64 aTyped;
	aTyped.AddU32(0x04030201U);
	DeterministicHash64 aBytes;
	const uint8_t aLittleEndian[]{1, 2, 3, 4};
	aBytes.AddBytes(aLittleEndian);
	Require(aTyped.Finish() == aBytes.Finish(), "integer hashing is not canonical little-endian");

	DeterministicHash64 aFalse;
	aFalse.AddBool(false);
	DeterministicHash64 aTrue;
	aTrue.AddBool(true);
	Require(aFalse.Finish() != aTrue.Finish(), "boolean values collide");

	std::cout << "Deterministic hash tests passed\n";
	return 0;
}
