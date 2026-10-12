# nighthike

A Pebble watchface written in C using the Pebble SDK.

## Display & location

The left-aligned vertical display shows `LOCAL TIME`, a clock with seconds
using the watch's 12/24-hour preference, then `LOCATION` above latitude and
longitude in decimal degrees rounded to five decimal places with N/S/E/W
suffixes. This gives about 1.1m latitude resolution, not guaranteed GPS accuracy.

Bundled Ubuntu Mono selects sizes from the display dimensions:

| Display | Clock | Coordinates | Metric grid |
| --- | --- | --- | --- |
| Small (144px rectangular) | 28px | 22px | 16px |
| Small round (180px) | 22px | 20px | 16px |
| Medium (at least 192 x 200) | 32px | 28px | 16px |
| Large (at least 240 x 240) | 32px | 32px | 16px |

Row heights grow with the fonts; round displays use circle-safe row margins.
Font sizes are screen pixels rather than typographic points.
Section headings use full 16px rows to avoid clipping. Metric headings remain
regular Ubuntu Mono, with separate 16px Ubuntu Mono Bold values underneath.
Medium and large displays use `HEADING`, `ACCURACY`, `ALTITUDE`, and `HEART RATE`;
compact displays use `HDG`, `ACC`, `ALT`, and `HR`. GPS status appears
beside `LOCATION` rather than taking a separate bottom row.

Open the watchface's settings in the Pebble phone app to choose Metric or
Imperial units and a backlight color. Metric is the default; Imperial displays
accuracy in feet/miles and altitude in feet/miles. Red is the default backlight
tint. Available choices are red, orange, yellow, green, blue, white, or the
watch's system default. Color tinting is ignored on watches without a color
backlight. Preferences are saved on the watch and applied when the face is active.

`ACC` is the phone's reported horizontal accuracy, rounded up in metres in
Metric mode or feet/miles in Imperial mode; a `--` value means accuracy was
not provided. `HDG` is the
watch's magnetic compass heading clockwise from north, not GPS course or true
north. `M` marks magnetic heading, `*` marks calibration in progress, and
`HDG CAL` means the reading is not yet valid. Watches without a compass show
`HDG --`.

The bottom 2x2 grid places heading and accuracy above altitude and heart rate.
Each cell shows its label above its value. `ALT` is the phone's reported altitude
above the WGS84 ellipsoid, displayed in metres in Metric mode or feet/miles in
Imperial mode; it is not necessarily elevation above mean sea level. Negative
altitude and sea level are valid. A `--` value means the phone did not provide
altitude. Altitude refreshes with each location fix.

A heart glyph from the built-in system fallback font labels the watch's
current Pebble Health heart rate in beats per minute.
The number below the heart is BPM; no unit suffix is shown in that narrow cell.
Enable health access and wear a watch with a supported heart-rate sensor.
Unsupported hardware, denied permission, and unavailable readings show `--`.
Health events update the display, with an additional availability check every
minute. The watchface does not request a faster sensor sampling rate. Health
readings stay on the watch and are not sent to the phone.

Location comes from the paired phone, not an onboard watch GPS. Allow location
access in the Pebble phone app. The app requests a high-accuracy fix on launch
and every minute. Watchface buttons retain their system behavior. Actual accuracy depends on the
phone's location service, not the number of displayed decimal places. If a
refresh fails, the last coordinates stay visible with a `GPS stale` status.

On watches with a color backlight, the selected tint is applied while the
watchface is in the foreground and the user's default is restored on focus loss
or exit. Brightness and automatic backlight timing remain unchanged.

## Building & running

```sh
~/.local/bin/pebble build                          # build for all targetPlatforms
~/.local/bin/pebble install --emulator emery       # install on the emery emulator
~/.local/bin/pebble install --phone <ip>           # install to a paired phone
```

The backlight override requires an SDK with `light_set_color_rgb888()` and
`light_set_system_color()` (available in the installed SDK).

Run all tests with `node --test tests/*.test.js`. The coordinate-rounding test
uses the host C compiler (`cc`) to exercise the actual watch formatter.

## Target platforms

`targetPlatforms` in `package.json` controls which watches you build for. The
modern Pebble hardware is **emery** (Pebble Time 2), **gabbro** (Pebble Round
2), and **flint** (Pebble 2 Duo); the original Pebble platforms (aplite,
basalt, chalk, diorite) are included by default for backwards compatibility.

## Project layout

```
src/c/           C source for the watchapp
src/pkjs/        PebbleKit JS (phone-side) source, if any
worker_src/c/    Background worker source, if any
resources/       Images, fonts, and other bundled resources
package.json     Project metadata (UUID, platforms, resources, message keys)
wscript          Build rules — usually no need to edit
```

This project is configured as a watchface via `pebble.watchapp.watchface` in
`package.json`.

## Documentation

Full SDK docs, tutorials, and API reference: <https://developer.repebble.com>
