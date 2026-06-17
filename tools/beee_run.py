#!/usr/bin/env python3
"""
beee_run - wrap any command and let Beee react in real time.

Usage:
    python beee_run.py <command> [args...]

Examples:
    python beee_run.py git push
    python beee_run.py npm test
    python beee_run.py pio run

Beee shows a progress screen while the command runs, then a proud/happy face on
success or a panicking glitch with a quip on failure. No third-party dependency
(standard library only), so it runs anywhere Python 3 is available.

Configure the device URL with the BEEE_URL environment variable
(default: http://beee.local).
"""
import json
import os
import random
import subprocess
import sys
import urllib.request

BEEE = os.environ.get("BEEE_URL", "http://beee.local")

OK_QUIPS = [
    "C'est passe, je suis fier.",
    "Nickel. On enchaine.",
    "Propre. Tu progresses.",
]
PUSH_QUIPS = [
    "Code envoye ! A moi la liberte !",
    "Push reussi. Le monde est pret.",
]
FAIL_QUIPS = [
    "Encore un bug ? Je regrette de ne pas etre une horloge...",
    "Ca a plante. Classique.",
    "Rouge partout. Bravo l'artiste.",
]


def _post(path, data):
    try:
        req = urllib.request.Request(
            BEEE + path,
            data=json.dumps(data).encode("utf-8"),
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        urllib.request.urlopen(req, timeout=4).read()
    except Exception:
        pass  # never let the companion break your command


def progress(percent, text):
    _post("/api/progress", {"percent": percent, "text": text})


def emotion(emo, text):
    _post("/api/emotion", {"emotion": emo, "text": text})


def main(argv):
    if len(argv) < 2:
        print("usage: beee_run <command> [args...]")
        return 1

    cmd = argv[1:]
    label = os.path.basename(cmd[0])
    is_push = ("git" in cmd[0].lower()) and ("push" in [a.lower() for a in cmd])

    progress(-1, (label + "...")[:18])
    try:
        rc = subprocess.call(cmd)
    except FileNotFoundError:
        emotion("sad", "Commande introuvable.")
        return 127

    if rc == 0:
        if is_push:
            emotion("hacker", random.choice(PUSH_QUIPS))
        else:
            emotion("happy", random.choice(OK_QUIPS))
    else:
        emotion("panic", random.choice(FAIL_QUIPS))
    return rc


if __name__ == "__main__":
    sys.exit(main(sys.argv))
