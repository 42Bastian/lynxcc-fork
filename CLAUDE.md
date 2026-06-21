# lynxcc instructions

## Documentation stays in sync with code

Any code change MUST be reflected in the documentation in the same pass. When you
add, remove, rename, or change the behaviour of anything user-visible (constants,
macros, functions, headers, build targets, examples), update every place that
documents it before considering the task done:

- `include/*.h` and `asminc/*.inc` doc comments
- `doc/*.html` (TGI, fonts, function reference, samples, etc.)
- `design/LYNX_TGI_DESIGN.md` and any other `design/*_DESIGN.md` source-of-truth docs
- relevant `README`/comments in `examples/`

All `*_DESIGN.md` design documents live in the `design/` directory. New design
docs go there too, and references to them from code/docs use the `design/` path.

When adding or editing an inline SVG diagram in `doc/*.html`, follow
`design/DOC_SVG_STYLE_DESIGN.md` (viewBox 720, theme CSS variables only,
mono/sans font conventions, `<figure>`/`<figcaption>` wrapper) so diagrams stay
consistent across the docs and work in both light and dark themes.

Removing or changing a symbol means grepping the whole tree for it and fixing
docs, not just the code. If a symbol is intentionally absent, document *why* so
the omission doesn't later read as an oversight.
