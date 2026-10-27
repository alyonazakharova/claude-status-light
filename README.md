# Claude Status Light

A little LED cat face on your desk that shows what Claude Code is doing, so you can look
away from the terminal and still know when it needs you.

The 6x10 matrix is mounted landscape (10 wide, 6 tall) behind a cat-face case and draws
eyes, a nose and a mouth:

```
. # # . . . . # # .     eyes (2x2, color shows the state)
. # # . . . . # # .
. . . . N N . . . .     nose (pink)
. M . . M M . . M .     mouth (ω, dim white)
. M . . M M . . M .
. . M M . . M M . .
```

| Claude Code state | Serial cmd | Eyes |
|---|---|---|
| Working | `B` | blue eyes, each blinking at random |
| Waiting for your response | `R` | red eyes blink 3 times together, then stay on |
| Done (or interrupted with Esc) | `G` | green eyes blink 3 times together, then stay on |
| Off | `O` | everything off |
| Layout test | `T` | top-left LED white, top row yellow, left column cyan |

The nose and mouth stay lit in every state except off and the layout test.

It works through [Claude Code hooks](https://docs.claude.com/en/docs/claude-code/hooks):
on each event a shell script sends one character over USB serial to the board.

## What you need

- A **Seeed Studio XIAO** board (any variant supported by the Arduino IDE)
- A **6x10 RGB Matrix for XIAO** (WS2812 LEDs, data on pin `D0`)
- Optionally, a 3D-printed cat-face case for the matrix
- A USB-C data cable (not a charge-only cable)
- A computer running macOS or Linux with [Claude Code](https://docs.claude.com/en/docs/claude-code) installed
- [Arduino IDE](https://www.arduino.cc/en/software) 2.x

Over USB, the board has to be plugged into the computer that runs Claude Code. ESP32 boards
can also work over Wi-Fi (see [Optional: Wi-Fi](#optional-wi-fi-esp32-boards)).

## Step 1: Get the code

```sh
git clone https://github.com/alyonazakharova/claude-status-light.git ~/claude-status-light
chmod +x ~/claude-status-light/host/claude-status-light.sh
```

The examples below assume this location. If you clone it somewhere else, change the paths
to match.

## Step 2: Assemble the hardware

1. Solder or seat the XIAO onto the RGB matrix board.
2. Connect the XIAO to your computer with the USB-C cable.

## Step 3: Set up the Arduino IDE

1. Install the board package for your XIAO. Follow
   [Seeed's guide](https://wiki.seeedstudio.com/Seeed_Studio_XIAO_Series_Introduction/) for
   your variant. It tells you which *Additional Boards Manager URL* to add and which package
   to install in **Tools → Board → Boards Manager**.
2. Open **Tools → Manage Libraries**, search for **Adafruit NeoPixel**, and install it.

## Step 4: Flash the firmware

1. In the Arduino IDE, open `claude_status_light/claude_status_light.ino`.
2. Choose your board in **Tools → Board** and its port in **Tools → Port**
   (on macOS it looks like `/dev/cu.usbmodemXXXX`, on Linux `/dev/ttyACM0`).
3. Click **Upload**. The matrix stays dark after flashing, which is expected.

## Step 5: Check the LED layout

1. Open **Tools → Serial Monitor**.
2. Hold the matrix the way it will sit in the case: landscape, 10 LEDs wide and 6 tall.
3. Type `T` and press Enter. You should see a yellow line along the **top** edge (10 LEDs), a
   cyan line down the **left** edge (6 LEDs) and a white LED in the top-left corner.
4. If the picture is wrong, edit the settings at the top of the `.ino` file and upload again:
   - The yellow line is along the bottom (the face would be upside down): change `ROTATION`
     from `1` to `3`, or from `3` to `1`.
   - The lines zig-zag instead of being straight: set `SERPENTINE` to `true`.
   - The white LED is top-right instead of top-left: you can ignore it, because the face is
     symmetric.
5. Try the other commands too: `B` (blue, blinking), `R` (red), `G` (green), `O` (off).
6. **Close the Serial Monitor.** Only one program can use the port at a time, so the hook
   script can't reach the board while it's open.

The LED brightness is set by `BRIGHTNESS` (0–255). Keep it low: 60 LEDs at full white
draw far more current than USB can supply.

## Step 6: Test the script by hand

```sh
~/claude-status-light/host/claude-status-light.sh working
~/claude-status-light/host/claude-status-light.sh waiting
~/claude-status-light/host/claude-status-light.sh done
~/claude-status-light/host/claude-status-light.sh off
```

The script picks the first `/dev/cu.usbmodem*` or `/dev/ttyACM*` port it finds. If you have
several USB serial devices, or nothing happens, set the port yourself:

```sh
export CLAUDE_STATUS_LIGHT_PORT=/dev/cu.usbmodem1101
```

Put that line in your `~/.zshrc` or `~/.bashrc` so Claude Code inherits it.

On Linux, your user may need access to serial ports:
`sudo usermod -aG dialout $USER`, then log out and back in.

## Step 7: Connect it to Claude Code

1. Open `~/.claude/settings.json` (create it if it doesn't exist).
2. Copy the `"hooks"` block from [`host/hooks.json`](host/hooks.json) into it. If you already
   have a `"hooks"` section, add these four events to it rather than replacing it.
3. Restart Claude Code.

The hooks map Claude Code events to the display:

| Hook event | Display |
|---|---|
| `UserPromptSubmit`, `PostToolUse` | working (blue) |
| `Notification` (Claude needs permission or input) | waiting (red) |
| `Stop` (Claude finished its turn) | done (green) |

## Step 8: Use it

Start Claude Code and give it a task. The cat gets randomly blinking blue eyes while Claude
works, red eyes when it asks you something, and green eyes when it's done.

Claude Code has no hook for pressing Esc, so while Claude works the script also watches the
session transcript in the background. When it sees that you interrupted, it switches the
display to green.

## Troubleshooting

- **Nothing lights up from the script, but the Serial Monitor works.** Close the Serial
  Monitor, then check that the port exists (`ls /dev/cu.usbmodem*` or `ls /dev/ttyACM*`) and
  set `CLAUDE_STATUS_LIGHT_PORT`.
- **No port shows up at all.** Try another USB cable (many are charge-only). If it still
  doesn't appear, put the board in bootloader mode and upload again: on ESP32 XIAOs hold
  **BOOT** while plugging in USB; on SAMD21/RP2040 XIAOs double-tap the reset button.
- **The Serial Monitor shows nothing on an ESP32 XIAO.** In **Tools**, set
  **USB CDC On Boot** to **Enabled** and upload again, so `Serial` uses the USB port.
- **The display works by hand but not from Claude Code.** Check the paths in
  `~/.claude/settings.json` and that the script is executable (`chmod +x`).
- **Wrong colors or a garbled picture.** Run the layout test from Step 5.

## Optional: Wi-Fi (ESP32 boards)

On an ESP32 XIAO (C3, C6, S3) the light can also join your Wi-Fi and take commands over
HTTP. USB is then only needed for power, so the cat can sit anywhere in the room, and Claude
Code can even run on another computer on the same network. USB serial keeps working too.

1. **Add your Wi-Fi details.** Copy `claude_status_light/secrets.example.h` to
   `claude_status_light/secrets.h` (same folder) and fill in your network name and password.
   `secrets.h` is git-ignored, so the password stays on your computer. The board needs a
   2.4 GHz network; the XIAO can't join 5 GHz-only networks.
2. **Flash the firmware again** as in Step 4. The firmware turns Wi-Fi on whenever
   `secrets.h` is present.
3. **Check that it's online.** Open the Serial Monitor and press the board's RESET button.
   After a few seconds it prints something like
   `Wi-Fi connected: http://claude-light.local/ (192.168.1.42)`.
   Open http://claude-light.local/ in a browser: it answers with the current state.
4. **Try it by hand:**

   ```sh
   curl "http://claude-light.local/cmd?c=B"
   CLAUDE_STATUS_LIGHT_URL=http://claude-light.local ~/claude-status-light/host/claude-status-light.sh done
   ```

5. **Tell the hooks to use Wi-Fi.** Add an `env` entry to `~/.claude/settings.json`, next to
   `"hooks"`, and restart Claude Code:

   ```json
   "env": {
     "CLAUDE_STATUS_LIGHT_URL": "http://claude-light.local"
   }
   ```

   The script tries HTTP first and falls back to USB if the light doesn't answer within
   about a second.

Notes:
- `.local` names need mDNS (built into macOS; on Linux, `avahi-daemon`). If the name doesn't
  resolve, use the IP address from the Serial Monitor instead, and ideally give the board a
  fixed address in your router.
- On macOS, the first request may make your terminal app ask for **Local Network** access.
  Allow it, or HTTP requests will fail and the script will fall back to USB.
- Anyone on your network can change the light. There's no password, since all it does is
  change the cat's eyes.
- To change the name, edit `HOSTNAME` at the top of the `.ino` file.

### Other wireless options (future)

| Board | Wi-Fi | Bluetooth LE | Thread / Zigbee |
|---|---|---|---|
| XIAO ESP32-C6 | Wi-Fi 6 (2.4 GHz) | BLE 5 | yes (802.15.4) |
| XIAO ESP32-C3 / ESP32-S3 | 2.4 GHz | BLE 5 | no |
| XIAO nRF52840 | no | BLE 5 | no |
| XIAO SAMD21 / RP2040 | no | no | no |

- **Wi-Fi + MQTT:** the board subscribes to a topic on a broker (Mosquitto, Home
  Assistant). Several lights and sessions can share one topic.
- **Bluetooth LE:** no network needed, but sending from a shell script needs a helper (for
  example a small Python `bleak` script), and the range is about one room.
- **Thread / Matter (ESP32-C6 only):** the light shows up in Apple Home, Google Home or Home
  Assistant.
- **Setup without editing files:** a captive-portal page to enter Wi-Fi details from a phone.

## Future ideas

- **More wireless options:** MQTT, BLE, Matter, and Wi-Fi setup from a phone (see above).
- **Other coding agents:** for example OpenAI Codex CLI, whose `notify` setting can trigger
  "done". The serial commands are tool-agnostic, so this mostly means a new hook config per agent.
- **Several sessions at once:** one column or color per session, or show "waiting" if any
  session needs you.
- **More states:** for example a distinct color for permission prompts or for errors.
- **Auto brightness or night mode:** dim the light at night, or add a light sensor.
- **Idle timeout:** turn off a "done" light after a while.
- **Sound or vibration:** a buzzer for when Claude is waiting on you.
- **Battery power:** a LiPo pack on the XIAO's battery pads makes the wireless version
  cable-free.
- **3D-printed case:** with a diffuser so the dots look softer.

## Project layout

```
claude_status_light/claude_status_light.ino   firmware for the XIAO
claude_status_light/secrets.example.h         template for Wi-Fi details (copy to secrets.h)
host/claude-status-light.sh                   hook script that talks to the board
host/hooks.json                               example Claude Code hook config
```
