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
    const std::string s1 = sentence("s1", row("1", "A", "0", "root") + row("2", "B", "1", "dep"));
    const std::string s2 = sentence("s2", row("1", "C", "0", "root"));
} // namespace

// Feeding

TEST(StreamParser, OneByteChunks) {
    StreamParser parser;
    ASSERT_FALSE(feed_chunked(parser, s1 + "\n" + s2 + "\n", 1));

    auto res = parser.finish();
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(2u, (*res)->num_sentences());
    EXPECT_EQ(write(parse_ok(s1 + "\n" + s2 + "\n").get()), write(res->get()));
}

TEST(StreamParser, SentencesAppearWhenClosed) {
    StreamParser parser;
    EXPECT_EQ(nullptr, parser.treebank());

    ASSERT_FALSE(parser.feed(s1));
    EXPECT_TRUE(nullptr == parser.treebank() || parser.treebank()->empty());

    ASSERT_FALSE(parser.feed("\n"));
    ASSERT_NE(nullptr, parser.treebank());
    EXPECT_EQ(1u, parser.treebank()->num_sentences());
    EXPECT_EQ(5u, parser.num_lines()); // 4 sentence lines + the closing blank line

    ASSERT_FALSE(parser.feed(s2)); // finish() handles the missing blank line
    auto res = parser.finish();
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(2u, (*res)->num_sentences());
}

TEST(StreamParser, ChunkEndingWithCarriageReturn) {
    StreamParser parser;
    const std::string text = "# sent_id = s1\r\n1\tA\t_\t_\t_\t_\t0\troot\t_\t_\r\n";
    const std::size_t cr = text.rfind('\r');
    ASSERT_FALSE(parser.feed(std::string_view(text).substr(0, cr + 1)));
    ASSERT_FALSE(parser.feed(std::string_view(text).substr(cr + 1)));

    auto res = parser.finish();
    ASSERT_TRUE(res.has_value());
    EXPECT_FALSE(first_sentence(res->get())->get_token_by_id(1).value()->get_field_value("MISC").has_value());
}

TEST(StreamParser, EmptyFeedsAreHarmless) {
    StreamParser parser;
    ASSERT_FALSE(parser.feed(""));
    ASSERT_FALSE(parser.feed(s2));
    ASSERT_FALSE(parser.feed(""));
    auto res = parser.finish();
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(1u, (*res)->num_sentences());
}

TEST(StreamParser, Options) {
    StreamParser parser{ParserOptions{.empty_value_indicator = '*'}};
    ASSERT_FALSE(parser.feed("# sent_id = s1\n1\t_\t*\t*\t*\t*\t0\troot\t*\t*\n"));
    auto res = parser.finish();
    ASSERT_TRUE(res.has_value());

    const Token* t = first_sentence(res->get())->get_token_by_id(1).value();
    EXPECT_EQ("_", field(t, "FORM"));
    EXPECT_FALSE(t->get_field_value("LEMMA").has_value());
}

TEST(StreamParser, EmptyInputGivesEmptyTreebank) {
    StreamParser parser;
    auto res = parser.finish();
    ASSERT_TRUE(res.has_value());
    EXPECT_TRUE((*res)->empty());
}

// Errors

TEST(StreamParser, ErrorIsStickyLocatedAndRolledBack) {
    // Lines 1-4: s1, 5: blank, 6-7: s2 metadata, 8: token with a wrong ID.
    const std::string bad = s1 + "\n" + sentence("s2", row("5", "C", "0", "root")) + "\n";

    StreamParser parser;
    const auto err = feed_chunked(parser, bad, 5);
    EXPECT_EQ(ErrorCode::InvalidTokenId, err);
    EXPECT_EQ(err, parser.error());
    EXPECT_EQ(6u, parser.error_line());

    EXPECT_EQ(err, parser.feed(s2));
    auto res = parser.finish();
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(err, res.error());

    // The half-built s2 is gone; s1 survives.
    ASSERT_NE(nullptr, parser.treebank());
    EXPECT_EQ(1u, parser.treebank()->num_sentences());
    EXPECT_TRUE(parser.treebank()->get_sentence_by_id("s1").has_value());
    EXPECT_FALSE(parser.treebank()->get_sentence_by_id("s2").has_value());
}

TEST(StreamParser, ErrorInFinalSentenceWithoutBlankLine) {
    StreamParser parser;
    ASSERT_FALSE(parser.feed(s1 + "\n" + "# sent_id = s2\n1\tA\n"));

    auto res = parser.finish();
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(ErrorCode::TokenFieldCountMismatch, res.error());
    EXPECT_EQ(6u, parser.error_line());
}

TEST(StreamParser, ResetAllowsReuse) {
    StreamParser parser;
    ASSERT_TRUE(parser.feed(row("1", "A", "0", "root") + "\n"));
    ASSERT_TRUE(parser.error());

    parser.reset();
    EXPECT_FALSE(parser.error());
    EXPECT_EQ(0u, parser.error_line());
    EXPECT_EQ(0u, parser.num_lines());
    EXPECT_EQ(nullptr, parser.treebank());

    ASSERT_FALSE(parser.feed(s2));
    auto res = parser.finish();
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(1u, (*res)->num_sentences());
}

TEST(StreamParser, FinishResetsForNextInput) {
    StreamParser parser;
    ASSERT_FALSE(parser.feed(s1));
    auto first = parser.finish();
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(nullptr, parser.treebank());
    EXPECT_EQ(0u, parser.num_lines());

    // A new input starts over, including column detection.
    ASSERT_FALSE(parser.feed("# global.columns = ID FORM\n# sent_id = s1\n1\tA\n"));
    auto second = parser.finish();
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(2u, (*second)->num_columns());
    EXPECT_EQ(10u, (*first)->num_columns());
}

// Equivalence with Parser on real treebanks

namespace {
    class StreamParserDataFile : public ::testing::TestWithParam<DataFile> {};
} // namespace

TEST_P(StreamParserDataFile, RandomChunksMatchWholeFileParse) {
    const auto path = data_path(GetParam().stem);
    auto whole = Parser{}.parse_file(path);
    ASSERT_TRUE(whole.has_value()) << whole.error().message();

    StreamParser parser;
    ASSERT_FALSE(feed_chunked(parser, read_file(path), 4096, 7));
    auto chunked = parser.finish();
    ASSERT_TRUE(chunked.has_value()) << chunked.error().message();

    EXPECT_EQ(GetParam().num_sentences, (*chunked)->num_sentences());
    EXPECT_EQ(write(whole->get()), write(chunked->get()));
}

INSTANTIATE_TEST_SUITE_P(StreamParser, StreamParserDataFile, ::testing::ValuesIn(c_data_files),
                         [](const ::testing::TestParamInfo<DataFile>& info) {
                             std::string name(info.param.stem);
                             std::ranges::replace(name, '-', '_');
                             return name;
                         });
