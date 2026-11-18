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
 * @file conllu/document.hpp
 */

#ifndef TG_CONLLU_DOCUMENT_HPP
#define TG_CONLLU_DOCUMENT_HPP 1

#include "conllu/error.hpp"
#include "conllu/paragraph.hpp"
#include "conllu/sentence.hpp"

#include <expected>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tg::conllu {
    class Treebank;

    /**
     * @class Document
     * @brief A document within a Treebank: an ordered list of paragraphs.
     * @ingroup conllu
     *
     * A document is explicit when it is marked in the file with `# newdoc` (optionally `# newdoc id = ...`), and
     * implicit otherwise. Only the first document of a treebank can be implicit: it holds the sentences before the
     * first `# newdoc`, and nothing marks it in the output. Documents created through the API are always explicit.
     * Empty documents are allowed, and skipped by the Writer.
     *
     * Document comments are the comment lines between `# newdoc` and the `# newpar` of the document's first sentence
     * (e.g. `# meta::title = ...`). When `# newpar` does not follow, the lines after `# newdoc` belong to the
     * sentence. The Writer adds bare `# newdoc` and `# newpar` lines where they are needed to keep the document
     * comments in place.
     */
    class Document final {
        std::string id_{};
        bool explicit_;

        std::vector<std::pair<std::string, std::string>> metadata_{};

        std::vector<std::unique_ptr<Paragraph>> paragraphs_{};

        Treebank* treebank_;

        explicit(true) constexpr Document(Treebank* treebank, bool is_explicit) noexcept;

        [[nodiscard]] constexpr auto find_metadata_(std::string_view key) noexcept;
        [[nodiscard]] constexpr auto find_metadata_(std::string_view key) const noexcept;

        [[nodiscard]] constexpr std::expected<Paragraph*, std::error_code> add_paragraph_(bool is_explicit) noexcept;

        friend class Paragraph;
        friend class StreamParser;
        friend class Treebank;
        friend class Writer;

    public:
        /**
         * @name Constructors
         */
        /** @{ */
        /**
         * @brief Default construction is disabled; documents are created via Treebank::add_document.
         */
        constexpr Document() noexcept = delete;
        /**
         * @brief Copy construction is disabled.
         */
        constexpr Document(const Document&) noexcept = delete;
        /**
         * @brief Move construction is disabled.
         */
        constexpr Document(Document&&) noexcept = delete;
        /** @} */
        /**
         * @name Treebank
         */
        /** @{ */
        /**
         * @brief Pointer to the treebank that owns this document.
         */
        [[nodiscard]] constexpr Treebank* treebank() const noexcept;
        /** @} */
        /**
         * @name Identity
         */
        /** @{ */
        /**
         * @brief Whether the document is marked with `# newdoc` in the file.
         */
        [[nodiscard]] constexpr bool is_explicit() const noexcept;
        /**
         * @brief The document id (`# newdoc id = ...`), or std::nullopt if it has none.
         */
        [[nodiscard]] constexpr std::optional<std::string_view> get_id() const noexcept;
        /**
         * @brief Sets the document id, making the document explicit.
         * @param[in] id New id; std::nullopt or empty for a bare `# newdoc`.
         * @return Empty error_code on success, or an error describing the failure.
         */
        constexpr std::error_code set_id(std::optional<std::string_view> id = std::nullopt) noexcept;
        /** @} */
        /**
         * @name Metadata
         * Document comments, with the same semantics as Sentence's metadata.
         */
        /** @{ */
        /**
         * @brief View over the document comments, as key/value pairs in file order. A bare `# key` line has an
         * empty value.
         */
        [[nodiscard]] constexpr auto metadata() const noexcept;
        /**
         * @brief Number of document comments.
         */
        [[nodiscard]] constexpr std::size_t num_metadata_entries() const noexcept;
        /**
         * @brief Looks up a document comment by key.
         * @param[in] key Comment key to search for.
         * @return Comment value, or std::nullopt if no such key exists.
         */
        [[nodiscard]] constexpr std::optional<std::string_view> get_metadata(std::string_view key) const noexcept;
        /**
         * @brief Adds a new document comment.
         * @param[in] key Comment key; must be non-empty, not already present, and not reserved (`newdoc`, `newpar`,
         * `sent_id`, `text`).
         * @param[in] value Comment value.
         * @return Empty error_code on success, or an error describing the failure.
         */
        [[nodiscard]] constexpr std::error_code set_metadata(std::string_view key,
                                                             std::string_view value = std::string_view{}) noexcept;
        /**
         * @brief Parses a `#`-prefixed comment line and adds it as a document comment.
         * @param[in] line Comment line, in `# key = value` or `# key` form.
         * @return Empty error_code on success, or an error describing the failure.
         */
        [[nodiscard]] constexpr std::error_code set_metadata_from_comment_line(std::string_view line) noexcept;
        /**
         * @brief Removes a document comment, if present.
         * @param[in] key Comment key to remove.
         */
        constexpr void unset_metadata(std::string_view key) noexcept;
        /**
         * @brief Removes all document comments.
         */
        constexpr void remove_all_metadata() noexcept;
        /** @} */
        /**
         * @name Paragraphs
         */
        /** @{ */
        /**
         * @brief View over the document's paragraphs, in order.
         */
        [[nodiscard]] constexpr auto paragraphs() const noexcept;
        /**
         * @brief Number of paragraphs in the document.
         */
        [[nodiscard]] constexpr std::size_t num_paragraphs() const noexcept;
        /**
         * @brief Constructs a new, empty, explicit paragraph and appends it to the document.
         * @param[in] id Paragraph id; std::nullopt or empty for a bare `# newpar`.
         * @return Pointer to the newly created paragraph, or an error_code on failure.
         */
        [[nodiscard]] constexpr std::expected<Paragraph*, std::error_code> add_paragraph(
        std::optional<std::string_view> id = std::nullopt) noexcept;
        /**
         * @brief Removes the given paragraph and its sentences.
         * @param[in] paragraph Pointer to the paragraph to remove.
         * @return Empty error_code on success, or an error describing the failure.
         */
        constexpr std::error_code remove_paragraph(const Paragraph* paragraph) noexcept;
        /** @} */
        /**
         * @name Sentences
         */
        /** @{ */
        /**
         * @brief View over the sentences of all the document's paragraphs, in order.
         */
        [[nodiscard]] constexpr auto sentences() const noexcept;
        /**
         * @brief Number of sentences in the document.
         */
        [[nodiscard]] constexpr std::size_t num_sentences() const noexcept;
        /** @} */
        /**
         * @name Structure
         */
        /** @{ */
        /**
         * @brief Splits the document so that a new document starts at @p first.
         *
         * @p first and everything after it move to a new explicit document without an id, inserted right after this
         * one; the paragraph containing @p first is split at the same place. Document comments stay with this
         * document. If @p first already starts this document, nothing moves: this document is made explicit and
         * returned.
         * @param[in] first Sentence of this document that starts the new document.
         * @return Pointer to the document starting at @p first, or an error_code on failure.
         */
        [[nodiscard]] constexpr std::expected<Document*, std::error_code> split(Sentence* first) noexcept;
        /**
         * @brief Moves the paragraphs of the next document to the end of this one, and removes the next document
         * along with its id and comments.
         *
         * As when its `# newdoc` line is deleted from a file, an implicit first paragraph of the next document
         * continues this document's last paragraph.
         * @return Empty error_code on success, or ErrorCode::NothingToMerge for the treebank's last document.
         */
        constexpr std::error_code merge_next() noexcept;
        /** @} */
        /**
         * @name State
         */
        /** @{ */
        /**
         * @brief Whether the document contains no sentences.
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
        constexpr Document& operator=(const Document&) noexcept = delete;
        /**
         * @brief Move assignment is disabled.
         */
        constexpr Document& operator=(Document&&) noexcept = delete;
        /** @} */
        /**
         * @name Destructor
         */
        /** @{ */
        /**
         * @brief Destructor.
         */
        ~Document() noexcept = default;
        /** @} */
    };
} // namespace tg::conllu

#endif // TG_CONLLU_DOCUMENT_HPP
