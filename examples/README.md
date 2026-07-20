# moredeps consumption examples

These examples show how to download `moredeps.json` and the prebuilt library
packages it describes, so you can vendor them into your own project or even
re-publish them to your own GitHub release for safe-keeping.

| Example | What it does |
|---------|--------------|
| [`download-deps.sh`](download-deps.sh) | Bash + `curl`/`jq`: download `moredeps.json`, pick a few deps, fetch their zips, unzip. |
| [`download-deps.py`](download-deps.py) | Python 3 equivalent with SHA-256 verification. |
| [`vendor-release.sh`](vendor-release.sh) | Local script using the `gh` CLI to download deps and create a release in *your* repo. |
| [`vendor-release.yml`](vendor-release.yml) | GitHub Actions workflow that does the same thing on a schedule or on demand. |
| [`cmake-fetch/`](cmake-fetch/) | A tiny CMake project that fetches and unpacks deps at configure time. |
| [`vfs-lua/`](vfs-lua/) | A sandboxed Lua example that loads scripts from a PhysFS-mounted archive instead of the host filesystem. |
| [`vfs-mtcc/`](vfs-mtcc/) | An mtcc example that reads C source from PhysFS, compiles it in memory, and runs it with a whitelisted symbol set. |

## Why re-publish?

`moredeps` only keeps the **latest 3 builds**. If you pin a build in your
project, that release may disappear later. The vendor examples copy the
selected zips (and `moredeps.json`) into a release **in your own repository**, so
you control the lifetime of the binaries.

The only caveat: GitHub itself can still remove them. But at least you are not
tied to the `moreward/moredeps` release rotation.

## Quick try

```bash
# From the repo root
./examples/download-deps.sh \
  --release latest \
  --deps sokol,glfw,cglm \
  --output ./third_party

# Or with Python
python3 examples/download-deps.py \
  --release latest \
  --deps sokol glfw cglm \
  --output ./third_party
```

See the individual files for full usage and options.
