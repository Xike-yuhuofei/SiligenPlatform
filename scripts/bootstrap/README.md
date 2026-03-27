# third_party bootstrap

## Files

- `third-party-manifest.json`: bundle metadata, archive names, SHA256 and validation paths.
- `bootstrap-third-party.ps1`: hydrate `third_party/` from local bundles or a remote artifact base URI.
- `export-third-party-bundles.ps1`: package the current local `third_party/` into uploadable zip bundles and refresh the manifest hashes.

## Recommended flow

1. Run `export-third-party-bundles.ps1` on a machine that already has a valid local `third_party/`.
2. Upload the generated zip files to a release asset bucket or another artifact store.
3. Set `SILIGEN_THIRD_PARTY_BASE_URI` in CI and on fresh clones.
4. Use `build.ps1`, `test.ps1`, `ci.ps1` or run `bootstrap-third-party.ps1` directly.

## Environment variables

- `SILIGEN_THIRD_PARTY_ARTIFACT_ROOT`: local directory containing the exported zip bundles.
- `SILIGEN_THIRD_PARTY_BASE_URI`: remote base URI used to download bundle archives.
