---
name: docmd-docs
description: Maintain the docmd documentation site in docs-site.
---

# docmd Docs

Use this skill when editing the `docs-site` documentation site.

## Rules

- Keep authoring content in Markdown only.
- Do not use MDX, JSX, or raw HTML in Markdown files.
- Prefer docmd containers: `callout`, `tabs`, `steps`, `collapsible`, `grids`, `grid`, and `card`.
- Do not use `::: button`.
- Keep every Markdown page listed in `docmd.config.json` navigation.
- Run `npm run check` from the repository root before committing documentation changes.
