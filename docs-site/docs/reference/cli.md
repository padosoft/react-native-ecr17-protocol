---
title: CLI
description: Repository and documentation commands.
---

# CLI

The repository root exposes documentation commands that delegate to `docs-site`.

## Documentation

```bash
npm run dev
npm run check
npm run build
```

## docmd site commands

```bash
cd docs-site
npm run dev
npm run check
npm run build
```

`npm run build` runs docmd static generation, builds the semantic search index, and copies `.docmd-search` into `_site/.docmd-search`.

## Package checks

```bash
cd package
bunx tsc --noEmit -p tsconfig.ci.json
```

## Native build

Native app builds are required after changes under Android, iOS, Nitro config, or integrated C++ client/adapter code.
