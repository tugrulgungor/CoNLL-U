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
 * @file conllu/writer.hpp
 */

#ifndef TG_CONLLU_WRITER_HPP
#define TG_CONLLU_WRITER_HPP 1

#include "conllu/treebank.hpp"

#include <filesystem>
#include <ostream>

namespace tg::conllu {
    /**
     * @struct WriterOptions
     * @brief Configuration options controlling how a Writer serializes a Treebank.
     * @ingroup conllu
     */
    struct WriterOptions final {
        /**
         * @brief Character used to represent an empty field value (default: `_`).
         */
        char empty_value_indicator{'_'};
    };

    /**
     * @class Writer
     * @brief Serializes a Treebank into CoNLL-U formatted output.
     * @ingroup conllu
     */
    class Writer final {
        WriterOptions options_{};

        [[nodiscard]] static std::error_code write_columns_(std::ostream &output, const Treebank *treebank) noexcept;
        [[nodiscard]] std::error_code write_sentence_(std::ostream &output, const Sentence *sentence) const noexcept;
        [[nodiscard]] static std::error_code write_token_(std::ostream &output, const Token *token) noexcept;
        [[nodiscard]] std::error_code write_word_(std::ostream &output, const Word *word) const noexcept;

        static std::ostream &write_(std::ostream &output) noexcept(false);

    public:
        /**
         * @name Constructors
         */
        /** @{ */
        /**
         * @brief Default constructor. Initializes a writer with default options.
         */
        constexpr Writer() noexcept = default;
        /**
         * @brief Constructs a writer with the given options.
         * @param[in] options Writing configuration to use.
         */
        explicit(true) constexpr Writer(WriterOptions options) noexcept;
        /** @} */
        /**
         * @name Writing
         */
        /** @{ */
        /**
         * @brief Writes a treebank in CoNLL-U format to a stream.
         * @param[in] treebank Treebank to serialize.
         * @param[in] stream Output stream to write to.
         * @return Empty error_code on success, or an error describing the first failure encountered.
         */
        [[nodiscard]] std::error_code write(const Treebank *treebank, std::ostream &&stream) const noexcept;
        /**
         * @brief Serializes @p treebank in CoNLL-U format and writes it to the file at @p path.
         * @param[in] treebank Treebank to serialize.
         * @param[in] path Path to the file to write.
         * @return Empty error_code on success, or an error describing why the file could not be
         * opened/written or why serialization failed.
         */
        [[nodiscard]] std::error_code write_file(const Treebank *treebank,
                                                 const std::filesystem::path &path) const noexcept;
        /** @} */
        /**
         * @name Destructor
         */
        /** @{ */
        /**
         * @brief Destructor.
         */
        constexpr ~Writer() noexcept = default;
        /** @} */
    };
} // namespace tg::conllu

#include "conllu/impl/writer.ipp"

#endif // TG_CONLLU_WRITER_HPP
