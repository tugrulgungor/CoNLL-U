/*
 * CoNLL-U - https://github.com/tugrulgungor/CoNLL-U
 *
 * Copyright (c) 2026. All rights reserved.
 * Tuğrul Güngör - https://conll-u.tugrulgungor.me
 *
 * Distributed under the MIT License.
 * https://opensource.org/license/mit/
 */

#ifndef TG_CONLLU_IMPL_SENTENCE_IPP
#define TG_CONLLU_IMPL_SENTENCE_IPP 1

#include "conllu/internal/string.hpp"
#include "conllu/internal/validate.hpp"
#include "conllu/sentence.hpp"
#include "conllu/word.hpp"

namespace tg::conllu {
    inline constexpr void Sentence::split_metadata_line_(std::string_view& line, std::string_view& key,
                                                         std::string_view& value) noexcept {
        line.remove_prefix(std::size_t{1});

        internal::trim(line);
        [[maybe_unused]] const bool _ = internal::split<'=', false>(line, key, value);

        internal::trim_end(key);
        internal::trim_start(value);
    }

    inline constexpr std::optional<std::error_code> Sentence::set_known_metadata_(
    const std::string_view key, const std::string_view value) noexcept {
        if ("sent_id" == key)
            return set_sent_id_(value);

        if ("text" == key)
            return set_text(value);

        return std::nullopt;
    }

    inline constexpr std::error_code Sentence::set_sent_id_(const std::string_view sent_id) noexcept {
        if (!internal::is_sent_id_valid(sent_id))
            return make_error_code(ErrorCode::InvalidSentenceId);

        if (treebank_->sentence_idx_.contains(sent_id))
            return make_error_code(ErrorCode::DuplicateSentenceId);

        const auto it = treebank_->sentence_idx_.find(sent_id_);
        if (treebank_->sentence_idx_.end() == it)
            return make_error_code(ErrorCode::SentenceNotFoundInTreebank);

        const std::size_t idx = it->second;

        std::pair<std::string, std::string> temp{};
        try {
            if (sent_id.size() < temp.first.capacity()) {
                temp.first.reserve(temp.first.capacity() + std::size_t{1});
                temp.second.reserve(temp.second.capacity() + std::size_t{1});
            }

            temp.first.assign(sent_id);
            temp.second.assign(sent_id);

            treebank_->sentence_idx_.emplace(temp.second, idx);
        }
        catch (...) {
            return make_error_code(ErrorCode::InsufficientMemory);
        }

        treebank_->sentence_idx_.erase(sent_id_);

        sent_id_.swap(temp.second);
        metadata_["sent_id"].swap(temp.first);

        return {};
    }

    inline constexpr std::expected<Word*, std::error_code> Sentence::add_word_unsafe_(
    const Word::Range range) noexcept {
        try {
            std::unique_ptr<Word> word(new Word(this));
            word->range_ = range;

            auto ptr = word.get();
            words_.push_back(std::move(word));
            return ptr;
        }
        catch (...) {
            return std::unexpected(make_error_code(ErrorCode::InsufficientMemory));
        }
    }

    inline constexpr void Sentence::remove_empty_words_() noexcept {
        auto it = words_.begin();
        while (words_.end() != it) {
            if (std::size_t{2} > it->get()->num_tokens())
                it = words_.erase(it);
            else
                std::advance(it, std::size_t{1});
        }
    }

    // Constructors
    inline constexpr Sentence::Sentence(Treebank* treebank) noexcept : treebank_(treebank) {
    }

    // Treebank
    inline constexpr Treebank* Sentence::treebank() const noexcept {
        return treebank_;
    }

    // Identity
    inline constexpr std::string_view Sentence::get_sent_id() const noexcept {
        return sent_id_;
    }

    inline constexpr std::error_code Sentence::set_sent_id(const std::string_view sent_id) noexcept {
        return set_metadata("sent_id", sent_id);
    }

    // Text
    inline constexpr std::string_view Sentence::get_text() const noexcept {
        return text_;
    }

    inline constexpr std::error_code Sentence::set_text(const std::string_view text) noexcept {
        try {
            std::string new_text;
            new_text.assign(text);

            metadata_["text"] = new_text;

            text_.swap(new_text);
            return {};
        }
        catch (...) {
            return make_error_code(ErrorCode::InsufficientMemory);
        }
    }

    // Tokens
    inline constexpr auto Sentence::tokens() const noexcept {
        return std::views::transform(
        tokens_, [](const std::unique_ptr<Token>& token) noexcept -> Token* { return token.get(); });
    }

    inline constexpr std::expected<Token*, std::error_code> Sentence::add_token() noexcept {
        try {
            std::unique_ptr<Token> token(new Token(this, nullptr));
            token->id_ = tokens_.size() + std::size_t{1};
            token->fields_.resize(treebank_->columns_.size(), std::nullopt);

            if (const auto id_opt = treebank_->get_column_pos("ID"); id_opt.has_value())
                token->fields_[id_opt.value()] = std::to_string(token->id_);

            const auto ptr = token.get();

            tokens_.push_back(std::move(token));

            return ptr;
        }
        catch (...) {
            return std::unexpected(make_error_code(ErrorCode::InsufficientMemory));
        }
    }

    inline constexpr std::error_code Sentence::remove_token(const TokenId id,
                                                            const bool skip_dependency_validation) noexcept {
        if (TokenId{0} == id || tokens_.size() < id)
            return make_error_code(ErrorCode::TokenNotFoundInSentence);

        const auto head_idx_opt = treebank_->get_column_pos("HEAD");
        if (!skip_dependency_validation && head_idx_opt.has_value()) {
            for (const Token* token : tokens()) {
                if (token->head_.value_or(TokenId{0}) == id)
                    return make_error_code(ErrorCode::InvalidTokenHead);
            }
        }

        auto it = tokens_.begin();
        std::advance(it, id - TokenId{1});

        Word* last_word{nullptr};
        for (const Token* token : tokens()) {
            if (token->id_ < id)
                continue;

            Word* word = token->get_word().value_or(nullptr);
            if (last_word == word)
                continue;

            last_word = word;
            if (nullptr == last_word)
                continue;

            if (!last_word->contains_token_id(id))
                last_word->range_.first--;

            last_word->range_.last--;
        }

        it = tokens_.erase(it);
        while (tokens_.end() != it) {
            it->get()->id_--;
            std::advance(it, std::size_t{1});
        }

        if (head_idx_opt.has_value()) {
            const auto head_idx = head_idx_opt.value();
            for (Token* token : tokens()) {
                const TokenId head = token->head_.value_or(TokenId{0});

                if (skip_dependency_validation && head == id) {
                    token->head_ = TokenId{0};
                    token->fields_[head_idx] = std::nullopt;
                }
                else if (head > id) {
                    token->head_ = head - TokenId{1};
                    token->fields_[head_idx] = std::to_string(token->head_.value());
                }
            }
        }

        remove_empty_words_();

        return {};
    }

    inline constexpr std::error_code Sentence::remove_token(Token* token,
                                                            const bool skip_dependency_validation) noexcept {
        if (nullptr == token)
            return make_error_code(ErrorCode::InvalidPointer);

        if (std::ranges::any_of(tokens(), [token](const Token* tok) noexcept -> bool { return tok == token; }))
            return remove_token(token->id_, skip_dependency_validation);

        return make_error_code(ErrorCode::TokenNotFoundInSentence);
    }

    inline constexpr void Sentence::remove_all_tokens() noexcept {
        tokens_.clear();
        remove_all_words(false);
    }

    inline constexpr auto Sentence::words() const noexcept {
        return std::views::transform(words_,
                                     [](const std::unique_ptr<Word>& word) noexcept -> Word* { return word.get(); });
    }

    inline constexpr std::size_t Sentence::num_words() const noexcept {
        return words_.size();
    }

    inline constexpr std::expected<Word*, std::error_code> Sentence::add_word(const TokenId first,
                                                                              const TokenId last) noexcept {
        return add_word(Word::Range(first, last));
    }

    inline constexpr std::expected<Word*, std::error_code> Sentence::add_word(const Word::Range range) noexcept {
        if (!range.is_valid(num_tokens()))
            return std::unexpected(make_error_code(ErrorCode::InvalidWordRange));

        for (const Word* word : words()) {
            if (word->range_.overlaps(range))
                return std::unexpected(make_error_code(ErrorCode::OverlappingWordRange));
        }

        return add_word_unsafe_(range);
    }

    inline constexpr std::error_code Sentence::remove_word(const Word* word, const bool remove_tokens) noexcept {
        if (!word)
            return make_error_code(ErrorCode::InvalidPointer);

        for (auto it = words_.begin(); it != words_.end(); ++it) {
            if (it->get() != word)
                continue;

            if (remove_tokens) {
                Word::Range range = word->range_;
                while (range.last >= range.first) {
                    [[maybe_unused]] const auto _ = remove_token(range.first, true);
                    --range.last;
                }
            }
            else {
                words_.erase(it);
            }

            return {};
        }

        return make_error_code(ErrorCode::WordNotFoundInSentence);
    }

    inline constexpr void Sentence::remove_all_words(const bool remove_tokens) noexcept {
        if (remove_tokens) {
            while (!words_.empty()) {
                Word::Range range = words_[0]->range_;
                while (range.last >= range.first) {
                    [[maybe_unused]] const auto _ = remove_token(range.first, true);
                    --range.last;
                }
            }
        }
        else {
            words_.clear();
        }
    }

    // Metadata
    inline constexpr auto Sentence::metadata() const noexcept {
        return metadata_ |
               std::views::transform([](const auto& entry) noexcept -> std::pair<std::string_view, std::string_view> {
                   return {entry.first, entry.second};
               });
    }

    inline constexpr std::size_t Sentence::num_metadata_entries() const noexcept {
        return metadata_.size();
    }

    inline constexpr std::size_t Sentence::num_tokens() const noexcept {
        return tokens_.size();
    }

    inline constexpr std::optional<Token*> Sentence::get_token_by_id(const TokenId id) const noexcept {
        if (TokenId{0} == id || tokens_.size() < id)
            return std::nullopt;

        return tokens_[id - TokenId{1}].get();
    }

    inline constexpr std::optional<std::string_view> Sentence::get_metadata(const std::string_view key) const noexcept {
        if (const auto it = metadata_.find(key); metadata_.end() != it)
            return it->second;

        return std::nullopt;
    }

    inline constexpr std::error_code Sentence::set_metadata(const std::string_view key,
                                                            const std::string_view value) noexcept {
        if (key.empty())
            return make_error_code(ErrorCode::InvalidSentenceMetaKey);

        if (metadata_.contains(key))
            return make_error_code(ErrorCode::DuplicateSentenceMetaEntry);

        if (const auto err_opt = set_known_metadata_(key, value); err_opt.has_value())
            return err_opt.value();

        try {
            metadata_.emplace(key, value);
            return {};
        }
        catch (...) {
            return make_error_code(ErrorCode::InsufficientMemory);
        }
    }

    inline constexpr std::error_code Sentence::set_metadata_from_comment_line(std::string_view line) noexcept {
        if (line.empty() || '#' != line[0])
            return make_error_code(ErrorCode::InvalidSentenceMetaLine);

        std::string_view key;
        std::string_view value;

        split_metadata_line_(line, key, value);

        if (line.empty())
            return {};

        if (key.empty())
            return make_error_code(ErrorCode::InvalidSentenceMetaKey);

        return set_metadata(key, value);
    }

    inline constexpr std::error_code Sentence::unset_metadata(const std::string_view key) noexcept {
        if ("sent_id" == key)
            return make_error_code(ErrorCode::SentenceIdCannotBeRemoved);

        if (const auto it = metadata_.find(key); metadata_.end() != it)
            metadata_.erase(it);

        if ("text" == key)
            text_.clear();

        return {};
    }

    inline constexpr void Sentence::remove_all_metadata() noexcept {
        std::erase_if(
        metadata_, [](const std::pair<std::string, std::string>& entry) -> bool { return "sent_id" != entry.first; });

        text_.clear();
    }

    // State
    inline constexpr bool Sentence::empty() const noexcept {
        return tokens_.empty();
    }

    inline constexpr void Sentence::clear() noexcept {
        remove_all_tokens();
        remove_all_metadata();
    }

    inline constexpr std::error_code Sentence::validate() const noexcept {
        for (const Word* word : words()) {
            if (const auto err = word->validate())
                return err;
        }

        for (const Token* token : tokens()) {
            if (const auto err = token->validate())
                return err;
        }

        return {};
    }
} // namespace tg::conllu

#endif // TG_CONLLU_IMPL_SENTENCE_IPP
