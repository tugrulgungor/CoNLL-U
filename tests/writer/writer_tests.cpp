/*
 * CoNLL-U - https://github.com/tugrulgungor/CoNLL-U
 *
 * Copyright (c) 2026. All rights reserved.
 * Tuğrul Güngör - https://conll-u.tugrulgungor.me
 *
 * Distributed under the MIT License.
 * https://opensource.org/license/mit/
 */

#include "../common/test_utils.hpp"

using namespace tg::conllu;
using namespace tg::conllu::tests;

namespace {
    std::vector<std::string> lines_of(const std::string_view text) {
        std::vector<std::string> lines;
        std::istringstream in{std::string(text)};
        for (std::string line; std::getline(in, line);)
            lines.push_back(line);
        return lines;
    }

    std::size_t index_of(const std::vector<std::string>& lines, const std::string_view prefix) {
        for (std::size_t i{0}; i < lines.size(); ++i) {
            if (lines[i].starts_with(prefix))
                return i;
        }
        return lines.size();
    }
} // namespace

TEST(Writer, DeclaresColumnsFirst) {
    auto treebank = parse_ok(two_sentences());
    ASSERT_TRUE(treebank);

    const auto lines = lines_of(write(treebank.get()));
    ASSERT_FALSE(lines.empty());
    EXPECT_EQ("# global.columns = ID FORM LEMMA UPOS XPOS FEATS HEAD DEPREL DEPS MISC", lines.front());
}

TEST(Writer, SentenceLayout) {
    auto treebank = parse_ok(sentence("s1", row("1", "Hello", "0", "root") + row("2", "world", "1", "obj")));
    ASSERT_TRUE(treebank);

    const std::string out = write(treebank.get());
    EXPECT_NE(std::string::npos, out.find("# sent_id = s1\n"));
    EXPECT_NE(std::string::npos, out.find("# text = text of s1\n"));
    EXPECT_NE(std::string::npos, out.find(row("1", "Hello", "0", "root") + row("2", "world", "1", "obj") + "\n"));
    EXPECT_TRUE(out.ends_with("\n\n"));
}

TEST(Writer, RowOrdering) {
    // Sentence-level empty nodes first, then each multiword token before its first token, each N.M after token N.
    auto treebank = parse_ok(sentence("s1", row("0.1", "E01") + row("0.2", "E02") + row("1-2", "AB") +
                                            row("1", "A", "0", "root") + row("1.1", "E11") + row("2", "B", "1", "dep") +
                                            row("3", "C", "1", "dep")));
    ASSERT_TRUE(treebank);

    const auto lines = lines_of(write(treebank.get()));
    const std::vector<std::string_view> expected{"0.1\t", "0.2\t", "1-2\t", "1\t", "1.1\t", "2\t", "3\t"};

    // Metadata comes first; rows follow, consecutively, in the expected order.
    const std::size_t first_row = index_of(lines, "0.1\t");
    ASSERT_LT(first_row, lines.size());
    ASSERT_LT(0u, first_row);
    EXPECT_TRUE(lines[first_row - 1].starts_with("# "));

    std::size_t previous = first_row - 1;
    for (const std::string_view prefix : expected) {
        const std::size_t at = index_of(lines, prefix);
        ASSERT_LT(at, lines.size()) << prefix;
        EXPECT_EQ(previous + 1, at) << prefix;
        previous = at;
    }
}

TEST(Writer, MultiwordTokenFields) {
    auto treebank = parse_ok("# sent_id = s1\n"
                             "1-2\tAB\t_\t_\t_\tTypo=Yes\t_\t_\t_\tSpaceAfter=No\n" +
                             row("1", "A", "0", "root") + row("2", "B", "1", "dep"));
    ASSERT_TRUE(treebank);
    EXPECT_NE(std::string::npos,
              write(treebank.get()).find("1-2\tAB\t_\t_\t_\tTypo=Yes\t_\t_\t_\tSpaceAfter=No\n"));
}

TEST(Writer, HandBuiltTreebank) {
    auto treebank = standard_treebank();
    Sentence* s = treebank->add_sentence("built").value();
    ASSERT_FALSE(s->set_text("Hi"));
    Token* t = s->add_token().value();
    ASSERT_FALSE(t->set_field_value("FORM", "Hi"));
    ASSERT_FALSE(t->set_head(0));
    ASSERT_FALSE(t->set_field_value("DEPREL", "root"));
    ASSERT_TRUE(s->add_empty_node().has_value());

    const std::string out = write(treebank.get());
    EXPECT_NE(std::string::npos, out.find("0.1\t_\t_\t_\t_\t_\t_\t_\t_\t_\n" + row("1", "Hi", "0", "root")));

    auto again = parse_ok(out);
    ASSERT_TRUE(again);
    EXPECT_EQ(rows_only(out), rows_only(write(again.get())));
}

TEST(Writer, CustomEmptyValueIndicator) {
    auto treebank = parse_ok("# sent_id = s1\n1-2\tAB\t_\t_\t_\t_\t_\t_\t_\t_\n" + row("1", "A", "0", "root") +
                             row("2", "B", "1", "dep"));
    ASSERT_TRUE(treebank);

    const std::string out = write(treebank.get(), WriterOptions{.empty_value_indicator = '*'});
    EXPECT_NE(std::string::npos, out.find("1-2\tAB\t*\t*\t*\t*\t*\t*\t*\t*\n"));
    EXPECT_NE(std::string::npos, out.find("1\tA\t*\t*\t*\t*\t0\troot\t*\t*\n"));
    EXPECT_EQ(std::string::npos, out.find("\t_"));
}

TEST(Writer, FailingStream) {
    auto treebank = parse_ok(two_sentences());
    ASSERT_TRUE(treebank);
    EXPECT_EQ(ErrorCode::StreamError, Writer{}.write(treebank.get(), std::ostream(nullptr)));
}

TEST(Writer, WriteFileRoundTrip) {
    auto treebank = parse_ok(two_sentences());
    ASSERT_TRUE(treebank);

    const auto path = std::filesystem::temp_directory_path() /
                      std::format("conllu-writer-test-{}.conllu", std::random_device{}());
    ASSERT_FALSE(Writer{}.write_file(treebank.get(), path));

    auto again = Parser{}.parse_file(path);
    std::filesystem::remove(path);
    ASSERT_TRUE(again.has_value()) << again.error().message();
    EXPECT_EQ(write(treebank.get()), write(again->get()));

    EXPECT_EQ(ErrorCode::StreamError, Writer{}.write_file(treebank.get(), "no/such/directory/out.conllu"));
}

// Round trips on real treebanks

namespace {
    class WriterDataFile : public ::testing::TestWithParam<DataFile> {};
} // namespace

TEST_P(WriterDataFile, MatchesTheSourceFile) {
    const auto path = data_path(GetParam().stem);
    auto treebank = Parser{}.parse_file(path);
    ASSERT_TRUE(treebank.has_value()) << treebank.error().message();

    // The output is the source plus the leading `# global.columns` line, except for line endings and spaces around
    // a comment's `=` (`#  key  =  value` is read as key "key" and value "value", and written as `# key = value`).
    std::vector<std::string> expected = lines_of(read_file(path));
    for (std::string& line : expected) {
        if (line.ends_with('\r'))
            line.pop_back();

        if (line.starts_with('#')) {
            const auto trim = [](std::string_view s) {
                while (!s.empty() && ' ' == s.front())
                    s.remove_prefix(1);
                while (!s.empty() && ' ' == s.back())
                    s.remove_suffix(1);
                return std::string(s);
            };
            const std::string_view body = std::string_view(line).substr(1);
            const std::size_t eq = body.find('=');
            const std::string key = trim(body.substr(0, eq));
            const std::string value = std::string_view::npos == eq ? std::string() : trim(body.substr(eq + 1));
            line = value.empty() ? "# " + key : "# " + key + " = " + value;
        }
    }

    std::vector<std::string> actual = lines_of(write(treebank->get()));
    ASSERT_FALSE(actual.empty());
    EXPECT_TRUE(actual.front().starts_with("# global.columns = "));
    actual.erase(actual.begin());

    ASSERT_EQ(expected.size(), actual.size());
    for (std::size_t i{0}; i < expected.size(); ++i)
        ASSERT_EQ(expected[i], actual[i]) << "line " << i + 1;
}

TEST_P(WriterDataFile, WriteParseWriteIsStable) {
    auto first = Parser{}.parse_file(data_path(GetParam().stem));
    ASSERT_TRUE(first.has_value()) << first.error().message();

    const std::string out = write(first->get());
    auto second = parse_ok(out);
    ASSERT_TRUE(second);
    EXPECT_EQ(out, write(second.get()));
}

INSTANTIATE_TEST_SUITE_P(Writer, WriterDataFile, ::testing::ValuesIn(c_data_files),
                         [](const ::testing::TestParamInfo<DataFile>& info) {
                             std::string name(info.param.stem);
                             std::ranges::replace(name, '-', '_');
                             return name;
                         });
