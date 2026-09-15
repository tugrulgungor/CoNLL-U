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
 * @file conllu/error.hpp
 */

#ifndef TG_CONLLU_ERROR_HPP
#define TG_CONLLU_ERROR_HPP 1

#include <system_error>

namespace tg::conllu {
    /**
     * @brief Error codes reported by parsing, writing, and treebank mutation operations.
     * @ingroup conllu
     */
    enum class ErrorCode : int {
        /**
         * @brief No error.
         */
        Success = int{0},

        /**
         * @brief Reading from or writing to the underlying stream failed (e.g. an exception
         *        was thrown while buffering input).
         */
        StreamError,
        /**
         * @brief An allocation failed (e.g. while growing a container or copying a string).
         */
        InsufficientMemory,
        /**
         * @brief A supplied index or position was out of range for the target container.
         */
        PositionOutOfRange,
        /**
         * @brief A required pointer argument was null, or did not belong to the expected owner.
         */
        InvalidPointer,

        /**
         * @brief Reserved for a malformed column declaration line.
         */
        InvalidColumnsLine,
        /**
         * @brief A column name failed validation (e.g. empty or containing disallowed characters).
         */
        InvalidColumnName,
        /**
         * @brief A column with the given name already exists in the treebank.
         */
        DuplicateColumnName,
        /**
         * @brief No column with the given name or position exists.
         */
        ColumnNotFound,

        /**
         * @brief The first line of a sentence block was not a `#`-prefixed meta line.
         */
        SentenceMustStartWithMetaLine,
        /**
         * @brief A `#`-prefixed meta line is not a comment line (does not start with `#`).
         */
        InvalidSentenceMetaLine,
        /**
         * @brief An empty metadata key was supplied.
         */
        InvalidSentenceMetaKey,
        /**
         * @brief The same metadata key was set more than once for a sentence.
         */
        DuplicateSentenceMetaEntry,
        /**
         * @brief A sentence block had no `sent_id` meta line.
         */
        MissingSentenceId,
        /**
         * @brief A sentence identifier failed validation.
         */
        InvalidSentenceId,
        /**
         * @brief A sentence with the given identifier already exists in the treebank.
         */
        DuplicateSentenceId,
        /**
         * @brief An attempt was made to remove the reserved `sent_id` metadata entry.
         */
        SentenceIdCannotBeRemoved,
        /**
         * @brief No sentence with the given identifier or pointer exists in the treebank.
         */
        SentenceNotFoundInTreebank,

        /**
         * @brief A token identifier (ID field) failed validation.
         */
        InvalidTokenId,
        /**
         * @brief A token line's number of fields does not match the treebank's column count.
         */
        TokenFieldCountMismatch,
        /**
         * @brief An attempt was made to modify a field that cannot be set directly (e.g. ID).
         */
        ImmutableTokenField,
        /**
         * @brief An empty node's identifier is inconsistent with its parent token.
         */
        TokenEmptyNodeMismatch,
        /**
         * @brief No empty node with the given identifier or pointer exists on the token.
         */
        NodeNotFoundInToken,
        /**
         * @brief A HEAD value failed validation (e.g. self-reference or no matching token).
         */
        InvalidTokenHead,
        /**
         * @brief No token with the given identifier or pointer exists in the sentence.
         */
        TokenNotFoundInSentence,
        /**
         * @brief A token's number of fields does not match what the treebank expects.
         */
        InvalidNumberOfFields,

        /**
         * @brief A word's token range is malformed or out of bounds for its sentence.
         */
        InvalidWordRange,
        /**
         * @brief A word's token range overlaps another word's range in the same sentence.
         */
        OverlappingWordRange,
        /**
         * @brief No word with the given pointer exists in the sentence.
         */
        WordNotFoundInSentence,
        /**
         * @brief A word line appeared after one of the individual token lines it should span; word lines must precede
         * their constituent tokens.
         */
        WordMustAppearBeforeToken,
        /**
         * @brief A word line set a field it is not permitted to carry (only ID, FORM, MISC, and a `Typo=Yes` FEATS
         * flag are allowed on a word line).
         */
        InvalidWordFieldValue
    };

    /**
     * @class ErrorCategory
     * @brief The std::error_category implementation backing tg::conllu::ErrorCode.
     * @ingroup conllu
     */
    class ErrorCategory final : public std::error_category {
    public:
        /**
         * @brief Name of the error category.
         * @return `"tg::conllu"`.
         */
        [[nodiscard]] constexpr const char* name() const noexcept override;

        /**
         * @brief Human-readable message for an ErrorCode value.
         * @param[in] ev Underlying integer value of an ErrorCode.
         * @return Description of the error, or `"<unknown>"` if @p ev is not a recognized ErrorCode.
         */
        [[nodiscard]] constexpr std::string message(int ev) const noexcept override;
    };

    /**
     * @brief Accesses the singleton ErrorCategory instance used by tg::conllu::ErrorCode.
     * @return Reference to the shared category instance.
     */
    [[nodiscard]] constexpr const ErrorCategory& get_error_category() noexcept;

    /**
     * @brief Constructs a std::error_code from a tg::conllu::ErrorCode.
     * @param[in] ec Error code to wrap.
     * @return A std::error_code using tg::conllu's error category.
     */
    [[nodiscard]] constexpr std::error_code make_error_code(ErrorCode ec) noexcept;
} // namespace tg::conllu

template <>
struct std::is_error_code_enum<tg::conllu::ErrorCode> : std::true_type {};

#include "conllu/impl/error.ipp"

#endif // TG_CONLLU_ERROR_HPP
