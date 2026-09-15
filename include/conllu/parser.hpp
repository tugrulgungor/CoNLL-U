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
 * @file conllu/parser.hpp
 */

#ifndef TG_CONLLU_PARSER_HPP
#define TG_CONLLU_PARSER_HPP 1

#include "conllu/treebank.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <istream>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#if __has_include(<spanstream>)
#include <spanstream>
#endif // __has_include(<spanstream>)

namespace tg::conllu {
    /**
     * @struct ParserOptions
     * @brief Configuration options controlling how a Parser interprets CoNLL-U input.
     * @ingroup conllu
     */
    struct ParserOptions final {
        /**
         * @brief Character used to represent an empty field value (default: `_`).
         */
        char empty_value_indicator{'_'};
    };

    /**
     * @class Parser
     * @brief Parses CoNLL-U formatted input into a Treebank.
     * @ingroup conllu
     */
    class Parser final {
        ParserOptions options_{};
        std::string sb0_{};
        std::string sb1_{};
        std::vector<std::size_t> vb0_{};
        std::unordered_set<std::string_view> vb1_{};

        [[nodiscard]] std::error_code read_block_(std::istream &input, std::vector<std::string_view> &block) noexcept;

        [[nodiscard]] static std::optional<std::error_code> parse_columns_(std::string_view line,
                                                                           Treebank *treebank) noexcept;
        [[nodiscard]] std::error_code parse_sentence_(std::vector<std::string_view> &lines,
                                                      Treebank *treebank) const noexcept;

        [[nodiscard]] static std::size_t count_sentence_meta_lines_(
        const std::vector<std::string_view> &lines) noexcept;
        [[nodiscard]] static std::optional<std::string_view> find_sentence_sent_id_(
        std::vector<std::string_view> &lines, std::size_t &limit) noexcept;

        [[nodiscard]] std::expected<Token *, std::error_code> parse_token_(const Treebank *treebank, Sentence *sentence,
                                                                           Token *last_token,
                                                                           std::string_view line) const noexcept;

        [[nodiscard]] std::error_code parse_word_(const Treebank *treebank, Sentence *sentence, const Token *last_token,
                                                  const std::vector<std::string_view> &field_values,
                                                  std::string_view id) const noexcept;

        [[nodiscard]] std::optional<std::string_view> value_view_(std::string_view value) const noexcept;

    public:
        /**
         * @name Constructors
         */
        /** @{ */
        /**
         * @brief Default constructor. Initializes a parser with default options.
         */
        constexpr Parser() noexcept = default;
        /**
         * @brief Constructs a parser with the given options.
         * @param[in] options Parsing configuration to use.
         */
        explicit(true) constexpr Parser(ParserOptions options) noexcept;
        /** @} */
        /**
         * @name Parsing
         */
        /** @{ */
        /**
         * @brief Parses a CoNLL-U formatted stream into a new Treebank.
         * @param[in] stream Input stream to read from.
         * @return The parsed treebank, or an error_code describing the first failure encountered.
         */
        [[nodiscard]] std::expected<std::unique_ptr<Treebank>, std::error_code> parse(std::istream &&stream) noexcept;
        /**
         * @brief Opens the file at @p path and parses its contents as CoNLL-U formatted text into a new Treebank.
         * @param[in] path Path to the CoNLL-U file to read.
         * @return The parsed treebank, or an error_code describing why the file could not be opened/read or why parsing
         * failed.
         */
        [[nodiscard]] std::expected<std::unique_ptr<Treebank>, std::error_code> parse_file(
        const std::filesystem::path &path) noexcept;
        /**
         * @brief Parses CoNLL-U formatted text already held in memory into a new Treebank.
         * @param[in] data CoNLL-U formatted text to parse.
         * @return The parsed treebank, or an error_code describing the first failure encountered.
         */
        [[nodiscard]] std::expected<std::unique_ptr<Treebank>, std::error_code> parse_memory(
        std::string_view data) noexcept;
        /**
         * @brief Parses CoNLL-U formatted text from a raw byte buffer into a new Treebank.
         * @tparam DataType Element type of @p data; must be exactly one byte wide.
         * @param[in] data Pointer to the start of the buffer to parse.
         * @param[in] size Number of elements (bytes) pointed to by @p data.
         * @return The parsed treebank, or an error_code describing the first failure encountered.
         */
        template <typename DataType>
            requires(sizeof(DataType) == sizeof(std::uint8_t))
        [[nodiscard]] std::expected<std::unique_ptr<Treebank>, std::error_code> parse_memory(const DataType *data,
                                                                                             std::size_t size) noexcept;
        /** @} */
        /**
         * @name Destructor
         */
        /** @{ */
        /**
         * @brief Destructor.
         */
        constexpr ~Parser() noexcept = default;
        /** @} */
    };
} // namespace tg::conllu

#include "conllu/impl/parser.ipp"

#endif // TG_CONLLU_PARSER_HPP
