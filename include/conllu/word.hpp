/*
 * CoNLL-U - https://github.com/tugrulgungor/CoNLL-U
 *
 * Copyright (c) 2026. All rights reserved.
 * Tuğrul Güngör - https://conll-u.tugrulgungor.me
 *
 * Distributed under the MIT License.
 * https://opensource.org/license/mit/
 */

/**
 * @file conllu/word.hpp
 */

#ifndef TG_CONLLU_WORD_HPP
#define TG_CONLLU_WORD_HPP 1

#include "conllu/token.hpp"

namespace tg::conllu {
    class Sentence;

    /**
     * @class Word
     * @brief A multiword token: a contiguous span of two or more Token%s sharing a single surface form.
     * @ingroup conllu
     */
    class Word final {
    public:
        /**
         * @struct Range
         * @brief A closed, inclusive interval of Token%s' identifiers, spanning `[first, last]`.
         */
        struct Range final {
            /**
             * @brief Identifier of the first token in the range.
             */
            TokenId first;
            /**
             * @brief Identifier of the last token in the range.
             */
            TokenId last;

            /**
             * @brief Number of tokens covered by the range.
             * @return `last - first + 1`.
             */
            [[nodiscard]] constexpr std::size_t count() const noexcept;
            /**
             * @brief Checks whether a token identifier falls within the range.
             * @param[in] id Token identifier to check.
             * @return True if `first <= id <= last`.
             */
            [[nodiscard]] constexpr bool contains(TokenId id) const noexcept;
            /**
             * @brief Checks whether this range shares any token identifier with another range.
             * @param[in] range Range to compare against.
             * @return True if the two ranges intersect, including a shared boundary token.
             */
            [[nodiscard]] constexpr bool overlaps(Range range) const noexcept;
            /**
             * @brief Checks whether this range has the same bounds as another range.
             * @param[in] range Range to compare against.
             * @return True if both ranges have identical @c first and @c last values.
             */
            [[nodiscard]] constexpr bool equals(Range range) const noexcept;
            /**
             * @brief Checks whether the range is well-formed on its own.
             * @return True if `first < last`, `first > 0`, and the range spans more than one token.
             */
            [[nodiscard]] constexpr bool is_valid() const noexcept;
            /**
             * @brief Checks whether the range is well-formed and fits within a sentence of a given size.
             * @param[in] max Highest valid token identifier (typically a sentence's token count).
             * @return True if is_valid() and `last <= max`.
             */
            [[nodiscard]] constexpr bool is_valid(TokenId max) const noexcept;
        };

    private:
        Range range_{};
        std::optional<std::string> form_{std::nullopt};
        std::optional<std::string> misc_{std::nullopt};
        std::optional<bool> typo_{std::nullopt};

        Sentence* sentence_{nullptr};

        explicit(true) constexpr Word(Sentence* sentence) noexcept;

        friend class Sentence;
        friend class Writer;

    public:
        /**
         * @name Constructors
         */
        /** @{ */
        /**
         * @brief Default construction is disabled; words are created via Sentence::add_word.
         */
        constexpr Word() noexcept = delete;
        /** @} */
        /**
         * @name Range
         */
        /** @{ */
        /**
         * @brief Sets the token range covered by this word.
         * @param[in] range New range; must be valid for the owning sentence and must not overlap
         *            any other word in the sentence.
         * @return Empty error_code on success, or an error describing the failure.
         */
        constexpr std::error_code set_range(Range range) noexcept;
        /**
         * @brief Sets the token range covered by this word.
         * @param[in] first Identifier of the first token in the range.
         * @param[in] last Identifier of the last token in the range.
         * @return Empty error_code on success, or an error describing the failure.
         */
        constexpr std::error_code set_range(TokenId first, TokenId last) noexcept;
        /**
         * @brief Identifier of the first token in the word's range.
         */
        [[nodiscard]] constexpr TokenId first_id() const noexcept;
        /**
         * @brief Identifier of the last token in the word's range.
         */
        [[nodiscard]] constexpr TokenId last_id() const noexcept;
        /** @} */
        /**
         * @name Form
         */
        /** @{ */
        /**
         * @brief Word form (surface form of the multiword token).
         */
        [[nodiscard]] constexpr std::optional<std::string_view> get_form() const noexcept;
        /**
         * @brief Sets the word form.
         * @param[in] form New form, or std::nullopt to unset it.
         * @return Empty error_code on success, or an error describing the failure.
         */
        constexpr std::error_code set_form(const std::optional<std::string_view>& form) noexcept;
        /**
         * @brief Unsets the word form.
         */
        constexpr void unset_form() noexcept;
        /** @} */
        /**
         * @name Misc
         */
        /** @{ */
        /**
         * @brief Word-level MISC field.
         */
        [[nodiscard]] constexpr std::optional<std::string_view> get_misc() const noexcept;
        /**
         * @brief Sets the word-level MISC field.
         * @param[in] misc New value, or std::nullopt to unset it.
         * @return Empty error_code on success, or an error describing the failure.
         */
        constexpr std::error_code set_misc(const std::optional<std::string_view>& misc) noexcept;
        /**
         * @brief Unsets the word-level MISC field.
         */
        constexpr void unset_misc() noexcept;
        /** @} */
        /**
         * @name Typo
         */
        /** @{ */
        /**
         * @brief Whether the word is flagged as a typo.
         */
        [[nodiscard]] constexpr std::optional<bool> get_typo() const noexcept;
        /**
         * @brief Sets whether the word is flagged as a typo.
         * @param[in] typo New value, or std::nullopt to unset it.
         */
        constexpr void set_typo(std::optional<bool> typo) noexcept;
        /**
         * @brief Unsets the typo flag.
         */
        constexpr void unset_typo() noexcept;
        /** @} */
        /**
         * @name Tokens
         */
        /** @{ */
        /**
         * @brief View over the tokens covered by this word, in sentence order.
         */
        [[nodiscard]] constexpr auto tokens() const noexcept;
        /**
         * @brief Number of tokens covered by this word.
         */
        [[nodiscard]] constexpr std::size_t num_tokens() const noexcept;
        /**
         * @brief Checks whether a token belongs to this word.
         * @param[in] token Token to check.
         * @return True if @p token's identifier falls within this word's range.
         */
        [[nodiscard]] constexpr bool contains_token(Token* token) const noexcept;
        /**
         * @brief Checks whether a token identifier belongs to this word.
         * @param[in] id Token identifier to check.
         * @return True if @p id falls within this word's range.
         */
        [[nodiscard]] constexpr bool contains_token_id(TokenId id) const noexcept;
        /** @} */
        /**
         * @name State
         */
        /** @{ */
        /**
         * @brief Checks the word for structural inconsistencies.
         * @return Empty error_code on success, or an error describing the first issue found.
         */
        [[nodiscard]] constexpr std::error_code validate() const noexcept;
        /** @} */
        /**
         * @name Destructor
         */
        /** @{ */
        /**
         * @brief Destructor.
         */
        constexpr ~Word() noexcept = default;
        /** @} */
    };
} // namespace tg::conllu

#endif // TG_CONLLU_WORD_HPP
