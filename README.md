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

The board has to be plugged into the same computer that runs Claude Code.

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

## Going wireless (planned)

Right now the board must be plugged into the computer that runs Claude Code, because the
script writes to a local USB serial port. Boards with a radio can drop that cable and only
need USB for power, so the light can sit anywhere in the room, and Claude Code can even run
on another machine.

Which XIAOs can do it:

| Board | Wi-Fi | Bluetooth LE | Thread / Zigbee |
|---|---|---|---|
| XIAO ESP32-C6 | Wi-Fi 6 (2.4 GHz) | BLE 5 | yes (802.15.4) |
| XIAO ESP32-C3 / ESP32-S3 | 2.4 GHz | BLE 5 | no |
| XIAO nRF52840 | no | BLE 5 | no |
| XIAO SAMD21 / RP2040 | no | no | no |

The firmware only needs a new way to receive the same `R` / `B` / `G` / `O` / `T` commands.
The rest stays as it is. The options, from simplest to most involved:

1. **Wi-Fi + HTTP (recommended first step).** The board joins your Wi-Fi and runs a tiny web
   server. The script sends `curl http://claude-light.local/cmd?c=B` instead of writing to
   the serial port, and finds the board by its mDNS name. This is easy to debug from a browser
   and works from any machine on the network.
2. **Wi-Fi + MQTT.** The board subscribes to a topic on an MQTT broker (for example
   Mosquitto or Home Assistant's) and the script publishes to it. This takes more setup, but
   several lights and several Claude sessions can share one topic, and it works through NAT
   if the broker is reachable.
3. **Bluetooth LE.** The board exposes one writable characteristic and the computer writes
   the command byte to it. It needs no network, but sending from a shell script is fiddly
   (it needs a helper such as a small Python `bleak` script), and the range is about one room.
4. **Thread / Matter (ESP32-C6 only).** The light shows up as a Matter device in Apple Home,
   Google Home or Home Assistant, and the hook triggers it through the smart-home hub. It's
   the most work, but it fits neatly into an existing smart home.

Planned shape of the change:
- The firmware keeps USB serial and adds Wi-Fi, with credentials in a `secrets.h` that is
  not committed (and later, a captive-portal setup page).
- The script uses HTTP when `CLAUDE_STATUS_LIGHT_URL` is set, and USB serial otherwise.
- The firmware falls back to a "no connection" pattern when Wi-Fi drops.

## Future ideas

- **Wireless:** Wi-Fi first, then MQTT and BLE (see above).
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
host/claude-status-light.sh                   hook script that talks to the board
host/hooks.json                               example Claude Code hook config
```
