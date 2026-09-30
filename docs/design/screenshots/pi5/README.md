# Pi 5 V3D observations — 2026-09-30

These are original full-client captures, not substitutes for the remaining
[platform visual matrix](../../../raspberry-pi5.md#visual-verification-matrix).
They are developer observations, not independent visual acceptance.

- Source checkpoint: `5cd69ff0c588656053994de9a084f18a8073f90d`.
- Artifact: `duel6r-linux-aarch64.tar.gz` from native ARM
  [CI run 36772825547](https://github.com/mkapusnik-apps/duel6r/actions/runs/36772825547).
- Archive SHA-256: `642cbe96c4fabfe51b76c57669c4db7a76b8d4763c583f42ff5c189e84e132ab`.
- Executable SHA-256: `1da2c6401c6ba0be423c7de9325cac36831a874dfd6e044018b990f05a55a1ad`.
- Device: Raspberry Pi 5 Model B Rev 1.0, 8 GB; Debian Bookworm 64-bit,
  kernel `6.12.47+rpt-rpi-2712`. Application runs in Ubuntu 24.04 Docker userspace.
- Display: isolated Weston 13 headless GL compositor with kiosk shell and
  Xwayland, using the physical `/dev/dri/renderD128`; no HDMI sink attached.
- GPU: Broadcom V3D 7.1.7.0, Mesa `25.2.8-0ubuntu0.24.04.3`, accelerated GLX.
  Software rendering was not forced. The application's `gl_info` confirms V3D.
- Viewport: 1280×900 for every capture, one complete client window.
- Capture: ImageMagick `import -window WINDOW_ID FILE.png`. No crop, rescale,
  normalization, retouching or compositing was applied to these PNGs.
- Scenario: a disposable execution copy, synthetic P01/P02 roster, dummy audio,
  keyboard events sent to the application window. Startup commands: `gl_info`
  and `show_fps true`. Local Play, not a cross-machine network session.
- Prerequisites: checkpoint static self-review, native crypto/transport preflight,
  V3D scalar/batch renderer check, CI archive and full manifest verification.

| Wireframe | Path | Reachable state | SHA-256 |
| --- | --- | --- | --- |
| MENU-01-A | [MENU-01-A.png](MENU-01-A.png) | Ready Deathmatch setup with two selected players | `2d08fbe1c57e33d254bd84151dd0b6eb7272d668adf4442c1b4cb2ea0630c368` |
| CONS-01 | [CONS-01.png](CONS-01.png) | Console over menu after startup graphics diagnostics | `202d68b197b18eb6ea91facdd2d7467d42f830f64d3c9b4051d88053dd61d720` |
| PLAY-01 | [PLAY-01.png](PLAY-01.png) | Live shared arena, two players, water and elevators | `6acdd8cd97e6f3fc0ccdb651edac99203f1ea06aa373b9f5e361094b9e7193cc` |
| OVER-01 | [OVER-01.png](OVER-01.png) | Held-Tab two-player score overlay | `a9b4bf4e4f11b25ac2510247397aec3d3ddf19574da0a2810aa37254ab148695` |

Assessment: native panel layout, text and controls are visible without clipping;
the console retains the menu context; gameplay uses one complete shared arena;
the two score rows and SCORE heading are visible. Existing viewport and score
assertions passed on the gameplay captures. Return-to-menu pixels matched the
initial menu and the application exited successfully with empty stderr.
This does not establish all menu focus/content-extreme variants, other game modes
on V3D, the remaining screen/wireframe representatives, physical pointer/controller
input, audible audio or HDMI presentation. Visual gate remains **blocked**.
