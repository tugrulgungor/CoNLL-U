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
 * @file conllu/token.hpp
 */

#ifndef TG_CONLLU_TOKEN_HPP
#define TG_CONLLU_TOKEN_HPP 1

#include "conllu/error.hpp"

#include <charconv>
#include <expected>
#include <format>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace tg::conllu {
    class Sentence;
    class Word;

    /**
     * @brief Identifier for a token or empty node within a sentence.
     * @ingroup conllu
     */
    using TokenId = std::uint64_t;

    /**
     * @class Token
     * @brief A single token within a Sentence, or an empty node within a Token.
     * @ingroup conllu
     */
    class Token final {
        TokenId id_{};
        std::optional<TokenId> head_{std::nullopt};

        std::vector<std::optional<std::string>> fields_{};

        std::vector<std::unique_ptr<Token>> empty_nodes_{};

        Token *parent_{nullptr};
        Sentence *sentence_{nullptr};

        [[nodiscard]] constexpr std::optional<std::error_code> set_known_field_(
        std::size_t position, std::string_view name, const std::optional<std::string_view> &value) noexcept;
        [[nodiscard]] constexpr std::error_code set_field_value_(std::size_t position,
                                                                 const std::optional<std::string_view> &value) noexcept;
        [[nodiscard]] constexpr std::error_code set_field_value_(std::size_t position, std::string_view name,
                                                                 const std::optional<std::string_view> &value) noexcept;

        [[nodiscard]] constexpr std::error_code set_head_(const std::optional<std::string_view> &value) noexcept;

        explicit(true) constexpr Token(Sentence *sentence, Token *parent) noexcept;

        friend class Parser;
        friend class Sentence;
        friend class Treebank;
        friend class Word;
        friend class Writer;

    public:
        /**
         * @name Constructors
         */
        /** @{ */
        /**
         * @brief Default construction is disabled; tokens are created via Sentence::add_token
         *        or Token::add_empty_node.
         */
        constexpr Token() noexcept = delete;
        /** @} */
        /**
         * @name Identity
         */
        /** @{ */
        /**
         * @brief Pointer to the sentence that owns this token.
         */
        [[nodiscard]] constexpr Sentence *sentence() const noexcept;
        /**
         * @brief Pointer to the parent token, or nullptr if this is not an empty node.
         */
        [[nodiscard]] constexpr Token *get_parent() const noexcept;
        /**
         * @brief Token identifier. For an empty node, this is its position among its parent's empty nodes.
         */
        [[nodiscard]] constexpr TokenId get_id() const noexcept;
        /**
         * @brief Resolves the HEAD field to the referenced token.
         * @return Pointer to the head token, or std::nullopt if there is none (unset, root, or unresolvable).
         */
        [[nodiscard]] constexpr std::optional<Token *> get_head() const noexcept;
        /**
         * @brief Sets the HEAD field, linking this token to its syntactic head.
         * @param[in] id Identifier of the head token, or 0 to clear the head (root/unset). Must not equal this token's
         * own identifier, and if non-zero must refer to an existing token in the same sentence.
         * @return Empty error_code on success, or an error describing the failure.
         */
        constexpr std::error_code set_head(TokenId id) noexcept;
        /**
         * @brief Whether this token is an empty node.
         */
        [[nodiscard]] constexpr bool is_empty_node() const noexcept;
        /**
         * @brief The Word this token belongs to, if any.
         * @return Pointer to the owning Word, or std::nullopt if this token is an empty node or is not part of any
         * word.
         */
        [[nodiscard]] constexpr std::optional<Word *> get_word() const noexcept;
        /**
         * @brief Whether this token belongs to a Word.
         */
        [[nodiscard]] constexpr bool is_in_word() const noexcept;
        /** @} */
        /**
         * @name Fields
         */
        /** @{ */
        /**
         * @brief View over the field values, in column order.
         */
        [[nodiscard]] constexpr auto fields() const noexcept;
        /**
         * @brief Number of fields on this token (normally equal to Treebank::num_columns).
         */
        [[nodiscard]] constexpr std::size_t num_fields() const noexcept;
        /**
         * @brief Looks up a field value by column position.
         * @param[in] position Column position to search for.
         * @return Field value, or std::nullopt if the field is unset or @p position is out of range.
         */
        [[nodiscard]] constexpr std::optional<std::string_view> get_field_value(std::size_t position) const noexcept;
        /**
         * @brief Looks up a field value by column name.
         * @param[in] name Column name to search for.
         * @return Field value, or std::nullopt if the field is unset or no such column exists.
         */
        [[nodiscard]] constexpr std::optional<std::string_view> get_field_value(std::string_view name) const noexcept;
        /**
         * @brief Sets a field value by column position.
         * @param[in] position Column position to set.
         * @param[in] value New field value.
         * @return Empty error_code on success, or an error describing the failure.
         */
        [[nodiscard]] constexpr std::error_code set_field_value(std::size_t position, std::string_view value) noexcept;
        /**
         * @brief Sets a field value by column name.
         * @param[in] name Column name to set.
         * @param[in] value New field value.
         * @return Empty error_code on success, or an error describing the failure.
         */
        [[nodiscard]] constexpr std::error_code set_field_value(std::string_view name, std::string_view value) noexcept;
        /**
         * @brief Unsets a field value by column position.
         * @param[in] position Column position to unset.
         * @return Empty error_code on success, or an error describing the failure.
         */
        [[nodiscard]] constexpr std::error_code unset_field_value(std::size_t position) noexcept;
        /**
         * @brief Unsets a field value by column name.
         * @param[in] name Column name to unset.
         * @return Empty error_code on success, or an error describing the failure.
         */
        [[nodiscard]] constexpr std::error_code unset_field_value(std::string_view name) noexcept;
        /** @} */
        /**
         * @name Empty nodes
         */
        /** @{ */
        /**
         * @brief View over the empty nodes attached to this token, in order.
         */
        [[nodiscard]] constexpr auto empty_nodes() const noexcept;
        /**
         * @brief Number of empty nodes attached to this token.
         */
        [[nodiscard]] constexpr std::size_t empty_node_count() const noexcept;
        /**
         * @brief Constructs and appends a new empty node.
         * @return Pointer to the newly created empty node, or an error_code on failure.
         */
        [[nodiscard]] constexpr std::expected<Token *, std::error_code> add_empty_node() noexcept;
        /**
         * @brief Removes the empty node with the given identifier.
         * @param[in] id Identifier of the empty node to remove.
         * @return Empty error_code on success, or an error describing the failure.
         */
        constexpr std::error_code remove_empty_node(TokenId id) noexcept;
        /**
         * @brief Removes the given empty node.
         * @param[in] node Pointer to the empty node to remove.
         * @return Empty error_code on success, or an error describing the failure.
         */
        constexpr std::error_code remove_empty_node(Token *node) noexcept;
        /**
         * @brief Removes all empty nodes attached to this token.
         */
        constexpr void remove_all_empty_nodes() noexcept;
        /** @} */
        /**
         * @name State
         */
        /** @{ */
        /**
         * @brief Checks the token for structural inconsistencies.
         * @return Empty error_code on success, or an error describing the first issue found.
         */
        [[nodiscard]] constexpr std::error_code validate() const noexcept;
        /** @} */
        /**
         * @name Destructor
         */
        /** @{ */
        /**
         * @brief Destructor.
         */
        constexpr ~Token() noexcept = default;
        /** @} */
    };
} // namespace tg::conllu

#endif // TG_CONLLU_TOKEN_HPP
