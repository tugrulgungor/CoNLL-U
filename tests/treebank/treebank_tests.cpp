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

// Columns

TEST(TreebankColumns, AddAndLookUp) {
    Treebank treebank;
    EXPECT_EQ(0u, treebank.num_columns());

    EXPECT_EQ(0u, treebank.add_column("ID").value());
    EXPECT_EQ(1u, treebank.add_column("FORM").value());
    EXPECT_EQ(2u, treebank.add_column("PARSEME:MWE").value());

    EXPECT_EQ(3u, treebank.num_columns());
    EXPECT_EQ(1u, treebank.get_column_pos("FORM"));
    EXPECT_EQ("PARSEME:MWE", treebank.get_column_name(2));
    EXPECT_FALSE(treebank.get_column_pos("LEMMA").has_value());
    EXPECT_FALSE(treebank.get_column_name(3).has_value());
    EXPECT_EQ((std::vector<std::string_view>{"ID", "FORM", "PARSEME:MWE"}), treebank.columns());
}

TEST(TreebankColumns, RejectsInvalidNames) {
    Treebank treebank;
    for (const std::string_view name : {"", "id", "Form", "A B", "A:", ":A", "A::B", "A:B:C", "A1"})
        EXPECT_EQ(ErrorCode::InvalidColumnName, treebank.add_column(name).error()) << '"' << name << '"';

    EXPECT_EQ(0u, treebank.num_columns());
}

TEST(TreebankColumns, RejectsDuplicates) {
    Treebank treebank;
    ASSERT_TRUE(treebank.add_column("FORM").has_value());
    EXPECT_EQ(ErrorCode::DuplicateColumnName, treebank.add_column("FORM").error());
    EXPECT_EQ(1u, treebank.num_columns());
}

TEST(TreebankColumns, AddColumnExtendsEveryRow) {
    auto treebank = parse_ok(sentence("s1", row("0.1", "E0") + row("1", "A", "0", "root") + row("1.1", "E1")));
    ASSERT_TRUE(treebank);
    ASSERT_TRUE(treebank->add_column("EXTRA").has_value());

    const Sentence* s = first_sentence(treebank.get());
    const Token* token = s->get_token_by_id(1).value();
    EXPECT_EQ(11u, token->num_fields());
    EXPECT_EQ(11u, (*s->empty_nodes().begin())->num_fields());
    EXPECT_EQ(11u, (*token->empty_nodes().begin())->num_fields());
    EXPECT_FALSE(token->get_field_value("EXTRA").has_value());
    EXPECT_FALSE(treebank->validate());
}

// Sentences

TEST(TreebankSentences, AddAndLookUp) {
    auto treebank = standard_treebank();
    EXPECT_TRUE(treebank->empty());

    Sentence* a = treebank->add_sentence("a").value();
    Sentence* b = treebank->add_sentence("b").value();

    EXPECT_FALSE(treebank->empty());
    EXPECT_EQ(2u, treebank->num_sentences());
    EXPECT_EQ(a, treebank->get_sentence_by_id("a"));
    EXPECT_EQ(b, treebank->get_sentence_by_id("b"));
    EXPECT_FALSE(treebank->get_sentence_by_id("c").has_value());
    EXPECT_EQ(treebank.get(), a->treebank());
    EXPECT_EQ("a", a->get_sent_id());
    EXPECT_EQ("a", a->get_metadata("sent_id"));
}

TEST(TreebankSentences, RejectsInvalidAndDuplicateIds) {
    auto treebank = standard_treebank();
    ASSERT_TRUE(treebank->add_sentence("a").has_value());

    EXPECT_EQ(ErrorCode::DuplicateSentenceId, treebank->add_sentence("a").error());
    for (const std::string_view id : {"", "a b", "a/b", "a\tb"})
        EXPECT_EQ(ErrorCode::InvalidSentenceId, treebank->add_sentence(id).error()) << '"' << id << '"';

    EXPECT_EQ(1u, treebank->num_sentences());
}

TEST(TreebankSentences, RemoveKeepsIndexConsistent) {
    auto treebank = standard_treebank();
    for (const std::string_view id : {"a", "b", "c", "d"})
        ASSERT_TRUE(treebank->add_sentence(id).has_value());

    EXPECT_FALSE(treebank->remove_sentence("b"));
    EXPECT_EQ(ErrorCode::SentenceNotFoundInTreebank, treebank->remove_sentence("b"));

    Sentence* c = treebank->get_sentence_by_id("c").value();
    EXPECT_FALSE(treebank->remove_sentence(c));
    EXPECT_EQ(ErrorCode::InvalidPointer, treebank->remove_sentence(static_cast<const Sentence*>(nullptr)));

    ASSERT_EQ(2u, treebank->num_sentences());
    EXPECT_EQ("a", treebank->get_sentence_by_id("a").value()->get_sent_id());
    EXPECT_EQ("d", treebank->get_sentence_by_id("d").value()->get_sent_id());
}

TEST(TreebankSentences, RemoveForeignSentenceFails) {
    auto treebank = standard_treebank();
    auto other = standard_treebank();
    const Sentence* foreign = other->add_sentence("x").value();

    EXPECT_EQ(ErrorCode::SentenceNotFoundInTreebank, treebank->remove_sentence(foreign));
}

TEST(TreebankSentences, RenameUpdatesIndex) {
    auto treebank = standard_treebank();
    Sentence* s = treebank->add_sentence("old").value();
    ASSERT_TRUE(treebank->add_sentence("taken").has_value());

    EXPECT_FALSE(s->set_sent_id("new"));
    EXPECT_EQ("new", s->get_sent_id());
    EXPECT_EQ("new", s->get_metadata("sent_id"));
    EXPECT_EQ(s, treebank->get_sentence_by_id("new"));
    EXPECT_FALSE(treebank->get_sentence_by_id("old").has_value());

    EXPECT_EQ(ErrorCode::DuplicateSentenceId, s->set_sent_id("taken"));
    EXPECT_EQ(ErrorCode::InvalidSentenceId, s->set_sent_id("has space"));
    EXPECT_EQ("new", s->get_sent_id());
}

// State

TEST(TreebankState, ClearRemovesEverything) {
    auto treebank = parse_ok(two_sentences());
    ASSERT_TRUE(treebank);

    treebank->clear();
    EXPECT_TRUE(treebank->empty());
    EXPECT_EQ(0u, treebank->num_columns());
    EXPECT_FALSE(treebank->get_sentence_by_id("s1").has_value());
}

TEST(TreebankState, HandBuiltTreebankValidates) {
    auto treebank = standard_treebank();
    Sentence* s = treebank->add_sentence("s").value();
    ASSERT_TRUE(s->add_token().has_value());
    ASSERT_TRUE(s->add_token().has_value());
    ASSERT_TRUE(s->add_empty_node().has_value());
    ASSERT_TRUE(s->get_token_by_id(1).value()->add_empty_node().has_value());
    ASSERT_TRUE(s->add_word(1, 2).has_value());
    EXPECT_FALSE(treebank->validate());
}
