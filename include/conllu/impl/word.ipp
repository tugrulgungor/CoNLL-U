/*
 * CoNLL-U - https://github.com/tugrulgungor/CoNLL-U
 *
 * Copyright (c) 2026. All rights reserved.
 * Tuğrul Güngör - https://conll-u.tugrulgungor.me
 *
 * Distributed under the MIT License.
 * https://opensource.org/license/mit/
 */

#ifndef TG_CONLLU_IMPL_WORD_IPP
#define TG_CONLLU_IMPL_WORD_IPP 1

#include "conllu/treebank.hpp"

namespace tg::conllu {
    inline constexpr std::size_t Word::Range::count() const noexcept {
        return (last - first) + std::size_t{1};
    }

    inline constexpr bool Word::Range::contains(const TokenId id) const noexcept {
        return id >= first && id <= last;
    }

    inline constexpr bool Word::Range::overlaps(const Range range) const noexcept {
        return contains(range.first) || contains(range.last) || range.contains(first) || range.contains(last);
    }

    inline constexpr bool Word::Range::equals(const Range range) const noexcept {
        return range.first == first && range.last == last;
    }

    inline constexpr bool Word::Range::is_valid() const noexcept {
        return first < last && TokenId{0} < first && std::size_t{1} < count();
    }

    inline constexpr bool Word::Range::is_valid(const TokenId max) const noexcept {
        return is_valid() && last <= max;
    }

    // Constructors
    inline constexpr Word::Word(Sentence* sentence) noexcept : sentence_(sentence) {
    }

    // Range
    inline constexpr std::error_code Word::set_range(const Range range) noexcept {
        if (range_.equals(range))
            return {};

        if (!range.is_valid(sentence_->num_tokens()))
            return make_error_code(ErrorCode::InvalidWordRange);

        for (const Word* word : sentence_->words()) {
            if (word == this)
                continue;

            if (word->range_.overlaps(range))
                return make_error_code(ErrorCode::OverlappingWordRange);
        }

        range_ = range;
        return {};
    }

    inline constexpr std::error_code Word::set_range(const TokenId first, const TokenId last) noexcept {
        return set_range(Range{.first = first, .last = last});
    }

    inline constexpr TokenId Word::first_id() const noexcept {
        return range_.first;
    }

    inline constexpr TokenId Word::last_id() const noexcept {
        return range_.last;
    }

    // Form
    inline constexpr std::optional<std::string_view> Word::get_form() const noexcept {
        return form_;
    }
    inline constexpr std::error_code Word::set_form(const std::optional<std::string_view>& form) noexcept {
        try {
            form_ = form;
            return {};
        }
        catch (...) {
            return make_error_code(ErrorCode::InsufficientMemory);
        }
    }
    inline constexpr void Word::unset_form() noexcept {
        form_ = std::nullopt;
    }

    // Misc
    inline constexpr std::optional<std::string_view> Word::get_misc() const noexcept {
        return misc_;
    }
    inline constexpr std::error_code Word::set_misc(const std::optional<std::string_view>& misc) noexcept {
        try {
            misc_ = misc;
            return {};
        }
        catch (...) {
            return make_error_code(ErrorCode::InsufficientMemory);
        }
    }
    inline constexpr void Word::unset_misc() noexcept {
        misc_ = std::nullopt;
    }

    // Typo
    inline constexpr std::optional<bool> Word::get_typo() const noexcept {
        return typo_;
    }
    inline constexpr void Word::set_typo(const std::optional<bool> typo) noexcept {
        typo_ = typo;
    }
    inline constexpr void Word::unset_typo() noexcept {
        typo_ = std::nullopt;
    }

    // Tokens
    inline constexpr auto Word::tokens() const noexcept {
        return std::views::filter(sentence_->tokens(),
                                  [&](const Token* token) noexcept -> bool { return range_.contains(token->id_); });
    }

    inline constexpr std::size_t Word::num_tokens() const noexcept {
        return last_id() - first_id() + std::size_t{1};
    }

    inline constexpr bool Word::contains_token(Token* token) const noexcept {
        return std::ranges::any_of(tokens(), [token](const Token* t) noexcept -> bool { return t == token; });
    }

    inline constexpr bool Word::contains_token_id(const TokenId id) const noexcept {
        return range_.contains(id);
    }

    // State
    inline constexpr std::error_code Word::validate() const noexcept {
        if (!range_.is_valid(sentence_->num_tokens()))
            return make_error_code(ErrorCode::InvalidWordRange);

        for (const Word* word : sentence_->words()) {
            if (word == this)
                continue;

            if (word->range_.overlaps(range_))
                return make_error_code(ErrorCode::OverlappingWordRange);
        }

        return {};
    }
} // namespace tg::conllu

#endif // TG_CONLLU_IMPL_WORD_IPP
