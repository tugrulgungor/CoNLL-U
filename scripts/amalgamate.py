#  CoNLL-U - https://github.com/tugrulgungor/CoNLL-U
#
#  Copyright (c) 2026. All rights reserved.
#  Tuğrul Güngör - https://conll-u.tugrulgungor.me
#
#  Distributed under the MIT License.
#  https://opensource.org/license/mit/

import os

# Local `#include "conllu/..."` lines are stripped, so both lists must follow dependency order.
# Class definitions: a class must come after every class it uses by value or as a template argument.
HEADERS: list[str] = [
    "conllu/internal/string",
    "conllu/internal/string_hash",
    "conllu/internal/validate",
    "conllu/error",
    "conllu/token",
    "conllu/word",
    "conllu/sentence",
    "conllu/treebank",
    "conllu/stream_parser",
    "conllu/parser",
    "conllu/writer"
]

# Inline definitions: functions with a deduced (`auto`) return type must be defined before their first use
# (e.g. token.ipp calls Sentence::words()). Mirrors the include order at the bottom of treebank.hpp.
IMPLS: list[str] = [
    "conllu/internal/string",
    "conllu/internal/string_hash",
    "conllu/internal/validate",
    "conllu/error",
    "conllu/sentence",
    "conllu/token",
    "conllu/treebank",
    "conllu/word",
    "conllu/stream_parser",
    "conllu/parser",
    "conllu/writer"
]


def expand_sources() -> list[str]:
    sources: list[str] = []

    for source in HEADERS:
        sources.append(f'../include/{source}.hpp')

    for source in IMPLS:
        parts = source.split('/')
        parts.insert(len(parts) - 1, 'impl')
        sources.append(f'../include/{'/'.join(parts)}.ipp')

    return sources


def get_copyright_block() -> list[str]:
    lines: list[str] = []

    with open('../include/conllu.hpp', 'r', encoding='UTF-8') as file:

        for line in file:
            if not line.strip():
                break

            lines.append(line)

        lines.append('\n')

    return lines


def generate_single_header() -> None:
    sources = expand_sources()

    os.makedirs('../single_header', exist_ok=True)

    copyright_lines = get_copyright_block()
    include_lines: list[str] = []
    code_lines: list[str] = []

    for source in sources:
        with open(source, 'r', encoding='UTF-8') as ifile:
            line_idx: int = 0

            has_include_block: list[str] = []
            has_include_block_started: bool = False

            in_block_comment: bool = False

            for line in ifile:
                line_idx += 1
                if 10 > line_idx:
                    continue

                # Drop /* ... */ comment blocks. Tracked as state: a code line may itself start with '*'
                # (e.g. a wrapped dereference), so a leading '*' alone does not mark a comment line.
                if in_block_comment:
                    if '*/' in line:
                        in_block_comment = False
                    continue

                if line.lstrip().startswith('/*'):
                    in_block_comment = '*/' not in line
                    continue

                if line.startswith('#if __has_include') and '.ipp' not in source:
                    has_include_block_started = True
                    has_include_block.append(line)
                    continue

                if has_include_block_started:
                    has_include_block.append(line)

                    if line.startswith('#endif'):
                        include_lines.append(''.join(has_include_block))
                        has_include_block = []

                        has_include_block_started = False

                    continue

                if line.startswith('#include "'):
                    continue

                if ' TG_CONLLU_' in line:
                    continue

                if line.startswith('#include <'):
                    if line not in include_lines:
                        include_lines.append(line)
                else:
                    code_lines.append(line)

    include_lines.sort()

    code_lines = [x for i, x in enumerate(code_lines) if x != "\n" or i == 0 or code_lines[i - 1] != "\n"]

    with open('../single_header/conllu.hpp', 'w', encoding='UTF-8') as ofile:
        ofile.writelines(copyright_lines)

        ofile.writelines([
            '#ifndef TG_CONLLU_SINGLE_HEADER_HPP\n',
            '#define TG_CONLLU_SINGLE_HEADER_HPP 1\n',
            '\n'
        ])

        ofile.writelines(include_lines)
        ofile.writelines(code_lines)

        ofile.writelines([
            '#endif // TG_CONLLU_SINGLE_HEADER_HPP\n'
        ])


if '__main__' == __name__:
    generate_single_header()
