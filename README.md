# Claude Status Light

A small LED matrix on your desk that shows what Claude Code is doing, so you can look away
from the terminal and still know when it needs you.

| Claude Code state | Serial cmd | Display |
|---|---|---|
| Waiting for your response | `R` | red dot in the middle |
| Working | `B` | blue dots sliding across |
| Done (or interrupted with Esc) | `G` | 3 green blinks, then a steady green dot |
| Off | `O` | blank |
| Layout test | `T` | LED 0 white, row 0 yellow, column 0 cyan |

It works through [Claude Code hooks](https://docs.claude.com/en/docs/claude-code/hooks):
on each event a shell script sends one character over USB serial to the board.

## What you need

- A **Seeed Studio XIAO** board (any variant supported by the Arduino IDE)
- A **6x10 RGB Matrix for XIAO** (WS2812 LEDs, data on pin `D0`)
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
2. Type `T` and press Enter. You should see a white LED in a corner, a yellow line along the
   top row and a cyan line down the first column.
3. If the picture is wrong, edit the settings at the top of the `.ino` file and upload again:
   - The yellow row zig-zags instead of being a straight line: set `SERPENTINE` to `true`.
   - The yellow line runs down instead of across: swap the `WIDTH` and `HEIGHT` values.
4. Try the other commands too: `R`, `B`, `G`, `O`.
5. **Close the Serial Monitor.** Only one program can use the port at a time, so the hook
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

Start Claude Code and give it a task. The matrix turns blue while Claude works, red when it
asks you something, and green when it's done.

Claude Code has no hook for pressing Esc, so while Claude works the script also watches the
session transcript in the background. When it sees that you interrupted, it switches the
display to green.

## Troubleshooting

- **Nothing lights up from the script, but the Serial Monitor works.** Close the Serial
  Monitor, then check that the port exists (`ls /dev/cu.usbmodem*` or `ls /dev/ttyACM*`) and
  set `CLAUDE_STATUS_LIGHT_PORT`.
- **No port shows up at all.** Try another USB cable (many are charge-only), or double-tap
  the XIAO's reset button to enter bootloader mode and upload again.
- **The display works by hand but not from Claude Code.** Check the paths in
  `~/.claude/settings.json` and that the script is executable (`chmod +x`).
- **Wrong colors or a garbled picture.** Run the layout test from Step 5.

## Project layout

```
claude_status_light/claude_status_light.ino   firmware for the XIAO
host/claude-status-light.sh                   hook script that talks to the board
host/hooks.json                               example Claude Code hook config
```
