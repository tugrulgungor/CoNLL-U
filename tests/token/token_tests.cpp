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
    class TokenTest : public ::testing::Test {
    protected:
        std::unique_ptr<Treebank> treebank_ = standard_treebank();
        Sentence* sentence_ = treebank_->add_sentence("s").value();

        void SetUp() override {
            for (int i = 0; i < 3; ++i)
                ASSERT_TRUE(sentence_->add_token().has_value());
        }

        Token* token(const TokenId id) const {
            return sentence_->get_token_by_id(id).value();
        }
    };
} // namespace

// Fields

TEST_F(TokenTest, NewTokenHasOnlyId) {
    const Token* t = token(2);
    EXPECT_EQ(10u, t->num_fields());
    EXPECT_EQ("2", t->get_field_value("ID"));
    EXPECT_EQ("2", t->get_field_value(0));
    for (std::size_t pos{1}; pos < t->num_fields(); ++pos)
        EXPECT_FALSE(t->get_field_value(pos).has_value()) << "column " << pos;
}

TEST_F(TokenTest, SetAndUnsetByNameAndPosition) {
    Token* t = token(1);

    EXPECT_FALSE(t->set_field_value("FORM", "dogs"));
    EXPECT_FALSE(t->set_field_value(2, "dog"));
    EXPECT_EQ("dogs", t->get_field_value(1));
    EXPECT_EQ("dog", t->get_field_value("LEMMA"));

    EXPECT_FALSE(t->unset_field_value("FORM"));
    EXPECT_FALSE(t->unset_field_value(2));
    EXPECT_FALSE(t->get_field_value("FORM").has_value());
    EXPECT_FALSE(t->get_field_value("LEMMA").has_value());

    const auto view = t->fields();
    std::vector<std::optional<std::string_view>> fields(view.begin(), view.end());
    EXPECT_EQ(10u, fields.size());
    EXPECT_EQ("1", fields[0]);
}

TEST_F(TokenTest, RejectsUnknownColumnsAndPositions) {
    Token* t = token(1);
    EXPECT_EQ(ErrorCode::ColumnNotFound, t->set_field_value("NOPE", "x"));
    EXPECT_EQ(ErrorCode::ColumnNotFound, t->unset_field_value("NOPE"));
    EXPECT_EQ(ErrorCode::PositionOutOfRange, t->set_field_value(10, "x"));
    EXPECT_EQ(ErrorCode::PositionOutOfRange, t->unset_field_value(10));
    EXPECT_FALSE(t->get_field_value("NOPE").has_value());
    EXPECT_FALSE(t->get_field_value(99).has_value());
}

TEST_F(TokenTest, IdIsImmutable) {
    Token* t = token(1);
    EXPECT_EQ(ErrorCode::ImmutableTokenField, t->set_field_value("ID", "7"));
    EXPECT_EQ(ErrorCode::ImmutableTokenField, t->unset_field_value(0));
    EXPECT_EQ("1", t->get_field_value("ID"));
    EXPECT_EQ(1u, t->get_id());
}

// HEAD

TEST_F(TokenTest, SetHeadLinksTokens) {
    EXPECT_FALSE(token(2)->set_head(1));
    EXPECT_EQ(token(1), token(2)->get_head());
    EXPECT_EQ("1", token(2)->get_field_value("HEAD"));

    EXPECT_FALSE(token(1)->set_head(0)); // root
    EXPECT_FALSE(token(1)->get_head().has_value());
    EXPECT_EQ("0", token(1)->get_field_value("HEAD"));
}

TEST_F(TokenTest, SetHeadRejectsSelfAndMissingTokens) {
    EXPECT_EQ(ErrorCode::InvalidTokenHead, token(2)->set_head(2));
    EXPECT_EQ(ErrorCode::InvalidTokenHead, token(2)->set_head(4));
    EXPECT_FALSE(token(2)->get_field_value("HEAD").has_value());
}

TEST_F(TokenTest, HeadFieldGoesThroughSetHead) {
    EXPECT_FALSE(token(3)->set_field_value("HEAD", "2"));
    EXPECT_EQ(token(2), token(3)->get_head());

    EXPECT_EQ(ErrorCode::InvalidTokenHead, token(3)->set_field_value("HEAD", "x"));
    EXPECT_EQ(ErrorCode::InvalidTokenHead, token(3)->set_field_value("HEAD", "9"));
    EXPECT_EQ(token(2), token(3)->get_head());
}

// Empty nodes (N.M)

TEST_F(TokenTest, AddEmptyNodes) {
    Token* t = token(2);
    const Token* a = t->add_empty_node().value();
    const Token* b = t->add_empty_node().value();

    EXPECT_EQ(2u, t->empty_node_count());
    EXPECT_TRUE(a->is_empty_node());
    EXPECT_EQ(t, a->get_parent());
    EXPECT_EQ(1u, a->get_id());
    EXPECT_EQ(2u, b->get_id());
    EXPECT_EQ("2.1", a->get_field_value("ID"));
    EXPECT_EQ("2.2", b->get_field_value("ID"));
    EXPECT_EQ(10u, a->num_fields());
    EXPECT_FALSE(a->get_word().has_value());
    EXPECT_EQ(nullptr, t->get_parent());
    EXPECT_FALSE(t->is_empty_node());
    EXPECT_FALSE(sentence_->validate());
}

TEST_F(TokenTest, EmptyNodesCannotNest) {
    Token* node = token(1)->add_empty_node().value();
    EXPECT_EQ(ErrorCode::TokenEmptyNodeMismatch, node->add_empty_node().error());
}

TEST_F(TokenTest, RemoveMiddleEmptyNodeRenumbers) {
    Token* t = token(3);
    for (int i = 0; i < 3; ++i)
        ASSERT_TRUE(t->add_empty_node().has_value());

    EXPECT_FALSE(t->remove_empty_node(TokenId{2}));
    ASSERT_EQ(2u, t->empty_node_count());

    TokenId expected{0};
    for (const Token* node : t->empty_nodes()) {
        ++expected;
        EXPECT_EQ(expected, node->get_id());
        EXPECT_EQ(std::format("3.{}", expected), node->get_field_value("ID"));
    }
    EXPECT_FALSE(sentence_->validate());
}

TEST_F(TokenTest, RemoveEmptyNodeByPointerAndErrors) {
    Token* t = token(1);
    Token* node = t->add_empty_node().value();
    Token* other = token(2)->add_empty_node().value();

    EXPECT_EQ(ErrorCode::NodeNotFoundInToken, t->remove_empty_node(other));
    EXPECT_EQ(ErrorCode::InvalidPointer, t->remove_empty_node(static_cast<Token*>(nullptr)));
    EXPECT_EQ(ErrorCode::NodeNotFoundInToken, t->remove_empty_node(TokenId{0}));
    EXPECT_EQ(ErrorCode::NodeNotFoundInToken, t->remove_empty_node(TokenId{2}));

    EXPECT_FALSE(t->remove_empty_node(node));
    EXPECT_EQ(0u, t->empty_node_count());

    ASSERT_TRUE(t->add_empty_node().has_value());
    ASSERT_TRUE(t->add_empty_node().has_value());
    t->remove_all_empty_nodes();
    EXPECT_EQ(0u, t->empty_node_count());
}

// Words

TEST_F(TokenTest, WordMembership) {
    const Word* word = sentence_->add_word(2, 3).value();
    EXPECT_EQ(word, token(2)->get_word());
    EXPECT_EQ(word, token(3)->get_word());
    EXPECT_FALSE(token(1)->get_word().has_value());

    EXPECT_TRUE(word->contains_token(token(2)));
    EXPECT_FALSE(word->contains_token(token(1)));
    auto view = word->tokens();
    std::vector<const Token*> covered(view.begin(), view.end());
    EXPECT_EQ((std::vector<const Token*>{token(2), token(3)}), covered);
}
