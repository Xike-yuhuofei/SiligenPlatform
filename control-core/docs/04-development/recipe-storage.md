# Recipe Storage and Backup

## Storage layout
Base directory: `data/recipes/`

- Recipes: `data/recipes/recipes/<recipe_id>.json`
- Versions: `data/recipes/versions/<recipe_id>/<version_id>.json`
- Templates: `data/recipes/templates/<template_id>.json`
- Audit log: `data/recipes/audit/<recipe_id>.json`
- Schemas: `data/recipes/schemas/*.json` (fallback: `src/infrastructure/resources/config/files/recipes/schemas/`)

## File formats
- JSON with `schemaVersion` set to `1.0` for recipe/version/audit files.
- Recipe file structure:
  - `{ "schemaVersion": "1.0", "recipe": { ... } }`
- Version file structure:
  - `{ "schemaVersion": "1.0", "version": { ... } }`
- Template files store a single template object (no wrapper).
- Audit files store `{ "schemaVersion": "1.0", "records": [ ... ] }`.
- Schema files store a single schema object (no array wrapper).

## Concurrency and integrity
- Writes are file-based and not transactional across multiple files.
- Avoid concurrent recipe writes during bulk operations.
- Prefer controlled import/export through the current TCP/HMI-backed capability chain.

## Backup and restore
1. Stop any HMI/TCP-backed recipe operations that may write to `data/recipes/`.
2. Copy the entire `data/recipes/` directory to a safe location.
3. Restore by copying the directory back to the same location.

## Transfer between machines
- Prefer the current bundle export/import flow exposed through HMI or TCP-backed recipe capabilities.
- When the calling workflow exposes conflict policies, choose an explicit strategy such as `rename` / `skip` / `replace`.
