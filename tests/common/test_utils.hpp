/*
 * CoNLL-U - https://github.com/tugrulgungor/CoNLL-U
 *
 * Copyright (c) 2026. All rights reserved.
 * Tuğrul Güngör - https://conll-u.tugrulgungor.me
 *
 * Distributed under the MIT License.
 * https://opensource.org/license/mit/
 */

#ifndef TG_CONLLU_TESTS_TEST_UTILS_HPP
#define TG_CONLLU_TESTS_TEST_UTILS_HPP 1

#include <conllu.hpp>

#include <gtest/gtest.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <string_view>

namespace tg::conllu::tests {
    /** The UD treebanks bundled in tests/.data (file stem, sentence count). */
    struct DataFile final {
        std::string_view stem;
        std::size_t num_sentences;
    };

    inline constexpr std::array<DataFile, 5> c_data_files{{
    {"de_hdt-ud-test", 18459},
    {"en_gum-ud-test", 1464},
    {"en_littleprince-ud-test", 500},
    {"ru_taiga-ud-test", 1217},
    {"tr_boun-ud-test", 979},
    }};

    inline std::filesystem::path data_path(const std::string_view stem) {
        return std::filesystem::path(CoNLL_U_TESTS_DATA_DIR) / (std::string(stem) + ".conllu");
    }

    inline std::string read_file(const std::filesystem::path& path) {
        std::ifstream in(path, std::ios::binary);
        std::ostringstream out;
        out << in.rdbuf();
        return out.str();
    }

    /** One standard 10-column CoNLL-U line (with trailing newline). */
    inline std::string row(const std::string_view id, const std::string_view form, const std::string_view head = "_",
                           const std::string_view deprel = "_", const std::string_view deps = "_") {
        return std::format("{}\t{}\t_\t_\t_\t_\t{}\t{}\t{}\t_\n", id, form, head, deprel, deps);
    }

    /** One sentence block: sent_id + text metadata followed by @p rows (no trailing blank line). */
    inline std::string sentence(const std::string_view sent_id, const std::string_view rows) {
        return std::format("# sent_id = {}\n# text = text of {}\n{}", sent_id, sent_id, rows);
    }

    /** A small two-sentence document using the standard columns. */
    inline std::string two_sentences() {
        return sentence("s1", row("1", "Hello", "0", "root") + row("2", "world", "1", "obj")) + "\n" +
               sentence("s2", row("1", "Bye", "0", "root")) + "\n";
    }

    inline std::expected<std::unique_ptr<Treebank>, std::error_code> parse(const std::string_view text) {
        return Parser{}.parse_memory(text);
    }

    /** Parses @p text and fails the current test if it does not parse. */
    inline std::unique_ptr<Treebank> parse_ok(const std::string_view text) {
        auto res = parse(text);
        EXPECT_TRUE(res.has_value()) << (res ? "" : res.error().message());
        return res ? std::move(res.value()) : nullptr;
    }

    inline std::string write(const Treebank* treebank, const WriterOptions options = {}) {
        std::stringbuf buffer;
        const auto err = Writer{options}.write(treebank, std::ostream(&buffer));
        EXPECT_FALSE(err) << err.message();
        return buffer.str();
    }

    /**
     * Keeps only token/word/empty-node rows and blank lines, with
     * `\n` line endings (bundled data files are checked out with CRLF on Windows).
     */
    inline std::string rows_only(const std::string_view text) {
        std::string out;
        std::istringstream in{std::string(text)};
        for (std::string line; std::getline(in, line);) {
            if (line.ends_with('\r'))
                line.pop_back();

            if (line.empty() || '#' != line.front())
                out += line + "\n";
        }
        return out;
    }

    /** An empty treebank with the 10 standard CoNLL-U columns. */
    inline std::unique_ptr<Treebank> standard_treebank() {
        auto treebank = std::make_unique<Treebank>();
        for (const std::string_view name :
             {"ID", "FORM", "LEMMA", "UPOS", "XPOS", "FEATS", "HEAD", "DEPREL", "DEPS", "MISC"}) {
            EXPECT_TRUE(treebank->add_column(name).has_value());
        }
        return treebank;
    }

    /** Feeds @p text to @p parser in chunks of random size in [1, max_chunk]. */
    inline std::error_code feed_chunked(StreamParser& parser, const std::string_view text, const std::size_t max_chunk,
                                        const unsigned seed = 42) {
        std::mt19937 rng(seed);
        std::uniform_int_distribution<std::size_t> dist(1, max_chunk);

        for (std::size_t pos{0}; pos < text.size();) {
            const std::size_t count = std::min(dist(rng), text.size() - pos);
            if (const auto err = parser.feed(text.substr(pos, count)))
                return err;
            pos += count;
        }

        return {};
    }

    inline std::string field(const Token* token, const std::string_view column) {
        return std::string(token->get_field_value(column).value_or("<unset>"));
    }

    inline Sentence* first_sentence(const Treebank* treebank) {
        return *treebank->sentences().begin();
    }
} // namespace tg::conllu::tests

#endif // TG_CONLLU_TESTS_TEST_UTILS_HPP
