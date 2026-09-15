/*
 * CoNLL-U - https://github.com/tugrulgungor/CoNLL-U
 *
 * Copyright (c) 2026. All rights reserved.
 * Tuğrul Güngör - https://conll-u.tugrulgungor.me
 *
 * Distributed under the MIT License.
 * https://opensource.org/license/mit/
 */

#ifndef TG_CONLLU_IMPL_ERROR_IPP
#define TG_CONLLU_IMPL_ERROR_IPP 1

#include "conllu/error.hpp"

namespace tg::conllu {
    inline constexpr const char* ErrorCategory::name() const noexcept {
        return "tg::conllu";
    }

    inline constexpr std::string ErrorCategory::message(int ev) const noexcept {
        switch (static_cast<ErrorCode>(ev)) {
            case ErrorCode::Success:
                return "success";
            case ErrorCode::StreamError:
                return "stream error";
            case ErrorCode::InsufficientMemory:
                return "insufficient memory";
            case ErrorCode::PositionOutOfRange:
                return "position out of range";
            case ErrorCode::InvalidPointer:
                return "invalid pointer";
            case ErrorCode::InvalidColumnsLine:
                return "invalid columns line";
            case ErrorCode::InvalidColumnName:
                return "invalid column name";
            case ErrorCode::DuplicateColumnName:
                return "duplicate column name";
            case ErrorCode::ColumnNotFound:
                return "column not found";
            case ErrorCode::SentenceMustStartWithMetaLine:
                return "sentence must start with a meta line";
            case ErrorCode::InvalidSentenceMetaLine:
                return "invalid sentence meta line";
            case ErrorCode::InvalidSentenceMetaKey:
                return "invalid sentence meta key";
            case ErrorCode::DuplicateSentenceMetaEntry:
                return "duplicate sentence meta entry";
            case ErrorCode::MissingSentenceId:
                return "missing sentence id";
            case ErrorCode::InvalidSentenceId:
                return "invalid sentence id";
            case ErrorCode::DuplicateSentenceId:
                return "duplicate sentence id";
            case ErrorCode::SentenceIdCannotBeRemoved:
                return "sentence id cannot be removed";
            case ErrorCode::SentenceNotFoundInTreebank:
                return "sentence not found in treebank";
            case ErrorCode::InvalidTokenId:
                return "invalid token id";
            case ErrorCode::TokenFieldCountMismatch:
                return "token field count mismatch";
            case ErrorCode::ImmutableTokenField:
                return "immutable token field";
            case ErrorCode::TokenEmptyNodeMismatch:
                return "token empty node mismatch";
            case ErrorCode::NodeNotFoundInToken:
                return "node not found in token";
            case ErrorCode::InvalidTokenHead:
                return "invalid token head";
            case ErrorCode::TokenNotFoundInSentence:
                return "token not found in sentence";
            case ErrorCode::InvalidNumberOfFields:
                return "invalid number of fields";
            case ErrorCode::InvalidWordRange:
                return "invalid word range";
            case ErrorCode::OverlappingWordRange:
                return "overlapping word range";
            case ErrorCode::WordNotFoundInSentence:
                return "word not found in sentence";
            case ErrorCode::WordMustAppearBeforeToken:
                return "word must appear before token";
            case ErrorCode::InvalidWordFieldValue:
                return "invalid word field value";
        }

        return "<unknown>";
    }

    inline constexpr const ErrorCategory& get_error_category() noexcept {
        static ErrorCategory instance;
        return instance;
    }

    inline constexpr std::error_code make_error_code(ErrorCode ec) noexcept {
        return std::error_code{static_cast<int>(ec), get_error_category()};
    }
} // namespace tg::conllu

#endif // TG_CONLLU_IMPL_ERROR_IPP
