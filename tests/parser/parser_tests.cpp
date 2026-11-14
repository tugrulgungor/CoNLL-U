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

// Well-formed input

TEST(Parser, StandardColumnsAndContent) {
    auto treebank = parse_ok(two_sentences());
    ASSERT_TRUE(treebank);

    EXPECT_EQ((std::vector<std::string_view>{"ID", "FORM", "LEMMA", "UPOS", "XPOS", "FEATS", "HEAD", "DEPREL", "DEPS",
                                             "MISC"}),
              treebank->columns());
    ASSERT_EQ(2u, treebank->num_sentences());

    const Sentence* s1 = treebank->get_sentence_by_id("s1").value();
    EXPECT_EQ("text of s1", s1->get_text());
    ASSERT_EQ(2u, s1->num_tokens());

    const Token* hello = s1->get_token_by_id(1).value();
    const Token* world = s1->get_token_by_id(2).value();
    EXPECT_EQ("Hello", field(hello, "FORM"));
    EXPECT_EQ("obj", field(world, "DEPREL"));
    EXPECT_EQ(hello, world->get_head());
    EXPECT_FALSE(hello->get_head().has_value()); // HEAD 0 = root
    EXPECT_FALSE(hello->get_field_value("LEMMA").has_value()); // "_" = unset
    EXPECT_FALSE(treebank->validate());
}

TEST(Parser, MetadataLines) {
    auto treebank = parse_ok("# newdoc id = d1\n# sent_id = s1\n# text = A\n# flag\n" + row("1", "A", "0", "root"));
    ASSERT_TRUE(treebank);

    const Sentence* s = first_sentence(treebank.get());
    EXPECT_EQ("s1", s->get_sent_id());
    EXPECT_EQ("d1", s->get_metadata("newdoc id"));
    EXPECT_EQ("", s->get_metadata("flag"));
    EXPECT_EQ(4u, s->num_metadata_entries());
}

TEST(Parser, GlobalColumnsDeclaration) {
    auto treebank = parse_ok("# global.columns = ID FORM HEAD PARSEME:MWE\n"
                             "# sent_id = s1\n"
                             "1\tA\t0\t1:VID\n"
                             "2\tB\t1\t1\n");
    ASSERT_TRUE(treebank);

    EXPECT_EQ((std::vector<std::string_view>{"ID", "FORM", "HEAD", "PARSEME:MWE"}), treebank->columns());
    const Sentence* s = first_sentence(treebank.get());
    EXPECT_EQ("1:VID", field(s->get_token_by_id(1).value(), "PARSEME:MWE"));
    EXPECT_EQ(s->get_token_by_id(1).value(), s->get_token_by_id(2).value()->get_head());
}

TEST(Parser, MultiwordTokens) {
    auto treebank = parse_ok("# sent_id = s1\n"
                             "1-2\tvámonos\t_\t_\t_\t_\t_\t_\t_\tSpaceAfter=No\n"
                             "1\tvamos\t_\t_\t_\t_\t0\troot\t_\t_\n"
                             "2\tnos\t_\t_\t_\t_\t1\tobj\t_\t_\n"
                             "3-4\tdel\t_\t_\t_\tTypo=Yes\t_\t_\t_\t_\n"
                             "3\tde\t_\t_\t_\t_\t1\tcase\t_\t_\n"
                             "4\tel\t_\t_\t_\t_\t1\tdet\t_\t_\n");
    ASSERT_TRUE(treebank);

    const Sentence* s = first_sentence(treebank.get());
    ASSERT_EQ(2u, s->num_words());
    const Word* first = *s->words().begin();
    EXPECT_EQ(1u, first->first_id());
    EXPECT_EQ(2u, first->last_id());
    EXPECT_EQ("vámonos", first->get_form());
    EXPECT_EQ("SpaceAfter=No", first->get_misc());
    EXPECT_FALSE(first->get_typo().has_value());
    EXPECT_EQ(std::optional<bool>{true}, (*std::ranges::next(s->words().begin()))->get_typo());
    EXPECT_EQ(first, s->get_token_by_id(2).value()->get_word());
}

TEST(Parser, EmptyNodes) {
    auto treebank = parse_ok(sentence("s1", row("0.1", "E01", "_", "_", "1:dep") + row("0.2", "E02") +
                                            row("1", "A", "0", "root") + row("1.1", "E11") + row("1.2", "E12") +
                                            row("2", "B", "1", "dep")));
    ASSERT_TRUE(treebank);

    const Sentence* s = first_sentence(treebank.get());
    EXPECT_EQ(2u, s->num_tokens());
    ASSERT_EQ(2u, s->empty_node_count());
    EXPECT_EQ("E01", field(*s->empty_nodes().begin(), "FORM"));
    EXPECT_EQ("1:dep", field(*s->empty_nodes().begin(), "DEPS"));
    EXPECT_EQ(nullptr, (*s->empty_nodes().begin())->get_parent());

    const Token* one = s->get_token_by_id(1).value();
    ASSERT_EQ(2u, one->empty_node_count());
    EXPECT_EQ("1.2", field(*std::ranges::next(one->empty_nodes().begin()), "ID"));
    EXPECT_EQ(one, (*one->empty_nodes().begin())->get_parent());
    EXPECT_EQ(0u, s->get_token_by_id(2).value()->empty_node_count());
}

TEST(Parser, EmptyNodesBeforeMultiwordToken) {
    auto treebank = parse_ok(sentence("s1", row("0.1", "E") + row("1-2", "AB") + row("1", "A", "0", "root") +
                                            row("2", "B", "1", "dep")));
    ASSERT_TRUE(treebank);
    EXPECT_EQ(1u, first_sentence(treebank.get())->empty_node_count());
}

TEST(Parser, CustomEmptyValueIndicator) {
    auto res = Parser{ParserOptions{.empty_value_indicator = '*'}}.parse_memory(
    "# sent_id = s1\n1\t_\t*\t*\t*\t*\t0\troot\t*\t*\n");
    ASSERT_TRUE(res.has_value());

    const Token* t = first_sentence(res->get())->get_token_by_id(1).value();
    EXPECT_EQ("_", field(t, "FORM")); // a literal underscore
    EXPECT_FALSE(t->get_field_value("LEMMA").has_value());
}

// Layout tolerance

TEST(Parser, NoTrailingNewline) {
    auto treebank = parse_ok(sentence("s1", row("1", "A", "0", "root")) + "\n" + "# sent_id = s2\n1\tB\t_\t_\t_\t_\t0\troot\t_\t_");
    ASSERT_TRUE(treebank);
    EXPECT_EQ(2u, treebank->num_sentences());
    EXPECT_EQ("root", field(treebank->get_sentence_by_id("s2").value()->get_token_by_id(1).value(), "DEPREL"));
}

TEST(Parser, RepeatedLeadingAndTrailingBlankLines) {
    const std::string s1 = sentence("s1", row("1", "A", "0", "root"));
    const std::string s2 = sentence("s2", row("1", "B", "0", "root"));
    auto treebank = parse_ok("\n\n" + s1 + "\n\n\n" + s2 + "\n\n");
    ASSERT_TRUE(treebank);
    EXPECT_EQ(2u, treebank->num_sentences());
}

TEST(Parser, CrLfLineEndings) {
    std::string text;
    for (const char c : two_sentences()) {
        if ('\n' == c)
            text += '\r';
        text += c;
    }

    auto crlf = parse_ok(text);
    auto lf = parse_ok(two_sentences());
    ASSERT_TRUE(crlf && lf);
    EXPECT_EQ(write(lf.get()), write(crlf.get()));
    EXPECT_EQ(std::string::npos, write(crlf.get()).find('\r'));
    EXPECT_FALSE(first_sentence(crlf.get())->get_token_by_id(1).value()->get_field_value("MISC").has_value());
}

TEST(Parser, GlobalColumnsInItsOwnBlock) {
    auto treebank = parse_ok("# global.columns = ID FORM\n\n# sent_id = s1\n1\tA\n");
    ASSERT_TRUE(treebank);
    EXPECT_EQ(2u, treebank->num_columns());
    EXPECT_EQ(1u, treebank->num_sentences());
}

TEST(Parser, EmptyInput) {
    for (const std::string_view text : {"", "\n", "\n\n\n"}) {
        auto treebank = parse_ok(text);
        ASSERT_TRUE(treebank);
        EXPECT_TRUE(treebank->empty());
    }
}

// Entry points

TEST(Parser, ParseStreamAndBytes) {
    auto from_stream = Parser{}.parse(std::istringstream(two_sentences()));
    ASSERT_TRUE(from_stream.has_value());
    EXPECT_EQ(2u, (*from_stream)->num_sentences());

    const std::string text = two_sentences();
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(text.data());
    auto from_bytes = Parser{}.parse_memory(bytes, text.size());
    ASSERT_TRUE(from_bytes.has_value());
    EXPECT_EQ(2u, (*from_bytes)->num_sentences());
}

TEST(Parser, ParseFileMissing) {
    EXPECT_EQ(ErrorCode::StreamError, Parser{}.parse_file("this/file/does/not/exist.conllu").error());
}

// Malformed input

namespace {
    struct BadInput final {
        std::string name;
        std::string text;
        ErrorCode expected;
    };

    void PrintTo(const BadInput& input, std::ostream* os) {
        *os << input.name;
    }

    class ParserRejects : public ::testing::TestWithParam<BadInput> {};

    std::string ok(const std::string_view id = "s1") {
        return sentence(id, row("1", "A", "0", "root"));
    }
} // namespace

TEST_P(ParserRejects, WithExpectedError) {
    const auto res = parse(GetParam().text);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(GetParam().expected, res.error()) << res.error().message();
}

INSTANTIATE_TEST_SUITE_P(
Parser, ParserRejects,
::testing::Values(
// Sentence structure and metadata
BadInput{"FirstBlockWithoutMetaLine", row("1", "A", "0", "root"), ErrorCode::SentenceMustStartWithMetaLine},
BadInput{"MissingSentId", "# text = A\n" + row("1", "A", "0", "root"), ErrorCode::MissingSentenceId},
BadInput{"DuplicateSentId", ok() + "\n" + ok(), ErrorCode::DuplicateSentenceId},
BadInput{"InvalidSentId", "# sent_id = a/b\n" + row("1", "A", "0", "root"), ErrorCode::InvalidSentenceId},
BadInput{"DuplicateMetadataKey", "# sent_id = s1\n# k = 1\n# k = 2\n" + row("1", "A", "0", "root"),
         ErrorCode::DuplicateSentenceMetaEntry},
BadInput{"InvalidDeclaredColumn", "# global.columns = ID form\n# sent_id = s1\n1\tA\n", ErrorCode::InvalidColumnName},
BadInput{"DuplicateDeclaredColumn", "# global.columns = ID ID\n# sent_id = s1\n1\t1\n", ErrorCode::DuplicateColumnName},
// Tokens
BadInput{"TooFewFields", "# sent_id = s1\n1\tA\t_\n", ErrorCode::TokenFieldCountMismatch},
BadInput{"TooManyFields", "# sent_id = s1\n" + row("1", "A", "0", "root\t_"), ErrorCode::TokenFieldCountMismatch},
BadInput{"TokenIdNotSequential", sentence("s1", row("1", "A", "0", "root") + row("3", "B", "1", "dep")),
         ErrorCode::InvalidTokenId},
BadInput{"TokenIdNotANumber", sentence("s1", row("x", "A", "0", "root")), ErrorCode::InvalidTokenId},
BadInput{"HeadNotANumber", sentence("s1", row("1", "A", "x", "root")), ErrorCode::InvalidTokenHead},
BadInput{"HeadOutOfRange", sentence("s1", row("1", "A", "0", "root") + row("2", "B", "5", "dep")),
         ErrorCode::InvalidTokenHead},
BadInput{"HeadIsSelf", sentence("s1", row("1", "A", "1", "root")), ErrorCode::InvalidTokenHead},
// Multiword tokens
BadInput{"WordAfterItsFirstToken",
         sentence("s1", row("1", "A", "0", "root") + row("1-2", "AB") + row("2", "B", "1", "dep")),
         ErrorCode::WordMustAppearBeforeToken},
BadInput{"WordRangeReversed", sentence("s1", row("2-1", "AB") + row("1", "A", "0", "root") + row("2", "B", "1", "dep")),
         ErrorCode::InvalidWordRange},
BadInput{"WordRangeBeyondTokens", sentence("s1", row("1-3", "ABC") + row("1", "A", "0", "root") + row("2", "B", "1", "dep")),
         ErrorCode::InvalidWordRange},
BadInput{"WordRangesOverlap",
         sentence("s1", row("1-2", "AB") + row("1", "A", "0", "root") + row("2-3", "BC") + row("2", "B", "1", "dep") +
                        row("3", "C", "1", "dep")),
         ErrorCode::OverlappingWordRange},
BadInput{"WordWithLemma", "# sent_id = s1\n1-2\tAB\tab\t_\t_\t_\t_\t_\t_\t_\n" + row("1", "A", "0", "root") +
                          row("2", "B", "1", "dep"),
         ErrorCode::InvalidWordFieldValue},
BadInput{"WordWithOtherFeats", "# sent_id = s1\n1-2\tAB\t_\t_\t_\tNumber=Sing\t_\t_\t_\t_\n" +
                               row("1", "A", "0", "root") + row("2", "B", "1", "dep"),
         ErrorCode::InvalidWordFieldValue},
// Empty nodes
BadInput{"EmptyNodeBeforeItsToken", sentence("s1", row("1.1", "E") + row("1", "A", "0", "root")),
         ErrorCode::TokenEmptyNodeMismatch},
BadInput{"SentenceLevelEmptyNodeAfterToken", sentence("s1", row("1", "A", "0", "root") + row("0.1", "E")),
         ErrorCode::TokenEmptyNodeMismatch},
BadInput{"SentenceLevelEmptyNodeAfterWord",
         sentence("s1", row("1-2", "AB") + row("0.1", "E") + row("1", "A", "0", "root") + row("2", "B", "1", "dep")),
         ErrorCode::TokenEmptyNodeMismatch},
BadInput{"EmptyNodeNumberingGap", sentence("s1", row("1", "A", "0", "root") + row("1.2", "E")),
         ErrorCode::TokenEmptyNodeMismatch},
BadInput{"SentenceLevelEmptyNodeNumberingGap", sentence("s1", row("0.2", "E") + row("1", "A", "0", "root")),
         ErrorCode::TokenEmptyNodeMismatch},
BadInput{"EmptyNodeOfWrongToken",
         sentence("s1", row("1", "A", "0", "root") + row("2", "B", "1", "dep") + row("1.1", "E")),
         ErrorCode::TokenEmptyNodeMismatch},
BadInput{"EmptyNodeWithHead", sentence("s1", row("1", "A", "0", "root") + row("1.1", "E", "1", "dep")),
         ErrorCode::InvalidTokenHead}),
[](const ::testing::TestParamInfo<BadInput>& info) { return info.param.name; });

// Bundled UD treebanks

namespace {
    class ParserDataFile : public ::testing::TestWithParam<DataFile> {};
} // namespace

TEST_P(ParserDataFile, ParsesAndValidates) {
    auto res = Parser{}.parse_file(data_path(GetParam().stem));
    ASSERT_TRUE(res.has_value()) << res.error().message();
    EXPECT_EQ(GetParam().num_sentences, (*res)->num_sentences());
    EXPECT_FALSE((*res)->validate());
}

INSTANTIATE_TEST_SUITE_P(Parser, ParserDataFile, ::testing::ValuesIn(c_data_files),
                         [](const ::testing::TestParamInfo<DataFile>& info) {
                             std::string name(info.param.stem);
                             std::ranges::replace(name, '-', '_');
                             return name;
                         });

TEST(ParserDataFiles, EnglishGumHasSentenceLevelEmptyNodes) {
    auto res = Parser{}.parse_file(data_path("en_gum-ud-test"));
    ASSERT_TRUE(res.has_value()) << res.error().message();

    std::size_t count{0};
    for (const Sentence* s : (*res)->sentences())
        count += s->empty_node_count();
    EXPECT_LT(0u, count);
}
