<div align="center">

# CoNLL-U

**A modern, header-only C++23 library for reading, editing, and writing [Universal Dependencies](https://universaldependencies.org) treebanks.**

[![Version](https://img.shields.io/badge/version-0.1.0-orange)](https://github.com/tugrulgungor/CoNLL-U/releases)
[![Tests](https://img.shields.io/github/actions/workflow/status/tugrulgungor/CoNLL-U/tests.yml?branch=v0-dev&label=tests)](https://github.com/tugrulgungor/CoNLL-U/actions/workflows/tests.yml)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-00599C?logo=cplusplus&logoColor=white)](https://en.cppreference.com/w/cpp/23)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue)](LICENSE.MIT)

[**Live demo**](https://conll-u.tugrulgungor.me) · [**API reference**](https://conll-u.tugrulgungor.me/api) · [**Format spec**](https://universaldependencies.org/format.html)

</div>

---

## Usage

```cpp
#include <conllu.hpp>

namespace conllu = tg::conllu;
```

Nothing throws: every call that can fail returns a `std::error_code` (empty on success) or a `std::expected`. The examples work with this sentence:

```text
# sent_id = s1
# text = Don't go.
1-2	Don't	_	_	_	_	_	_	_	_
1	Do	do	AUX	_	_	3	aux	_	_
2	n't	not	PART	_	_	3	advmod	_	_
3	go	go	VERB	_	_	0	root	_	_
4	.	.	PUNCT	_	_	3	punct	_	_
```

### Parsing

```cpp
conllu::Parser parser;

auto from_file = parser.parse_file("in.conllu");
auto from_memory = parser.parse_memory(text);  // any std::string_view
auto from_stream = parser.parse(std::ifstream("in.conllu", std::ios::binary));

if (!from_file) {
    std::println(stderr, "{}", from_file.error().message());
    return 1;
}
std::unique_ptr<conllu::Treebank> treebank = std::move(*from_file);
```

Files with a `# global.columns` header ([CoNLL-U Plus](https://universaldependencies.org/ext-format.html)) are read with their own columns; otherwise the 10 standard ones are used.

### Reading

```cpp
for (const conllu::Sentence* sentence : treebank->sentences()) {
    std::println("{}: {}", sentence->get_sent_id(), sentence->get_text());

    for (const conllu::Word* word : sentence->words())  // multiword tokens
        std::println("  {}-{} {}", word->first_id(), word->last_id(), word->get_form().value_or("_"));

    for (const conllu::Token* token : sentence->tokens()) {
        const auto form = token->get_field_value("FORM").value_or("_");  // "_" in the file is std::nullopt
        const auto deprel = token->get_field_value("DEPREL").value_or("_");
        const auto head = token->get_head();  // std::nullopt for the root

        std::println("  {} {:<4} {:<6} → {}", token->get_id(), form, deprel,
                     head ? (*head)->get_field_value("FORM").value_or("_") : "ROOT");
    }
}
```

```text
s1: Don't go.
  1-2 Don't
  1 Do   aux    → go
  2 n't  advmod → go
  3 go   root   → ROOT
  4 .    punct  → go
```

Empty nodes are reached through `token->empty_nodes()` (`N.1`, `N.2`, …) and `sentence->empty_nodes()` (`0.1`, …). For direct lookups there are `get_sentence_by_id()`, `get_token_by_id()`, and `get_metadata()`.

### Documents and paragraphs

<p align="center"><img src="assets/treebank-structure.svg" alt="A CoNLL-U file next to the treebank, documents, paragraphs, and sentences it is parsed into" width="880"></p>

```cpp
for (const conllu::Document* document : treebank->documents()) {
    std::println("{}: {} sentences", document->get_id().value_or("(no id)"), document->num_sentences());

    for (const conllu::Paragraph* paragraph : document->paragraphs())
        std::println("  paragraph: {} sentences", paragraph->num_sentences());
}
```

```text
GUM_academic_discrimination: 54 sentences
  paragraph: 2 sentences
  paragraph: 1 sentences
  paragraph: 6 sentences
  …
```

Every sentence belongs to a paragraph, and every paragraph to a document, as marked by `# newdoc` and `# newpar`. Sentences before the first marker go into an implicit document or paragraph, which is written back without one.

```cpp
conllu::Document* d1 = treebank->add_document("d1").value();
conllu::Paragraph* par = d1->add_paragraph().value();
(void)par->add_sentence("d1-s1", "Hello.");
conllu::Sentence* bye = par->add_sentence("d1-s2", "Bye.").value();

conllu::Document* d2 = d1->split(bye).value();  // d1-s2 now starts a new document, d2
(void)d1->merge_next();                         // which joins d1 again
```

### Editing

```cpp
std::error_code edit(conllu::Sentence* sentence) {
    conllu::Token* go = sentence->get_token_by_id(3).value();

    if (auto err = go->set_field_value("FEATS", "VerbForm=Inf"))
        return err;
    if (auto err = sentence->set_metadata("text_es", "No vayas."))
        return err;

    // Removing a token renumbers everything after it: IDs, heads, multiword ranges.
    if (auto err = sentence->remove_token(4))
        return err;

    auto now = sentence->add_token();  // IDs are assigned, never set: this one is 4
    if (!now)
        return now.error();
    if (auto err = (*now)->set_field_value("FORM", "now"))
        return err;

    return (*now)->set_head(go->get_id());  // fails with InvalidTokenHead for a missing or self head
}
```

The same pattern covers `add_sentence()`, `add_word()`, `add_empty_node()`, `add_column()`, and their `remove_` counterparts. Every change is validated as it is made.

### Writing

```cpp
conllu::Writer writer;

if (auto err = writer.write_file(treebank.get(), "out.conllu"))
    std::println(stderr, "{}", err.message());

(void)writer.write(treebank.get(), std::move(std::cout));  // streams are taken by rvalue reference
```

```text
# global.columns = ID FORM LEMMA UPOS XPOS FEATS HEAD DEPREL DEPS MISC
# sent_id = s1
# text = Don't go.
# text_es = No vayas.
1-2	Don't	_	_	_	_	_	_	_	_
1	Do	do	AUX	_	_	3	aux	_	_
2	n't	not	PART	_	_	3	advmod	_	_
3	go	go	VERB	_	VerbForm=Inf	0	root	_	_
4	now	_	_	_	_	3	_	_	_
```

Output always declares its columns and keeps comment lines in their original order, so it reads back unchanged.

### Streaming

`StreamParser` builds the treebank from chunks of any size — a socket, a decompressor, a browser upload — and adds each sentence as soon as it is complete.

```cpp
conllu::StreamParser stream;

char buffer[4096];
while (source.read(buffer, sizeof buffer) || source.gcount() > 0) {
    if (auto err = stream.feed({buffer, static_cast<std::size_t>(source.gcount())})) {
        std::println(stderr, "{} (sentence starting at line {})", err.message(), stream.error_line());
        return 1;
    }
    // stream.treebank() already holds every sentence completed so far
}

auto streamed = stream.finish();  // std::expected<std::unique_ptr<Treebank>, std::error_code>
```

### Errors

```cpp
auto result = conllu::Parser{}.parse_memory("# sent_id = s1\n1\tgo\n");

if (!result && result.error() == conllu::ErrorCode::TokenFieldCountMismatch)
    std::println("{}", result.error().message());  // token field count mismatch
```
