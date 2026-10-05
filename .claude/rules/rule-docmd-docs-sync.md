# docmd docs sync

When changing public APIs, configuration fields, protocol behavior, transport behavior, or safety rules, update the matching page under `apps/docs/docs` in the same branch.

Before a documentation change is complete:

- Run `npm run check`.
- Run `npm run build`.
- Confirm the changed page is present in `apps/docs/docmd.config.json` navigation.
- Keep `.docmd-search/config.json` committed and generated search batches ignored.
