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
    inline constexpr std::expected<Document*, std::error_code> Treebank::add_document_(
    const bool is_explicit) noexcept {
        try {
            std::unique_ptr<Document> document(new Document(this, is_explicit));

            Document* ptr = document.get();
            documents_.push_back(std::move(document));

            return ptr;
        }
        catch (...) {
            return std::unexpected(make_error_code(ErrorCode::InsufficientMemory));
        }
    }

    inline constexpr std::expected<Sentence*, std::error_code> Treebank::insert_sentence_(
    Paragraph* paragraph, const std::string_view sent_id, const std::string_view text) noexcept {
        if (!internal::is_sent_id_valid(sent_id))
            return std::unexpected(make_error_code(ErrorCode::InvalidSentenceId));

        if (sentence_idx_.contains(sent_id))
            return std::unexpected(make_error_code(ErrorCode::DuplicateSentenceId));

        try {
            std::unique_ptr<Sentence> sentence(new Sentence(this));
            sentence->paragraph_ = paragraph;
            sentence->sent_id_.assign(sent_id);
            sentence->metadata_.emplace_back("sent_id", sent_id);

            // Mirrored in the metadata, like Sentence::set_text: that is what the Writer outputs.
            if (!text.empty()) {
                sentence->text_.assign(text);
                sentence->metadata_.emplace_back("text", text);
            }

            paragraph->sentences_.reserve(paragraph->sentences_.size() + std::size_t{1});

            Sentence* ptr = sentence.get();
            sentence_idx_.emplace(ptr->sent_id_, ptr);

            // Cannot throw: the capacity is reserved.
            paragraph->sentences_.push_back(std::move(sentence));
            ++num_sentences_;

            return ptr;
        }
        catch (...) {
            return std::unexpected(make_error_code(ErrorCode::InsufficientMemory));
        }
    }

    inline constexpr void Treebank::forget_sentences_(const Paragraph* paragraph) noexcept {
        for (const std::unique_ptr<Sentence>& sentence : paragraph->sentences_)
            sentence_idx_.erase(sentence->sent_id_);

        num_sentences_ -= paragraph->sentences_.size();
    }

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

        // Visits every row that carries fields: sentence-level empty nodes, tokens, and tokens' empty nodes.
        const auto for_each_row = [this](auto&& fn) {
            for (const std::unique_ptr<Document>& document : documents_) {
                for (const std::unique_ptr<Paragraph>& paragraph : document->paragraphs_) {
                    for (const std::unique_ptr<Sentence>& sentence : paragraph->sentences_) {
                        for (const std::unique_ptr<Token>& node : sentence->root_.empty_nodes_)
                            fn(node.get());

                        for (const std::unique_ptr<Token>& token : sentence->tokens_) {
                            fn(token.get());

                            for (const std::unique_ptr<Token>& node : token->empty_nodes_)
                                fn(node.get());
                        }
                    }
                }
            }
        };

        try {
            for_each_row([](Token* row) { row->fields_.emplace_back(std::nullopt); });
        }
        catch (...) {
            for_each_row([new_column_count](Token* row) noexcept {
                if (new_column_count == row->fields_.size())
                    row->fields_.pop_back();
            });

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

    // Documents
    inline constexpr auto Treebank::documents() const noexcept {
        return std::views::transform(
        documents_, [](const std::unique_ptr<Document>& document) noexcept -> Document* { return document.get(); });
    }

    inline constexpr std::size_t Treebank::num_documents() const noexcept {
        return documents_.size();
    }

    inline constexpr std::expected<Document*, std::error_code> Treebank::add_document(
    const std::optional<std::string_view> id) noexcept {
        if (!internal::is_boundary_id_valid(id.value_or(std::string_view())))
            return std::unexpected(make_error_code(ErrorCode::InvalidDocumentId));

        const auto res = add_document_(true);
        if (!res)
            return res;

        if (const auto err = (*res)->set_id(id)) {
            documents_.pop_back();
            return std::unexpected(err);
        }

        return res;
    }

    inline constexpr std::error_code Treebank::remove_document(const Document* document) noexcept {
        if (nullptr == document)
            return make_error_code(ErrorCode::InvalidPointer);

        const auto it = std::ranges::find(documents_, document, &std::unique_ptr<Document>::get);
        if (documents_.end() == it)
            return make_error_code(ErrorCode::DocumentNotFoundInTreebank);

        for (const std::unique_ptr<Paragraph>& paragraph : document->paragraphs_)
            forget_sentences_(paragraph.get());

        documents_.erase(it);

        return {};
    }

    // Sentences
    inline constexpr auto Treebank::sentences() const noexcept {
        return documents_ |
               std::views::transform(
               [](const std::unique_ptr<Document>& document) noexcept
               -> const std::vector<std::unique_ptr<Paragraph>>& { return document->paragraphs_; }) |
               std::views::join |
               std::views::transform(
               [](const std::unique_ptr<Paragraph>& paragraph) noexcept
               -> const std::vector<std::unique_ptr<Sentence>>& { return paragraph->sentences_; }) |
               std::views::join |
               std::views::transform(
               [](const std::unique_ptr<Sentence>& sentence) noexcept -> Sentence* { return sentence.get(); });
    }

    inline constexpr std::size_t Treebank::num_sentences() const noexcept {
        return num_sentences_;
    }

    inline constexpr std::expected<Sentence*, std::error_code> Treebank::add_sentence(
    const std::string_view sent_id, const std::string_view text) noexcept {
        // Checked here too, so that an invalid id leaves no implicit document or paragraph behind.
        if (!internal::is_sent_id_valid(sent_id))
            return std::unexpected(make_error_code(ErrorCode::InvalidSentenceId));

        if (sentence_idx_.contains(sent_id))
            return std::unexpected(make_error_code(ErrorCode::DuplicateSentenceId));

        const bool new_document = documents_.empty();
        if (new_document) {
            if (const auto res = add_document_(false); !res)
                return std::unexpected(res.error());
        }

        Document* document = documents_.back().get();

        const bool new_paragraph = document->paragraphs_.empty();
        if (new_paragraph) {
            if (const auto res = document->add_paragraph_(false); !res) {
                if (new_document)
                    documents_.pop_back();

                return std::unexpected(res.error());
            }
        }

        const auto res = insert_sentence_(document->paragraphs_.back().get(), sent_id, text);
        if (!res) {
            if (new_paragraph)
                document->paragraphs_.pop_back();

            if (new_document)
                documents_.pop_back();
        }

        return res;
    }

    inline constexpr std::optional<Sentence*> Treebank::get_sentence_by_id(
    const std::string_view sent_id) const noexcept {
        const auto it = sentence_idx_.find(sent_id);
        if (sentence_idx_.end() == it)
            return std::nullopt;

        return it->second;
    }

    inline constexpr std::error_code Treebank::remove_sentence(const std::string_view sent_id) noexcept {
        const auto it = sentence_idx_.find(sent_id);
        if (sentence_idx_.end() == it)
            return make_error_code(ErrorCode::SentenceNotFoundInTreebank);

        Paragraph* paragraph = it->second->paragraph_;
        const auto sent_it = std::ranges::find(paragraph->sentences_, it->second, &std::unique_ptr<Sentence>::get);

        sentence_idx_.erase(it);
        paragraph->sentences_.erase(sent_it);
        --num_sentences_;

        return {};
    }

    inline constexpr std::error_code Treebank::remove_sentence(const Sentence* sentence) noexcept {
        if (nullptr == sentence)
            return make_error_code(ErrorCode::InvalidPointer);

        if (const auto it = sentence_idx_.find(sentence->sent_id_); sentence_idx_.end() != it && sentence == it->second)
            return remove_sentence(sentence->sent_id_);

        return make_error_code(ErrorCode::SentenceNotFoundInTreebank);
    }

    // State
    inline constexpr bool Treebank::empty() const noexcept {
        return std::size_t{0} == num_sentences_;
    }

    inline constexpr void Treebank::clear() noexcept {
        column_idx_.clear();
        column_views_.clear();
        columns_.clear();

        sentence_idx_.clear();
        documents_.clear();
        num_sentences_ = std::size_t{0};
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
