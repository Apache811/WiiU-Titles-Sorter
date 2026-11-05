# Repository and release maintenance

Titles Sorter v1.0.0 has already been published. This document is therefore a reference for repository hygiene and future source releases, not a guide to publishing the initial v1.0.0 release.

## Repository hygiene

Before committing changes, inspect the staged files:

```sh
git status --short
git diff --cached --stat
git diff --cached
```

Do not stage personal Home Menu save dumps, local build outputs, generated binaries unless they are intended release assets, logs containing identifiers, credentials or private paths.

The local Core test fixture `tests/fixtures/sample_menu_save.dat` is intentionally excluded by `.gitignore`. It remains a local test input and is not part of the published repository. Check the ignore rule with:

```sh
git check-ignore -v tests/fixtures/sample_menu_save.dat
```

No `LICENSE` file is included, by maintainer decision. Do not describe the project as open source or imply that reuse, modification or redistribution rights are granted.

## Future source changes

For ordinary maintenance:

```sh
git add <reviewed-files>
git diff --cached
git commit -m "Describe the change"
git push
```

Update the documentation when shipped behaviour changes. In particular, keep `README.md`, `FEATURES_AND_STATUS.md`, `CHANGELOG.md`, `UI_CONTRACT.md` and `TEST_RESULTS.md` consistent with the implementation and the evidence for the changed feature.

## Future releases

For a subsequent release:

1. Validate the changed functionality with the relevant host tests and Wii U checks.
2. Update the version and changelog.
3. Review repository contents and ensure the local fixture and private data remain excluded.
4. Create a new annotated version tag. Do not move or reuse an existing release tag.

Example:

```sh
git tag -a v1.0.1 -m "Titles Sorter v1.0.1"
git push origin v1.0.1
```

Create and publish the corresponding GitHub Release from that tag, and attach only the intended release artifacts.

## Security and privacy

- Never commit authentication tokens, passwords, private keys or personal console dumps.
- Avoid publishing logs containing console/account identifiers or personal absolute paths.
- Keep `build/` and local generated outputs out of source commits unless a particular artifact is deliberately part of a release.
