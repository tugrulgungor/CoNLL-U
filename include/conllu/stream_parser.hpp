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
 * @file conllu/stream_parser.hpp
 */

#ifndef TG_CONLLU_STREAM_PARSER_HPP
#define TG_CONLLU_STREAM_PARSER_HPP 1

#include "conllu/treebank.hpp"

#include <algorithm>
#include <cstdint>
#include <expected>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace tg::conllu {
    /**
     * @struct ParserOptions
     * @brief Configuration options controlling how CoNLL-U input is interpreted.
     * @ingroup conllu
     */
    struct ParserOptions final {
        /**
         * @brief Character used to represent an empty field value (default: `_`).
         */
        char empty_value_indicator{'_'};
    };

    /**
     * @class StreamParser
     * @brief Incremental (push) parser: builds a Treebank in memory from CoNLL-U text fed in arbitrary chunks.
     * @ingroup conllu
     *
     * Chunk boundaries may fall anywhere, including mid-line. Each sentence is parsed into the treebank as soon as
     * the blank line that closes it has been fed; finish() parses a final sentence that lacks a trailing blank line
     * and hands over the treebank. Both `\n` and `\r\n` line endings are accepted, and runs of blank lines between
     * sentences are ignored.
     *
     * The first error is sticky: every subsequent feed() and finish() returns it until reset() is called.
     */
    class StreamParser final {
        enum class State : std::uint8_t {
            ExpectHeader,
            Body,
            Failed,
        };

        ParserOptions options_{};

        std::unique_ptr<Treebank> treebank_{};

        State state_{State::ExpectHeader};
        std::error_code error_{};

        std::size_t line_{0};
        std::size_t error_line_{0};

        std::string carry_{};
        std::string block_{};
        std::vector<std::size_t> block_line_sizes_{};
        std::size_t block_first_line_{0};
        std::vector<std::string_view> block_lines_{};

        [[nodiscard]] std::error_code fail_(std::error_code err, std::size_t line) noexcept;
        [[nodiscard]] std::error_code ensure_treebank_() noexcept;

        [[nodiscard]] std::error_code consume_line_(std::string_view line) noexcept(false);
        [[nodiscard]] std::error_code flush_block_() noexcept(false);
        void clear_block_() noexcept;

        [[nodiscard]] static std::optional<std::error_code> parse_columns_(std::string_view line,
                                                                           Treebank* treebank) noexcept;
        [[nodiscard]] std::error_code parse_sentence_(std::vector<std::string_view>& lines,
                                                      Treebank* treebank) const noexcept;

        [[nodiscard]] static std::size_t count_sentence_meta_lines_(
        const std::vector<std::string_view>& lines) noexcept;
        [[nodiscard]] static std::optional<std::string_view> find_sentence_sent_id_(
        std::vector<std::string_view>& lines, std::size_t& limit) noexcept;

        [[nodiscard]] std::expected<Token*, std::error_code> parse_token_(const Treebank* treebank, Sentence* sentence,
                                                                          Token* last_token,
                                                                          std::string_view line) const noexcept;

        [[nodiscard]] std::error_code parse_word_(const Treebank* treebank, Sentence* sentence, const Token* last_token,
                                                  const std::vector<std::string_view>& field_values,
                                                  std::string_view id) const noexcept;

        [[nodiscard]] std::optional<std::string_view> value_view_(std::string_view value) const noexcept;

    public:
        /**
         * @name Constructors
         */
        /** @{ */
        /**
         * @brief Default constructor. Initializes a stream parser with default options.
         */
        StreamParser() noexcept = default;
        /**
         * @brief Constructs a stream parser with the given options.
         * @param[in] options Parsing configuration to use.
         */
        explicit(true) StreamParser(ParserOptions options) noexcept;
        /** @} */
        /**
         * @name Parsing
         */
        /** @{ */
        /**
         * @brief Feeds the next chunk of CoNLL-U formatted text.
         * @param[in] chunk Next piece of input; may be empty and may end mid-line. It is not retained after the call.
         * @return Empty error_code on success, or the (sticky) error describing the first failure encountered.
         */
        [[nodiscard]] std::error_code feed(std::string_view chunk) noexcept;
        /**
         * @brief Signals end of input: parses any pending sentence and hands over the treebank.
         *
         * On success the parser is reset and can be reused for a new input. On failure the error stays sticky, and
         * the partial treebank remains available through treebank() until reset() is called.
         * @return The parsed treebank, or an error_code describing the first failure encountered.
         */
        [[nodiscard]] std::expected<std::unique_ptr<Treebank>, std::error_code> finish() noexcept;
        /**
         * @brief Discards all input, state, errors and the treebank under construction.
         */
        void reset() noexcept;
        /** @} */
        /**
         * @name Progress
         */
        /** @{ */
        /**
         * @brief The treebank built so far (complete sentences only), or nullptr if nothing has been fed yet.
         */
        [[nodiscard]] const Treebank* treebank() const noexcept;
        /**
         * @brief Number of complete lines consumed so far.
         */
        [[nodiscard]] std::size_t num_lines() const noexcept;
        /**
         * @brief The sticky error, or an empty error_code if none has occurred.
         */
        [[nodiscard]] std::error_code error() const noexcept;
        /**
         * @brief 1-based line number locating the error: the first line of the sentence that failed to parse, or 0
         * if there is no error.
         */
        [[nodiscard]] std::size_t error_line() const noexcept;
        /** @} */
        /**
         * @name Destructor
         */
        /** @{ */
        /**
         * @brief Destructor.
         */
        ~StreamParser() noexcept = default;
        /** @} */
    };
} // namespace tg::conllu

#include "conllu/impl/stream_parser.ipp"

#endif // TG_CONLLU_STREAM_PARSER_HPP
