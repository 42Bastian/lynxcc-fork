# lynxcc instructions

## Documentation stays in sync with code

Any code change MUST be reflected in the documentation in the same pass. When you
add, remove, rename, or change the behaviour of anything user-visible (constants,
macros, functions, headers, build targets, samples), update every place that
documents it before considering the task done:

- `include/*.h` and `asminc/*.inc` doc comments
- `doc/*.html` (TGI, fonts, function reference, samples, etc.)
- `LYNX_TGI_DESIGN.md` and any other `*_DESIGN.md` source-of-truth docs
- relevant `README`/comments in `samples/`

Removing or changing a symbol means grepping the whole tree for it and fixing
docs, not just the code. If a symbol is intentionally absent, document *why* so
the omission doesn't later read as an oversight.
