/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file enum_over_optimisation.cpp Test whether we do not trigger an over optimisation of enums.
 *
 * For more details, see http://gcc.gnu.org/PR43680 and PR#5246.
 */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

enum TestEnum : int8_t {
	ZERO,
	ONE,
	TWO
};

TEST_CASE("EnumOverOptimisation_LoopNotTerminating")
{
	int8_t count = 0;
	for (TestEnum var = ZERO; var <= TWO; var = static_cast<TestEnum>(var + 1), count++) {
		CHECK(count <= 2);
	}
}

TEST_CASE("EnumOverOptimisation_BoundsCheck")
{
	TestEnum three = static_cast<TestEnum>(3);
	CHECK(TWO < three);

	TestEnum negative_one = static_cast<TestEnum>(-1);
	CHECK(negative_one < ZERO);
}
