/*
 * CoNLL-U - https://github.com/tugrulgungor/CoNLL-U
 *
 * Copyright (c) 2026. All rights reserved.
 * Tuğrul Güngör - https://conll-u.tugrulgungor.me
 *
 * Distributed under the MIT License.
 * https://opensource.org/license/mit/
 */

#ifndef TG_CONLLU_INTERNAL_STRING_HASH_HPP
#define TG_CONLLU_INTERNAL_STRING_HASH_HPP 1

#include <string>
#include <string_view>

namespace tg::conllu::internal {
    struct StringHash final {
        using is_transparent = void;

        constexpr std::size_t operator()(std::string_view sv) const noexcept;
        constexpr std::size_t operator()(const std::string &s) const noexcept;
    };
} // namespace tg::conllu::internal

#include "conllu/internal/impl/string_hash.ipp"

#endif // TG_CONLLU_INTERNAL_STRING_HASH_HPP
