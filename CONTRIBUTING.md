# Contributing to cyd-dashboard

Thanks for helping out! This project is an ESP32 touchscreen dashboard for the
Cheap Yellow Display (CYD). For an end-user overview see the [README](README.md);
for deep technical details (partition layout, provisioning, releases, airline
logos) see [DEVELOPER.md](DEVELOPER.md). To report a security issue, see
[SECURITY.md](SECURITY.md).

## How to contribute

- **Bug reports & feature ideas** — open an issue describing the problem or
  request. Include the device, firmware version (see **Settings → About**), and
  what you expected vs. what happened.
- **Code** — submit a pull request following the workflow below.

## Conventional commits (required)

PR titles must use [Conventional Commits](https://www.conventionalcommits.org/).
This is enforced by CI and is what `release-please` uses to compute the next
semver version, so please use the correct type:

| Type | When to use |
|---|---|
| `feat: ...` | a new feature or behavior change |
| `fix: ...` | a bug fix / correction |
| `chore:`, `docs:`, `refactor:`, `build:`, `ci:` | non-functional changes |
| `feat!:` / `fix!:`, or a `BREAKING CHANGE:` footer | a breaking change |

The subject should start with a capital letter, e.g. `feat: Add a new setting`.

## Development workflow

All changes are made through **pull requests** merged into `main`. Never commit
directly to `main`.

1. **Create a feature branch** off the latest `main`:
   ```bash
   git checkout main
   git pull
   git checkout -b feat/short-description
   ```
2. **Make your changes** and commit them on the branch, following the project's
   existing commit style (short imperative subjects).
3. **Push the branch** to GitHub:
   ```bash
   git push -u origin feat/short-description
   ```
4. **Open a pull request** against `main` (e.g. `gh pr create` or via the GitHub
   UI) with a conventional-commit title, a summary of the change, and how it was
   verified (compile/upload results, screenshots of the display, etc.).
5. **Review and address feedback**, keeping the branch up to date with `main` as
   needed (rebase or merge, then re-push).
6. **Merge the PR into `main`** once it's reviewed and the checks are green.
   Delete the branch locally and on the remote after merging:
   ```bash
   git branch -d feat/short-description
   git push origin --delete feat/short-description
   ```

Keep PRs small and focused on a single logical change so they're quick to
review. The primary branch is `main`; everything that lands there is intended
to be shippable.

## Building & verifying

CI builds every PR with the production FQBN. To build locally:

```bash
arduino-cli compile --fqbn esp32:esp32:jczn_2432s028r:PartitionScheme=custom cyd-dashboard
```

See [DEVELOPER.md](DEVELOPER.md) for hardware wiring, flashing/upload, partition
and logo provisioning, and the release process.
