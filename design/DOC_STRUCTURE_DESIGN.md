<!--
SPDX-License-Identifier: CC-BY-4.0
Lynx Game Development SDK documentation, (c) 2026 the lynxcc authors,
licensed under Creative Commons Attribution 4.0 International.
-->

# Documentation page structure — shared chrome via web components

## Problem

Every page in `doc/*.html` carried its own verbatim copy of the topbar
navigation (~3.5 KB of identical markup) and the footer line. A change to the
top bar — adding a link, renaming a label, reordering a dropdown — meant editing
all ~29 HTML files by hand, and the copies drifted (the graphics-fonts page, for
example, had a slightly different footer).

## Constraint

The doc set ships as plain static HTML with **no generation step** (see
`doc/Makefile`) and is meant to open directly from disk (`file://`). That rules
out a server-side include and also rules out a client-side `fetch()` of a shared
fragment, because browsers block `fetch()` of local files under the
same-origin/CORS policy. Any shared-chrome mechanism therefore has to work with
the markup already present in a loaded asset.

## Design

The topbar and footer are defined **once** in `doc/doc.js` and injected on every
page through two light-DOM custom elements:

- `<site-nav></site-nav>` — renders the top bar from the `TOPBAR_HTML` constant
  (the single source of truth for the nav). It derives the active page from the
  current filename (`location.pathname`) and adds the `active` class to the
  matching `.dropdown-menu` link and its parent `.dropdown`; pages therefore do
  **not** declare which entry is current. `index.html` matches nothing and shows
  no active entry, as before.
- `<site-foot></site-foot>` — renders the shared footer line. An optional
  `note="…"` attribute appends one extra sentence (used only by
  `lynx_gfx_fonts.html`).

Both elements render into the **light DOM** (no shadow root), so the existing
`doc.css` rules for `.topbar`, `.nav`, `.dropdown`, `.theme-toggle` and
`.site-foot` apply unchanged. Because `doc.js` is loaded at the end of `<body>`,
the markup is already parsed when the elements upgrade; the injected markup is
identical to what the pages previously hard-coded, so rendering is unchanged.

The theme-toggle and dropdown wiring lives in `wireChrome()`, called after the
top bar is injected. All four dropdowns get click/touch/keyboard toggling (the
previous inline script wired only the first); CSS still handles hover-open.

## Page contract

Each page contains, in place of the old markup:

```html
<body>
<site-nav></site-nav>
<main …>…page content…</main>
<site-foot></site-foot>
<script src="doc.js"></script>
</body>
```

The `<title>` stays per-page in `<head>` (it cannot be a component) and follows
the convention **`lynxcc - <descriptor>`**.

## Editing the nav or footer

Change the nav links/labels or the footer text in `doc/doc.js` only
(`TOPBAR_HTML` / the `SiteFoot` `BASE` string). Do not reintroduce per-page
topbar markup. This supersedes the old workflow where a nav change (e.g. adding
the Licenses entry) had to be swept across every `doc/*.html`.

## Trade-off

The top bar renders after `doc.js` runs, so there is a brief layout settle on
first paint. This is acceptable for a local, JS-required doc set (the theme
toggle already depends on JS) and avoids both a build step and the `file://`
`fetch()` limitation.

## Related

The top bar also hosts the quick-jump search palette (magnifier button /
`Cmd-K` / `/`), whose behaviour and generated section index live in the same
`doc.js`. See `design/DOC_SEARCH_DESIGN.md`.
