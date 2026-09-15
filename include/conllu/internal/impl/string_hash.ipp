/*
 * CoNLL-U - https://github.com/tugrulgungor/CoNLL-U
 *
 * Copyright (c) 2026. All rights reserved.
 * Tuğrul Güngör - https://conll-u.tugrulgungor.me
 *
 * Distributed under the MIT License.
 * https://opensource.org/license/mit/
 */

#ifndef TG_CONLLU_INTERNAL_IMPL_STRING_HASH_IPP
#define TG_CONLLU_INTERNAL_IMPL_STRING_HASH_IPP 1

#include "conllu/internal/string_hash.hpp"

namespace tg::conllu::internal {
    inline constexpr std::size_t StringHash::operator()(const std::string_view sv) const noexcept {
        return std::hash<std::string_view>{}(sv);
    }

    inline constexpr std::size_t StringHash::operator()(const std::string& s) const noexcept {
        return std::hash<std::string_view>{}(std::string_view{s});
    }
} // namespace tg::conllu::internal

#endif // TG_CONLLU_INTERNAL_IMPL_STRING_HASH_IPP
