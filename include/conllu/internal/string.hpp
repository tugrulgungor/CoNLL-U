/*
 * CoNLL-U - https://github.com/tugrulgungor/CoNLL-U
 *
 * Copyright (c) 2026. All rights reserved.
 * Tuğrul Güngör - https://conll-u.tugrulgungor.me
 *
 * Distributed under the MIT License.
 * https://opensource.org/license/mit/
 */

#ifndef TG_CONLLU_INTERNAL_STRING_HPP
#define TG_CONLLU_INTERNAL_STRING_HPP 1

#include <charconv>
#include <string_view>

namespace tg::conllu::internal {
    template <char C = ' '>
    constexpr void trim_start(std::string_view &str) noexcept;
    template <char C = ' '>
    constexpr void trim_end(std::string_view &str) noexcept;
    template <char C = ' '>
    constexpr void trim(std::string_view &str) noexcept;

    template <typename NumberType>
        requires std::is_integral_v<NumberType>
    [[nodiscard]] constexpr bool parse_number(std::string_view str, NumberType &number) noexcept;

    template <char Delimiter, bool Strict>
    [[nodiscard]] constexpr bool split(std::string_view str, std::string_view &first,
                                       std::string_view &second) noexcept;

    template <char Delimiter, typename NumberType>
        requires std::is_integral_v<NumberType> && std::is_unsigned_v<NumberType>
    [[nodiscard]] constexpr bool split_and_parse(std::string_view str, NumberType &first, NumberType &second) noexcept;
} // namespace tg::conllu::internal

#include "conllu/internal/impl/string.ipp"

#endif // TG_CONLLU_INTERNAL_STRING_HPP
