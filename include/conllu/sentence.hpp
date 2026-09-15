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
 * @file conllu/sentence.hpp
 */

#ifndef TG_CONLLU_SENTENCE_HPP
#define TG_CONLLU_SENTENCE_HPP 1

#include "conllu/error.hpp"
#include "conllu/internal/string_hash.hpp"
#include "conllu/word.hpp"

#include <expected>
#include <string>
#include <unordered_map>
#include <vector>

namespace tg::conllu {
    class Treebank;

    /**
     * @class Sentence
     * @brief A single sentence within a Treebank.
     * @ingroup conllu
     */
    class Sentence final {
        std::string sent_id_{};
        std::string text_{};

        std::unordered_map<std::string, std::string, internal::StringHash, std::equal_to<>> metadata_{};

        std::vector<std::unique_ptr<Token>> tokens_{};
        std::vector<std::unique_ptr<Word>> words_{};

        Treebank* treebank_;

        static constexpr void split_metadata_line_(std::string_view& line, std::string_view& key,
                                                   std::string_view& value) noexcept;

        [[nodiscard]] constexpr std::optional<std::error_code> set_known_metadata_(std::string_view key,
                                                                                   std::string_view value) noexcept;

        [[nodiscard]] constexpr std::error_code set_sent_id_(std::string_view sent_id) noexcept;

        constexpr std::expected<Word*, std::error_code> add_word_unsafe_(Word::Range range) noexcept;
        constexpr void remove_empty_words_() noexcept;

        explicit(true) constexpr Sentence(Treebank* treebank) noexcept;

        friend class Parser;
        friend class Token;
        friend class Treebank;
        friend class Word;
        friend class Writer;

    public:
        /**
         * @name Constructors
         */
        /** @{ */
        /**
         * @brief Default construction is disabled; sentences are created via Treebank::add_sentence.
         */
        constexpr Sentence() noexcept = delete;
        /** @} */
        /**
         * @name Treebank
         */
        /** @{ */
        /**
         * @brief Pointer to the treebank that owns this sentence.
         */
        [[nodiscard]] constexpr Treebank* treebank() const noexcept;
        /** @} */
        /**
         * @name Identity
         */
        /** @{ */
        /**
         * @brief Sentence identifier (`sent_id` metadata).
         */
        [[nodiscard]] constexpr std::string_view get_sent_id() const noexcept;
        /**
         * @brief Changes the sentence identifier.
         * @param[in] sent_id New identifier for the sentence.
         * @return Empty error_code on success, or an error describing the failure.
         */
        constexpr std::error_code set_sent_id(std::string_view sent_id) noexcept;
        /** @} */
        /**
         * @name Text
         */
        /** @{ */
        /**
         * @brief Sentence text (`text` metadata).
         */
        [[nodiscard]] constexpr std::string_view get_text() const noexcept;
        /**
         * @brief Sets the sentence text.
         * @param[in] text New text for the sentence.
         * @return Empty error_code on success, or an error describing the failure.
         */
        [[nodiscard]] constexpr std::error_code set_text(std::string_view text) noexcept;
        /** @} */
        /**
         * @name Tokens
         */
        /** @{ */
        /**
         * @brief View over the tokens, in sentence order.
         */
        [[nodiscard]] constexpr auto tokens() const noexcept;
        /**
         * @brief Number of tokens in the sentence.
         */
        [[nodiscard]] constexpr std::size_t num_tokens() const noexcept;
        /**
         * @brief Looks up a token by its identifier.
         * @param[in] id Token identifier to search for.
         * @return Pointer to the token, or std::nullopt if no such token exists.
         */
        [[nodiscard]] constexpr std::optional<Token*> get_token_by_id(TokenId id) const noexcept;
        /**
         * @brief Constructs and appends a new token.
         * @return Pointer to the newly created token, or an error_code on failure.
         */
        [[nodiscard]] constexpr std::expected<Token*, std::error_code> add_token() noexcept;
        /**
         * @brief Removes the token with the given identifier.
         * @param[in] id Identifier of the token to remove.
         * @param[in] skip_dependency_validation If false, fails when another token's HEAD references @p id;
         *            if true, clears such HEAD references instead of failing.
         * @return Empty error_code on success, or an error describing the failure.
         */
        constexpr std::error_code remove_token(TokenId id, bool skip_dependency_validation = false) noexcept;
        /**
         * @brief Removes the given token.
         * @param[in] token Pointer to the token to remove.
         * @param[in] skip_dependency_validation If false, fails when another token's HEAD references @p token;
         *            if true, clears such HEAD references instead of failing.
         * @return Empty error_code on success, or an error describing the failure.
         */
        constexpr std::error_code remove_token(Token* token, bool skip_dependency_validation = false) noexcept;
        /**
         * @brief Removes all tokens from the sentence.
         */
        constexpr void remove_all_tokens() noexcept;
        /** @} */
        /**
         * @name Words
         */
        /** @{ */
        /**
         * @brief View over the words, in treebank/range order.
         */
        [[nodiscard]] constexpr auto words() const noexcept;
        /**
         * @brief Number of words in the sentence.
         */
        [[nodiscard]] constexpr std::size_t num_words() const noexcept;
        /**
         * @brief Constructs and appends a new word spanning the given token range.
         * @param[in] first Identifier of the first token in the range.
         * @param[in] last Identifier of the last token in the range.
         * @return Pointer to the newly created word, or an error_code if the range is invalid or overlaps an existing
         * word.
         */
        [[nodiscard]] constexpr std::expected<Word*, std::error_code> add_word(TokenId first, TokenId last) noexcept;
        /**
         * @brief Constructs and appends a new word spanning the given token range.
         * @param[in] range Token range for the new word.
         * @return Pointer to the newly created word, or an error_code if the range is invalid or overlaps an existing
         * word.
         */
        [[nodiscard]] constexpr std::expected<Word*, std::error_code> add_word(Word::Range range) noexcept;
        /**
         * @brief Removes the given word.
         * @param[in] word Pointer to the word to remove.
         * @param[in] remove_tokens If true, also removes every token covered by the word's range (clearing any dangling
         * HEAD references); if false, only the word grouping is removed and its tokens remain in the sentence.
         * @return Empty error_code on success, or an error describing the failure.
         */
        constexpr std::error_code remove_word(const Word* word, bool remove_tokens = false) noexcept;
        /**
         * @brief Removes all words from the sentence.
         * @param[in] remove_tokens If true, also removes every token covered by each word's range; if false, only the
         * word groupings are removed and their tokens remain.
         */
        constexpr void remove_all_words(bool remove_tokens = false) noexcept;
        /** @} */
        /**
         * @name Metadata
         */
        /** @{ */
        /**
         * @brief View over the metadata entries, as key/value pairs.
         */
        [[nodiscard]] constexpr auto metadata() const noexcept;
        /**
         * @brief Number of metadata entries.
         */
        [[nodiscard]] constexpr std::size_t num_metadata_entries() const noexcept;
        /**
         * @brief Looks up a metadata value by key.
         * @param[in] key Metadata key to search for.
         * @return Metadata value, or std::nullopt if no such key exists.
         */
        [[nodiscard]] constexpr std::optional<std::string_view> get_metadata(std::string_view key) const noexcept;
        /**
         * @brief Adds a new metadata entry.
         * @param[in] key Metadata key; must be non-empty and not already present.
         * @param[in] value Metadata value.
         * @return Empty error_code on success, or an error describing the failure.
         */
        [[nodiscard]] constexpr std::error_code set_metadata(std::string_view key,
                                                             std::string_view value = std::string_view{}) noexcept;
        /**
         * @brief Parses a `#`-prefixed comment line and adds it as a metadata entry.
         * @param[in] line Comment line, in `# key = value` or `# key` form.
         * @return Empty error_code on success, or an error describing the failure.
         */
        [[nodiscard]] constexpr std::error_code set_metadata_from_comment_line(std::string_view line) noexcept;
        /**
         * @brief Removes a metadata entry.
         * @param[in] key Metadata key to remove; the `sent_id` key cannot be removed.
         * @return Empty error_code on success, or an error describing the failure.
         */
        constexpr std::error_code unset_metadata(std::string_view key) noexcept;
        /**
         * @brief Removes all metadata entries except `sent_id`, and clears the text.
         */
        constexpr void remove_all_metadata() noexcept;
        /** @} */
        /**
         * @name State
         */
        /** @{ */
        /**
         * @brief Whether the sentence contains no tokens.
         */
        [[nodiscard]] constexpr bool empty() const noexcept;
        /**
         * @brief Removes all tokens and metadata (except `sent_id`), resetting the sentence's content.
         */
        constexpr void clear() noexcept;
        /**
         * @brief Checks the sentence for structural inconsistencies.
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
        constexpr ~Sentence() noexcept = default;
        /** @} */
    };
} // namespace tg::conllu

#endif // TG_CONLLU_SENTENCE_HPP
