# Beee

[![firmware](https://github.com/infinition/beee/actions/workflows/firmware.yml/badge.svg)](https://github.com/infinition/beee/actions/workflows/firmware.yml)
[![build-exe](https://github.com/infinition/beee/actions/workflows/build-exe.yml/badge.svg)](https://github.com/infinition/beee/actions/workflows/build-exe.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-ESP32--S3-blue.svg)](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.47)
[![Built with PlatformIO](https://img.shields.io/badge/built%20with-PlatformIO-orange.svg)](https://platformio.org/)

Beee is a tiny animated face that sits on your desk and gives your AI agents a body.

It runs on a cheap ESP32-S3 touch screen. Agents talk to it over a small HTTP API
(or through MCP), so they can show how they feel, say a line, react to what happens
on your machine, and the part I actually built it for: pop a question on the screen
and wait for you to physically tap Yes or No before doing something you might regret.

It started as a "wouldn't it be fun if" weekend idea and ended up living on my desk.

## What it does

- A little face with moods. It blinks, glances around on its own, winks, breathes,
  talks, and switches between expressions (happy, sad, thinking, panicking, sleeping,
  smug, and a full hacker mode).
- Physical approvals. An agent can throw a Yes/No or a multiple-choice question on the
  screen and wait until you tap an answer. Risky actions get a flashing red border and
  a countdown so you do not rubber-stamp them by reflex.
- It reacts to your machine. A small background agent notices your git commits and
  pushes and makes Beee react, with nothing to type.
- A few moods on demand: party (neon colors), night (dim and sleepy), focus (a
  pomodoro countdown), and a scrolling hacker terminal.
- It pokes you back. Notifications make it blink to grab your attention.
- You can pet it. Touching the face nudges a small personality engine (energy, trust,
  boredom, chaos, affection).
- It speaks English or French, picked when you flash it.

## The hardware

Built and tested on the Waveshare ESP32-S3-Touch-LCD-1.47:

- 172x320 IPS screen, JD9853 driver over SPI
- AXS5106L capacitive touch
- No motion sensor on this board, so flipping the screen is a command you send rather
  than something automatic

You need the board, a USB-C cable, and your WiFi name and password.

## Getting it running

You need PlatformIO, either the VS Code extension or `pip install platformio`.

1. Plug the board in and find its COM port (Device Manager on Windows).
2. Double-click `build.bat`.
3. The first time, it asks for the COM port, the language (`fr` or `en`), your WiFi
   name and your WiFi password. It saves that to `beee_config.json` (kept out of git),
   writes a `secrets.h`, then compiles and uploads.

Changed your WiFi or want to switch language later? Delete `beee_config.json` and run
`build.bat` again.

Rather do it by hand? Copy `esp32_firmware/src/secrets.example.h` to `secrets.h`, fill
it in, and run `pio run --target upload` from the `esp32_firmware` folder.

Once it boots it shows its name, joins your WiFi, prints its IP on the serial monitor,
and answers at `http://beee.local`.

## Letting an agent use it

Run the MCP server on a machine that can see the device on your network:

```bash
cd mcp_server
pip install -r requirements.txt
python mcp_server.py
```

Register it with your agent. For a stdio MCP client (Claude Desktop, Cursor, and the
like):

```json
{
  "mcpServers": {
    "beee": { "command": "python", "args": ["/full/path/to/mcp_server/mcp_server.py"] }
  }
}
```

Then paste this into the agent's instructions so it knows when to use the thing:

```
You have a physical companion named Beee on the user's desk, controlled through
MCP tools. Use it to make your presence tangible:
- Reflect your state with a short message: say_on_device or show_emotion
  (emotions: idle, happy, sad, angry, thinking, sleeping, panic, sarcastic, hacker).
- Before any sensitive or irreversible action (deleting files, sending a message,
  spending money, deploying, running a risky command), ask the human to approve it
  ON the device: request_approval(question) for yes/no, or ask_choice(question,
  options) for several options. These BLOCK until the human taps a button, so only
  use them when you truly need a human decision. Use urgency="critical" for
  dangerous actions.
- Get the human's attention with notify(text). React to events with
  trigger_reaction("build_success" | "build_failed" | "mail" | "alert" |
  "task_done"). Set ambiance with set_mode("party" | "night" | "hacker") or focus
  sessions with start_focus(minutes).
Keep on-screen text under about 60 characters. Do not spam the device; use it for
moments that matter.
```

The tools the agent gets: `show_emotion`, `say_on_device`, `trigger_reaction`,
`get_device_status`, `request_approval`, `ask_choice`, `notify`, `set_mode`,
`start_focus`, `set_working`, `work_done`, `get_decision_history`, `rotate_screen`.

## Reacting to what you do on your PC

There are two pieces in `tools/`, and they cover different needs.

### The background agent

`beee_agent.py` runs quietly and watches the git repositories under a folder you
choose. Commit something, push something, and Beee reacts on its own. It can also
glance at which app you have in front (using ctypes, no extra library) and how hard
your CPU is working (that part wants psutil).

```bash
cp tools/beee_agent_config.example.json tools/beee_agent_config.json
# set watch_root and beee_url, then:
python tools/beee_agent.py
```

If you want it as a single executable that starts with Windows, run
`tools\build_exe.bat`, then drop a shortcut to `dist\beee_agent.exe` into
`shell:startup`. GitHub Actions also builds that exe for every tagged release.

### The command wrapper

Telling for sure whether a build or a test actually passed is hard to guess from the
outside, so for that one case you wrap the command:

```bash
python tools/beee_run.py npm test
python tools/beee_run.py pio run
```

Beee shows a progress bar while it runs, looks proud if it passes, and throws a small
sarcastic fit if it fails. Set `BEEE_URL` if the device is not at `http://beee.local`.
There is also an optional git `pre-push` hook in `tools/git-hooks/`.

## HTTP API

The firmware serves a REST API on port 80. Use the device IP if `beee.local` does not
resolve on your network.

| Method | Path | Body | What it does |
| --- | --- | --- | --- |
| GET | `/` | | Web dashboard |
| GET | `/api/status` | | Current mood, text and metrics |
| POST | `/api/emotion` | `{emotion, text, eye_x?, eye_y?}` | Set expression and speech |
| POST | `/api/trigger` | `{event}` | Trigger a reaction (build_success, build_failed, mail, alert, task_done, ignored) |
| POST | `/api/prompt` | `{id, question, options?, urgency?, timeout_s?}` | Show an approval or multiple-choice pop-up |
| GET | `/api/prompt/result` | `?id=` | Poll the pop-up result |
| GET | `/api/history` | | Last decisions made on pop-ups |
| POST | `/api/mode` | `{mode}` | normal, party, night, hacker |
| POST | `/api/focus` | `{minutes}` | Start a focus countdown |
| POST | `/api/notify` | `{text, urgency?}` | Blink for attention and show a message |
| POST | `/api/progress` | `{percent, text}` | Show a progress screen (percent -1 means indeterminate) |
| POST | `/api/rotate` | `{flip?}` | Flip the screen 180 degrees |

A quick example:

```bash
curl -X POST http://beee.local/api/prompt \
  -H "Content-Type: application/json" \
  -d '{"id":"a1","question":"Deploy to production?","options":["Deploy","Cancel"],"urgency":"critical","timeout_s":30}'
```

## The optional Python backend

`python_server/` is a FastAPI service that can stream media and run the persona engine.
The firmware already serves its own dashboard and API, so you only need this if you
want the streaming path.

```bash
cd python_server
pip install -r requirements.txt
python server.py
```

## Config and secrets

Your WiFi details and the chosen language never get committed. They live in
`beee_config.json` and in the generated `esp32_firmware/src/secrets.h`, both ignored
by git. The `.example` files show the format if you would rather set things up by hand.

## Repository layout

```
esp32_firmware/   The device firmware (PlatformIO)
python_server/    Optional FastAPI backend
mcp_server/       MCP server that exposes Beee to agents
tools/            Background agent, command wrapper, git hook, exe builder
build.bat         Interactive build and flash launcher (Windows)
```

## License

MIT. See [LICENSE](LICENSE). Use it, change it, build your own desk gremlin.

## Star History

<a href="https://www.star-history.com/?repos=infinition%2Fbeee&type=date&legend=top-left">
 <picture>
   <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/chart?repos=infinition/beee&type=date&theme=dark&legend=top-left" />
   <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/chart?repos=infinition/beee&type=date&legend=top-left" />
   <img alt="Star History Chart" src="https://api.star-history.com/chart?repos=infinition/beee&type=date&legend=top-left" />
 </picture>
</a>
