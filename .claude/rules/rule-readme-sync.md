# README sync

The repository home page on GitHub only renders a root `README.md`. The polished,
npm-published README lives in `package/README.md`, and the root `README.md` is a
**verbatim mirror** of it so the same page shows on the GitHub home page.

Because `package/README.md` uses absolute URLs for every badge, image, and link
(it ships to npm), the two files are byte-identical — the mirror is a plain copy,
no path rewriting.

When you change `package/README.md`:

- Regenerate the root copy with `npm run sync:readme` (never hand-edit root `README.md`).
- Commit both files together.

CI (`ts-checks`) runs `npm run check:readme`, which fails if the root `README.md`
is out of sync, so a stale mirror never lands.
