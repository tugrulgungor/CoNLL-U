/*
 * CoNLL-U - https://github.com/tugrulgungor/CoNLL-U
 *
 * Copyright (c) 2026. All rights reserved.
 * Tuğrul Güngör - https://conll-u.tugrulgungor.me
 *
 * Distributed under the MIT License.
 * https://opensource.org/license/mit/
 */

#ifndef TG_CONLLU_IMPL_TREEBANK_IPP
#define TG_CONLLU_IMPL_TREEBANK_IPP 1

#include "conllu/internal/validate.hpp"
#include "conllu/treebank.hpp"

namespace tg::conllu {
    // Columns
    inline constexpr const std::vector<std::string_view>& Treebank::columns() const noexcept {
        return column_views_;
    }

    inline constexpr std::size_t Treebank::num_columns() const noexcept {
        return columns_.size();
    }

    inline constexpr std::optional<std::size_t> Treebank::get_column_pos(const std::string_view name) const noexcept {
        if (const auto it = column_idx_.find(name); column_idx_.end() != it)
            return it->second;

        return std::nullopt;
    }

    inline constexpr std::optional<std::string_view> Treebank::get_column_name(
    const std::size_t position) const noexcept {
        if (columns_.size() <= position)
            return std::nullopt;

        return columns_[position];
    }

    inline constexpr std::expected<std::size_t, std::error_code> Treebank::add_column(
    const std::string_view name) noexcept {
        if (!internal::is_column_name_valid(name))
            return std::unexpected(make_error_code(ErrorCode::InvalidColumnName));

        if (column_idx_.contains(name))
            return std::unexpected(make_error_code(ErrorCode::DuplicateColumnName));

        const std::size_t idx = columns_.size();

        const std::size_t new_column_count = columns_.size() + std::size_t{1};

        try {
            for (const std::unique_ptr<Sentence>& sentence : sentences_) {
                for (const std::unique_ptr<Token>& token : sentence->tokens_)
                    token->fields_.emplace_back(std::nullopt);
            }
        }
        catch (...) {
            for (const std::unique_ptr<Sentence>& sentence : sentences_) {
                for (const std::unique_ptr<Token>& token : sentence->tokens_) {
                    if (new_column_count == token->fields_.size())
                        token->fields_.pop_back();
                }
            }

            return std::unexpected(make_error_code(ErrorCode::InsufficientMemory));
        }

        try {
            columns_.emplace_back(name);
            column_views_.emplace_back(columns_.back());
            column_idx_.emplace(columns_.back(), idx);

            return idx;
        }
        catch (...) {
            if (idx != columns_.size())
                columns_.pop_back();

            if (idx != column_views_.size())
                column_views_.pop_back();

            return std::unexpected(make_error_code(ErrorCode::InsufficientMemory));
        }
    }

    // Sentences
    inline constexpr auto Treebank::sentences() const noexcept {
        return std::views::transform(
        sentences_, [](const std::unique_ptr<Sentence>& sentence) noexcept -> Sentence* { return sentence.get(); });
    }

    inline constexpr std::expected<Sentence*, std::error_code> Treebank::add_sentence(
    const std::string_view sent_id, const std::string_view text) noexcept {
        if (!internal::is_sent_id_valid(sent_id))
            return std::unexpected(make_error_code(ErrorCode::InvalidSentenceId));

        if (sentence_idx_.contains(sent_id))
            return std::unexpected(make_error_code(ErrorCode::DuplicateSentenceId));

        const std::size_t idx = sentences_.size();

        try {
            // @TODO: Needs stronger exception safety (rollback).
            std::unique_ptr<Sentence> sentence(new Sentence(this));
            sentence->sent_id_.assign(sent_id);
            sentence->metadata_.emplace("sent_id", sent_id);

            if (!text.empty())
                sentence->text_.assign(text);

            const auto ptr = sentence.get();

            sentence_idx_.emplace(sentence->sent_id_, idx);
            sentences_.push_back(std::move(sentence));

            return ptr;
        }
        catch (...) {
            sentence_idx_.erase(sent_id);

            return std::unexpected(make_error_code(ErrorCode::InsufficientMemory));
        }
    }

    inline constexpr std::size_t Treebank::num_sentences() const noexcept {
        return sentences_.size();
    }

    inline constexpr std::optional<Sentence*> Treebank::get_sentence_by_id(
    const std::string_view sent_id) const noexcept {
        const auto it = sentence_idx_.find(sent_id);
        if (sentence_idx_.end() == it)
            return std::nullopt;

        const std::size_t idx = it->second;
        return sentences_[idx].get();
    }

    inline constexpr std::error_code Treebank::remove_sentence(const std::string_view sent_id) noexcept {
        const auto it = sentence_idx_.find(sent_id);
        if (sentence_idx_.end() == it)
            return make_error_code(ErrorCode::SentenceNotFoundInTreebank);

        const std::size_t idx = it->second;

        auto sent_it = sentences_.begin();
        std::advance(sent_it, idx);
        sent_it = sentences_.erase(sent_it);

        sentence_idx_.erase(it);

        while (sent_it != sentences_.end()) {
            sentence_idx_[sent_it->get()->sent_id_] -= std::size_t{1};
            sent_it = std::next(sent_it);
        }

        return {};
    }

    inline constexpr std::error_code Treebank::remove_sentence(const Sentence* sentence) noexcept {
        if (nullptr == sentence)
            return make_error_code(ErrorCode::InvalidPointer);

        if (std::ranges::any_of(sentences(), [sentence](const Sentence* s) noexcept -> bool { return sentence == s; }))
            return remove_sentence(sentence->sent_id_);

        return make_error_code(ErrorCode::SentenceNotFoundInTreebank);
    }

    // State
    inline constexpr bool Treebank::empty() const noexcept {
        return sentences_.empty();
    }

    inline constexpr void Treebank::clear() noexcept {
        column_idx_.clear();
        column_views_.clear();
        columns_.clear();

        sentence_idx_.clear();
        sentences_.clear();
    }

    inline constexpr std::error_code Treebank::validate() const noexcept {
        for (const Sentence* sentence : sentences()) {
            if (const auto err = sentence->validate())
                return err;
        }

        return {};
    }
} // namespace tg::conllu

#endif // TG_CONLLU_IMPL_TREEBANK_IPP
