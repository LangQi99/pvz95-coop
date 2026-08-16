/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "Common.h"

#include <cstdlib>
#include <iostream>
#include <string>

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
	Sexy::SRand(0x12345678U);
	const std::string aSavedState = Sexy::GetRandState();
	const int aFirst = Sexy::Rand();
	const int aSecond = Sexy::Rand();

	Sexy::SetRandState(aSavedState);
	Require(Sexy::Rand() == aFirst, "restored RNG did not reproduce the first value");
	Require(Sexy::Rand() == aSecond, "restored RNG did not reproduce the second value");

	std::cout << "Random state tests passed\n";
	return 0;
}
