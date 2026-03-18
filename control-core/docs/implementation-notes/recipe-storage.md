# Recipe Storage and Backup Notes

## Storage Layout
- Base directory: `data/recipes/`
- Recipes: `data/recipes/recipes/<recipe_id>.json`
- Versions: `data/recipes/versions/<recipe_id>/<version_id>.json`
- Templates: `data/recipes/templates/<template_id>.json`
- Audit log: `data/recipes/audit/<recipe_id>.json`
- Schemas: `data/recipes/schemas/*.json` (one file per schema)

## File Formats
- JSON with `schemaVersion` set to `1.0`.
- Recipe files contain a single `recipe` object.
- Version files contain a single `version` object.
- Template files store a single template object (no wrapper).
- Audit records are stored in `{ "schemaVersion": "1.0", "records": [ ... ] }`.
- Schema files use a single schema object (not an array).
- Legacy `templates.json` is read for backward compatibility but new templates are stored per-file.

## Backup
1. Ensure no concurrent HMI/TCP-backed recipe writes are in progress.
2. Copy the entire `data/recipes/` directory to a safe location.
3. Restore by copying the directory back to the same location.

## Transfer Between Machines
- Prefer the current bundle export/import flow exposed through HMI or TCP-backed recipe capabilities.
- When the calling workflow exposes conflict policies, choose an explicit strategy such as `rename` / `skip` / `replace`.

## Operational Notes
- Writes are file-based and not transactional across multiple files.
- Avoid manual edits to JSON unless you also validate against the parameter schema.
