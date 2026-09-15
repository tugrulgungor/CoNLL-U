/*
 * CoNLL-U - https://github.com/tugrulgungor/CoNLL-U
 *
 * Copyright (c) 2026. All rights reserved.
 * Tuğrul Güngör - https://conll-u.tugrulgungor.me
 *
 * Distributed under the MIT License.
 * https://opensource.org/license/mit/
 */

#ifndef TG_CONLLU_IMPL_PARSER_IPP
#define TG_CONLLU_IMPL_PARSER_IPP 1

#include "conllu/internal/string.hpp"
#include "conllu/parser.hpp"

namespace tg::conllu {
    inline std::error_code Parser::read_block_(std::istream& input, std::vector<std::string_view>& block) noexcept {
        try {
            block.clear();

            sb0_.clear();
            sb1_.clear();
            vb0_.clear();

            while (std::getline(input, sb0_)) {
                if (sb0_.empty())
                    break;

                sb1_.append(sb0_);
                vb0_.emplace_back(sb0_.size());
            }

            if (vb0_.empty())
                return {};

            block.reserve(vb0_.size());

            std::size_t offset{0};
            for (const std::size_t s : vb0_) {
                block.emplace_back(sb1_.data() + offset, s);
                offset += s;
            }

            return {};
        }
        catch (...) {
            return make_error_code(ErrorCode::StreamError);
        }
    }
    inline std::optional<std::error_code> Parser::parse_columns_(std::string_view line, Treebank* treebank) noexcept {
        if ('#' != line[0])
            return make_error_code(ErrorCode::SentenceMustStartWithMetaLine);

        std::string_view key;
        std::string_view value;
        Sentence::split_metadata_line_(line, key, value);

        if ("global.columns" != key) {
            try {
                constexpr std::array<std::string_view, std::size_t{10}> c_{"ID",    "FORM", "LEMMA",  "UPOS", "XPOS",
                                                                           "FEATS", "HEAD", "DEPREL", "DEPS", "MISC"};

                treebank->column_views_.reserve(c_.size());
                treebank->column_idx_.reserve(c_.size());

                for (const std::string_view name : c_) {
                    if (const auto res = treebank->add_column(name); !res)
                        return res.error();
                }

                return std::nullopt;
            }
            catch (...) {
                return make_error_code(ErrorCode::InsufficientMemory);
            }
        }
        else {
            try {
                for (auto&& token : value | std::views::split(' ')) {
                    const std::string_view name{token.data(), token.size()};

                    if (const auto res = treebank->add_column(name); !res)
                        return res.error();
                }

                return make_error_code(ErrorCode::Success);
            }
            catch (...) {
                return make_error_code(ErrorCode::InsufficientMemory);
            }
        }
    }
    inline std::error_code Parser::parse_sentence_(std::vector<std::string_view>& lines,
                                                   Treebank* treebank) const noexcept {
        std::size_t num_meta_lines = count_sentence_meta_lines_(lines);
        const std::optional<std::string_view> sent_id = find_sentence_sent_id_(lines, num_meta_lines);
        if (!sent_id.has_value())
            return make_error_code(ErrorCode::MissingSentenceId);

        const auto sent_res = treebank->add_sentence(sent_id.value());
        if (!sent_res)
            return sent_res.error();

        Sentence* sentence = sent_res.value();
        std::size_t line_idx{0};
        while (line_idx < num_meta_lines) {
            const std::string_view line = lines[line_idx++];
            if (const auto err = sentence->set_metadata_from_comment_line(line))
                return err;
        }

        Token* last_token{nullptr};
        while (line_idx < lines.size()) {
            const std::string_view line = lines[line_idx++];
            if (line.empty())
                break;

            const auto res = parse_token_(treebank, sentence, last_token, line);
            if (!res) {
                printf("<%s>\n", std::string(line).c_str());
                return res.error();
            }

            last_token = res.value();
        }

        if (lines.size() != line_idx)
            return make_error_code(ErrorCode::InvalidPointer);

        return sentence->validate();
    }

    inline std::size_t Parser::count_sentence_meta_lines_(const std::vector<std::string_view>& lines) noexcept {
        std::size_t count{0};

        for (const std::string_view line : lines) {
            if ('#' == line[0])
                ++count;
            else
                break;
        }

        return count;
    }

    inline std::optional<std::string_view> Parser::find_sentence_sent_id_(std::vector<std::string_view>& lines,
                                                                          std::size_t& limit) noexcept {
        for (std::size_t i{0}; i < limit; ++i) {
            std::string_view line = lines[i];

            std::string_view key;
            std::string_view value;

            Sentence::split_metadata_line_(line, key, value);

            if ("sent_id" == key) {
                auto it = lines.begin();
                std::advance(it, i);
                lines.erase(it);

                --limit;
                return value;
            }
        }

        return std::nullopt;
    }

    inline std::expected<Token*, std::error_code> Parser::parse_token_(const Treebank* treebank, Sentence* sentence,
                                                                       Token* last_token,
                                                                       std::string_view line) const noexcept {
        const std::size_t num_columns = treebank->columns_.size();

        std::vector<std::string_view> field_values{};
        try {
            field_values.reserve(num_columns);

            for (const auto& span : line | std::views::split('\t'))
                field_values.emplace_back(span.data(), span.size());
        }
        catch (...) {
            return std::unexpected(make_error_code(ErrorCode::InsufficientMemory));
        }

        if (field_values.empty() || num_columns != field_values.size())
            return std::unexpected(make_error_code(ErrorCode::TokenFieldCountMismatch));

        Token* parent{nullptr};
        if (const auto id_idx_res = treebank->get_column_pos("ID"); id_idx_res.has_value()) {
            const std::string_view id = field_values[id_idx_res.value()];
            if (id.contains('-')) {
                if (const auto err = parse_word_(treebank, sentence, last_token, field_values, id))
                    return std::unexpected(err);

                return nullptr;
            }

            if (id.contains('.')) {
                if (nullptr == last_token || nullptr != last_token->parent_)
                    return std::unexpected(make_error_code(ErrorCode::TokenEmptyNodeMismatch));

                parent = last_token;
            }
        }

        try {
            std::unique_ptr<Token> token(new Token(sentence, parent));
            token->fields_.resize(num_columns, std::nullopt);

            std::size_t field_idx{0};
            for (const auto value : field_values) {
                const auto value_view = value_view_(value);

                if (const auto err = token->set_field_value_(field_idx, treebank->column_views_[field_idx], value_view))
                    return std::unexpected(err);

                ++field_idx;
            }

            if (nullptr == parent) {
                auto ptr = token.get();
                sentence->tokens_.emplace_back(std::move(token));
                return ptr;
            }
            else {
                parent->empty_nodes_.emplace_back(std::move(token));
                return parent;
            }
        }
        catch (...) {
            return std::unexpected(make_error_code(ErrorCode::InsufficientMemory));
        }
    }

    inline std::error_code Parser::parse_word_(const Treebank* treebank, Sentence* sentence, const Token* last_token,
                                               const std::vector<std::string_view>& field_values,
                                               const std::string_view id) const noexcept {
        Word::Range range{};
        if (!internal::split_and_parse<'-', TokenId>(id, range.first, range.last))
            return make_error_code(ErrorCode::InvalidWordRange);

        if (!range.is_valid())
            return make_error_code(ErrorCode::InvalidWordRange);

        if (nullptr != last_token) {
            if (last_token->is_empty_node())
                last_token = last_token->parent_;

            if (range.first <= last_token->id_)
                return make_error_code(ErrorCode::WordMustAppearBeforeToken);
        }

        const std::size_t id_idx = treebank->get_column_pos("ID").value_or(field_values.size());
        const std::size_t form_idx = treebank->get_column_pos("FORM").value_or(field_values.size());
        const std::size_t misc_idx = treebank->get_column_pos("MISC").value_or(field_values.size());
        const std::size_t feats_idx = treebank->get_column_pos("FEATS").value_or(field_values.size());

        const auto res = sentence->add_word_unsafe_(range);
        if (!res)
            return res.error();

        Word* word = res.value();

        for (std::size_t idx{0}; idx < field_values.size(); ++idx) {
            const auto field = value_view_(field_values[idx]);

            if (id_idx == idx)
                continue;

            if (form_idx == idx) {
                if (const auto err = word->set_form(field))
                    return err;

                continue;
            }

            if (misc_idx == idx) {
                if (const auto err = word->set_misc(field))
                    return err;

                continue;
            }

            if (feats_idx == idx) {
                if (!field.has_value())
                    continue;

                if ("Typo=Yes" == field) {
                    word->set_typo(true);
                    continue;
                }

                return make_error_code(ErrorCode::InvalidWordFieldValue);
            }

            if (field.has_value())
                return make_error_code(ErrorCode::InvalidWordFieldValue);
        }

        return {};
    }

    inline std::optional<std::string_view> Parser::value_view_(const std::string_view value) const noexcept {
        if (std::size_t{1} == value.size() && options_.empty_value_indicator == value[0])
            return std::nullopt;

        return value;
    }

    // Constructors
    inline constexpr Parser::Parser(const ParserOptions options) noexcept : options_(options) {
    }

    // Parsing
    inline std::expected<std::unique_ptr<Treebank>, std::error_code> Parser::parse(std::istream&& stream) noexcept {
        std::vector<std::string_view> block{};

        std::unique_ptr<Treebank> treebank(new (std::nothrow) Treebank);
        if (!treebank)
            return std::unexpected(make_error_code(ErrorCode::InsufficientMemory));

        Treebank* treebank_ptr = treebank.get();

        std::size_t block_idx{0};
        while (true) {
            if (const auto err = read_block_(stream, block))
                return std::unexpected(err);

            if (block.empty())
                break;

            if (std::size_t{0} == block_idx) {
                if (const auto res = parse_columns_(block.front(), treebank_ptr); res.has_value()) {
                    if (const std::error_code err = res.value())
                        return std::unexpected(err);

                    block.erase(block.begin());
                }
            }

            if (const auto res = parse_sentence_(block, treebank_ptr))
                return std::unexpected(res);

            ++block_idx;
        }

        return std::move(treebank);
    }

    inline std::expected<std::unique_ptr<Treebank>, std::error_code> Parser::parse_file(
    const std::filesystem::path& path) noexcept {
        std::ifstream file(path, std::ios::binary);
        return parse(std::move(file));
    }

    inline std::expected<std::unique_ptr<Treebank>, std::error_code> Parser::parse_memory(
    std::string_view data) noexcept {
#if __has_include(<spanstream>)
        std::ispanstream stream{std::span<const char>(data)};
#else  // __has_include(<spanstream>)
        struct _mb_ : std::streambuf {
            _mb_(const char* buf, const std::size_t size) noexcept {
                const auto g = const_cast<char*>(buf);
                setg(g, g, g + size);
            }
        };

        struct _is_ : virtual _mb_, std::istream {
            _is_(const char* buf, const std::size_t size) :
                _mb_(buf, size), std::istream(static_cast<std::streambuf*>(this)) {
            }
        };

        _is_ stream{data.data(), data.size()};
#endif // __has_include(<spanstream>)

        return parse(std::move(stream));
    }

    template <typename DataType>
        requires(sizeof(DataType) == sizeof(unsigned char))
    inline std::expected<std::unique_ptr<Treebank>, std::error_code> Parser::parse_memory(
    const DataType* data, const std::size_t size) noexcept {
        const std::string_view sv{reinterpret_cast<const char*>(data), size};
        return parse_memory(sv);
    }
} // namespace tg::conllu

#endif // TG_CONLLU_IMPL_PARSER_IPP
