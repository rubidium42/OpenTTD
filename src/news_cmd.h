/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file news_cmd.h Command definitions related to news messages. */

#ifndef NEWS_CMD_H
#define NEWS_CMD_H

#include "command_func.h"
#include "company_type.h"
#include "news_func.h"

CommandCost CmdCustomNewsItem(DoCommandFlag flags, NewsType type, CompanyID company, NewsReference reference, const std::string &text);

DEF_CMD_TRAIT(CMD_CUSTOM_NEWS_ITEM, CmdCustomNewsItem, CMD_STR_CTRL | CMD_DEITY, CMDT_OTHER_MANAGEMENT)

template <typename Tcont, typename Titer>
inline EndianBufferWriter<Tcont, Titer> &operator <<(EndianBufferWriter<Tcont, Titer> &buffer, const NewsReference &reference)
{
	return buffer << static_cast<uint8_t>(reference.index()) << SerialiseNewsReference(reference);
}

template <std::size_t I = 0>
NewsReference DeserializeNewsReference(uint8_t index, uint32_t serialised_reference)
{
	if constexpr (I < std::variant_size_v<NewsReference>) {
		if (I == index) {
			if constexpr (!std::is_same_v<std::monostate, std::variant_alternative_t<I, NewsReference>>) {
				return static_cast<std::variant_alternative_t<I, NewsReference>>(serialised_reference);
			}
		} else {
			return DeserializeNewsReference<I + 1>(index, serialised_reference);
		}
	}
	return std::monostate{};
}

inline EndianBufferReader &operator >>(EndianBufferReader &buffer, NewsReference &reference)
{
	uint8_t index;
	uint32_t serialised_reference;
	buffer = buffer >> index >> serialised_reference;

	reference = DeserializeNewsReference(index, serialised_reference);
	return buffer;
}


#endif /* NEWS_CMD_H */
