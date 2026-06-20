# Documentation SVG style

Reference for the inline SVG diagrams in `doc/*.html` (memory maps in
`lynx.html`, the cart/dirent layouts, the packed-vs-literal figure in
`sp65.html`, etc.). Follow this so new diagrams match the existing ones without
re-deriving the conventions each time.

## Container

Wrap every diagram in a centred `<figure>` with an optional `<figcaption>`:

```html
<figure style="margin:1.2em 0;text-align:center">
<svg viewBox="0 0 720 H" style="width:100%;height:auto;max-width:700px"
     role="img" aria-label="One-line description of the diagram">
  ...
</svg>
<figcaption style="color:var(--ink-faint);font-size:.85rem;margin-top:.4em">
  Caption sentence.</figcaption>
</figure>
```

- `viewBox` width is always **720**; height is whatever the content needs.
- `max-width:700px`, `width:100%;height:auto` so it scales and never overflows.
  Use a smaller `max-width` (e.g. 560px) for tall/narrow diagrams.
- Always set `role="img"` and a descriptive `aria-label`.

## Colours — theme variables only, never literals

The docs have light/dark themes (`doc.css`). Every fill/stroke MUST use a CSS
variable so both themes work. Available tokens:

| Variable        | Use                                              |
|-----------------|--------------------------------------------------|
| `--ink`         | primary text / strong labels                     |
| `--ink-soft`    | secondary text, field values                     |
| `--ink-faint`   | captions, byte-range hints, de-emphasised text   |
| `--panel`       | neutral cell background                          |
| `--panel-2`     | secondary/alternate cell background              |
| `--border`      | cell strokes                                     |
| `--accent`      | the highlight colour (orange-red light / warm dark) |
| `--accent-2`    | the secondary highlight (teal light / cyan dark) |

Tinting: layer `fill="var(--accent)" fill-opacity="0.14"` (light tint) up to
`0.6` to get several distinct shades from one accent without hard-coding colour.
Stroke the same shape with the solid `var(--accent)` for an outlined-panel look.
Use `var(--accent-2)` when a second, clearly different hue is needed (e.g. one
distinct element among accent-tinted ones).

## Typography

- Monospace for numbers, byte values, bit-fields, captions:
  `font-family="ui-monospace,Menlo,Consolas,monospace"`.
- Sans for field names / prose labels:
  `font-family="-apple-system,Segoe UI,Roboto,sans-serif"`.
- Sizes run ~9–12px: 11–12 for labels, 9–10 for hints/sub-labels.
- Centre cell labels with `text-anchor="middle"`; left-align running notes at the
  diagram's left margin (x≈24).

## Layout conventions

- Group related `<text>` with a parent `<g>` that carries the shared
  `font-family`/`font-size`/`fill`/`text-anchor`, then only override per-element
  attributes that differ. Keeps the markup short and consistent.
- Rectangular byte/pixel cells: draw `<rect>`s on a regular x-grid, captions
  above, field names centred inside, sub-hints below.
- Use `stroke-dasharray="3 3"` for optional / conditional elements (e.g. a pad
  byte that only sometimes appears).
- Entities: use `&times;`, `&middot;`, `&ndash;`, `&mdash;`, `&nbsp;` rather than
  raw characters.

## Keep it in sync

These diagrams document real layouts (memory maps, SCB/sprite encodings, cart
format). When the thing being drawn changes, update the SVG in the same pass —
same rule as the rest of the docs (see `CLAUDE.md`).
