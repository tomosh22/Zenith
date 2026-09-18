import React, { useEffect, useMemo, useRef } from 'react';
import { defaultKeymap, history, historyKeymap, indentWithTab } from '@codemirror/commands';
import { markdown } from '@codemirror/lang-markdown';
import { Compartment, EditorState } from '@codemirror/state';
import { EditorView, keymap } from '@codemirror/view';

/**
 * CodeMirror keeps the source line separator in EditorState. A native
 * textarea is allowed to canonicalise CRLF while it is assigned, which is
 * exactly the byte loss the Vault document editor is designed to avoid.
 */
export function RawMarkdownEditor({ value = '', onChange, disabled = false, label = 'Issue description', className = '', testId = 'issue-description-editor' }) {
  const host = useRef(null);
  const view = useRef(null);
  const editableCompartment = useRef(new Compartment());
  const callback = useRef(onChange);
  callback.current = onChange;
  const lineSeparator = useMemo(() => String(value).includes('\r\n') ? '\r\n' : String(value).includes('\r') ? '\r' : '\n', [value]);

  useEffect(() => {
    if (!host.current) return undefined;
    const state = EditorState.create({
      doc: String(value ?? ''),
      extensions: [
        EditorState.lineSeparator.of(lineSeparator),
        markdown(),
        history(),
        keymap.of([...defaultKeymap, ...historyKeymap, indentWithTab]),
        EditorView.lineWrapping,
        editableCompartment.current.of(EditorView.editable.of(!disabled)),
        EditorView.contentAttributes.of({ 'aria-label': label, role: 'textbox', 'data-testid': testId }),
        EditorView.updateListener.of(update => {
          if (update.docChanged) callback.current?.(update.state.sliceDoc());
        }),
      ],
    });
    view.current = new EditorView({ state, parent: host.current });
    return () => { view.current?.destroy(); view.current = null; };
    // Recreate only when the selected issue's source is replaced. Draft
    // updates are handled by the dispatch effect below.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  useEffect(() => {
    const current = view.current;
    if (!current) return;
    const source = current.state.doc.toString();
    const next = String(value ?? '');
    if (source !== next) current.dispatch({ changes: { from: 0, to: source.length, insert: next } });
    current.dispatch({ effects: editableCompartment.current.reconfigure(EditorView.editable.of(!disabled)) });
  }, [value, disabled]);

  return <div className={`vault-issue-control vault-issue-body-control ${className}`.trim()} ref={host} data-testid={`${testId}-host`} />;
}
