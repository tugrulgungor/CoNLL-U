/*
 * CoNLL-U - https://github.com/tugrulgungor/CoNLL-U
 *
 * Copyright (c) 2026. All rights reserved.
 * Tuğrul Güngör - https://conll-u.tugrulgungor.me
 *
 * Distributed under the MIT License.
 * https://opensource.org/license/mit/
 */

#ifndef TG_CONLLU_INTERNAL_IMPL_VALIDATE_IPP
#define TG_CONLLU_INTERNAL_IMPL_VALIDATE_IPP 1

#include "conllu/internal/validate.hpp"

namespace tg::conllu::internal {
    inline constexpr bool is_column_name_valid(const std::string_view name) noexcept {
        if (name.empty())
            return false;

        bool split{false};
        char prev{'\0'};
        for (const char c : name) {
            if ('A' <= c && c <= 'Z') {
                prev = c;
                continue;
            }

            if (':' == c) {
                if ('\0' == prev || c == prev || split)
                    return false;

                split = true;
                prev = c;
                continue;
            }

            return false;
        }

        return ':' != name.back();
    }
    inline constexpr bool is_sent_id_valid(const std::string_view sent_id) noexcept {
        if (sent_id.empty())
            return false;

        if (sent_id.contains('/') || sent_id.contains(' ') || sent_id.contains('\t') || sent_id.contains('\n') ||
            sent_id.contains('\r'))
            return false;

        return true;
    }

    // A `# newdoc id = ...` / `# newpar id = ...` value: one line, without the surrounding spaces a reader would trim.
    inline constexpr bool is_boundary_id_valid(const std::string_view id) noexcept {
        if (id.contains('\n') || id.contains('\r'))
            return false;

        return id.empty() || (' ' != id.front() && '\t' != id.front() && ' ' != id.back() && '\t' != id.back());
    }
} // namespace tg::conllu::internal

#endif // TG_CONLLU_INTERNAL_IMPL_VALIDATE_IPP
