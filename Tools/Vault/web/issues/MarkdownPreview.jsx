import React, { useMemo } from 'react';
import DOMPurify from 'dompurify';
import { marked } from 'marked';

import { extractMentions } from '../../shared/jira/utils/mentions.js';
import { extractReferences, parseReference } from '../../shared/jira/utils/references.js';

function text(value) {
  return value === undefined || value === null ? '' : String(value);
}

function issueKeyFromHref(href) {
  const reference = parseReference(href);
  if (reference?.kind === 'issue') return `${reference.projectKey}-${reference.issueNumber}`;
  return null;
}

function referenceCard(reference) {
  const card = reference?.card ?? reference;
  if (!card || typeof card !== 'object') return null;
  const url = text(reference?.url ?? card.url);
  const title = text(card.title ?? card.name, url);
  const subtitle = text(card.subtitle ?? card.description);
  return { url, title, subtitle };
}

/**
 * Render a Markdown source string without changing the source held by the
 * editor.  Marked supplies the familiar Jira-style headings/lists/links and
 * DOMPurify keeps imported ticket text from becoming executable markup.
 * Known product links remain ordinary anchors, but the click handler opens a
 * Vault issue in the surrounding shell when the URL identifies one.
 */
export function MarkdownPreview({ body = '', references = [], onOpenIssue, className = '', label = 'Rendered description' }) {
  const source = text(body);
  const html = useMemo(() => {
    if (!source) return '';
    const rendered = marked.parse(source, { breaks: true, gfm: true });
    return DOMPurify.sanitize(rendered, {
      USE_PROFILES: { html: true },
      ADD_ATTR: ['target', 'rel'],
    });
  }, [source]);
  const mentions = useMemo(() => extractMentions(source), [source]);
  const parsedReferences = useMemo(() => extractReferences(source).map(referenceCard).filter(Boolean), [source]);
  const suppliedReferences = useMemo(() => (Array.isArray(references) ? references.map(referenceCard).filter(Boolean) : []), [references]);
  const cards = suppliedReferences.length ? suppliedReferences : parsedReferences;

  function openReference(event) {
    const anchor = event.target?.closest?.('a');
    if (!anchor) return;
    const key = anchor.dataset.issueKey ?? issueKeyFromHref(anchor.href);
    if (key && onOpenIssue) {
      event.preventDefault();
      onOpenIssue(key);
    }
  }

  return (
    <div className={`vault-issue-markdown-preview ${className}`.trim()} data-testid="issue-markdown-preview" aria-label={label}>
      {source ? <div className="vault-issue-markdown" onClick={openReference} dangerouslySetInnerHTML={{ __html: html }} /> : <p className="vault-issue-empty">No description.</p>}
      {mentions.length ? (
        <p className="vault-issue-mentions" aria-label="Mentions">Mentioned: {mentions.map((mention, index) => <React.Fragment key={mention}>{index ? ', ' : ''}<span>@{mention}</span></React.Fragment>)}</p>
      ) : null}
      {cards.length ? (
        <ul className="vault-issue-reference-list" aria-label="Referenced items">
          {cards.map((card, index) => <li key={`${card.url}-${index}`} className="vault-issue-reference-card"><a href={card.url} onClick={openReference}>{card.title}</a>{card.subtitle ? <small>{card.subtitle}</small> : null}</li>)}
        </ul>
      ) : null}
    </div>
  );
}

