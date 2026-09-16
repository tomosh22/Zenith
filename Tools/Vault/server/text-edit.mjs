import { isDeepStrictEqual } from 'node:util';
import { parseDocument } from 'yaml';

function sourceOf(input) {
  const bytes = Buffer.from(input);
  const text = bytes.toString('utf8');
  if (!Buffer.from(text).equals(bytes)) throw new Error('Document is not valid UTF-8');
  return { bytes, text };
}

function frontmatter(text) {
  const open = /^(?:\uFEFF)?---[ \t]*\r?\n/.exec(text);
  if (!open) throw new Error('Structured edits require YAML frontmatter');
  const start = open[0].length;
  const close = /^(?:---|\.\.\.)[ \t]*(?:\r?\n|$)/m.exec(text.slice(start));
  if (!close) throw new Error('Unclosed YAML frontmatter');
  const end = start + close.index;
  const yaml = text.slice(start, end);
  const document = parseDocument(yaml, { uniqueKeys: true, keepSourceTokens: true });
  if (document.errors.length) throw new Error(document.errors[0].message);
  const fields = document.toJS();
  if (!fields || typeof fields !== 'object' || Array.isArray(fields)) {
    throw new Error('Frontmatter must be a mapping');
  }
  return { start, end, yaml, document, fields, bodyStart: end + close[0].length };
}

function lineEnd(text, start) {
  const newline = text.indexOf('\n', start);
  return newline === -1 ? text.length : newline;
}

function blockCommentSuffix(yaml, start, comment) {
  if (comment === undefined) return '';
  const end = lineEnd(yaml, start);
  const line = yaml.slice(start, end).replace(/\r$/, '');
  const match = /[ \t]+#.*$/.exec(line);
  return match ? line.slice(match.index) : ` #${comment}`;
}

/** Pure text patch: untouched fields, comments, body, BOM and line endings survive. */
export function patchFields(input, updates) {
  const { bytes, text } = sourceOf(input);
  const fm = frontmatter(text);
  const newline = text.includes('\r\n') ? '\r\n' : '\n';
  const edits = [];
  const additions = [];
  for (const [key, value] of Object.entries(updates)) {
    if (!/^[A-Za-z][A-Za-z0-9_]*$/.test(key)) throw new Error(`Invalid field name: ${key}`);
    const encoded = JSON.stringify(value);
    if (encoded === undefined || !isDeepStrictEqual(JSON.parse(encoded), value)) {
      throw new Error(`Field ${key} must contain JSON-compatible data`);
    }
    if (isDeepStrictEqual(fm.fields[key], value)) continue;
    const pair = fm.document.contents.items.find(item => item.key?.value === key);
    if (!pair) {
      additions.push(`${key}: ${encoded}${newline}`);
      continue;
    }
    if (!pair.value?.range) throw new Error(`Cannot locate field ${key}`);
    const [start, end, nodeEnd = end] = pair.value.range;
    const isBlockScalar = pair.value.type === 'BLOCK_LITERAL' || pair.value.type === 'BLOCK_FOLDED';
    const replacementEnd = isBlockScalar ? nodeEnd : end;
    const isEmptyValue = start === end;
    // yaml's empty Scalar range starts after the mapping colon (and after any
    // spaces or anchor token). Add a separator when the source had none.
    const needsLeadingSpace = isEmptyValue
      && (start === 0 || !/[ \t]/.test(fm.yaml[start - 1]));
    const leadingSpace = needsLeadingSpace ? ' ' : '';
    let trailingText = '';
    if (isBlockScalar) {
      // A block scalar's range includes its indicator, inline comment, body,
      // and final line break. Keep the comment and line break outside the
      // replacement so later fields and the closing delimiter stay intact.
      trailingText = blockCommentSuffix(fm.yaml, start, pair.value.comment);
      if (fm.yaml[replacementEnd] !== '\n' && fm.yaml[replacementEnd] !== '\r') trailingText += newline;
    } else if (/\r?\n/.test(fm.yaml.slice(start, replacementEnd))
      && fm.yaml[replacementEnd] !== '\n' && fm.yaml[replacementEnd] !== '\r') {
      // Block collections include their final line break in the node range.
      // Put it back when the next token is a later field or the delimiter.
      trailingText = newline;
    } else if (isEmptyValue && pair.value.comment !== undefined) {
      // The empty range ends immediately before '# comment'; leave a YAML
      // separator after the replacement value.
      trailingText = ' ';
    }
    edits.push({
      start: fm.start + start,
      end: fm.start + replacementEnd,
      value: leadingSpace + encoded + trailingText,
    });
  }
  if (!edits.length && !additions.length) return bytes;
  if (additions.length) edits.push({ start: fm.end, end: fm.end, value: additions.join('') });
  let result = text;
  for (const edit of edits.sort((a, b) => b.start - a.start)) {
    result = result.slice(0, edit.start) + edit.value + result.slice(edit.end);
  }
  const verified = frontmatter(result);
  for (const [key, value] of Object.entries(updates)) {
    if (!isDeepStrictEqual(verified.fields[key], value)) throw new Error(`Field patch did not preserve ${key}`);
  }
  return Buffer.from(result);
}

/** Editor no-op saves are returned byte-for-byte, without parsing/rendering Markdown. */
export function replaceDocument(input, next) {
  const original = sourceOf(input);
  const replacement = sourceOf(next);
  return original.bytes.equals(replacement.bytes) ? original.bytes : replacement.bytes;
}

export function replaceBody(input, body) {
  const { text } = sourceOf(input);
  const fm = frontmatter(text);
  const replacement = sourceOf(body);
  return Buffer.concat([Buffer.from(text.slice(0, fm.bodyStart)), replacement.bytes]);
}
