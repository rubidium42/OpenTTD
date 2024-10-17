/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file map_type.h Types related to maps. */

#ifndef MAP_TYPE_H
#define MAP_TYPE_H

/**
 * An offset value between two tiles.
 *
 * This value is used for the difference between
 * two tiles. It can be added to a TileIndex to get
 * the resulting TileIndex of the start tile applied
 * with this saved difference.
 *
 * @see TileDiffXY(int, int)
 */
#ifndef _DEBUG
typedef int32_t TileIndexDiff;
#else
struct TileIndexDiff {
public:
	friend inline TileIndexDiff TileDiffXY(int x, int y);
	friend TileIndex operator-(const TileIndex &tile, const TileIndexDiff &offset);
	friend TileIndex operator+(const TileIndex &tile, const TileIndexDiff &offset);

	constexpr TileIndexDiff operator*(int amount) const { return { this->x * amount, this->y * amount }; }
	constexpr TileIndexDiff& operator*=(int amount) { this->x *= amount; this->y *= amount; return *this; }
	constexpr TileIndexDiff& operator+=(const TileIndexDiff &other) { this->x += other.x; this->y += other.y; return *this; }

	constexpr bool operator<=>(const TileIndexDiff&) const = default;

private:
	constexpr TileIndexDiff(int x, int y) : x(x), y(y) {}

	int x;
	int y;
};
#endif

TileIndex operator-(const TileIndex &tile, const TileIndexDiff &offset);
TileIndex operator+(const TileIndex &tile, const TileIndexDiff &offset);
TileIndex& operator-=(TileIndex &tile, const TileIndexDiff &offset);
TileIndex& operator+=(TileIndex &tile, const TileIndexDiff &offset);

/**
 * A pair-construct of a TileIndexDiff.
 *
 * This can be used to save the difference between to
 * tiles as a pair of x and y value.
 */
struct TileIndexDiffC {
	int16_t x;        ///< The x value of the coordinate
	int16_t y;        ///< The y value of the coordinate
};

/** Minimal and maximal map width and height */
static const uint MIN_MAP_SIZE_BITS = 6;                       ///< Minimal size of map is equal to 2 ^ MIN_MAP_SIZE_BITS
static const uint MAX_MAP_SIZE_BITS = 12;                      ///< Maximal size of map is equal to 2 ^ MAX_MAP_SIZE_BITS
static const uint MIN_MAP_SIZE      = 1U << MIN_MAP_SIZE_BITS; ///< Minimal map size = 64
static const uint MAX_MAP_SIZE      = 1U << MAX_MAP_SIZE_BITS; ///< Maximal map size = 4096

/** Argument for CmdLevelLand describing what to do. */
enum LevelMode : uint8_t {
	LM_LEVEL, ///< Level the land.
	LM_LOWER, ///< Lower the land.
	LM_RAISE, ///< Raise the land.
};

#endif /* MAP_TYPE_H */
