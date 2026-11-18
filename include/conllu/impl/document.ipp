/*
 * CoNLL-U - https://github.com/tugrulgungor/CoNLL-U
 *
 * Copyright (c) 2026. All rights reserved.
 * Tuğrul Güngör - https://conll-u.tugrulgungor.me
 *
 * Distributed under the MIT License.
 * https://opensource.org/license/mit/
 */

#ifndef TG_CONLLU_IMPL_DOCUMENT_IPP
#define TG_CONLLU_IMPL_DOCUMENT_IPP 1

#include "conllu/document.hpp"
#include "conllu/internal/validate.hpp"

namespace tg::conllu {
    inline constexpr auto Document::find_metadata_(const std::string_view key) noexcept {
        return std::ranges::find(metadata_, key, &std::pair<std::string, std::string>::first);
    }

    inline constexpr auto Document::find_metadata_(const std::string_view key) const noexcept {
        return std::ranges::find(metadata_, key, &std::pair<std::string, std::string>::first);
    }

    inline constexpr std::expected<Paragraph*, std::error_code> Document::add_paragraph_(
    const bool is_explicit) noexcept {
        try {
            std::unique_ptr<Paragraph> paragraph(new Paragraph(this, is_explicit));

            Paragraph* ptr = paragraph.get();
            paragraphs_.push_back(std::move(paragraph));

            return ptr;
        }
        catch (...) {
            return std::unexpected(make_error_code(ErrorCode::InsufficientMemory));
        }
    }

    // Constructors
    inline constexpr Document::Document(Treebank* treebank, const bool is_explicit) noexcept :
        explicit_(is_explicit), treebank_(treebank) {
    }

    // Treebank
    inline constexpr Treebank* Document::treebank() const noexcept {
        return treebank_;
    }

    // Identity
    inline constexpr bool Document::is_explicit() const noexcept {
        return explicit_;
    }

    inline constexpr std::optional<std::string_view> Document::get_id() const noexcept {
        if (id_.empty())
            return std::nullopt;

        return id_;
    }

    inline constexpr std::error_code Document::set_id(const std::optional<std::string_view> id) noexcept {
        const std::string_view value = id.value_or(std::string_view());
        if (!internal::is_boundary_id_valid(value))
            return make_error_code(ErrorCode::InvalidDocumentId);

        try {
            id_.assign(value);
        }
        catch (...) {
            return make_error_code(ErrorCode::InsufficientMemory);
        }

        explicit_ = true;
        return {};
    }

    // Metadata
    inline constexpr auto Document::metadata() const noexcept {
        return metadata_ |
               std::views::transform([](const auto& entry) noexcept -> std::pair<std::string_view, std::string_view> {
                   return {entry.first, entry.second};
               });
    }

    inline constexpr std::size_t Document::num_metadata_entries() const noexcept {
        return metadata_.size();
    }

    inline constexpr std::optional<std::string_view> Document::get_metadata(const std::string_view key) const noexcept {
        if (const auto it = find_metadata_(key); metadata_.end() != it)
            return it->second;

        return std::nullopt;
    }

    inline constexpr std::error_code Document::set_metadata(const std::string_view key,
                                                            const std::string_view value) noexcept {
        if (key.empty())
            return make_error_code(ErrorCode::InvalidSentenceMetaKey);

        if ("newdoc" == key || "newdoc id" == key || "newpar" == key || "newpar id" == key || "sent_id" == key ||
            "text" == key)
            return make_error_code(ErrorCode::ReservedMetaKey);

        if (metadata_.end() != find_metadata_(key))
            return make_error_code(ErrorCode::DuplicateSentenceMetaEntry);

        try {
            metadata_.emplace_back(key, value);
            return {};
        }
        catch (...) {
            return make_error_code(ErrorCode::InsufficientMemory);
        }
    }

    inline constexpr std::error_code Document::set_metadata_from_comment_line(std::string_view line) noexcept {
        if (line.empty() || '#' != line[0])
            return make_error_code(ErrorCode::InvalidSentenceMetaLine);

        std::string_view key;
        std::string_view value;

        Sentence::split_metadata_line_(line, key, value);

        if (line.empty())
            return {};

        if (key.empty())
            return make_error_code(ErrorCode::InvalidSentenceMetaKey);

        return set_metadata(key, value);
    }

    inline constexpr void Document::unset_metadata(const std::string_view key) noexcept {
        if (const auto it = find_metadata_(key); metadata_.end() != it)
            metadata_.erase(it);
    }

    inline constexpr void Document::remove_all_metadata() noexcept {
        metadata_.clear();
    }

    // Paragraphs
    inline constexpr auto Document::paragraphs() const noexcept {
        return std::views::transform(
        paragraphs_,
        [](const std::unique_ptr<Paragraph>& paragraph) noexcept -> Paragraph* { return paragraph.get(); });
    }

    inline constexpr std::size_t Document::num_paragraphs() const noexcept {
        return paragraphs_.size();
    }

    inline constexpr std::expected<Paragraph*, std::error_code> Document::add_paragraph(
    const std::optional<std::string_view> id) noexcept {
        if (!internal::is_boundary_id_valid(id.value_or(std::string_view())))
            return std::unexpected(make_error_code(ErrorCode::InvalidParagraphId));

        const auto res = add_paragraph_(true);
        if (!res)
            return res;

        if (const auto err = (*res)->set_id(id)) {
            paragraphs_.pop_back();
            return std::unexpected(err);
        }

        return res;
    }

    inline constexpr std::error_code Document::remove_paragraph(const Paragraph* paragraph) noexcept {
        if (nullptr == paragraph)
            return make_error_code(ErrorCode::InvalidPointer);

        const auto it = std::ranges::find(paragraphs_, paragraph, &std::unique_ptr<Paragraph>::get);
        if (paragraphs_.end() == it)
            return make_error_code(ErrorCode::ParagraphNotFoundInDocument);

        treebank_->forget_sentences_(paragraph);
        paragraphs_.erase(it);

        return {};
    }

    // Sentences
    inline constexpr auto Document::sentences() const noexcept {
        return paragraphs_ |
               std::views::transform(
               [](const std::unique_ptr<Paragraph>& paragraph) noexcept
               -> const std::vector<std::unique_ptr<Sentence>>& { return paragraph->sentences_; }) |
               std::views::join |
               std::views::transform(
               [](const std::unique_ptr<Sentence>& sentence) noexcept -> Sentence* { return sentence.get(); });
    }

    inline constexpr std::size_t Document::num_sentences() const noexcept {
        std::size_t count{0};
        for (const std::unique_ptr<Paragraph>& paragraph : paragraphs_)
            count += paragraph->sentences_.size();

        return count;
    }

    // Structure
    inline constexpr std::expected<Document*, std::error_code> Document::split(Sentence* first) noexcept {
        if (nullptr == first)
            return std::unexpected(make_error_code(ErrorCode::InvalidPointer));

        Paragraph* paragraph = first->paragraph_;
        if (nullptr == paragraph || this != paragraph->document_)
            return std::unexpected(make_error_code(ErrorCode::SentenceNotFoundInDocument));

        const auto par_it = std::ranges::find(paragraphs_, paragraph, &std::unique_ptr<Paragraph>::get);
        const auto at = std::ranges::find(paragraph->sentences_, first, &std::unique_ptr<Sentence>::get);
        const bool starts_paragraph = paragraph->sentences_.begin() == at;

        // `first` already starts this document: every sentence before it is in an empty paragraph.
        if (starts_paragraph && std::all_of(paragraphs_.begin(), par_it,
                                            [](const std::unique_ptr<Paragraph>& p) { return p->sentences_.empty(); }))
        {
            explicit_ = true;
            return this;
        }

        // Paragraphs moving as a whole: from `paragraph` on if `first` starts it, otherwise the ones after it.
        const auto move_from = starts_paragraph ? par_it : std::next(par_it);

        auto& documents = treebank_->documents_;
        const auto self = std::ranges::find(documents, this, &std::unique_ptr<Document>::get);
        const auto self_idx = static_cast<std::size_t>(std::distance(documents.begin(), self));

        std::unique_ptr<Document> tail{};
        std::unique_ptr<Paragraph> head{};
        try {
            tail.reset(new Document(treebank_, true));
            tail->paragraphs_.reserve(static_cast<std::size_t>(std::distance(move_from, paragraphs_.end())) +
                                      (starts_paragraph ? std::size_t{0} : std::size_t{1}));

            // The rest of the split paragraph becomes the new document's first paragraph. It had no `# newpar` in
            // the middle of a paragraph, so it is implicit.
            if (!starts_paragraph) {
                head.reset(new Paragraph(tail.get(), false));
                head->sentences_.reserve(static_cast<std::size_t>(std::distance(at, paragraph->sentences_.end())));
            }

            documents.reserve(documents.size() + std::size_t{1});
        }
        catch (...) {
            return std::unexpected(make_error_code(ErrorCode::InsufficientMemory));
        }

        // Nothing below allocates: the moves and the insertion fit in the reserved capacity.
        if (head) {
            for (auto it = at; paragraph->sentences_.end() != it; ++it) {
                (*it)->paragraph_ = head.get();
                head->sentences_.push_back(std::move(*it));
            }
            paragraph->sentences_.erase(at, paragraph->sentences_.end());

            tail->paragraphs_.push_back(std::move(head));
        }

        for (auto it = move_from; paragraphs_.end() != it; ++it) {
            (*it)->document_ = tail.get();
            tail->paragraphs_.push_back(std::move(*it));
        }
        paragraphs_.erase(move_from, paragraphs_.end());

        Document* ptr = tail.get();
        documents.insert(documents.begin() + static_cast<std::ptrdiff_t>(self_idx + std::size_t{1}), std::move(tail));

        return ptr;
    }

    inline constexpr std::error_code Document::merge_next() noexcept {
        auto& documents = treebank_->documents_;
        const auto self = std::ranges::find(documents, this, &std::unique_ptr<Document>::get);

        const auto next_it = std::next(self);
        if (documents.end() == next_it)
            return make_error_code(ErrorCode::NothingToMerge);

        Document* next = next_it->get();

        // Without its `# newdoc`, an implicit first paragraph has no boundary of its own: it continues ours.
        const bool continues =
        !paragraphs_.empty() && !next->paragraphs_.empty() && !next->paragraphs_.front()->explicit_;

        Paragraph* last = continues ? paragraphs_.back().get() : nullptr;
        try {
            if (continues)
                last->sentences_.reserve(last->sentences_.size() + next->paragraphs_.front()->sentences_.size());

            paragraphs_.reserve(paragraphs_.size() + next->paragraphs_.size());
        }
        catch (...) {
            return make_error_code(ErrorCode::InsufficientMemory);
        }

        auto from = next->paragraphs_.begin();
        if (continues) {
            for (std::unique_ptr<Sentence>& sentence : (*from)->sentences_) {
                sentence->paragraph_ = last;
                last->sentences_.push_back(std::move(sentence));
            }
            ++from;
        }

        for (; next->paragraphs_.end() != from; ++from) {
            (*from)->document_ = this;
            paragraphs_.push_back(std::move(*from));
        }

        documents.erase(next_it);

        return {};
    }

    // State
    inline constexpr bool Document::empty() const noexcept {
        return std::ranges::all_of(paragraphs_,
                                   [](const std::unique_ptr<Paragraph>& paragraph) { return paragraph->empty(); });
    }
} // namespace tg::conllu

#endif // TG_CONLLU_IMPL_DOCUMENT_IPP
