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

#include "conllu/stream_parser.hpp"
#include "conllu/treebank.hpp"

#include <expected>
#include <filesystem>
#include <istream>
#include <memory>
#include <string_view>

namespace tg::conllu {
    /**
     * @class Parser
     * @brief Parses complete CoNLL-U formatted input (a stream, file or memory buffer) into a Treebank.
     * @ingroup conllu
     *
     * A convenience wrapper around StreamParser, which it feeds with the whole input.
     */
    class Parser final {
        ParserOptions options_{};

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
        [[nodiscard]] std::expected<std::unique_ptr<Treebank>, std::error_code> parse(std::istream&& stream) noexcept;
        /**
         * @brief Opens the file at @p path and parses its contents as CoNLL-U formatted text into a new Treebank.
         * @param[in] path Path to the CoNLL-U file to read.
         * @return The parsed treebank, or an error_code describing why the file could not be opened/read or why parsing
         * failed.
         */
        [[nodiscard]] std::expected<std::unique_ptr<Treebank>, std::error_code> parse_file(
        const std::filesystem::path& path) noexcept;
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
        [[nodiscard]] std::expected<std::unique_ptr<Treebank>, std::error_code> parse_memory(const DataType* data,
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
