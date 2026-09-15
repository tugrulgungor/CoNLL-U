/*
 * CoNLL-U - https://github.com/tugrulgungor/CoNLL-U
 *
 * Copyright (c) 2026. All rights reserved.
 * Tuğrul Güngör - https://conll-u.tugrulgungor.me
 *
 * Distributed under the MIT License.
 * https://opensource.org/license/mit/
 */

#ifndef TG_CONLLU_INTERNAL_VALIDATE_HPP
#define TG_CONLLU_INTERNAL_VALIDATE_HPP 1

#include <algorithm>
#include <string>
#include <string_view>

namespace tg::conllu::internal {
    [[nodiscard]] constexpr bool is_column_name_valid(std::string_view name) noexcept;
    [[nodiscard]] constexpr bool is_sent_id_valid(std::string_view sent_id) noexcept;
} // namespace tg::conllu::internal

#include "conllu/internal/impl/validate.ipp"

#endif // TG_CONLLU_INTERNAL_VALIDATE_HPP
