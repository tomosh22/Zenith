# Vendored JS

Three files, fetched byte-for-byte from `cdn.jsdelivr.net`, licence headers
kept intact. `report.py` inlines all three into the generated `index.html`
(in the order below) so the report is a single self-contained, fully offline
file — no `<script src=...>` ever points outside it.

| File | Package | Version | Licence | Source URL |
|---|---|---|---|---|
| `cytoscape.min.js` | [cytoscape](https://www.npmjs.com/package/cytoscape) | 3.30.4 | MIT | https://cdn.jsdelivr.net/npm/cytoscape@3.30.4/dist/cytoscape.min.js |
| `dagre.min.js` | [dagre](https://www.npmjs.com/package/dagre) | 0.8.5 | MIT | https://cdn.jsdelivr.net/npm/dagre@0.8.5/dist/dagre.min.js |
| `cytoscape-dagre.js` | [cytoscape-dagre](https://www.npmjs.com/package/cytoscape-dagre) | 2.5.0 | MIT | https://cdn.jsdelivr.net/npm/cytoscape-dagre@2.5.0/cytoscape-dagre.js |

## SHA-256

```
1bb5340e549511e111b31e5684872c949ad33d40ea5dba0ad8e7d90c62c7b3b9  cytoscape.min.js
62eb9787ccfdbdf4148d4d99d31dbf9ee4770eafee81e637d759b52aac22cd51  dagre.min.js
bf70fe402991dcbff33e05a7e4a5271c78020bb75e85d1c80ab7538e4157112e  cytoscape-dagre.js
```

Recompute with `sha256sum Tools/libgraph/vendor/*.js` (or
`Get-FileHash -Algorithm SHA256`).

## Load order

`report.py` inlines these in exactly this order, matched by the app code
(`template.html`):

1. `cytoscape.min.js`
2. `dagre.min.js`
3. `cytoscape-dagre.js`
4. app code, which registers the layout with
   `cytoscape.use(cytoscapeDagre)` only if `dagre` is not already a
   registered layout, then verifies it registered — showing a visible error
   banner instead of a blank canvas if not.

## Provenance

Fetched with `curl -fsSL -o <name> <source URL>` on 2026-09-23, approved by
the user as exactly these three files and nothing else. Nothing in this
directory is hand-edited; a version bump means re-fetching and updating this
file's table and hashes together.
