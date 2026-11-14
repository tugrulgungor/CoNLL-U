/*
 * CoNLL-U - https://github.com/tugrulgungor/CoNLL-U
 *
 * Copyright (c) 2026. All rights reserved.
 * Tuğrul Güngör - https://conll-u.tugrulgungor.me
 *
 * Distributed under the MIT License.
 * https://opensource.org/license/mit/
 */

#ifndef TG_CONLLU_IMPL_STREAM_PARSER_IPP
#define TG_CONLLU_IMPL_STREAM_PARSER_IPP 1

#include "conllu/internal/string.hpp"
#include "conllu/stream_parser.hpp"

namespace tg::conllu {
    inline std::error_code StreamParser::fail_(const std::error_code err, const std::size_t line) noexcept {
        state_ = State::Failed;
        error_ = err;
        error_line_ = line;
        return err;
    }

    inline std::error_code StreamParser::ensure_treebank_() noexcept {
        if (!treebank_) {
            treebank_.reset(new (std::nothrow) Treebank);
            if (!treebank_)
                return make_error_code(ErrorCode::InsufficientMemory);
        }

        return {};
    }

    inline std::error_code StreamParser::consume_line_(std::string_view line) noexcept(false) {
        ++line_;

        if (!line.empty() && '\r' == line.back())
            line.remove_suffix(std::size_t{1});

        if (line.empty())
            return flush_block_();

        if (block_line_sizes_.empty())
            block_first_line_ = line_;

        block_.append(line);
        block_line_sizes_.push_back(line.size());

        return {};
    }

    inline std::error_code StreamParser::flush_block_() noexcept(false) {
        // Blank lines with no pending sentence (leading, trailing or repeated) are ignored.
        if (block_line_sizes_.empty())
            return {};

        block_lines_.clear();
        block_lines_.reserve(block_line_sizes_.size());

        std::size_t offset{0};
        for (const std::size_t size : block_line_sizes_) {
            block_lines_.emplace_back(block_.data() + offset, size);
            offset += size;
        }

        if (const auto err = ensure_treebank_())
            return err;

        if (State::ExpectHeader == state_) {
            state_ = State::Body;

            // Declares the columns: either from a leading `# global.columns` line (consumed) or the 10 standard ones.
            if (const auto res = parse_columns_(block_lines_.front(), treebank_.get()); res.has_value()) {
                if (const std::error_code err = res.value())
                    return err;

                block_lines_.erase(block_lines_.begin());
            }
        }

        std::error_code err{};
        if (!block_lines_.empty()) {
            const std::size_t num_sentences = treebank_->num_sentences();

            err = parse_sentence_(block_lines_, treebank_.get());

            // parse_sentence_ adds the sentence before parsing its tokens: roll a half-built one back, so the
            // treebank only ever holds complete sentences.
            if (err && treebank_->num_sentences() > num_sentences) {
                const Sentence* partial =
                *std::ranges::next(treebank_->sentences().begin(), static_cast<std::ptrdiff_t>(num_sentences));
                [[maybe_unused]] const auto _ = treebank_->remove_sentence(partial);
            }
        }

        clear_block_();

        return err;
    }

    inline void StreamParser::clear_block_() noexcept {
        block_.clear();
        block_line_sizes_.clear();
        block_lines_.clear();
    }

    inline std::optional<std::error_code> StreamParser::parse_columns_(std::string_view line,
                                                                       Treebank* treebank) noexcept {
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
    inline std::error_code StreamParser::parse_sentence_(std::vector<std::string_view>& lines,
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
            if (!res)
                return res.error();

            last_token = res.value();
        }

        if (lines.size() != line_idx)
            return make_error_code(ErrorCode::InvalidPointer);

        return sentence->validate();
    }

    inline std::size_t StreamParser::count_sentence_meta_lines_(const std::vector<std::string_view>& lines) noexcept {
        std::size_t count{0};

        for (const std::string_view line : lines) {
            if ('#' == line[0])
                ++count;
            else
                break;
        }

        return count;
    }

    inline std::optional<std::string_view> StreamParser::find_sentence_sent_id_(std::vector<std::string_view>& lines,
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

    inline std::expected<Token*, std::error_code> StreamParser::parse_token_(const Treebank* treebank,
                                                                             Sentence* sentence, Token* last_token,
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
                if (nullptr == last_token) {
                    // Sentence-level empty node (0.N): only valid before the first token and the first word line.
                    // The ID check in Token::set_known_field_ then requires the "0." prefix (root ID is 0).
                    if (!sentence->tokens_.empty() || !sentence->words_.empty())
                        return std::unexpected(make_error_code(ErrorCode::TokenEmptyNodeMismatch));

                    parent = &sentence->root_;
                }
                else if (nullptr != last_token->parent_) {
                    return std::unexpected(make_error_code(ErrorCode::TokenEmptyNodeMismatch));
                }
                else {
                    parent = last_token;
                }
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

    inline std::error_code StreamParser::parse_word_(const Treebank* treebank, Sentence* sentence,
                                                     const Token* last_token,
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

    inline std::optional<std::string_view> StreamParser::value_view_(const std::string_view value) const noexcept {
        if (std::size_t{1} == value.size() && options_.empty_value_indicator == value[0])
            return std::nullopt;

        return value;
    }

    // Constructors
    inline StreamParser::StreamParser(const ParserOptions options) noexcept : options_(options) {
    }

    // Parsing
    inline std::error_code StreamParser::feed(std::string_view chunk) noexcept {
        if (State::Failed == state_)
            return error_;

        try {
            while (true) {
                const std::size_t pos = chunk.find('\n');
                if (std::string_view::npos == pos) {
                    carry_.append(chunk);
                    return {};
                }

                const std::string_view part = chunk.substr(std::size_t{0}, pos);
                chunk.remove_prefix(pos + std::size_t{1});

                std::error_code err{};
                if (carry_.empty()) {
                    err = consume_line_(part);
                }
                else {
                    carry_.append(part);
                    err = consume_line_(carry_);
                    carry_.clear();
                }

                if (err)
                    return fail_(err, block_first_line_);
            }
        }
        catch (...) {
            return fail_(make_error_code(ErrorCode::InsufficientMemory), line_ + std::size_t{1});
        }
    }

    inline std::expected<std::unique_ptr<Treebank>, std::error_code> StreamParser::finish() noexcept {
        if (State::Failed == state_)
            return std::unexpected(error_);

        try {
            // A final line without a terminating newline.
            if (!carry_.empty()) {
                const auto err = consume_line_(carry_);
                carry_.clear();

                if (err)
                    return std::unexpected(fail_(err, block_first_line_));
            }

            // A final sentence without a terminating blank line.
            if (const auto err = flush_block_())
                return std::unexpected(fail_(err, block_first_line_));
        }
        catch (...) {
            return std::unexpected(fail_(make_error_code(ErrorCode::InsufficientMemory), line_));
        }

        if (const auto err = ensure_treebank_())
            return std::unexpected(fail_(err, line_));

        std::unique_ptr<Treebank> treebank = std::move(treebank_);
        reset();

        return treebank;
    }

    inline void StreamParser::reset() noexcept {
        treebank_.reset();

        state_ = State::ExpectHeader;
        error_ = {};

        line_ = std::size_t{0};
        error_line_ = std::size_t{0};

        carry_.clear();
        clear_block_();
        block_first_line_ = std::size_t{0};
    }

    // Progress
    inline const Treebank* StreamParser::treebank() const noexcept {
        return treebank_.get();
    }

    inline std::size_t StreamParser::num_lines() const noexcept {
        return line_;
    }

    inline std::error_code StreamParser::error() const noexcept {
        return error_;
    }

    inline std::size_t StreamParser::error_line() const noexcept {
        return error_line_;
    }
} // namespace tg::conllu

#endif // TG_CONLLU_IMPL_STREAM_PARSER_IPP
