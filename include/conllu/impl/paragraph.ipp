/*
 * CoNLL-U - https://github.com/tugrulgungor/CoNLL-U
 *
 * Copyright (c) 2026. All rights reserved.
 * Tuğrul Güngör - https://conll-u.tugrulgungor.me
 *
 * Distributed under the MIT License.
 * https://opensource.org/license/mit/
 */

#ifndef TG_CONLLU_IMPL_PARAGRAPH_IPP
#define TG_CONLLU_IMPL_PARAGRAPH_IPP 1

#include "conllu/internal/validate.hpp"
#include "conllu/paragraph.hpp"

namespace tg::conllu {
    // Constructors
    inline constexpr Paragraph::Paragraph(Document* document, const bool is_explicit) noexcept :
        explicit_(is_explicit), document_(document) {
    }

    // Document
    inline constexpr Document* Paragraph::document() const noexcept {
        return document_;
    }

    // Identity
    inline constexpr bool Paragraph::is_explicit() const noexcept {
        return explicit_;
    }

    inline constexpr std::optional<std::string_view> Paragraph::get_id() const noexcept {
        if (id_.empty())
            return std::nullopt;

        return id_;
    }

    inline constexpr std::error_code Paragraph::set_id(const std::optional<std::string_view> id) noexcept {
        const std::string_view value = id.value_or(std::string_view());
        if (!internal::is_boundary_id_valid(value))
            return make_error_code(ErrorCode::InvalidParagraphId);

        try {
            id_.assign(value);
        }
        catch (...) {
            return make_error_code(ErrorCode::InsufficientMemory);
        }

        explicit_ = true;
        return {};
    }

    // Sentences
    inline constexpr auto Paragraph::sentences() const noexcept {
        return std::views::transform(
        sentences_, [](const std::unique_ptr<Sentence>& sentence) noexcept -> Sentence* { return sentence.get(); });
    }

    inline constexpr std::size_t Paragraph::num_sentences() const noexcept {
        return sentences_.size();
    }

    inline constexpr std::expected<Sentence*, std::error_code> Paragraph::add_sentence(
    const std::string_view sent_id, const std::string_view text) noexcept {
        return document_->treebank_->insert_sentence_(this, sent_id, text);
    }

    // Structure
    inline constexpr std::expected<Paragraph*, std::error_code> Paragraph::split(Sentence* first) noexcept {
        if (nullptr == first)
            return std::unexpected(make_error_code(ErrorCode::InvalidPointer));

        if (this != first->paragraph_)
            return std::unexpected(make_error_code(ErrorCode::SentenceNotFoundInParagraph));

        const auto at = std::ranges::find(sentences_, first, &std::unique_ptr<Sentence>::get);
        if (sentences_.begin() == at) {
            explicit_ = true;
            return this;
        }

        auto& siblings = document_->paragraphs_;
        const auto self = std::ranges::find(siblings, this, &std::unique_ptr<Paragraph>::get);
        const auto self_idx = static_cast<std::size_t>(std::distance(siblings.begin(), self));

        std::unique_ptr<Paragraph> tail{};
        try {
            tail.reset(new Paragraph(document_, true));
            tail->sentences_.reserve(static_cast<std::size_t>(std::distance(at, sentences_.end())));
            siblings.reserve(siblings.size() + std::size_t{1});
        }
        catch (...) {
            return std::unexpected(make_error_code(ErrorCode::InsufficientMemory));
        }

        // Nothing below allocates: the moves and the insertion fit in the reserved capacity.
        for (auto it = at; sentences_.end() != it; ++it) {
            (*it)->paragraph_ = tail.get();
            tail->sentences_.push_back(std::move(*it));
        }
        sentences_.erase(at, sentences_.end());

        Paragraph* ptr = tail.get();
        siblings.insert(siblings.begin() + static_cast<std::ptrdiff_t>(self_idx + std::size_t{1}), std::move(tail));

        return ptr;
    }

    inline constexpr std::error_code Paragraph::merge_next() noexcept {
        auto& siblings = document_->paragraphs_;
        const auto self = std::ranges::find(siblings, this, &std::unique_ptr<Paragraph>::get);

        const auto next_it = std::next(self);
        if (siblings.end() == next_it)
            return make_error_code(ErrorCode::NothingToMerge);

        Paragraph* next = next_it->get();

        try {
            sentences_.reserve(sentences_.size() + next->sentences_.size());
        }
        catch (...) {
            return make_error_code(ErrorCode::InsufficientMemory);
        }

        for (std::unique_ptr<Sentence>& sentence : next->sentences_) {
            sentence->paragraph_ = this;
            sentences_.push_back(std::move(sentence));
        }

        siblings.erase(next_it);

        return {};
    }

    // State
    inline constexpr bool Paragraph::empty() const noexcept {
        return sentences_.empty();
    }
} // namespace tg::conllu

#endif // TG_CONLLU_IMPL_PARAGRAPH_IPP
