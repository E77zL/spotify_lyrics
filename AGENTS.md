# Project maintenance

- The product is named **Spotify Lyrics**. Do not add UI, HUD or Companion suffixes to product labels or release filenames. Preserve original author names, license texts and upstream URLs.
- README files are maintained by the repository owner. Preserve their edits during builds and local repository synchronization; do not regenerate or replace them from old templates. Change them only when the user explicitly asks. English uses `README.md`, Chinese uses `README.zh-CN.md`; `README.en.md` retains compatibility with older links.
- Keep existing installation/settings paths and control identifiers compatible so upgrades retain personal settings and per-song timing.
- Keep code comments in English. Use pinned, SHA256-verified dependencies and retain corresponding source and attribution in release artifacts.
- Run checks appropriate to changed behavior. `Build.ps1 -Offline` builds both installers and runs the native, provider, integration and installer checks; it does not publish to GitHub.
