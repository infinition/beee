#!/usr/bin/env python3
"""
beee_agent - ambient background watcher for Beee.

Unlike beee_run (which wraps a single command), this agent runs in the background
and detects your activity automatically, with no need to prefix anything:

  - Git commits and pushes, by watching .git/logs in your repositories.
  - Active application (optional), to set an ambient mood.
  - CPU load (optional, needs psutil), to make Beee sweat when the machine is busy.

Only the standard library is required for the git and active-window features
(urllib + ctypes), so it packages cleanly into a single .exe with PyInstaller.

Config: beee_agent_config.json next to this file (see beee_agent_config.example.json).
"""
import json
import os
import sys
import time
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))


def load_config():
    path = os.path.join(HERE, "beee_agent_config.json")
    cfg = {
        "beee_url": os.environ.get("BEEE_URL", "http://beee.local"),
        "watch_root": ".",
        "poll_seconds": 3,
        "watch_cpu": False,
        "cpu_threshold": 92,
        "watch_active_window": False,
    }
    if os.path.exists(path):
        try:
            cfg.update(json.load(open(path, encoding="utf-8")))
        except Exception as e:
            print(f"[beee] config error, using defaults: {e}")
    return cfg


def post(url, path, data):
    try:
        req = urllib.request.Request(
            url + path,
            data=json.dumps(data).encode("utf-8"),
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        urllib.request.urlopen(req, timeout=4).read()
    except Exception:
        pass


def emotion(url, emo, text):
    post(url, "/api/emotion", {"emotion": emo, "text": text})


# ---------------------------------------------------------------- git watching

def find_repos(root, max_depth=4):
    repos = []
    root = os.path.abspath(root)
    base_depth = root.rstrip(os.sep).count(os.sep)
    for dirpath, dirnames, _ in os.walk(root):
        depth = dirpath.count(os.sep) - base_depth
        if depth > max_depth:
            dirnames[:] = []
            continue
        if ".git" in dirnames:
            repos.append(dirpath)
            dirnames[:] = [d for d in dirnames if d != ".git"]  # do not descend into .git
    return repos


def _last_line(path):
    try:
        with open(path, "rb") as f:
            data = f.read()
        lines = [l for l in data.splitlines() if l.strip()]
        return lines[-1].decode("utf-8", "replace") if lines else ""
    except Exception:
        return ""


def _remote_log_files(repo):
    base = os.path.join(repo, ".git", "logs", "refs", "remotes")
    out = []
    for dp, _, files in os.walk(base):
        for fn in files:
            out.append(os.path.join(dp, fn))
    return out


def git_snapshot(repo):
    snap = {"head": _last_line(os.path.join(repo, ".git", "logs", "HEAD"))}
    for f in _remote_log_files(repo):
        snap["r:" + f] = _last_line(f)
    return snap


def main():
    cfg = load_config()
    url = cfg["beee_url"]
    print(f"[beee] agent started. Target: {url}. Watching: {cfg['watch_root']}")

    repos = find_repos(cfg["watch_root"])
    print(f"[beee] {len(repos)} git repo(s) found.")
    snaps = {r: git_snapshot(r) for r in repos}

    last_repo_scan = time.time()
    last_app = None
    last_app_react = 0.0
    cpu_busy = False

    while True:
        # Rescan for new repos occasionally
        if time.time() - last_repo_scan > 60:
            last_repo_scan = time.time()
            for r in find_repos(cfg["watch_root"]):
                if r not in snaps:
                    snaps[r] = git_snapshot(r)

        # Git events
        for repo, old in list(snaps.items()):
            new = git_snapshot(repo)
            if new == old:
                continue
            pushed = any(k.startswith("r:") and new.get(k) != old.get(k) for k in set(new) | set(old))
            committed = new.get("head") != old.get("head")
            snaps[repo] = new
            name = os.path.basename(repo)
            if pushed:
                emotion(url, "hacker", f"Push {name} ! A moi la liberte !")
            elif committed:
                emotion(url, "happy", f"Commit sur {name}.")

        # Optional: active window mood
        if cfg.get("watch_active_window"):
            title = _active_window_title()
            cat = _categorize(title)
            if cat and cat != last_app and (time.time() - last_app_react) > 25:
                last_app = cat
                last_app_react = time.time()
                emo, txt = cat
                emotion(url, emo, txt)

        # Optional: CPU load
        if cfg.get("watch_cpu"):
            load = _cpu_percent()
            if load is not None:
                if load >= cfg["cpu_threshold"] and not cpu_busy:
                    cpu_busy = True
                    emotion(url, "panic", "Ca chauffe la-dedans !")
                elif load < cfg["cpu_threshold"] - 15 and cpu_busy:
                    cpu_busy = False
                    emotion(url, "happy", "Ouf, ca respire.")

        time.sleep(max(1, int(cfg["poll_seconds"])))


# --------------------------------------------------- optional helpers (Windows)

def _active_window_title():
    try:
        import ctypes
        u = ctypes.windll.user32
        h = u.GetForegroundWindow()
        n = u.GetWindowTextLengthW(h)
        buf = ctypes.create_unicode_buffer(n + 1)
        u.GetWindowTextW(h, buf, n + 1)
        return buf.value
    except Exception:
        return ""


def _categorize(title):
    t = (title or "").lower()
    if any(k in t for k in ("visual studio code", "cursor", "pycharm", "sublime", "vim", "neovim")):
        return ("thinking", "Au boulot, on code.")
    if any(k in t for k in ("youtube", "reddit", "twitch", "netflix")):
        return ("sarcastic", "Tu procrastines, non ?")
    if any(k in t for k in ("windows terminal", "powershell", "cmd.exe", "wsl")):
        return ("hacker", "Mode terminal engage.")
    return None


def _cpu_percent():
    try:
        import psutil
        return psutil.cpu_percent(interval=None)
    except Exception:
        return None


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        sys.exit(0)
