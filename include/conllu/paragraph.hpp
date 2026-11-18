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
 * @file conllu/paragraph.hpp
 */

#ifndef TG_CONLLU_PARAGRAPH_HPP
#define TG_CONLLU_PARAGRAPH_HPP 1

#include "conllu/error.hpp"
#include "conllu/sentence.hpp"

#include <expected>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace tg::conllu {
    class Document;

    /**
     * @class Paragraph
     * @brief A paragraph within a Document: an ordered list of sentences.
     * @ingroup conllu
     *
     * A paragraph is explicit when it is marked in the file with `# newpar` (optionally `# newpar id = ...`), and
     * implicit otherwise. Only the first paragraph of a document can be implicit: it holds the sentences before the
     * document's first `# newpar`, and nothing marks it in the output. Paragraphs created through the API are always
     * explicit. Empty paragraphs are allowed, and skipped by the Writer.
     */
    class Paragraph final {
        std::string id_{};
        bool explicit_;

        std::vector<std::unique_ptr<Sentence>> sentences_{};

        Document* document_;

        explicit(true) constexpr Paragraph(Document* document, bool is_explicit) noexcept;

        friend class Document;
        friend class StreamParser;
        friend class Treebank;
        friend class Writer;

    public:
        /**
         * @name Constructors
         */
        /** @{ */
        /**
         * @brief Default construction is disabled; paragraphs are created via Document::add_paragraph.
         */
        constexpr Paragraph() noexcept = delete;
        /**
         * @brief Copy construction is disabled.
         */
        constexpr Paragraph(const Paragraph&) noexcept = delete;
        /**
         * @brief Move construction is disabled.
         */
        constexpr Paragraph(Paragraph&&) noexcept = delete;
        /** @} */
        /**
         * @name Document
         */
        /** @{ */
        /**
         * @brief Pointer to the document that owns this paragraph.
         */
        [[nodiscard]] constexpr Document* document() const noexcept;
        /** @} */
        /**
         * @name Identity
         */
        /** @{ */
        /**
         * @brief Whether the paragraph is marked with `# newpar` in the file.
         */
        [[nodiscard]] constexpr bool is_explicit() const noexcept;
        /**
         * @brief The paragraph id (`# newpar id = ...`), or std::nullopt if it has none.
         */
        [[nodiscard]] constexpr std::optional<std::string_view> get_id() const noexcept;
        /**
         * @brief Sets the paragraph id, making the paragraph explicit.
         * @param[in] id New id; std::nullopt or empty for a bare `# newpar`.
         * @return Empty error_code on success, or an error describing the failure.
         */
        constexpr std::error_code set_id(std::optional<std::string_view> id = std::nullopt) noexcept;
        /** @} */
        /**
         * @name Sentences
         */
        /** @{ */
        /**
         * @brief View over the paragraph's sentences, in order.
         */
        [[nodiscard]] constexpr auto sentences() const noexcept;
        /**
         * @brief Number of sentences in the paragraph.
         */
        [[nodiscard]] constexpr std::size_t num_sentences() const noexcept;
        /**
         * @brief Constructs a new sentence and appends it to the paragraph.
         * @param[in] sent_id Unique identifier for the sentence.
         * @param[in] text Optional sentence text (`# text` metadata).
         * @return Pointer to the newly created sentence, or an error_code on failure.
         */
        [[nodiscard]] constexpr std::expected<Sentence*, std::error_code> add_sentence(
        std::string_view sent_id, std::string_view text = std::string_view()) noexcept;
        /** @} */
        /**
         * @name Structure
         */
        /** @{ */
        /**
         * @brief Splits the paragraph so that a new paragraph starts at @p first.
         *
         * @p first and the sentences after it move to a new explicit paragraph without an id, inserted right after
         * this one. If @p first already starts this paragraph, nothing moves: this paragraph is made explicit and
         * returned.
         * @param[in] first Sentence of this paragraph that starts the new paragraph.
         * @return Pointer to the paragraph starting at @p first, or an error_code on failure.
         */
        [[nodiscard]] constexpr std::expected<Paragraph*, std::error_code> split(Sentence* first) noexcept;
        /**
         * @brief Moves the sentences of the next paragraph in the same document to the end of this one, and removes
         * the next paragraph.
         * @return Empty error_code on success, or ErrorCode::NothingToMerge for the document's last paragraph.
         */
        constexpr std::error_code merge_next() noexcept;
        /** @} */
        /**
         * @name State
         */
        /** @{ */
        /**
         * @brief Whether the paragraph contains no sentences.
         */
        [[nodiscard]] constexpr bool empty() const noexcept;
        /** @} */
        /**
         * @name Operators
         */
        /** @{ */
        /**
         * @brief Copy assignment is disabled.
         */
        constexpr Paragraph& operator=(const Paragraph&) noexcept = delete;
        /**
         * @brief Move assignment is disabled.
         */
        constexpr Paragraph& operator=(Paragraph&&) noexcept = delete;
        /** @} */
        /**
         * @name Destructor
         */
        /** @{ */
        /**
         * @brief Destructor.
         */
        ~Paragraph() noexcept = default;
        /** @} */
    };
} // namespace tg::conllu

#endif // TG_CONLLU_PARAGRAPH_HPP
