/*
 * CoNLL-U - https://github.com/tugrulgungor/CoNLL-U
 *
 * Copyright (c) 2026. All rights reserved.
 * Tuğrul Güngör - https://conll-u.tugrulgungor.me
 *
 * Distributed under the MIT License.
 * https://opensource.org/license/mit/
 */

#ifndef TG_CONLLU_INTERNAL_IMPL_STRING_IPP
#define TG_CONLLU_INTERNAL_IMPL_STRING_IPP 1

#include "conllu/internal/string.hpp"

namespace tg::conllu::internal {
    template <char C>
    inline constexpr void trim_start(std::string_view &str) noexcept {
        while (!str.empty() && C == str[std::size_t{0}])
            str.remove_prefix(std::size_t{1});
    }
    template <char C>
    inline constexpr void trim_end(std::string_view &str) noexcept {
        while (!str.empty() && C == str.back())
            str.remove_suffix(std::size_t{1});
    }
    template <char C>
    inline constexpr void trim(std::string_view &str) noexcept {
        trim_start<C>(str);
        trim_end<C>(str);
    }

    template <typename NumberType>
        requires std::is_integral_v<NumberType>
    inline constexpr bool parse_number(const std::string_view str, NumberType &number) noexcept {
        const auto res = std::from_chars(str.data(), str.data() + str.size(), number);
        return (std::errc() == res.ec) && (res.ptr == (str.data() + str.size()));
    }

    template <char Delimiter, bool Strict>
    inline constexpr bool split(const std::string_view str, std::string_view &first,
                                std::string_view &second) noexcept {
        const std::size_t pos = str.find(Delimiter);
        if (std::string_view::npos == pos) {
            if constexpr (Strict) {
                return false;
            }
            else {
                first = str;
                return true;
            }
        }

        first = str.substr(std::size_t{0}, pos);
        second = str.substr(pos + std::size_t{1});
        return true;
    }

    template <char Delimiter, typename NumberType>
        requires std::is_integral_v<NumberType> && std::is_unsigned_v<NumberType>
    inline constexpr bool split_and_parse(const std::string_view str, NumberType &first, NumberType &second) noexcept {
        std::pair<std::string_view, std::string_view> sv{};
        if (!split<Delimiter, true>(str, sv.first, sv.second))
            return false;

        return parse_number<NumberType>(sv.first, first) && parse_number<NumberType>(sv.second, second);
    }
} // namespace tg::conllu::internal

#endif // TG_CONLLU_INTERNAL_IMPL_STRING_IPP
