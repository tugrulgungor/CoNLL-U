/*
 * CoNLL-U - https://github.com/tugrulgungor/CoNLL-U
 *
 * Copyright (c) 2026. All rights reserved.
 * Tuğrul Güngör - https://conll-u.tugrulgungor.me
 *
 * Distributed under the MIT License.
 * https://opensource.org/license/mit/
 */

#ifndef TG_CONLLU_IMPL_PARSER_IPP
#define TG_CONLLU_IMPL_PARSER_IPP 1

#include "conllu/parser.hpp"

#include <fstream>
#include <vector>

namespace tg::conllu {
    // Constructors
    inline constexpr Parser::Parser(const ParserOptions options) noexcept : options_(options) {
    }

    // Parsing
    inline std::expected<std::unique_ptr<Treebank>, std::error_code> Parser::parse(std::istream&& stream) noexcept {
        StreamParser parser{options_};

        try {
            std::vector<char> buffer(std::size_t{64} * std::size_t{1024});

            while (stream) {
                stream.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));

                const auto count = static_cast<std::size_t>(stream.gcount());
                if (std::size_t{0} == count)
                    break;

                if (const auto err = parser.feed(std::string_view{buffer.data(), count}))
                    return std::unexpected(err);
            }

            if (stream.bad())
                return std::unexpected(make_error_code(ErrorCode::StreamError));
        }
        catch (const std::bad_alloc&) {
            return std::unexpected(make_error_code(ErrorCode::InsufficientMemory));
        }
        catch (...) {
            return std::unexpected(make_error_code(ErrorCode::StreamError));
        }

        return parser.finish();
    }

    inline std::expected<std::unique_ptr<Treebank>, std::error_code> Parser::parse_file(
    const std::filesystem::path& path) noexcept {
        try {
            std::ifstream file(path, std::ios::binary);
            if (!file.is_open())
                return std::unexpected(make_error_code(ErrorCode::StreamError));

            return parse(std::move(file));
        }
        catch (...) {
            return std::unexpected(make_error_code(ErrorCode::StreamError));
        }
    }

    inline std::expected<std::unique_ptr<Treebank>, std::error_code> Parser::parse_memory(
    const std::string_view data) noexcept {
        StreamParser parser{options_};

        if (const auto err = parser.feed(data))
            return std::unexpected(err);

        return parser.finish();
    }

    template <typename DataType>
        requires(sizeof(DataType) == sizeof(std::uint8_t))
    inline std::expected<std::unique_ptr<Treebank>, std::error_code> Parser::parse_memory(
    const DataType* data, const std::size_t size) noexcept {
        const std::string_view sv{reinterpret_cast<const char*>(data), size};
        return parse_memory(sv);
    }
} // namespace tg::conllu

#endif // TG_CONLLU_IMPL_PARSER_IPP
