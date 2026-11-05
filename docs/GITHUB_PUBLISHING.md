# Publishing Titles Sorter to a new GitHub repository

These instructions create a new repository and push the local project to it. Choose repository visibility according to the maintainer’s release plan, and complete the privacy review in `RELEASE_CHECKLIST.md` before pushing.

## 1. Create the repository on GitHub

1. Sign in to GitHub and open <https://github.com/new>.
2. Set the repository name, for example `WiiU-Titles-Sorter` (or your preferred name).
3. Add a short description, such as: `Wii U homebrew for sorting and previewing the HOME Menu layout.`
4. Choose **Private** for now.
5. For this first push, leave **Add a README**, **Add .gitignore**, and **Choose a license** unchecked. The project already contains its own README and `.gitignore`; generating another initial commit on GitHub can make the first push less straightforward.
6. Click **Create repository**. Keep the page open; GitHub will show the repository URL.

## 2. Prepare the local project

Extract the reviewed project ZIP into a normal development folder, for example:

```text
D:\Projects\WiiU\TitlesSorter\
```

Open an MSYS2 terminal in the project root (the folder containing `Makefile` and `README.md`). Before adding files, complete the privacy review. The local save fixture is excluded by `.gitignore`; verify that it is ignored and is not staged. Keep the local fixture available if you intend to run the core integration test.

Check the files that Git would include:

```sh
git status --short
git check-ignore -v build build/wiiu/release/TitlesSorter.wuhb
```

The generated `build/` directory and binaries should be ignored. If Git reports personal save data, logs, local absolute paths, or generated binaries as untracked, stop and correct the ignore/review rules before proceeding.

## 3. Initialise Git and make the first commit

If this extracted folder does not already contain a `.git` directory, run:

```sh
git init
git branch -M main
git add .
git status --short
```

Review the staged files carefully. Then commit:

```sh
git commit -m "Initial release: Titles Sorter v0.5 read-only"
```

If Git asks for your identity, configure it (using the identity you want associated with your commits):

```sh
git config --global user.name "Your Name"
git config --global user.email "you@example.com"
```

Then repeat the commit command.

## 4. Connect the GitHub repository and push

Copy the HTTPS or SSH URL from the newly created GitHub repository page. For HTTPS, the command looks like this (replace the URL with your actual repository URL):

```sh
git remote add origin https://github.com/YOUR-ACCOUNT/WiiU-Titles-Sorter.git
git remote -v
git push -u origin main
```

GitHub authentication may open a browser or request a Personal Access Token; do not use your account password as a Git HTTPS password. If you use SSH, add your SSH public key to GitHub first and use the SSH repository URL instead.

Refresh the GitHub repository page and verify that the expected source tree and README are present. Keep the repository private until the privacy blocker is resolved.

## 5. Create the v0.5.0 release tag

Only after the release checklist is complete and the final commit is validated, create an annotated tag:

```sh
git tag -a v0.5.0 -m "Titles Sorter v0.5.0 - read-only preview"
git push origin v0.5.0
```

On GitHub, open the repository's **Releases** section, choose **Draft a new release**, select the `v0.5.0` tag, add the release title and notes, and publish it when ready. Attach the tested `TitlesSorter.wuhb` only after a successful Wii U target build and physical-console smoke test.

## 6. Future updates

For later source changes, commit and push normally:

```sh
git add .
git commit -m "Describe the change"
git push
```

For the next release, update the version/changelog, complete the relevant checks, create a new annotated tag (for example `v0.5.1`), and push it with `git push origin v0.5.1`. Do not move or reuse an existing release tag.

## Important notes

- Do not run `git add` on personal save dumps or local build outputs.
- Never commit authentication tokens, passwords, private keys, or personal console dumps.
- No `LICENSE` file is included by maintainer decision. Do not describe the project as open source or imply that reuse rights are granted.
- GitHub repository visibility can be changed later in repository Settings, but do so only after the privacy review is complete.
