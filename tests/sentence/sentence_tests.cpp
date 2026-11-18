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
    class SentenceTest : public ::testing::Test {
    protected:
        std::unique_ptr<Treebank> treebank_ = standard_treebank();
        Sentence* sentence_ = treebank_->add_sentence("s").value();

        void add_tokens(const std::size_t count) {
            for (std::size_t i{0}; i < count; ++i)
                ASSERT_TRUE(sentence_->add_token().has_value());
        }

        Token* token(const TokenId id) const {
            return sentence_->get_token_by_id(id).value();
        }
    };
} // namespace

// Metadata

TEST_F(SentenceTest, MetadataSetGetUnset) {
    EXPECT_FALSE(sentence_->set_metadata("genre", "news"));
    EXPECT_EQ("news", sentence_->get_metadata("genre"));
    EXPECT_FALSE(sentence_->set_metadata("flag"));
    EXPECT_EQ("", sentence_->get_metadata("flag"));
    EXPECT_FALSE(sentence_->get_metadata("missing").has_value());

    EXPECT_EQ(ErrorCode::DuplicateSentenceMetaEntry, sentence_->set_metadata("flag", "again"));
    EXPECT_EQ(ErrorCode::InvalidSentenceMetaKey, sentence_->set_metadata("", "value"));

    EXPECT_FALSE(sentence_->unset_metadata("genre"));
    EXPECT_FALSE(sentence_->get_metadata("genre").has_value());
    EXPECT_FALSE(sentence_->unset_metadata("never-set"));
    EXPECT_EQ(ErrorCode::SentenceIdCannotBeRemoved, sentence_->unset_metadata("sent_id"));
}

TEST_F(SentenceTest, MetadataFromCommentLine) {
    EXPECT_FALSE(sentence_->set_metadata_from_comment_line("# s_type = frag"));
    EXPECT_EQ("frag", sentence_->get_metadata("s_type"));

    EXPECT_FALSE(sentence_->set_metadata_from_comment_line("#  spaced   =   value  "));
    EXPECT_EQ("value", sentence_->get_metadata("spaced"));

    EXPECT_FALSE(sentence_->set_metadata_from_comment_line("# bare_key"));
    EXPECT_EQ("", sentence_->get_metadata("bare_key"));

    const std::size_t before = sentence_->num_metadata_entries();
    EXPECT_FALSE(sentence_->set_metadata_from_comment_line("#"));
    EXPECT_FALSE(sentence_->set_metadata_from_comment_line("#   "));
    EXPECT_EQ(before, sentence_->num_metadata_entries());

    EXPECT_EQ(ErrorCode::InvalidSentenceMetaLine, sentence_->set_metadata_from_comment_line("no hash"));
    EXPECT_EQ(ErrorCode::InvalidSentenceMetaLine, sentence_->set_metadata_from_comment_line(""));
    EXPECT_EQ(ErrorCode::InvalidSentenceMetaKey, sentence_->set_metadata_from_comment_line("# = value"));
}

TEST_F(SentenceTest, TextIsMirroredInMetadata) {
    EXPECT_FALSE(sentence_->set_text("Hello world"));
    EXPECT_EQ("Hello world", sentence_->get_text());
    EXPECT_EQ("Hello world", sentence_->get_metadata("text"));

    EXPECT_FALSE(sentence_->set_text("Replaced"));
    EXPECT_EQ("Replaced", sentence_->get_metadata("text"));

    EXPECT_FALSE(sentence_->unset_metadata("text"));
    EXPECT_EQ("", sentence_->get_text());
}

TEST(SentenceText, TextGivenToAddSentenceIsMirroredInMetadata) {
    auto treebank = standard_treebank();
    const Sentence* s = treebank->add_sentence("s", "Hello world").value();

    EXPECT_EQ("Hello world", s->get_text());
    EXPECT_EQ("Hello world", s->get_metadata("text")); // the Writer only writes metadata entries
}

TEST_F(SentenceTest, RemoveAllMetadataKeepsSentId) {
    ASSERT_FALSE(sentence_->set_text("t"));
    ASSERT_FALSE(sentence_->set_metadata("a", "1"));

    sentence_->remove_all_metadata();
    EXPECT_EQ(1u, sentence_->num_metadata_entries());
    EXPECT_EQ("s", sentence_->get_metadata("sent_id"));
    EXPECT_EQ("", sentence_->get_text());
}

// Tokens

TEST_F(SentenceTest, AddTokenAssignsSequentialIds) {
    EXPECT_TRUE(sentence_->empty());
    add_tokens(3);

    EXPECT_FALSE(sentence_->empty());
    EXPECT_EQ(3u, sentence_->num_tokens());
    TokenId expected{0};
    for (const Token* t : sentence_->tokens()) {
        ++expected;
        EXPECT_EQ(expected, t->get_id());
        EXPECT_EQ(std::to_string(expected), field(t, "ID"));
        EXPECT_EQ(sentence_, t->sentence());
        EXPECT_FALSE(t->is_empty_node());
    }

    EXPECT_FALSE(sentence_->get_token_by_id(0).has_value());
    EXPECT_FALSE(sentence_->get_token_by_id(4).has_value());
}

TEST_F(SentenceTest, RemoveTokenRefusesDanglingHeadsUnlessSkipped) {
    add_tokens(3);
    ASSERT_FALSE(token(1)->set_head(2));

    EXPECT_EQ(ErrorCode::InvalidTokenHead, sentence_->remove_token(2));
    EXPECT_EQ(3u, sentence_->num_tokens());

    EXPECT_FALSE(sentence_->remove_token(2, true));
    EXPECT_EQ(2u, sentence_->num_tokens());
    EXPECT_FALSE(token(1)->get_head().has_value());
    EXPECT_FALSE(token(1)->get_field_value("HEAD").has_value());
}

TEST_F(SentenceTest, RemoveTokenRenumbersIdsAndHeads) {
    add_tokens(4);
    ASSERT_FALSE(token(4)->set_head(3));
    ASSERT_FALSE(token(3)->set_head(1));

    EXPECT_FALSE(sentence_->remove_token(2));
    ASSERT_EQ(3u, sentence_->num_tokens());

    // Former tokens 3 and 4 are now 2 and 3, and their HEADs follow.
    EXPECT_EQ(2u, token(2)->get_id());
    EXPECT_EQ(3u, token(3)->get_id());
    EXPECT_EQ(token(2), token(3)->get_head());
    EXPECT_EQ("2", field(token(3), "HEAD"));
    EXPECT_EQ(token(1), token(2)->get_head());
    EXPECT_EQ("1", field(token(2), "HEAD"));
    EXPECT_FALSE(sentence_->validate());
}

TEST_F(SentenceTest, RemoveTokenRenumbersIdFields) {
    add_tokens(3);
    ASSERT_TRUE(token(3)->add_empty_node().has_value());

    EXPECT_FALSE(sentence_->remove_token(1));
    // The ID column is what the Writer outputs, so it has to follow the renumbering.
    EXPECT_EQ("1", field(token(1), "ID"));
    EXPECT_EQ("2", field(token(2), "ID"));
    EXPECT_EQ("2.1", field(*token(2)->empty_nodes().begin(), "ID"));
}

TEST_F(SentenceTest, RemoveTokenByPointer) {
    add_tokens(2);
    EXPECT_FALSE(sentence_->remove_token(token(1)));
    EXPECT_EQ(1u, sentence_->num_tokens());

    EXPECT_EQ(ErrorCode::InvalidPointer, sentence_->remove_token(static_cast<Token*>(nullptr)));
    EXPECT_EQ(ErrorCode::TokenNotFoundInSentence, sentence_->remove_token(TokenId{5}));
    EXPECT_EQ(ErrorCode::TokenNotFoundInSentence, sentence_->remove_token(TokenId{0}));

    Sentence* other = treebank_->add_sentence("other").value();
    Token* foreign = other->add_token().value();
    EXPECT_EQ(ErrorCode::TokenNotFoundInSentence, sentence_->remove_token(foreign));
}

// Words

TEST_F(SentenceTest, AddWordValidatesRange) {
    add_tokens(4);

    Word* word = sentence_->add_word(2, 3).value();
    EXPECT_EQ(2u, word->first_id());
    EXPECT_EQ(3u, word->last_id());
    EXPECT_EQ(2u, word->num_tokens());
    EXPECT_EQ(word, token(2)->get_word());
    EXPECT_TRUE(token(3)->is_in_word());
    EXPECT_FALSE(token(1)->is_in_word());

    EXPECT_EQ(ErrorCode::OverlappingWordRange, sentence_->add_word(3, 4).error());
    EXPECT_EQ(ErrorCode::OverlappingWordRange, sentence_->add_word(1, 4).error());
    EXPECT_EQ(ErrorCode::InvalidWordRange, sentence_->add_word(4, 1).error());
    EXPECT_EQ(ErrorCode::InvalidWordRange, sentence_->add_word(1, 1).error());
    EXPECT_EQ(ErrorCode::InvalidWordRange, sentence_->add_word(0, 1).error());
    EXPECT_EQ(ErrorCode::InvalidWordRange, sentence_->add_word(4, 5).error());
    EXPECT_EQ(1u, sentence_->num_words());
}

TEST_F(SentenceTest, RemoveTokenShiftsAndDropsWords) {
    add_tokens(5);
    ASSERT_TRUE(sentence_->add_word(1, 2).has_value());
    const Word* later = sentence_->add_word(4, 5).value();

    EXPECT_FALSE(sentence_->remove_token(3));
    EXPECT_EQ(3u, later->first_id());
    EXPECT_EQ(4u, later->last_id());

    // Removing a token from a two-token word leaves one token, so the word disappears.
    EXPECT_FALSE(sentence_->remove_token(1));
    EXPECT_EQ(1u, sentence_->num_words());
    EXPECT_FALSE(sentence_->validate());
}

TEST_F(SentenceTest, RemoveWordWithAndWithoutTokens) {
    add_tokens(5);
    const Word* a = sentence_->add_word(1, 2).value();
    const Word* b = sentence_->add_word(3, 4).value();

    EXPECT_FALSE(sentence_->remove_word(a));
    EXPECT_EQ(1u, sentence_->num_words());
    EXPECT_EQ(5u, sentence_->num_tokens());

    EXPECT_FALSE(sentence_->remove_word(b, true));
    EXPECT_EQ(0u, sentence_->num_words());
    EXPECT_EQ(3u, sentence_->num_tokens());

    EXPECT_EQ(ErrorCode::InvalidPointer, sentence_->remove_word(nullptr));
    Sentence* other = treebank_->add_sentence("other").value();
    ASSERT_TRUE(other->add_token().has_value());
    ASSERT_TRUE(other->add_token().has_value());
    EXPECT_EQ(ErrorCode::WordNotFoundInSentence, sentence_->remove_word(other->add_word(1, 2).value()));
}

TEST_F(SentenceTest, RemoveAllWords) {
    add_tokens(4);
    ASSERT_TRUE(sentence_->add_word(1, 2).has_value());
    ASSERT_TRUE(sentence_->add_word(3, 4).has_value());

    sentence_->remove_all_words();
    EXPECT_EQ(0u, sentence_->num_words());
    EXPECT_EQ(4u, sentence_->num_tokens());

    ASSERT_TRUE(sentence_->add_word(1, 2).has_value());
    sentence_->remove_all_words(true);
    EXPECT_EQ(0u, sentence_->num_words());
    EXPECT_EQ(2u, sentence_->num_tokens());
}

// Sentence-level empty nodes (0.N)

TEST_F(SentenceTest, SentenceLevelEmptyNodes) {
    add_tokens(1);
    EXPECT_EQ(0u, sentence_->empty_node_count());

    for (int i = 0; i < 3; ++i)
        ASSERT_TRUE(sentence_->add_empty_node().has_value());

    TokenId expected{0};
    for (const Token* node : sentence_->empty_nodes()) {
        ++expected;
        EXPECT_TRUE(node->is_empty_node());
        EXPECT_EQ(nullptr, node->get_parent());
        EXPECT_EQ(expected, node->get_id());
        EXPECT_EQ(std::format("0.{}", expected), field(node, "ID"));
        EXPECT_FALSE(node->get_word().has_value());
        EXPECT_FALSE(node->is_in_word());
    }
    EXPECT_EQ(3u, sentence_->empty_node_count());
    EXPECT_EQ(1u, sentence_->num_tokens()); // not counted as tokens
    EXPECT_FALSE(sentence_->validate());
}

TEST_F(SentenceTest, RemoveSentenceLevelEmptyNodeRenumbers) {
    for (int i = 0; i < 3; ++i)
        ASSERT_TRUE(sentence_->add_empty_node().has_value());

    EXPECT_FALSE(sentence_->remove_empty_node(TokenId{1}));
    ASSERT_EQ(2u, sentence_->empty_node_count());
    TokenId expected{0};
    for (const Token* node : sentence_->empty_nodes()) {
        ++expected;
        EXPECT_EQ(expected, node->get_id());
        EXPECT_EQ(std::format("0.{}", expected), field(node, "ID"));
    }

    EXPECT_FALSE(sentence_->remove_empty_node(*sentence_->empty_nodes().begin()));
    EXPECT_EQ(1u, sentence_->empty_node_count());

    EXPECT_EQ(ErrorCode::NodeNotFoundInToken, sentence_->remove_empty_node(TokenId{5}));
    EXPECT_EQ(ErrorCode::NodeNotFoundInToken, sentence_->remove_empty_node(TokenId{0}));
    EXPECT_EQ(ErrorCode::InvalidPointer, sentence_->remove_empty_node(static_cast<Token*>(nullptr)));

    add_tokens(1);
    EXPECT_EQ(ErrorCode::NodeNotFoundInToken, sentence_->remove_empty_node(token(1)));

    sentence_->remove_all_empty_nodes();
    EXPECT_EQ(0u, sentence_->empty_node_count());
}

// State

TEST_F(SentenceTest, ClearResetsContentButKeepsSentId) {
    add_tokens(2);
    ASSERT_TRUE(sentence_->add_word(1, 2).has_value());
    ASSERT_TRUE(sentence_->add_empty_node().has_value());
    ASSERT_FALSE(sentence_->set_text("t"));

    sentence_->clear();
    EXPECT_TRUE(sentence_->empty());
    EXPECT_EQ(0u, sentence_->num_words());
    EXPECT_EQ(0u, sentence_->empty_node_count());
    EXPECT_EQ("", sentence_->get_text());
    EXPECT_EQ("s", sentence_->get_sent_id());
    EXPECT_EQ(sentence_, treebank_->get_sentence_by_id("s"));
}
