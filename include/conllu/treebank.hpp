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
 * @file conllu/treebank.hpp
 */

#ifndef TG_CONLLU_TREEBANK_HPP
#define TG_CONLLU_TREEBANK_HPP 1

#include "conllu/error.hpp"
#include "conllu/internal/string_hash.hpp"
#include "conllu/sentence.hpp"

#include <deque>
#include <expected>
#include <ranges>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tg::conllu {
    /**
     * @class Treebank
     * @brief In-memory representation of a CoNLL-U treebank.
     * @ingroup conllu
     */
    class Treebank final {
        std::deque<std::string> columns_{};
        std::vector<std::string_view> column_views_{};
        std::unordered_map<std::string_view, std::size_t, internal::StringHash, std::equal_to<>> column_idx_{};

        std::vector<std::unique_ptr<Sentence>> sentences_{};
        std::unordered_map<std::string_view, std::size_t, internal::StringHash, std::equal_to<>> sentence_idx_{};

        friend class Parser;
        friend class Sentence;
        friend class Token;
        friend class Word;
        friend class Writer;

    public:
        /**
         * @name Constructors
         */
        /** @{ */
        /**
         * @brief Default constructor. Initializes an empty treebank.
         */
        constexpr Treebank() noexcept = default;
        /**
         * @brief Copy construction is disabled.
         */
        constexpr Treebank(const Treebank&) noexcept = delete;
        /**
         * @brief Move construction is disabled.
         */
        constexpr Treebank(Treebank&& other) noexcept = delete;
        /** @} */
        /**
         * @name Columns
         */
        /** @{ */
        /**
         * @brief Column names, in declaration order.
         */
        [[nodiscard]] constexpr const std::vector<std::string_view>& columns() const noexcept;
        /**
         * @brief Number of columns in the treebank.
         */
        [[nodiscard]] constexpr std::size_t num_columns() const noexcept;
        /**
         * @brief Looks up a column's position by name.
         * @param[in] name Column name to search for.
         * @return Position of the column, or std::nullopt if no such column exists.
         */
        [[nodiscard]] constexpr std::optional<std::size_t> get_column_pos(std::string_view name) const noexcept;
        /**
         * @brief Looks up a column's name by position.
         * @param[in] position Column position to search for.
         * @return Column name, or std::nullopt if @p position is out of range.
         */
        [[nodiscard]] constexpr std::optional<std::string_view> get_column_name(std::size_t position) const noexcept;
        /**
         * @brief Appends a new column, extending every existing token with an empty field for it.
         * @param[in] name Name of the column to add.
         * @return Position of the newly added column, or an error_code on failure.
         */
        constexpr std::expected<std::size_t, std::error_code> add_column(std::string_view name) noexcept;
        /** @} */
        /**
         * @name Sentences
         */
        /** @{ */
        /**
         * @brief View over the sentences, in treebank order.
         * @return A lazy view yielding a non-owning `Sentence*` for each sentence in the treebank.
         */
        [[nodiscard]] constexpr auto sentences() const noexcept;
        /**
         * @brief Number of sentences in the treebank.
         */
        [[nodiscard]] constexpr std::size_t num_sentences() const noexcept;
        /**
         * @brief Constructs and appends a new sentence.
         * @param[in] sent_id Unique identifier for the sentence.
         * @param[in] text Optional sentence text (`# text` metadata).
         * @return Pointer to the newly created sentence, or an error_code on failure.
         */
        [[nodiscard]] constexpr std::expected<Sentence*, std::error_code> add_sentence(
        std::string_view sent_id, std::string_view text = std::string_view()) noexcept;
        /**
         * @brief Looks up a sentence by its identifier.
         * @param[in] sent_id Sentence identifier to search for.
         * @return Pointer to the sentence, or std::nullopt if no such sentence exists.
         */
        [[nodiscard]] constexpr std::optional<Sentence*> get_sentence_by_id(std::string_view sent_id) const noexcept;
        /**
         * @brief Removes the sentence with the given identifier.
         * @param[in] sent_id Identifier of the sentence to remove.
         * @return Empty error_code on success, or an error describing the failure.
         */
        constexpr std::error_code remove_sentence(std::string_view sent_id) noexcept;
        /**
         * @brief Removes the given sentence.
         * @param[in] sentence Pointer to the sentence to remove.
         * @return Empty error_code on success, or an error describing the failure
         *         (including if @p sentence is null or does not belong to this treebank).
         */
        constexpr std::error_code remove_sentence(const Sentence* sentence) noexcept;
        /** @} */
        /**
         * @name State
         */
        /** @{ */
        /**
         * @brief Whether the treebank contains no sentences.
         */
        [[nodiscard]] constexpr bool empty() const noexcept;
        /**
         * @brief Removes all columns and sentences, resetting the treebank to its default-constructed state.
         */
        constexpr void clear() noexcept;
        /**
         * @brief Checks the treebank for structural inconsistencies.
         * @return Empty error_code on success, or an error describing the first issue found.
         */
        [[nodiscard]] constexpr std::error_code validate() const noexcept;
        /** @} */
        /**
         * @name Operators
         */
        /** @{ */
        /**
         * @brief Move assignment is disabled.
         */
        constexpr Treebank& operator=(Treebank&& other) noexcept = delete;
        /**
         * @brief Copy assignment is disabled.
         */
        constexpr Treebank& operator=(const Treebank&) noexcept = delete;
        /** @} */
        /**
         * @name Destructor
         */
        /** @{ */
        /**
         * @brief Destructor.
         */
        constexpr ~Treebank() noexcept = default;
        /** @} */
    };
} // namespace tg::conllu

#include "conllu/impl/sentence.ipp"
#include "conllu/impl/token.ipp"
#include "conllu/impl/treebank.ipp"
#include "conllu/impl/word.ipp"

#endif // TG_CONLLU_TREEBANK_HPP
