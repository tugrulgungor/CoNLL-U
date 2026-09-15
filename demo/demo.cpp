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

#include <format>

using namespace tg;

static void append_escaped(std::string &out, std::string_view value) {
    out += '"';
    for (unsigned char c : value) {
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

static void append_string_or_null(std::string &out, const std::optional<std::string_view> &value) {
    if (value) {
        append_escaped(out, *value);
    }
    else {
        out += "NULL";
    }
}

static std::string jsonify_token_fields(const conllu::Token *token) {
    std::string out;

    std::size_t idx{0};
    for (const std::optional<std::string_view> &field : token->fields()) {
        if (std::size_t{0} != idx++)
            out.push_back(',');

        if (field.has_value())
            append_escaped(out, field.value());
        else
            out.append(R"str("_")str");
    }

    return out;
}

static std::string jsonify(const conllu::Treebank *treebank) {
    const std::size_t num_columns = treebank->num_columns();

    const std::size_t id_idx = treebank->get_column_pos("ID").value_or(num_columns);
    const std::size_t form_idx = treebank->get_column_pos("FORM").value_or(num_columns);
    const std::size_t misc_idx = treebank->get_column_pos("MISC").value_or(num_columns);
    const std::size_t feats_idx = treebank->get_column_pos("FEATS").value_or(num_columns);

    std::string columns;
    for (const std::string_view column : treebank->columns()) {
        if (!columns.empty())
            columns.push_back(',');

        append_escaped(columns, column);
    }

    std::string sentences;
    for (const conllu::Sentence *sentence : treebank->sentences()) {
        std::string metadata;
        for (const std::pair<std::string_view, std::string_view> &entry : sentence->metadata()) {
            if (!metadata.empty())
                metadata.push_back(',');

            metadata.append(R"str({"key":)str");
            append_escaped(metadata, entry.first);
            metadata.append(R"str(,"value":)str");
            append_escaped(metadata, entry.second);
            metadata.push_back('}');
        }

        std::string words;
        for (const conllu::Word *word : sentence->words()) {
            if (!words.empty())
                words.push_back(',');

            words.push_back('[');
            for (std::size_t idx{0}; idx < num_columns; ++idx) {
                if (std::size_t{0} != idx)
                    words.push_back(',');

                if (id_idx == idx) {
                    words.append(std::format(R"str("{}-{}")str", word->first_id(), word->last_id()));
                    continue;
                }

                if (form_idx == idx) {
                    words.append(std::format(R"str("{}")str", word->get_form().value_or("_")));
                    continue;
                }

                if (misc_idx == idx) {
                    words.append(std::format(R"str("{}")str", word->get_misc().value_or("_")));
                    continue;
                }

                if (feats_idx == idx && word->get_typo().value_or(false)) {
                    words.append(R"str("Typo=Yes")str");
                    continue;
                }

                words.append(R"str("_")str");
            }
            words.push_back(']');
        }

        std::string tokens;
        for (const conllu::Token *token : sentence->tokens()) {
            if (!tokens.empty())
                tokens.push_back(',');

            std::string empty_nodes;
            for (const conllu::Token *node : token->empty_nodes()) {
                if (!empty_nodes.empty())
                    empty_nodes.push_back(',');

                empty_nodes.append(std::format(R"json({{"fields":[{}]}})json", jsonify_token_fields(node)));
            }

            tokens.append(
            std::format(R"json({{"fields":[{}],"empty_nodes":[{}]}})json", jsonify_token_fields(token), empty_nodes));
        }

        if (!sentences.empty())
            sentences.push_back(',');

        sentences.append(
        std::format(R"json({{"metadata":[{}],"words":[{}],"tokens":[{}]}})json", metadata, words, tokens));
    }

    return std::format(R"json({{"ok":true,"columns":[{}],"sentences":[{}]}})json", columns, sentences);
}

static std::string parse_conllu_file(const std::string &text) {
    conllu::Parser parser{};
    auto res = parser.parse_memory(text);
    if (!res)
        return std::format(R"json({{"ok":false,"error":"{}"}})json", res.error().message());

    std::unique_ptr<conllu::Treebank> treebank = std::move(res.value());
    return jsonify(treebank.get());
}

EMSCRIPTEN_BINDINGS(conllu_demo) {
    emscripten::function("parse_conllu_file", &parse_conllu_file);
}
