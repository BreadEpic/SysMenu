# SysMenu

I always thought that it was a shame the switch didnt have home menu music. So i forked systune to make real home music.

sys-tune is a great background audio player, but it isn't *home menu music*: you have to switch
it on by hand, and it keeps playing straight over your games. SysMenu changes that.

- **It starts on its own.** Music begins at boot, no overlay needed.
- **It gets out of the way.** A game starts, the music stops. Close the game and it comes back.
- **It comes back when you press HOME.** A game that is only suspended behind the HOME Menu
  counts as the HOME Menu, so the music returns the way it did on the 3DS and Wii U.
- **It knows about sleep.** Pauses before the console sleeps, picks back up on the lock screen.

Everything is configurable from the Tesla overlay under **Misc → Home menu music**.

## Installing

1. Build (see below) or grab the release zip, and extract it to the root of your SD card.
2. Put `.mp3`, `.flac`, `.wav` or `.wave` files in a `/music` folder on the SD card.
3. Reboot.

That's it. `/music` is used automatically, so there is nothing to set up for the common case.
To use a different folder or a single file, open the overlay's music browser, highlight a
folder or file and press **ZR** to set it as the start up item.

You still need [Tesla Menu](https://github.com/WerWolv/Tesla-Menu) and
[nx-ovlloader](https://github.com/WerWolv/nx-ovlloader) for the overlay, same as sys-tune.

## Where the music plays

With the default settings:

| Situation | Music |
| --- | --- |
| HOME Menu | plays |
| System Settings, eShop, Album, News | plays |
| Lock screen (the wake up screen) | plays |
| HOME pressed over a running game | plays |
| Actually playing a game | silent |
| Console asleep | silent |
| Headphones unplugged | silent |

The overlay has this same table worked out from *your* settings under
**Home menu music → Where does music play?**, so you never have to guess.

### About the lock screen

The lock screen (the "press it three times" wake up screen) is drawn by the HOME Menu, and
your game stays suspended behind it, so SysMenu treats it as the HOME Menu and plays. While
the console is genuinely asleep nothing can play at all: the system cuts audio on the way
down. Turn **Play on lock screen** off if you would rather stay silent until you unlock.

**Wake delay** holds playback for a moment after waking so the first samples don't crackle
while audio services are still coming back up. 1.5s by default.

## Tesla overlay options

Under **Misc → Home menu music**:

| Option | What it does |
| --- | --- |
| Home menu only | Pause as soon as a game takes over. Turn off to play everywhere, like stock sys-tune. |
| Play over suspended games | Keep playing when a game is only suspended behind the HOME Menu. |
| Start at boot | Begin playing at boot instead of waiting for you to press play. |
| Pause on sleep | Pause before the console sleeps. |
| Play on lock screen | Resume once the console wakes up. |
| Wake delay | How long to wait after waking before resuming. |
| Pause when unplugged | Pause when the headphones come out. |

The per game toggle on the main page (**Music in …**) still wins over all of these, so you
can force music on for one specific game while everything else stays quiet, or the reverse.

The HOME Menu now gets its own volume slider too, which is handy for ducking the menu's
click and pop sound effects underneath your music.

## config.ini

Settings live in `/config/sys-tune/config.ini` and match the overlay one for one.

```ini
[config]
home_menu_only = 1            ; pause once a game takes over
focus_detect = 1              ; a suspended game counts as the home menu
autoplay = 1                  ; start playing at boot
pause_on_sleep = 1            ; pause before sleeping
resume_on_wake = 1            ; resume on the lock screen
wake_delay_ms = 1500          ; delay after waking, milliseconds
pause_on_headphone_unplug = 1 ; pause when headphones are pulled out
load_path =                   ; start up file or folder, empty means /music
```

## How "play over suspended games" works

Nothing in the system tells a sysmodule whether a game is suspended behind the HOME Menu.
Applet IPC could answer it, but a sysmodule can't use those services. What the system *does*
record is the play log, which gets an `in_focus` / `out_of_focus` entry on every transition,
so SysMenu reads the most recent entry for the running game through `pdm:qry` and treats
"out of focus" as being back at the HOME Menu. The approach is
[TotalJustice's](https://github.com/ITotalJustice/sys-tune/tree/applet_focus_detect); this is
a tidied up port of it.

It is a read-only query on data the console already keeps, but it is still an indirect
signal, so it has rough edges:

- Games can open applets themselves (keyboard, amiibo, controller screen, in-game manual,
  the web browser). Those all count as *lost focus*, so the ones a game can open are
  explicitly ignored and the music stays quiet.
- Launching homebrew through hbmenu briefly drops focus. There is a 0.5s grace period before
  a focus loss counts, which covers most NROs; a very large one may blip the music on for a
  moment while it loads.
- If `pdm:qry` can't be opened the feature simply switches itself off, the overlay says
  **Unavailable**, and a suspended game keeps counting as in-game. Nothing breaks.

Turn **Play over suspended games** off if you'd rather not use it at all.

## Building

```sh
git clone --recursive https://github.com/BreadEpic/SysMenu
cd SysMenu
make dist
```

Needs devkitPro with devkitA64, libnx and switch-tools. `make dist` writes a ready to copy
SD card layout to `dist/` plus a zip.

If you cloned without `--recursive`, run `git submodule update --init` to fetch libtesla.

## Is this safe?

A sysmodule runs from the SD card and cannot touch NAND, so this cannot brick a console. The
realistic worst case is that the sysmodule fails to start and Atmosphère throws an error on
boot. The fix for that is to put the SD card in a PC and delete
`/atmosphere/contents/4200000000000000/`; the console then boots as normal without SysMenu.

Optional services are all handled as optional here on purpose: if `psc:m` (sleep) or
`pdm:qry` (focus) can't be opened, that feature turns itself off rather than taking the
sysmodule down with it.

## Credits

- [HookedBehemoth](https://github.com/HookedBehemoth/sys-tune) for sys-tune.
- [TotalJustice](https://github.com/ITotalJustice/sys-tune) for the audout rework, the config
  system and the applet focus detection this builds on.
- [mackron](http://mackron.github.io/) for the [audio decoders](https://github.com/mackron/dr_libs/).
- [WerWolv](https://werwolv.net/) for libtesla.

## Info for developers

The IPC interface lives in [`/ipc/`](/ipc/) and the overlay uses those bindings. API version 5
adds the home menu music commands; the overlay refuses to run against a mismatched sysmodule.
