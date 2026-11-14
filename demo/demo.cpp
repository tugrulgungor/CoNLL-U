/*
 * CoNLL-U - https://github.com/tugrulgungor/CoNLL-U
 *
 * Copyright (c) 2026. All rights reserved.
 * Tuğrul Güngör - https://conll-u.tugrulgungor.me
 *
 * Distributed under the MIT License.
 * https://opensource.org/license/mit/
 */

#include <conllu.hpp>

#include <emscripten/bind.h>

#include <charconv>
#include <cstdio>
#include <format>
#include <sstream>
#include <unordered_map>

using namespace tg;

namespace {
    void append_escaped(std::string& out, const std::string_view value) {
        out += '"';
        for (const unsigned char c : value) {
            switch (c) {
                case '"':
                    out += "\\\"";
                    break;
                case '\\':
                    out += "\\\\";
                    break;
                case '\n':
                    out += "\\n";
                    break;
                case '\r':
                    out += "\\r";
                    break;
                case '\t':
                    out += "\\t";
                    break;
                default:
                    if (c < 0x20) {
                        char buf[8];
                        std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                        out += buf;
                    }
                    else {
                        out += static_cast<char>(c);
                    }
            }
        }
        out += '"';
    }

    void append_optional(std::string& out, const std::optional<std::string_view>& value) {
        if (value.has_value())
            append_escaped(out, value.value());
        else
            out += "null";
    }

    std::string error_json(const std::error_code err) {
        std::string out = R"({"ok":false,"error":)";
        append_escaped(out, err.message());
        out += '}';
        return out;
    }

    void append_row(std::string& out, const std::string_view kind, const std::string_view id, auto&& fields) {
        if ('[' != out.back())
            out += ',';

        out += R"({"kind":")";
        out += kind;
        out += R"(","id":)";
        append_escaped(out, id);
        out += R"(,"fields":[)";

        bool first{true};
        for (const std::optional<std::string_view>& field : fields) {
            if (!first)
                out += ',';
            first = false;
            append_optional(out, field);
        }

        out += "]}";
    }
} // namespace

class Session final {
    using TokenId = conllu::TokenId;

    conllu::StreamParser parser_{};
    std::unique_ptr<conllu::Treebank> treebank_{};
    std::vector<conllu::Sentence*> sentences_{};

    conllu::Sentence* sentence_at_(const int idx) const {
        if (idx < 0 || static_cast<std::size_t>(idx) >= sentences_.size())
            return nullptr;

        return sentences_[static_cast<std::size_t>(idx)];
    }

    std::string stats_json_() const {
        std::size_t tokens{0};
        std::size_t words{0};
        std::size_t empty_nodes{0};

        for (const conllu::Sentence* sentence : sentences_) {
            tokens += sentence->num_tokens();
            words += sentence->num_words();
            empty_nodes += sentence->empty_node_count();

            for (const conllu::Token* token : sentence->tokens())
                empty_nodes += token->empty_node_count();
        }

        return std::format(R"({{"sentences":{},"tokens":{},"words":{},"empty_nodes":{}}})", sentences_.size(), tokens,
                           words, empty_nodes);
    }

    void append_word_row_(const conllu::Word* word, std::string& out) const {
        const std::size_t num_columns = treebank_->num_columns();
        std::vector<std::optional<std::string_view>> fields(num_columns);
        const std::string id = std::format("{}-{}", word->first_id(), word->last_id());

        if (const auto pos = treebank_->get_column_pos("ID"))
            fields[*pos] = id;
        if (const auto pos = treebank_->get_column_pos("FORM"))
            fields[*pos] = word->get_form();
        if (const auto pos = treebank_->get_column_pos("MISC"))
            fields[*pos] = word->get_misc();
        if (const auto pos = treebank_->get_column_pos("FEATS"); pos && word->get_typo().value_or(false))
            fields[*pos] = std::string_view{"Typo=Yes"};

        append_row(out, "word", id, fields);
    }

    std::string mutation_json_(const std::error_code err, const int idx) const {
        std::string out = err ? error_json(err) : std::string(R"({"ok":true})");
        out.pop_back();
        out += R"(,"sentence":)";
        out += sentence(idx);
        out += R"(,"stats":)";
        out += stats_json_();
        out += '}';
        return out;
    }

    std::error_code set_word_value_(conllu::Word* word, const std::string_view column,
                                    const std::optional<std::string_view>& value) const {
        if ("FORM" == column)
            return word->set_form(value);

        if ("MISC" == column)
            return word->set_misc(value);

        if ("FEATS" == column) {
            if (!value.has_value())
                word->unset_typo();
            else if ("Typo=Yes" == value.value())
                word->set_typo(true);
            else
                return conllu::make_error_code(conllu::ErrorCode::InvalidWordFieldValue);

            return {};
        }

        if ("ID" == column)
            return conllu::make_error_code(conllu::ErrorCode::ImmutableTokenField);

        return value.has_value() ? conllu::make_error_code(conllu::ErrorCode::InvalidWordFieldValue)
                                 : std::error_code{};
    }

    conllu::Token* find_token_or_node_(conllu::Sentence* sentence, const std::string_view row) const {
        TokenId parent_id{0};
        TokenId node_id{0};

        if (const std::size_t dot = row.find('.'); std::string_view::npos != dot) {
            if (std::from_chars(row.data(), row.data() + dot, parent_id).ec != std::errc{} ||
                std::from_chars(row.data() + dot + 1, row.data() + row.size(), node_id).ec != std::errc{})
                return nullptr;

            if (0 == parent_id) {
                for (conllu::Token* node : sentence->empty_nodes()) {
                    if (node->get_id() == node_id)
                        return node;
                }
                return nullptr;
            }

            const auto parent = sentence->get_token_by_id(parent_id);
            if (!parent)
                return nullptr;

            for (conllu::Token* node : (*parent)->empty_nodes()) {
                if (node->get_id() == node_id)
                    return node;
            }
            return nullptr;
        }

        if (std::from_chars(row.data(), row.data() + row.size(), parent_id).ec != std::errc{})
            return nullptr;

        return sentence->get_token_by_id(parent_id).value_or(nullptr);
    }

public:
    void reset() {
        parser_.reset();
        treebank_.reset();
        sentences_.clear();
    }

    std::string feed(const std::string& chunk) {
        if (const std::error_code err = parser_.feed(chunk)) {
            std::string out = error_json(err);
            out.pop_back();
            out += std::format(R"(,"line":{}}})", parser_.error_line());
            return out;
        }

        return {};
    }

    int parsed_sentences() const {
        const conllu::Treebank* treebank = parser_.treebank();
        return treebank ? static_cast<int>(treebank->num_sentences()) : 0;
    }

    std::string finish() {
        auto res = parser_.finish();
        if (!res) {
            std::string out = error_json(res.error());
            out.pop_back();
            out += std::format(R"(,"line":{}}})", parser_.error_line());
            return out;
        }

        treebank_ = std::move(res.value());
        sentences_.assign(treebank_->sentences().begin(), treebank_->sentences().end());

        std::string out = R"({"ok":true,"columns":[)";
        bool first{true};
        for (const std::string_view column : treebank_->columns()) {
            if (!first)
                out += ',';
            first = false;
            append_escaped(out, column);
        }

        out += R"(],"sentences":[)";
        const std::size_t form_pos = treebank_->get_column_pos("FORM").value_or(treebank_->num_columns());
        first = true;
        for (const conllu::Sentence* sentence : sentences_) {
            if (!first)
                out += ',';
            first = false;

            out += R"({"id":)";
            append_escaped(out, sentence->get_sent_id());
            out += R"(,"text":)";

            std::string text(sentence->get_text());
            if (text.empty()) {
                for (const conllu::Token* token : sentence->tokens()) {
                    if (const auto form = token->get_field_value(form_pos)) {
                        if (!text.empty())
                            text += ' ';
                        text += *form;
                    }
                }
            }
            append_escaped(out, text);
            out += '}';
        }

        out += R"(],"stats":)";
        out += stats_json_();
        out += '}';
        return out;
    }

    std::string sentence(const int idx) const {
        const conllu::Sentence* sentence = sentence_at_(idx);
        if (!sentence)
            return error_json(conllu::make_error_code(conllu::ErrorCode::SentenceNotFoundInTreebank));

        std::string out = std::format(R"({{"ok":true,"index":{},"sent_id":)", idx);
        append_escaped(out, sentence->get_sent_id());

        out += R"(,"metadata":[)";
        bool first{true};
        for (const auto& [key, value] : sentence->metadata()) {
            if (!first)
                out += ',';
            first = false;

            out += R"({"key":)";
            append_escaped(out, key);
            out += R"(,"value":)";
            append_escaped(out, value);
            out += '}';
        }

        out += R"(],"rows":[)";

        for (const conllu::Token* node : sentence->empty_nodes())
            append_row(out, "empty", std::format("0.{}", node->get_id()), node->fields());

        std::unordered_map<TokenId, const conllu::Word*> words_by_first{};
        for (const conllu::Word* word : sentence->words())
            words_by_first.emplace(word->first_id(), word);

        for (const conllu::Token* token : sentence->tokens()) {
            if (const auto it = words_by_first.find(token->get_id()); words_by_first.end() != it)
                append_word_row_(it->second, out);

            append_row(out, "token", std::to_string(token->get_id()), token->fields());

            for (const conllu::Token* node : token->empty_nodes())
                append_row(out, "empty", std::format("{}.{}", token->get_id(), node->get_id()), node->fields());
        }

        out += "]}";
        return out;
    }

    // An empty value (or "_") unsets the field.
    std::string set_value(const int idx, const std::string& row, const std::string& column,
                          const std::string& value) {
        conllu::Sentence* sentence = sentence_at_(idx);
        if (!sentence)
            return error_json(conllu::make_error_code(conllu::ErrorCode::SentenceNotFoundInTreebank));

        const std::optional<std::string_view> new_value =
        value.empty() || "_" == value ? std::nullopt : std::optional<std::string_view>{value};

        std::error_code err{};
        if (const std::size_t dash = row.find('-'); std::string::npos != dash) {
            TokenId first_id{0};
            std::from_chars(row.data(), row.data() + dash, first_id);

            conllu::Word* word{nullptr};
            for (conllu::Word* w : sentence->words()) {
                if (w->first_id() == first_id)
                    word = w;
            }

            err = word ? set_word_value_(word, column, new_value)
                       : conllu::make_error_code(conllu::ErrorCode::WordNotFoundInSentence);
        }
        else if (conllu::Token* token = find_token_or_node_(sentence, row)) {
            err = new_value ? token->set_field_value(column, *new_value) : token->unset_field_value(column);
        }
        else {
            err = conllu::make_error_code(conllu::ErrorCode::TokenNotFoundInSentence);
        }

        return mutation_json_(err, idx);
    }

    std::string add_token(const int idx) {
        conllu::Sentence* sentence = sentence_at_(idx);
        if (!sentence)
            return error_json(conllu::make_error_code(conllu::ErrorCode::SentenceNotFoundInTreebank));

        const auto res = sentence->add_token();
        return mutation_json_(res ? std::error_code{} : res.error(), idx);
    }

    std::string remove_token(const int idx, const int id, const bool detach_dependents) {
        conllu::Sentence* sentence = sentence_at_(idx);
        if (!sentence)
            return error_json(conllu::make_error_code(conllu::ErrorCode::SentenceNotFoundInTreebank));

        const std::error_code err = sentence->remove_token(static_cast<TokenId>(id), detach_dependents);

        std::string out = mutation_json_(err, idx);
        if (err == conllu::ErrorCode::InvalidTokenHead) {
            out.pop_back();
            out += R"(,"dependents":true})";
        }
        return out;
    }

    std::string validate() const {
        for (std::size_t i{0}; i < sentences_.size(); ++i) {
            if (const std::error_code err = sentences_[i]->validate()) {
                std::string out = error_json(err);
                out.pop_back();
                out += std::format(R"(,"index":{}}})", i);
                return out;
            }
        }

        return R"({"ok":true})";
    }

    std::string write() const {
        if (!treebank_)
            return {};

        std::stringbuf buffer;
        [[maybe_unused]] const auto _ = conllu::Writer{}.write(treebank_.get(), std::ostream(&buffer));
        return buffer.str();
    }
};

EMSCRIPTEN_BINDINGS(conllu_demo) {
    emscripten::class_<Session>("Session")
    .constructor<>()
    .function("reset", &Session::reset)
    .function("feed", &Session::feed)
    .function("parsed_sentences", &Session::parsed_sentences)
    .function("finish", &Session::finish)
    .function("sentence", &Session::sentence)
    .function("set_value", &Session::set_value)
    .function("add_token", &Session::add_token)
    .function("remove_token", &Session::remove_token)
    .function("validate", &Session::validate)
    .function("write", &Session::write);
}
