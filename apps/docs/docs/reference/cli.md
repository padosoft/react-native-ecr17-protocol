---
title: CLI
description: Repository and documentation commands.
---

# CLI

The repository root exposes documentation commands that delegate to `apps/docs`.

## Documentation

```bash
npm run dev
npm run check
npm run build
```

## docmd site commands

```bash
cd apps/docs
npm run dev
npm run check
npm run build
```

`npm run build` runs docmd static generation, builds the semantic search index, and copies `.docmd-search` into `_site/.docmd-search`.

## Package checks

```bash
cd packages/react-native-ecr17
bunx tsc --noEmit -p tsconfig.ci.json
```

## Native build

Native app builds are required after changes under Android, iOS, Nitro config, or integrated C++ client/adapter code.
