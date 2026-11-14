/*
 * CoNLL-U - https://github.com/tugrulgungor/CoNLL-U
 *
 * Copyright (c) 2026. All rights reserved.
 * Tuğrul Güngör - https://conll-u.tugrulgungor.me
 *
 * Distributed under the MIT License.
 * https://opensource.org/license/mit/
 */

'use strict';

importScripts('conllu.js');

const CHUNK_SIZE = 1 << 20;

let session = null;

const ready = createConlluDemoModule()
  .then(function (module) {
    session = new module.Session();
    postMessage({ type: 'ready' });
  })
  .catch(function (err) {
    postMessage({ type: 'fatal', error: String((err && err.message) || err) });
  });

function progress(loaded, total) {
  postMessage({ type: 'progress', loaded: loaded, total: total, sentences: session.parsed_sentences() });
}

// Feeds a stream of bytes to the parser; returns an error object, or null.
async function feedStream(reader, total) {
  const decoder = new TextDecoder();
  let loaded = 0;
  let lastReport = 0;

  for (;;) {
    const { done, value } = await reader.read();
    const text = done ? decoder.decode() : decoder.decode(value, { stream: true });

    if (text) {
      const err = session.feed(text);
      if (err) return JSON.parse(err);
    }
    if (done) return null;

    loaded += value.byteLength;
    const now = performance.now();
    if (now - lastReport > 50) {
      lastReport = now;
      progress(loaded, total);
    }
  }
}

function blobReader(blob) {
  let offset = 0;
  return {
    read: async function () {
      if (offset >= blob.size) return { done: true };
      const value = new Uint8Array(await blob.slice(offset, offset + CHUNK_SIZE).arrayBuffer());
      offset += value.byteLength;
      return { done: false, value: value };
    }
  };
}

async function parse(message) {
  session.reset();
  const started = performance.now();

  let reader;
  let total = 0;
  if (message.url) {
    const response = await fetch(message.url);
    if (!response.ok) throw new Error('Could not fetch "' + message.url + '" (HTTP ' + response.status + ')');
    total = Number(response.headers.get('Content-Length')) || message.size || 0;
    reader = response.body.getReader();
  } else {
    const blob = message.blob || new Blob([message.text || '']);
    total = blob.size;
    reader = blobReader(blob);
  }

  const err = await feedStream(reader, total);
  if (err) return err;

  const summary = JSON.parse(session.finish());
  summary.parse_ms = performance.now() - started;
  return summary;
}

const handlers = {
  parse: parse,
  sentence: function (m) { return JSON.parse(session.sentence(m.index)); },
  set: function (m) { return JSON.parse(session.set_value(m.index, m.row, m.column, m.value)); },
  add: function (m) { return JSON.parse(session.add_token(m.index)); },
  remove: function (m) { return JSON.parse(session.remove_token(m.index, m.token, !!m.detach)); },
  validate: function () { return JSON.parse(session.validate()); },
  write: function () { return { ok: true, text: session.write() }; }
};

onmessage = async function (event) {
  const message = event.data;
  try {
    await ready;
    if (!session) throw new Error('The parser failed to start.');
    const handler = handlers[message.type];
    if (!handler) throw new Error('Unknown request "' + message.type + '"');
    postMessage({ id: message.id, result: await handler(message) });
  } catch (err) {
    postMessage({ id: message.id, result: { ok: false, error: String((err && err.message) || err) } });
  }
};
