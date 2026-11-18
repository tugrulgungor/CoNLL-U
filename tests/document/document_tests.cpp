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
    using Ids = std::vector<std::string>;
    using Shape = std::vector<std::vector<Ids>>;  // documents > paragraphs > sentence ids

    const std::string c_tok = row("1", "A", "0", "root");

    // s0: no markers (implicit document and paragraph)
    // s1: newdoc id = d1, bare newpar     s2: -     s3: newpar id = p2
    // s4: bare newdoc, no newpar (implicit paragraph)     s5: newpar
    std::string layout() {
        return "# sent_id = s0\n" + c_tok + "\n" +
               "# newdoc id = d1\n# newpar\n# sent_id = s1\n" + c_tok + "\n" +
               "# sent_id = s2\n" + c_tok + "\n" +
               "# newpar id = p2\n# sent_id = s3\n" + c_tok + "\n" +
               "# newdoc\n# sent_id = s4\n" + c_tok + "\n" +
               "# newpar\n# sent_id = s5\n" + c_tok;
    }

    Ids ids(auto&& sentences) {
        Ids out;
        for (const Sentence* s : sentences)
            out.emplace_back(s->get_sent_id());
        return out;
    }

    Shape shape(const Treebank* treebank) {
        Shape out;
        for (const Document* doc : treebank->documents()) {
            out.emplace_back();
            for (const Paragraph* par : doc->paragraphs())
                out.back().push_back(ids(par->sentences()));
        }
        return out;
    }

    std::vector<std::string> comment_lines(const std::string_view text) {
        std::vector<std::string> out;
        std::istringstream in{std::string(text)};
        for (std::string line; std::getline(in, line);) {
            if (line.starts_with("#") && !line.starts_with("# global.columns"))
                out.push_back(line);
        }
        return out;
    }

    Document* document(const Treebank* treebank, const std::size_t idx) {
        return *std::ranges::next(treebank->documents().begin(), static_cast<std::ptrdiff_t>(idx));
    }

    Paragraph* paragraph(const Document* doc, const std::size_t idx) {
        return *std::ranges::next(doc->paragraphs().begin(), static_cast<std::ptrdiff_t>(idx));
    }

    Sentence* sentence(const Treebank* treebank, const std::string_view sent_id) {
        return treebank->get_sentence_by_id(sent_id).value();
    }
} // namespace

// Parsing

TEST(Parsing, BuildsDocumentsAndParagraphs) {
    auto treebank = parse_ok(layout());
    ASSERT_TRUE(treebank);

    EXPECT_EQ((Shape{{{"s0"}}, {{"s1", "s2"}, {"s3"}}, {{"s4"}, {"s5"}}}), shape(treebank.get()));
    EXPECT_EQ((Ids{"s0", "s1", "s2", "s3", "s4", "s5"}), ids(treebank->sentences()));
    EXPECT_EQ(6u, treebank->num_sentences());
    EXPECT_EQ(3u, treebank->num_documents());

    const Document* d0 = document(treebank.get(), 0);
    EXPECT_FALSE(d0->is_explicit());
    EXPECT_FALSE(paragraph(d0, 0)->is_explicit());

    const Document* d1 = document(treebank.get(), 1);
    EXPECT_TRUE(d1->is_explicit());
    EXPECT_EQ("d1", d1->get_id());
    EXPECT_TRUE(paragraph(d1, 0)->is_explicit());
    EXPECT_FALSE(paragraph(d1, 0)->get_id().has_value());
    EXPECT_EQ("p2", paragraph(d1, 1)->get_id());

    const Document* d2 = document(treebank.get(), 2);
    EXPECT_TRUE(d2->is_explicit());
    EXPECT_FALSE(d2->get_id().has_value());
    EXPECT_FALSE(paragraph(d2, 0)->is_explicit());
    EXPECT_TRUE(paragraph(d2, 1)->is_explicit());

    // Markers are structure, not sentence comments.
    const Sentence* s1 = sentence(treebank.get(), "s1");
    EXPECT_EQ(1u, s1->num_metadata_entries());
    EXPECT_EQ(d1, s1->paragraph()->document());
    EXPECT_EQ(d1->num_sentences(), std::ranges::distance(d1->sentences()));
}

TEST(Parsing, RoundTripsTheMarkers) {
    auto treebank = parse_ok(layout());
    ASSERT_TRUE(treebank);
    EXPECT_EQ(comment_lines(layout()), comment_lines(write(treebank.get())));
}

TEST(Parsing, RejectsTwoMarkersOfOneKind) {
    EXPECT_EQ(ErrorCode::DuplicateSentenceMetaEntry, parse("# newdoc\n# newdoc id = d\n# sent_id = s\n" + c_tok).error());
    EXPECT_EQ(ErrorCode::DuplicateSentenceMetaEntry, parse("# newpar id = p\n# newpar\n# sent_id = s\n" + c_tok).error());
}

TEST(Parsing, DocumentCommentsAreBetweenNewdocAndNewpar) {
    const std::string text = "# newdoc id = d1\n"
                             "# meta::title = A title\n"
                             "# global.Entity = GRP-etype\n"
                             "# newpar\n"
                             "# newpar_block = head (1 s)\n"
                             "# sent_id = s1\n"
                             "# text = A\n" +
                             c_tok + "\n";

    auto treebank = parse_ok(text);
    ASSERT_TRUE(treebank);

    const Document* doc = document(treebank.get(), 0);
    EXPECT_EQ("A title", doc->get_metadata("meta::title"));
    EXPECT_EQ(2u, doc->num_metadata_entries());

    Ids keys;
    for (const auto& [key, value] : sentence(treebank.get(), "s1")->metadata())
        keys.emplace_back(key);
    EXPECT_EQ((Ids{"newpar_block", "sent_id", "text"}), keys);

    EXPECT_EQ(comment_lines(text), comment_lines(write(treebank.get())));
}

TEST(Parsing, WithoutNewparTheCommentsBelongToTheSentence) {
    auto treebank = parse_ok("# newdoc id = d1\n# genre = news\n# sent_id = s1\n" + c_tok);
    ASSERT_TRUE(treebank);
    EXPECT_EQ(0u, document(treebank.get(), 0)->num_metadata_entries());
    EXPECT_EQ("news", sentence(treebank.get(), "s1")->get_metadata("genre"));
}

TEST(Parsing, KeepsCommentsBeforeTheMarkersInPlace) {
    // As in ru_taiga: `# newdoc_id` is a plain comment, before `# newpar`.
    const std::string text = "# sent_id = s0\n" + c_tok + "\n" +
                             "# newdoc_id = x\n# newpar\n# genre = g\n# sent_id = s1\n" + c_tok + "\n" +
                             "# sent_id = s2\n# newpar\n" + c_tok + "\n";

    auto treebank = parse_ok(text);
    ASSERT_TRUE(treebank);
    EXPECT_EQ((Shape{{{"s0"}, {"s1"}, {"s2"}}}), shape(treebank.get()));
    EXPECT_EQ(comment_lines(text), comment_lines(write(treebank.get())));
}

TEST(Parsing, FailedSentenceLeavesNoDocumentOrParagraphBehind) {
    StreamParser parser;
    ASSERT_FALSE(parser.feed("# sent_id = s1\n" + c_tok + "\n"));
    EXPECT_TRUE(parser.feed("# newdoc id = d2\n# newpar\n# sent_id = s2\n1\tA\n\n"));

    EXPECT_EQ((Shape{{{"s1"}}}), shape(parser.treebank()));
    EXPECT_EQ(1u, parser.treebank()->num_sentences());
}

// Building

TEST(Building, AddSentenceUsesImplicitContainers) {
    auto treebank = standard_treebank();
    ASSERT_TRUE(treebank->add_sentence("s1").has_value());
    ASSERT_TRUE(treebank->add_sentence("s2").has_value());

    EXPECT_EQ((Shape{{{"s1", "s2"}}}), shape(treebank.get()));
    EXPECT_FALSE(document(treebank.get(), 0)->is_explicit());
    EXPECT_TRUE(comment_lines(write(treebank.get())) == (std::vector<std::string>{"# sent_id = s1", "# sent_id = s2"}));
}

TEST(Building, AddDocumentParagraphAndSentence) {
    auto treebank = standard_treebank();

    Document* d1 = treebank->add_document("d1").value();
    Paragraph* p1 = d1->add_paragraph("p1").value();
    ASSERT_TRUE(p1->add_sentence("s1", "One.").has_value());
    Paragraph* p2 = d1->add_paragraph().value();
    ASSERT_TRUE(p2->add_sentence("s2").has_value());

    // Treebank::add_sentence continues the last paragraph of the last document.
    ASSERT_TRUE(treebank->add_sentence("s3").has_value());

    Document* d2 = treebank->add_document().value();
    ASSERT_TRUE(treebank->add_sentence("s4").has_value());

    EXPECT_EQ((Shape{{{"s1"}, {"s2", "s3"}}, {{"s4"}}}), shape(treebank.get()));
    EXPECT_TRUE(d2->is_explicit());
    EXPECT_FALSE(paragraph(d2, 0)->is_explicit());
    EXPECT_EQ(p1, sentence(treebank.get(), "s1")->paragraph());

    EXPECT_EQ((std::vector<std::string>{"# newdoc id = d1", "# newpar id = p1", "# sent_id = s1", "# text = One.",
                                        "# newpar", "# sent_id = s2", "# sent_id = s3", "# newdoc", "# sent_id = s4"}),
              comment_lines(write(treebank.get())));

    auto again = parse_ok(write(treebank.get()));
    ASSERT_TRUE(again);
    EXPECT_EQ(shape(treebank.get()), shape(again.get()));
}

TEST(Building, InvalidSentenceIdLeavesNoImplicitContainers) {
    auto treebank = standard_treebank();
    EXPECT_EQ(ErrorCode::InvalidSentenceId, treebank->add_sentence("has space").error());
    EXPECT_EQ(0u, treebank->num_documents());
}

TEST(Building, ValidatesIds) {
    auto treebank = standard_treebank();
    EXPECT_EQ(ErrorCode::InvalidDocumentId, treebank->add_document("a\nb").error());
    EXPECT_EQ(0u, treebank->num_documents());

    Document* doc = treebank->add_document().value();
    EXPECT_EQ(ErrorCode::InvalidParagraphId, doc->add_paragraph(" p").error());
    EXPECT_EQ(0u, doc->num_paragraphs());
    EXPECT_EQ(ErrorCode::InvalidDocumentId, doc->set_id("d "));

    ASSERT_FALSE(doc->set_id("d"));
    EXPECT_EQ("d", doc->get_id());
    ASSERT_FALSE(doc->set_id(""));  // an empty id is no id
    EXPECT_FALSE(doc->get_id().has_value());
    EXPECT_TRUE(doc->is_explicit());
}

TEST(Building, BoundaryKeysAreReserved) {
    auto treebank = standard_treebank();
    Sentence* s = treebank->add_sentence("s").value();
    EXPECT_EQ(ErrorCode::ReservedMetaKey, s->set_metadata("newdoc"));
    EXPECT_EQ(ErrorCode::ReservedMetaKey, s->set_metadata_from_comment_line("# newpar id = p"));

    Document* doc = s->paragraph()->document();
    for (const std::string_view key : {"newdoc", "newdoc id", "newpar", "newpar id", "sent_id", "text"})
        EXPECT_EQ(ErrorCode::ReservedMetaKey, doc->set_metadata(key, "x")) << key;
}

TEST(Building, DocumentCommentsGetTheMarkersTheyNeed) {
    auto treebank = standard_treebank();
    ASSERT_TRUE(treebank->add_sentence("s1").has_value());

    // An implicit document with an implicit paragraph: both markers are needed to read the comment back.
    Document* doc = document(treebank.get(), 0);
    ASSERT_FALSE(doc->set_metadata("meta::title", "T"));

    const std::string out = write(treebank.get());
    EXPECT_EQ((std::vector<std::string>{"# newdoc", "# meta::title = T", "# newpar", "# sent_id = s1"}),
              comment_lines(out));

    auto again = parse_ok(out);
    ASSERT_TRUE(again);
    EXPECT_EQ("T", document(again.get(), 0)->get_metadata("meta::title"));
}

TEST(Building, EmptyDocumentsAndParagraphsAreSkipped) {
    auto treebank = standard_treebank();
    Document* empty_doc = treebank->add_document("empty").value();
    ASSERT_TRUE(empty_doc->add_paragraph("empty-p").has_value());

    Document* doc = treebank->add_document("d").value();
    ASSERT_TRUE(doc->add_paragraph("unused").has_value());
    ASSERT_TRUE(doc->add_paragraph("p").value()->add_sentence("s1").has_value());

    EXPECT_TRUE(empty_doc->empty());
    EXPECT_EQ((std::vector<std::string>{"# newdoc id = d", "# newpar id = p", "# sent_id = s1"}),
              comment_lines(write(treebank.get())));
}

// Structure

TEST(Structure, SplitParagraph) {
    auto treebank = parse_ok(layout());
    ASSERT_TRUE(treebank);
    Sentence* s2 = sentence(treebank.get(), "s2");
    Paragraph* p = s2->paragraph();

    Paragraph* tail = p->split(s2).value();
    EXPECT_NE(p, tail);
    EXPECT_TRUE(tail->is_explicit());
    EXPECT_EQ(tail, s2->paragraph());
    EXPECT_EQ((Shape{{{"s0"}}, {{"s1"}, {"s2"}, {"s3"}}, {{"s4"}, {"s5"}}}), shape(treebank.get()));

    // Splitting at the first sentence moves nothing and makes the paragraph explicit.
    Sentence* s4 = sentence(treebank.get(), "s4");
    Paragraph* implicit = s4->paragraph();
    EXPECT_EQ(implicit, implicit->split(s4).value());
    EXPECT_TRUE(implicit->is_explicit());

    EXPECT_EQ(ErrorCode::SentenceNotFoundInParagraph, p->split(s4).error());
    EXPECT_EQ(ErrorCode::InvalidPointer, p->split(nullptr).error());
}

TEST(Structure, MergeParagraphs) {
    auto treebank = parse_ok(layout());
    ASSERT_TRUE(treebank);
    Paragraph* p = sentence(treebank.get(), "s1")->paragraph();

    ASSERT_FALSE(p->merge_next());
    EXPECT_EQ((Shape{{{"s0"}}, {{"s1", "s2", "s3"}}, {{"s4"}, {"s5"}}}), shape(treebank.get()));
    EXPECT_EQ(p, sentence(treebank.get(), "s3")->paragraph());

    EXPECT_EQ(ErrorCode::NothingToMerge, p->merge_next());  // the last paragraph of its document
}

TEST(Structure, SplitDocumentInsideAParagraph) {
    auto treebank = parse_ok(layout());
    ASSERT_TRUE(treebank);
    Document* d1 = document(treebank.get(), 1);
    ASSERT_FALSE(d1->set_metadata("genre", "news"));

    Document* tail = d1->split(sentence(treebank.get(), "s2")).value();
    EXPECT_EQ((Shape{{{"s0"}}, {{"s1"}}, {{"s2"}, {"s3"}}, {{"s4"}, {"s5"}}}), shape(treebank.get()));
    EXPECT_TRUE(tail->is_explicit());
    EXPECT_FALSE(tail->get_id().has_value());
    EXPECT_EQ(0u, tail->num_metadata_entries());
    EXPECT_EQ("news", d1->get_metadata("genre"));

    // The rest of the split paragraph had no `# newpar`: it is the new document's implicit first paragraph.
    EXPECT_FALSE(paragraph(tail, 0)->is_explicit());
    EXPECT_EQ("p2", paragraph(tail, 1)->get_id());
    EXPECT_EQ(tail, sentence(treebank.get(), "s3")->paragraph()->document());

    auto again = parse_ok(write(treebank.get()));
    ASSERT_TRUE(again);
    EXPECT_EQ(shape(treebank.get()), shape(again.get()));
}

TEST(Structure, SplitDocumentAtAParagraph) {
    auto treebank = parse_ok(layout());
    ASSERT_TRUE(treebank);
    Document* d1 = document(treebank.get(), 1);
    Paragraph* p2 = sentence(treebank.get(), "s3")->paragraph();

    Document* tail = d1->split(sentence(treebank.get(), "s3")).value();
    EXPECT_EQ((Shape{{{"s0"}}, {{"s1", "s2"}}, {{"s3"}}, {{"s4"}, {"s5"}}}), shape(treebank.get()));
    EXPECT_EQ(p2, paragraph(tail, 0));  // the paragraph itself moves, keeping its id
    EXPECT_EQ("p2", p2->get_id());
}

TEST(Structure, SplitDocumentAtItsStart) {
    auto treebank = parse_ok(layout());
    ASSERT_TRUE(treebank);
    Document* d0 = document(treebank.get(), 0);

    EXPECT_EQ(d0, d0->split(sentence(treebank.get(), "s0")).value());
    EXPECT_TRUE(d0->is_explicit());
    EXPECT_EQ(3u, treebank->num_documents());

    EXPECT_EQ(ErrorCode::SentenceNotFoundInDocument, d0->split(sentence(treebank.get(), "s1")).error());
}

TEST(Structure, MergeDocuments) {
    auto treebank = parse_ok(layout());
    ASSERT_TRUE(treebank);
    Document* d1 = document(treebank.get(), 1);

    // The next document's first paragraph is implicit: it continues d1's last paragraph.
    ASSERT_FALSE(d1->merge_next());
    EXPECT_EQ((Shape{{{"s0"}}, {{"s1", "s2"}, {"s3", "s4"}, {"s5"}}}), shape(treebank.get()));
    EXPECT_EQ("d1", d1->get_id());
    EXPECT_EQ(d1, sentence(treebank.get(), "s5")->paragraph()->document());
    EXPECT_EQ(ErrorCode::NothingToMerge, d1->merge_next());

    // d1's first paragraph is explicit: it stays a paragraph of its own.
    Document* d0 = document(treebank.get(), 0);
    ASSERT_FALSE(d0->merge_next());
    EXPECT_EQ((Shape{{{"s0"}, {"s1", "s2"}, {"s3", "s4"}, {"s5"}}}), shape(treebank.get()));
    EXPECT_FALSE(d0->is_explicit());
    EXPECT_EQ(6u, treebank->num_sentences());
}

// Removal

TEST(Removal, RemoveDocumentAndParagraph) {
    auto treebank = parse_ok(layout());
    ASSERT_TRUE(treebank);
    Document* d1 = document(treebank.get(), 1);

    ASSERT_FALSE(d1->remove_paragraph(paragraph(d1, 1)));
    EXPECT_FALSE(treebank->get_sentence_by_id("s3").has_value());
    EXPECT_EQ(5u, treebank->num_sentences());

    ASSERT_FALSE(treebank->remove_document(d1));
    EXPECT_EQ((Shape{{{"s0"}}, {{"s4"}, {"s5"}}}), shape(treebank.get()));
    EXPECT_FALSE(treebank->get_sentence_by_id("s1").has_value());
    EXPECT_EQ(3u, treebank->num_sentences());

    // Their ids are free again.
    EXPECT_TRUE(treebank->add_sentence("s1").has_value());

    Document* d0 = document(treebank.get(), 0);
    EXPECT_EQ(ErrorCode::ParagraphNotFoundInDocument, d0->remove_paragraph(paragraph(document(treebank.get(), 1), 0)));
    EXPECT_EQ(ErrorCode::InvalidPointer, treebank->remove_document(nullptr));

    auto other = parse_ok(layout());
    ASSERT_TRUE(other);
    EXPECT_EQ(ErrorCode::DocumentNotFoundInTreebank, treebank->remove_document(document(other.get(), 0)));
}

TEST(Removal, RemovingTheLastSentenceKeepsTheParagraph) {
    auto treebank = parse_ok(layout());
    ASSERT_TRUE(treebank);
    ASSERT_FALSE(treebank->remove_sentence("s3"));

    EXPECT_EQ((Shape{{{"s0"}}, {{"s1", "s2"}, {}}, {{"s4"}, {"s5"}}}), shape(treebank.get()));
    EXPECT_EQ(std::string::npos, write(treebank.get()).find("p2"));
}

// Bundled UD treebanks (expected counts taken from the raw files)

namespace {
    struct Expected final {
        std::string_view stem;
        std::size_t documents;
        std::size_t paragraphs;
        std::size_t documents_with_id;
        std::size_t document_comments;
    };

    class DocumentDataFile : public ::testing::TestWithParam<Expected> {};
} // namespace

TEST_P(DocumentDataFile, Counts) {
    auto res = Parser{}.parse_file(data_path(GetParam().stem));
    ASSERT_TRUE(res.has_value()) << res.error().message();

    std::size_t paragraphs{0};
    std::size_t with_id{0};
    std::size_t comments{0};
    std::size_t sentences{0};
    for (const Document* doc : (*res)->documents()) {
        with_id += doc->get_id().has_value() ? 1 : 0;
        comments += doc->num_metadata_entries();
        paragraphs += doc->num_paragraphs();
        sentences += doc->num_sentences();
    }

    EXPECT_EQ(GetParam().documents, (*res)->num_documents());
    EXPECT_EQ(GetParam().paragraphs, paragraphs);
    EXPECT_EQ(GetParam().documents_with_id, with_id);
    EXPECT_EQ(GetParam().document_comments, comments);
    EXPECT_EQ((*res)->num_sentences(), sentences);
}

INSTANTIATE_TEST_SUITE_P(Documents, DocumentDataFile,
                         ::testing::Values(Expected{"de_hdt-ud-test", 1, 1, 0, 0},
                                           Expected{"en_gum-ud-test", 30, 563, 30, 450},
                                           Expected{"en_littleprince-ud-test", 9, 9, 9, 0},
                                           Expected{"ru_taiga-ud-test", 4, 461, 0, 0},
                                           Expected{"tr_boun-ud-test", 1, 1, 0, 0}),
                         [](const ::testing::TestParamInfo<Expected>& info) {
                             std::string name(info.param.stem);
                             std::ranges::replace(name, '-', '_');
                             return name;
                         });
