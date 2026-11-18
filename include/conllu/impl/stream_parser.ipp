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
        if (!block_lines_.empty())
            err = parse_sentence_(block_lines_, treebank_.get());

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
        const std::size_t num_meta_lines = count_sentence_meta_lines_(lines);
        const auto sent_id = find_sentence_sent_id_(lines, num_meta_lines);
        if (!sent_id.has_value())
            return make_error_code(ErrorCode::MissingSentenceId);

        // Boundary markers: `# newdoc` / `# newdoc id = ...` and `# newpar` / `# newpar id = ...`.
        std::optional<std::size_t> newdoc{};
        std::optional<std::size_t> newpar{};
        std::string_view doc_id{};
        std::string_view par_id{};
        for (std::size_t i{0}; i < num_meta_lines; ++i) {
            std::string_view line = lines[i];
            std::string_view key;
            std::string_view value;
            Sentence::split_metadata_line_(line, key, value);

            const bool is_doc = "newdoc" == key || "newdoc id" == key;
            if (!is_doc && "newpar" != key && "newpar id" != key)
                continue;

            std::optional<std::size_t>& at = is_doc ? newdoc : newpar;
            if (at.has_value())
                return make_error_code(ErrorCode::DuplicateSentenceMetaEntry);

            at = i;
            (is_doc ? doc_id : par_id) = key.ends_with(" id") ? value : std::string_view();
        }

        // On failure, everything this block created is removed again, so the treebank only ever holds complete
        // sentences.
        const std::size_t num_documents = treebank->documents_.size();
        Document* last_document = treebank->documents_.empty() ? nullptr : treebank->documents_.back().get();
        const std::size_t num_paragraphs =
        nullptr == last_document ? std::size_t{0} : last_document->paragraphs_.size();

        Sentence* sentence{nullptr};
        const auto fail = [&](const std::error_code err) noexcept {
            if (nullptr != sentence) {
                [[maybe_unused]] const auto _ = treebank->remove_sentence(sentence);
            }

            if (nullptr != last_document) {
                while (last_document->paragraphs_.size() > num_paragraphs)
                    last_document->paragraphs_.pop_back();
            }

            while (treebank->documents_.size() > num_documents)
                treebank->documents_.pop_back();

            return err;
        };

        Document* document = last_document;
        if (newdoc.has_value() || nullptr == document) {
            const auto res = newdoc.has_value() ? treebank->add_document(doc_id) : treebank->add_document_(false);
            if (!res)
                return fail(res.error());

            document = res.value();
        }

        Paragraph* paragraph = document->paragraphs_.empty() ? nullptr : document->paragraphs_.back().get();
        if (newpar.has_value() || nullptr == paragraph) {
            const auto res = newpar.has_value() ? document->add_paragraph(par_id) : document->add_paragraph_(false);
            if (!res)
                return fail(res.error());

            paragraph = res.value();
        }

        const auto sent_res = paragraph->add_sentence(sent_id->second);
        if (!sent_res)
            return fail(sent_res.error());

        sentence = sent_res.value();

        // The lines between `# newdoc` and a following `# newpar` are document comments.
        const bool has_doc_comments = newdoc.has_value() && newpar.has_value() && *newdoc < *newpar;

        // Sentence comments before the first marker are written before it again.
        const std::size_t first_marker = std::min(newdoc.value_or(num_meta_lines), newpar.value_or(num_meta_lines));

        std::size_t line_idx{0};
        while (line_idx < num_meta_lines) {
            const std::size_t i = line_idx++;
            const std::string_view line = lines[i];

            // add_sentence already stored `sent_id`: move it to its place in the comment order.
            if (sent_id->first == i) {
                sentence->move_metadata_to_back_("sent_id");
                if (i < first_marker)
                    ++sentence->leading_comments_;

                continue;
            }

            if (newdoc == i || newpar == i)
                continue;

            if (has_doc_comments && *newdoc < i && i < *newpar) {
                std::string_view rest = line;
                std::string_view key;
                std::string_view value;
                Sentence::split_metadata_line_(rest, key, value);

                if ("text" != key) {
                    if (const auto err = document->set_metadata_from_comment_line(line))
                        return fail(err);

                    continue;
                }
            }

            const std::size_t num_entries = sentence->metadata_.size();
            if (const auto err = sentence->set_metadata_from_comment_line(line))
                return fail(err);

            if (i < first_marker && sentence->metadata_.size() > num_entries)
                ++sentence->leading_comments_;
        }

        Token* last_token{nullptr};
        while (line_idx < lines.size()) {
            const std::string_view line = lines[line_idx++];
            if (line.empty())
                break;

            const auto res = parse_token_(treebank, sentence, last_token, line);
            if (!res)
                return fail(res.error());

            last_token = res.value();
        }

        if (lines.size() != line_idx)
            return fail(make_error_code(ErrorCode::InvalidPointer));

        if (const auto err = sentence->validate())
            return fail(err);

        return {};
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

    inline std::optional<std::pair<std::size_t, std::string_view>> StreamParser::find_sentence_sent_id_(
    const std::vector<std::string_view>& lines, const std::size_t limit) noexcept {
        for (std::size_t i{0}; i < limit; ++i) {
            std::string_view line = lines[i];

            std::string_view key;
            std::string_view value;

            Sentence::split_metadata_line_(line, key, value);

            if ("sent_id" == key)
                return std::pair{i, value};
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
