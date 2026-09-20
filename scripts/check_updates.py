#!/usr/bin/env python3
"""Check each submodule for newer upstream commits/tags vs the pinned commit."""
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor


def run(cmd, cwd=None, timeout=None):
    return subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, timeout=timeout)


def get_submodules():
    out = run(["git", "config", "-f", ".gitmodules", "--get-regexp", r"submodule\..*\.(path|url)"])
    paths, urls = {}, {}
    for line in out.stdout.splitlines():
        key, val = line.split(" ", 1)
        name = re.match(r"submodule\.(.*)\.(path|url)", key).group(1)
        if key.endswith(".path"):
            paths[name] = val
        else:
            urls[name] = val
    return [(paths[n], urls[n]) for n in paths]


def semver_key(tag):
    nums = re.findall(r"\d+", tag)
    if not nums:
        return None
    return tuple(int(n) for n in nums)


def tag_template(tag):
    """Regex that matches tags of the same 'shape' as the given tag."""
    parts = re.split(r"(\d+)", tag)
    pat = "".join(re.escape(p) if not p.isdigit() else r"\d+" for p in parts)
    return re.compile("^" + pat + "$")


def check(path, url):
    st = run(["git", "submodule", "status", path]).stdout.strip()
    cur = st.split()[0].lstrip("+-")
    cur_desc = ""
    r = run(["git", "-C", path, "describe", "--tags", cur])
    if r.returncode == 0:
        cur_desc = r.stdout.strip()

    r = run(["git", "ls-remote", url, "HEAD", "refs/tags/*"], timeout=60)
    if r.returncode != 0:
        return (path, cur_desc or cur[:10], "ERROR", "ls-remote failed")

    head = None
    tags = []
    for line in r.stdout.splitlines():
        sha, ref = line.split("\t")
        if ref == "HEAD":
            head = sha
        elif not ref.endswith("^{}"):
            tags.append(ref.split("/")[-1])

    best_tag = None
    base = re.sub(r"-\d+-g[0-9a-f]+$", "", cur_desc) if cur_desc else ""
    tpl = tag_template(base) if re.search(r"\d", base) else None
    cands = [t for t in tags if tpl.match(t)] if tpl else []
    keyed = [(semver_key(t), t) for t in cands]
    keyed = [k for k in keyed if k[0]]
    if keyed:
        best_tag = max(keyed)[1]

    behind = "unknown"
    if head:
        r2 = run(["git", "-C", path, "merge-base", "--is-ancestor", cur, head])
        # cur may not be fetched against head; just compare SHAs and count if possible
        behind = "up-to-date" if cur == head else "behind-HEAD"

    return (path, cur_desc or cur[:10], best_tag or "-", behind)


def main():
    subs = get_submodules()
    results = []
    with ThreadPoolExecutor(max_workers=12) as ex:
        futs = {ex.submit(check, p, u): p for p, u in subs}
        for f in futs:
            try:
                results.append(f.result())
            except Exception as e:
                results.append((futs[f], "?", "?", f"error: {e}"))
    results.sort()
    w = max(len(r[0]) for r in results)
    for path, cur, tag, behind in results:
        print(f"{path:<{w}}  pinned={cur:<28}  latest_tag={tag:<22}  {behind}")


if __name__ == "__main__":
    sys.exit(main())
