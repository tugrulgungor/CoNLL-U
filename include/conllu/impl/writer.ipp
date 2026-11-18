/*
 * CoNLL-U - https://github.com/tugrulgungor/CoNLL-U
 *
 * Copyright (c) 2026. All rights reserved.
 * Tuğrul Güngör - https://conll-u.tugrulgungor.me
 *
 * Distributed under the MIT License.
 * https://opensource.org/license/mit/
 */

#ifndef TG_CONLLU_IMPL_WRITER_IPP
#define TG_CONLLU_IMPL_WRITER_IPP 1

#include "conllu/writer.hpp"

namespace tg::conllu {
    inline std::error_code Writer::write_columns_(std::ostream& output, const Treebank* treebank) noexcept {
        try {
            write_(output << "# global.columns =");

            for (const std::string& name : treebank->columns_)
                write_(output << " " << name);

            write_(output << "\n");

            return {};
        }
        catch (...) {
            return make_error_code(ErrorCode::StreamError);
        }
    }

    inline std::error_code Writer::write_document_(std::ostream& output, const Document* document) const noexcept {
        // Document comments are read back from between `# newdoc` and `# newpar`: with comments, both markers are
        // written even when the document or its first paragraph is implicit.
        const bool has_comments = !document->metadata_.empty();

        try {
            bool first_sentence{true};
            for (const std::unique_ptr<Paragraph>& paragraph : document->paragraphs_) {
                // Empty documents and paragraphs have no sentence to carry their marker: they are skipped.
                if (paragraph->sentences_.empty())
                    continue;

                const Document* doc_marker =
                first_sentence && (document->explicit_ || has_comments) ? document : nullptr;
                const Paragraph* par_marker =
                paragraph->explicit_ || (first_sentence && has_comments) ? paragraph.get() : nullptr;

                first_sentence = false;

                for (const std::unique_ptr<Sentence>& sentence : paragraph->sentences_) {
                    if (const auto err = write_sentence_(output, sentence.get(), doc_marker, par_marker))
                        return err;

                    doc_marker = nullptr;
                    par_marker = nullptr;
                }
            }

            return {};
        }
        catch (...) {
            return make_error_code(ErrorCode::StreamError);
        }
    }

    inline void Writer::write_comment_(std::ostream& output, const std::string_view key,
                                       const std::string_view value) noexcept(false) {
        if (value.empty())
            write_(output << "# " << key << "\n");
        else
            write_(output << "# " << key << " = " << value << "\n");
    }

    inline std::error_code Writer::write_sentence_(std::ostream& output, const Sentence* sentence,
                                                   const Document* document,
                                                   const Paragraph* paragraph) const noexcept {
        try {
            const auto write_markers = [&output, document, paragraph] {
                if (nullptr != document) {
                    write_comment_(output, document->id_.empty() ? "newdoc" : "newdoc id", document->id_);

                    for (const auto& [key, value] : document->metadata_)
                        write_comment_(output, key, value);
                }

                if (nullptr != paragraph)
                    write_comment_(output, paragraph->id_.empty() ? "newpar" : "newpar id", paragraph->id_);
            };

            const std::size_t leading = std::min(sentence->leading_comments_, sentence->metadata_.size());
            for (std::size_t i{0}; i < sentence->metadata_.size(); ++i) {
                if (leading == i)
                    write_markers();

                write_comment_(output, sentence->metadata_[i].first, sentence->metadata_[i].second);
            }

            if (leading == sentence->metadata_.size())
                write_markers();

            for (const Token* node : sentence->empty_nodes()) {
                if (const auto err = write_token_(output, node))
                    return err;
            }

            const Word* last_word{nullptr};
            for (const Token* token : sentence->tokens()) {
                const Word* word = token->get_word().value_or(nullptr);
                if (nullptr != word && word != last_word) {
                    if (const auto err = write_word_(output, word))
                        return err;
                }

                last_word = word;

                if (const auto err = write_token_(output, token))
                    return err;

                if (!token->is_empty_node()) {
                    for (const Token* node : token->empty_nodes()) {
                        if (const auto err = write_token_(output, node))
                            return err;
                    }
                }
            }

            write_(output << "\n");

            return {};
        }
        catch (...) {
            return make_error_code(ErrorCode::StreamError);
        }
    }

    inline std::error_code Writer::write_token_(std::ostream& output, const Token* token) const noexcept {
        try {
            bool first{true};
            for (const auto& field : token->fields_) {
                if (first)
                    first = false;
                else
                    write_(output << "\t");

                if (field.has_value()) {
                    write_(output << field.value());
                }
                else {
                    write_(output << options_.empty_value_indicator);
                }
            }

            write_(output << "\n");

            return {};
        }
        catch (...) {
            return make_error_code(ErrorCode::StreamError);
        }
    }

    inline std::error_code Writer::write_word_(std::ostream& output, const Word* word) const noexcept {
        const Treebank* treebank = word->sentence_->treebank_;
        const std::size_t num_columns = treebank->num_columns();

        const std::size_t id_idx = treebank->get_column_pos("ID").value_or(num_columns);
        const std::size_t form_idx = treebank->get_column_pos("FORM").value_or(num_columns);
        const std::size_t misc_idx = treebank->get_column_pos("MISC").value_or(num_columns);
        const std::size_t feats_idx = treebank->get_column_pos("FEATS").value_or(num_columns);

        try {
            for (std::size_t idx{0}; idx < num_columns; ++idx) {
                if (std::size_t{0} != idx)
                    write_(output << "\t");

                if (id_idx == idx) {
                    write_(output << word->range_.first << "-" << word->range_.last);
                    continue;
                }

                if (form_idx == idx && word->form_.has_value()) {
                    write_(output << word->form_.value());
                    continue;
                }

                if (misc_idx == idx && word->misc_.has_value()) {
                    write_(output << word->misc_.value());
                    continue;
                }

                if (feats_idx == idx && word->typo_.value_or(false)) {
                    write_(output << "Typo=Yes");
                    continue;
                }

                write_(output << options_.empty_value_indicator);
            }

            write_(output << "\n");

            return {};
        }
        catch (...) {
            return make_error_code(ErrorCode::StreamError);
        }
    }

    inline std::ostream& Writer::write_(std::ostream& output) noexcept(false) {
        if (!output)
            throw std::ios_base::failure("stream failure");

        return output;
    }

    // Constructors
    inline constexpr Writer::Writer(const WriterOptions options) noexcept : options_(options) {
    }

    // Writing
    inline std::error_code Writer::write(const Treebank* treebank, std::ostream&& stream) const noexcept {
        if (const auto err = write_columns_(stream, treebank))
            return err;

        for (const std::unique_ptr<Document>& document : treebank->documents_) {
            if (const auto err = write_document_(stream, document.get()))
                return err;
        }

        return {};
    }

    inline std::error_code Writer::write_file(const Treebank* treebank,
                                              const std::filesystem::path& path) const noexcept {
        std::ofstream stream(path, std::ios::binary);
        return write(treebank, std::move(stream));
    }
} // namespace tg::conllu

#endif // TG_CONLLU_IMPL_WRITER_IPP
