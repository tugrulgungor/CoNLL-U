/*
 * CoNLL-U - https://github.com/tugrulgungor/CoNLL-U
 *
 * Copyright (c) 2026. All rights reserved.
 * Tuğrul Güngör - https://conll-u.tugrulgungor.me
 *
 * Distributed under the MIT License.
 * https://opensource.org/license/mit/
 */

#ifndef TG_CONLLU_IMPL_TOKEN_IPP
#define TG_CONLLU_IMPL_TOKEN_IPP 1

#include "conllu/treebank.hpp"

namespace tg::conllu {
    inline constexpr std::optional<std::error_code> Token::set_known_field_(
    const std::size_t position, const std::string_view name, const std::optional<std::string_view>& value) noexcept {
        if ("ID" == name) {
            if (!value.has_value())
                return make_error_code(ErrorCode::InvalidTokenId);

            if (nullptr == parent_) {
                if (!internal::parse_number<TokenId>(value.value(), id_))
                    return make_error_code(ErrorCode::InvalidTokenId);

                if ((sentence_->tokens_.size() + std::size_t{1}) == id_)
                    return {};

                return make_error_code(ErrorCode::InvalidTokenId);
            }
            else {
                TokenId parent_id{0};
                TokenId node_id{0};
                if (!internal::split_and_parse<'.', TokenId>(value.value(), parent_id, node_id))
                    return make_error_code(ErrorCode::InvalidTokenId);

                if (parent_id != parent_->id_)
                    return make_error_code(ErrorCode::TokenEmptyNodeMismatch);

                if ((parent_->empty_nodes_.size() + std::size_t{1}) == node_id) {
                    id_ = node_id;
                    return {};
                }

                return make_error_code(ErrorCode::TokenEmptyNodeMismatch);
            }
        }

        if ("FORM" == name)
            return {};

        if ("HEAD" == name) {
            if (is_empty_node()) {
                if (value.has_value())
                    return make_error_code(ErrorCode::InvalidTokenHead);

                head_ = std::nullopt;
                return {};
            }

            if (!value.has_value()) {
                head_ = std::nullopt;
                return {};
            }

            TokenId id{0};
            if (!internal::parse_number<TokenId>(value.value(), id))
                return make_error_code(ErrorCode::InvalidTokenHead);

            head_ = id;

            return {};
        }

        return std::nullopt;
    }

    inline constexpr std::error_code Token::set_field_value_(const std::size_t position,
                                                             const std::optional<std::string_view>& value) noexcept {
        const std::string_view name = sentence_->treebank_->columns_[position];
        if ("ID" == name)
            return make_error_code(ErrorCode::ImmutableTokenField);

        if ("HEAD" == name)
            return set_head_(value);

        return set_field_value_(position, name, value);
    }

    inline constexpr std::error_code Token::set_field_value_(const std::size_t position, const std::string_view name,
                                                             const std::optional<std::string_view>& value) noexcept {
        std::optional<std::string> old_val = std::move(fields_[position]);

        try {
            fields_[position] = value;

            if (const auto err_opt = set_known_field_(position, name, value); err_opt.has_value()) {
                if (const auto err = err_opt.value()) {
                    fields_[position] = std::move(old_val);
                    return err;
                }
            }

            return {};
        }
        catch (...) {
            fields_[position] = std::move(old_val);
            return make_error_code(ErrorCode::InsufficientMemory);
        }
    }

    inline constexpr std::error_code Token::set_head_(const std::optional<std::string_view>& value) noexcept {
        if (!value.has_value())
            return set_head(TokenId{0});

        TokenId id;
        if (!internal::parse_number<TokenId>(value.value(), id))
            return make_error_code(ErrorCode::InvalidTokenHead);

        return set_head(id);
    }

    // Constructors
    inline constexpr Token::Token(Sentence* sentence, Token* parent) noexcept : parent_(parent), sentence_(sentence) {
    }

    // Identity
    inline constexpr Sentence* Token::sentence() const noexcept {
        return sentence_;
    }

    inline constexpr Token* Token::get_parent() const noexcept {
        return parent_;
    }

    inline constexpr TokenId Token::get_id() const noexcept {
        return id_;
    }

    inline constexpr std::optional<Token*> Token::get_head() const noexcept {
        if (!head_.has_value() || TokenId{0} == head_.value())
            return std::nullopt;

        const TokenId head_idx = head_.value() - TokenId{1};
        if (sentence_->tokens_.size() <= head_idx)
            return std::nullopt;

        return sentence_->tokens_[head_idx].get();
    }

    inline constexpr std::error_code Token::set_head(const TokenId id) noexcept {
        if (id == id_)
            return make_error_code(ErrorCode::InvalidTokenHead);

        if (TokenId{0} != id && !std::ranges::any_of(sentence_->tokens(), [&](const Token* token) noexcept -> bool {
                return token->id_ == id;
            }))
            return make_error_code(ErrorCode::InvalidTokenHead);

        const auto head_idx_opt = sentence_->treebank_->get_column_pos("HEAD");
        if (head_idx_opt.has_value()) {
            try {
                fields_[head_idx_opt.value()] = std::to_string(id);
            }
            catch (...) {
                return make_error_code(ErrorCode::InsufficientMemory);
            }
        }

        head_ = id;
        return {};
    }

    inline constexpr bool Token::is_empty_node() const noexcept {
        return nullptr != parent_;
    }

    inline constexpr std::optional<Word*> Token::get_word() const noexcept {
        if (is_empty_node())
            return std::nullopt;

        for (Word* word : sentence_->words()) {
            if (word->contains_token_id(id_))
                return word;
        }

        return std::nullopt;
    }

    inline constexpr bool Token::is_in_word() const noexcept {
        if (is_empty_node())
            return false;

        return std::ranges::any_of(sentence_->words(),
                                   [this](const Word* word) noexcept -> bool { return word->contains_token_id(id_); });
    }

    // Fields
    inline constexpr auto Token::fields() const noexcept {
        return std::views::transform(
        fields_, [](const std::optional<std::string>& field) noexcept -> std::optional<std::string_view> {
            if (field.has_value())
                return field.value();

            return std::nullopt;
        });
    }

    inline constexpr std::size_t Token::num_fields() const noexcept {
        return fields_.size();
    }

    inline constexpr std::optional<std::string_view> Token::get_field_value(const std::size_t position) const noexcept {
        if (position < fields_.size())
            return fields_[position];

        return std::nullopt;
    }

    inline constexpr std::optional<std::string_view> Token::get_field_value(
    const std::string_view name) const noexcept {
        return get_field_value(sentence_->treebank_->get_column_pos(name).value_or(fields_.size() + std::size_t{1}));
    }

    inline constexpr std::error_code Token::set_field_value(const std::size_t position,
                                                            const std::string_view value) noexcept {
        if (position < fields_.size())
            return set_field_value_(position, value);

        return make_error_code(ErrorCode::PositionOutOfRange);
    }

    inline constexpr std::error_code Token::set_field_value(const std::string_view name,
                                                            const std::string_view value) noexcept {
        const auto pos_opt = sentence_->treebank_->get_column_pos(name);
        if (!pos_opt.has_value())
            return make_error_code(ErrorCode::ColumnNotFound);

        return set_field_value(pos_opt.value(), value);
    }

    inline constexpr std::error_code Token::unset_field_value(const std::size_t position) noexcept {
        if (position < fields_.size())
            return set_field_value_(position, std::nullopt);

        return make_error_code(ErrorCode::PositionOutOfRange);
    }

    inline constexpr std::error_code Token::unset_field_value(const std::string_view name) noexcept {
        const auto pos_opt = sentence_->treebank_->get_column_pos(name);
        if (!pos_opt.has_value())
            return make_error_code(ErrorCode::ColumnNotFound);

        return unset_field_value(pos_opt.value());
    }

    // Empty nodes
    inline constexpr auto Token::empty_nodes() const noexcept {
        return std::views::transform(
        empty_nodes_, [](const std::unique_ptr<Token>& token) noexcept -> Token* { return token.get(); });
    }

    inline constexpr std::size_t Token::empty_node_count() const noexcept {
        return empty_nodes_.size();
    }

    inline constexpr std::expected<Token*, std::error_code> Token::add_empty_node() noexcept {
        if (is_empty_node())
            return std::unexpected(make_error_code(ErrorCode::TokenEmptyNodeMismatch));

        try {
            std::unique_ptr<Token> node(new Token(sentence_, this));

            node->id_ = empty_nodes_.size() + TokenId{1};

            node->fields_.resize(fields_.size(), std::nullopt);

            if (const auto id_idx_opt = sentence_->treebank_->get_column_pos("ID"); id_idx_opt.has_value())
                node->fields_[id_idx_opt.value()] = std::format("{}.{}", id_, node->id_);

            auto ptr = node.get();
            empty_nodes_.emplace_back(std::move(node));
            return ptr;
        }
        catch (...) {
            return std::unexpected(make_error_code(ErrorCode::InsufficientMemory));
        }
    }

    inline constexpr std::error_code Token::remove_empty_node(const TokenId id) noexcept {
        if (TokenId{0} == id || empty_nodes_.size() < id)
            return make_error_code(ErrorCode::NodeNotFoundInToken);

        if (id == empty_nodes_.size()) {
            empty_nodes_.pop_back();
            return {};
        }

        std::vector<std::string> ids{};
        const auto id_idx_opt = sentence()->treebank_->get_column_pos("ID");
        if (id_idx_opt.has_value()) {
            try {
                ids.reserve(empty_nodes_.size() - id);

                for (TokenId i = id; i < empty_nodes_.size(); ++i)
                    ids.push_back(std::format("{}.{}", parent_->id_, i));
            }
            catch (...) {
                return make_error_code(ErrorCode::InsufficientMemory);
            }
        }

        auto it = empty_nodes_.begin();
        std::advance(it, id - TokenId{1});

        it = empty_nodes_.erase(it);

        std::size_t idx{0};
        while (empty_nodes_.end() != it) {
            Token* node = it->get();
            node->id_--;

            if (id_idx_opt.has_value())
                node->fields_[id_idx_opt.value()] = std::move(ids[idx++]);

            std::advance(it, std::size_t{1});
        }

        return {};
    }

    inline constexpr std::error_code Token::remove_empty_node(Token* node) noexcept {
        if (nullptr == node)
            return make_error_code(ErrorCode::InvalidPointer);

        if (std::ranges::any_of(empty_nodes(), [node](const Token* n) noexcept -> bool { return n == node; }))
            return remove_empty_node(node->id_);

        return make_error_code(ErrorCode::NodeNotFoundInToken);
    }

    inline constexpr void Token::remove_all_empty_nodes() noexcept {
        empty_nodes_.clear();
    }

    // State
    inline constexpr std::error_code Token::validate() const noexcept {
        if (nullptr == sentence_)
            return make_error_code(ErrorCode::InvalidPointer);

        if (fields_.size() != sentence_->treebank_->num_columns())
            return make_error_code(ErrorCode::InvalidNumberOfFields);

        if (is_empty_node()) {
            if (nullptr == parent_)
                return make_error_code(ErrorCode::InvalidPointer);

            if (TokenId{0} == id_ || parent_->empty_nodes_.size() < id_)
                return make_error_code(ErrorCode::InvalidTokenId);

            if (head_.has_value())
                return make_error_code(ErrorCode::InvalidTokenHead);
        }
        else {
            if (TokenId{0} == id_ || sentence_->tokens_.size() < id_)
                return make_error_code(ErrorCode::InvalidTokenId);

            if (head_.has_value()) {
                if (sentence_->tokens_.size() < head_.value())
                    return make_error_code(ErrorCode::InvalidTokenHead);

                if (id_ == head_.value())
                    return make_error_code(ErrorCode::InvalidTokenHead);
            }

            for (const Token* node : empty_nodes()) {
                if (const auto err = node->validate())
                    return err;
            }
        }

        return {};
    }

} // namespace tg::conllu

#endif // TG_CONLLU_IMPL_TOKEN_IPP
