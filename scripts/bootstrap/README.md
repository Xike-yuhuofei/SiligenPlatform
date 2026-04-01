# third_party bootstrap

## Files

- `third-party-manifest.json`: bundle metadata, archive names, SHA256 and validation paths.
- `bundles/`: repo-tracked zip bundles used as the default bootstrap source on fresh clones.
- `bootstrap-third-party.ps1`: hydrate `third_party/` from repo-tracked bundles, a local artifact root, or a remote artifact base URI.
- `export-third-party-bundles.ps1`: package the current local `third_party/` into repo-tracked zip bundles and refresh the manifest hashes.

## Recommended flow

1. Run `export-third-party-bundles.ps1` on a machine that already has a valid local `third_party/`.
2. Commit `scripts/bootstrap/bundles/` and the updated manifest so fresh clones have an in-repo default source.
3. Optionally mirror the same zip files to a release asset bucket or another artifact store.
4. Use `build.ps1`, `test.ps1`, `ci.ps1` or run `bootstrap-third-party.ps1` directly.

## Environment variables

- `SILIGEN_THIRD_PARTY_ARTIFACT_ROOT`: optional local directory containing bundle archives; overrides the default repo-tracked `scripts/bootstrap/bundles/`.
- `SILIGEN_THIRD_PARTY_BASE_URI`: optional remote base URI used when no local bundle source is available.
