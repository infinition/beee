#!/usr/bin/env python3
"""
beee_agent - ambient background watcher for Beee.

Runs in the background and detects your activity automatically, with nothing to
prefix:

  - Git commits and pushes, by watching .git/logs in your repositories.
  - Active application (optional), to set an ambient mood.
  - CPU load (optional, needs psutil), to make Beee sweat when the machine is busy.

Git and active-window features use the standard library only (urllib + ctypes),
so it packages cleanly into a single .exe with PyInstaller.

Config: beee_agent_config.json next to the script or the exe
(see beee_agent_config.example.json). A beee_agent.log file is written next to it.
"""
import datetime
import json
import os
import sys
import time
import urllib.request


def app_dir():
    # When frozen by PyInstaller, __file__ points to a temp folder, so use the exe path.
    if getattr(sys, "frozen", False):
        return os.path.dirname(sys.executable)
    return os.path.dirname(os.path.abspath(__file__))


APP_DIR = app_dir()
LOG_PATH = os.path.join(APP_DIR, "beee_agent.log")


def log(msg):
    line = f"{datetime.datetime.now():%Y-%m-%d %H:%M:%S} {msg}"
    print(line)
    try:
        with open(LOG_PATH, "a", encoding="utf-8") as f:
            f.write(line + "\n")
    except Exception:
        pass


def load_config():
    cfg = {
        "beee_url": os.environ.get("BEEE_URL", "http://beee.local"),
        "watch_root": ["."],
        "poll_seconds": 3,
        "watch_cpu": False,
        "cpu_threshold": 92,
        "watch_active_window": False,
    }
    for d in (APP_DIR, os.getcwd()):
        p = os.path.join(d, "beee_agent_config.json")
        if os.path.exists(p):
            try:
                cfg.update(json.load(open(p, encoding="utf-8")))
                log(f"Config loaded from {p}")
            except Exception as e:
                log(f"Config error in {p}: {e}")
            break
    else:
        log("No beee_agent_config.json found, using defaults.")

    wr = cfg["watch_root"]
    cfg["watch_root"] = [wr] if isinstance(wr, str) else list(wr)
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
        return True
    except Exception as e:
        log(f"POST {path} failed: {e}")
        return False


def emotion(url, emo, text):
    post(url, "/api/emotion", {"emotion": emo, "text": text})


# ---------------------------------------------------------------- git watching

def find_repos(root, max_depth=5):
    repos = []
    root = os.path.abspath(root)
    if not os.path.isdir(root):
        return repos
    base_depth = root.rstrip(os.sep).count(os.sep)
    for dirpath, dirnames, _ in os.walk(root):
        depth = dirpath.count(os.sep) - base_depth
        if depth > max_depth:
            dirnames[:] = []
            continue
        if ".git" in dirnames:
            repos.append(dirpath)
            dirnames[:] = [d for d in dirnames if d != ".git"]
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
    log(f"Beee agent starting. Target: {url}. Roots: {cfg['watch_root']}")

    # Startup ping so you can see straight away that the agent reaches the device.
    if post(url, "/api/emotion", {"emotion": "happy", "text": "Agent en ligne."}):
        log("Startup ping OK (device reachable).")
    else:
        log("Startup ping FAILED (device not reachable, check beee_url).")

    repos = []
    for root in cfg["watch_root"]:
        found = find_repos(root)
        repos += found
        log(f"  root {root}: {len(found)} repo(s)")
    snaps = {r: git_snapshot(r) for r in repos}
    log(f"Watching {len(repos)} repo(s) total. Make a commit/push to test.")

    last_repo_scan = time.time()
    last_app = None
    last_app_react = 0.0
    cpu_busy = False

    while True:
        if time.time() - last_repo_scan > 60:
            last_repo_scan = time.time()
            for root in cfg["watch_root"]:
                for r in find_repos(root):
                    if r not in snaps:
                        snaps[r] = git_snapshot(r)
                        log(f"New repo watched: {r}")

        for repo, old in list(snaps.items()):
            new = git_snapshot(repo)
            if new == old:
                continue
            pushed = any(k.startswith("r:") and new.get(k) != old.get(k) for k in set(new) | set(old))
            committed = new.get("head") != old.get("head")
            snaps[repo] = new
            name = os.path.basename(repo)
            if pushed:
                log(f"PUSH detected in {name}")
                emotion(url, "happy", f"Push {name} ! A moi la liberte !")
            elif committed:
                log(f"COMMIT detected in {name}")
                emotion(url, "thinking", f"Commit sur {name}.")

        if cfg.get("watch_active_window"):
            cat = _categorize(_active_window_title())
            if cat and cat != last_app and (time.time() - last_app_react) > 25:
                last_app = cat
                last_app_react = time.time()
                emo, txt = cat
                emotion(url, emo, txt)

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
