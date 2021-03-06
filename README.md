Introduction
============

*mcmap* is a simple little glib/SDL utility that works as a proxy to
Minecraft, reads the connection traffic, and visualizes a map in a
window.

There are three main modes: a surface map (useful for general
navigation), a cross-section (single altitude level) map (useful for
finding things that are hidden in the ground) and a topographical
height-map (useful for... I don't know).

Also there are some miscellaneous features, like client-side
teleporting (though don't try it with health on).

Building
========

There's a Makefile; if you have SDL, glib and readline dev packages
installed, and pkg-config knows where to find them, a simple `make`
should suffice. (Although readline is actually just linked with
`-lreadline`.)

Usage
=====

Command line
------------

In the most basic form, `./mcmap -s 600x600 host:port` (or just `host`
for the default Minecraft port 25565).  The `-s` command line option
specifies the window size.  If you leave it out, the window will be
resizable, but resize events are not handled, so something bad will
probably happen.  (Fixing this is on the hypothetical TODO list.)

Do `./mcmap -h` for a list of options.

After starting up, connect with Minecraft.  The program will
automatically exit when you disconnect from within Minecraft.

Visuals
-------

At the default zoom level, the map window has one pixel for each block
in the world.  Block colors are hand-assigned, and should be more or
less sensible: edit in map.c if you wish.  Not all block types are
listed: unlisted blocks will be black.

The map is centered around the player (not scrollable at the moment),
and the position (and look direction) of the player is indicated with
a tiny little pink triangle marker.  Other players are indicated with
3x3 pink rectangles.

In the height-map, heights in the [64, 127] range are indicated with a
gradient from yellow and red, while heights in [0, 63] use a gradient
from blue to yellow.  (This was inspired by pynemap.)

Keys
----

Map modes are selected with the number keys:

* `1`: surface map, topmost non-air block.
* `2`: cross-section map, follows player altitude.
* `3`: cross-section map, doesn't follow the player.
* `4`: topographical map.

In addition, the up/down arrow keys can be used to set the altitude
level in the cross-section map modes.  This works also in mode 2,
though moving around will reset the altitude.

The page-up/page-down keys control the zoom level.  Each press of
page-up makes blocks one pixel larger.

Right-clicking a point on the map basically executes a //goto x z on
the clicked coordinates (or nearby).

Finally, the `n` key toggles "night mode" on: in this mode, blocks are
shaded based on how well lit (excluding sunlight) they are.

Chatting
--------

Thanks to a resounding demand made by a total of 0 people, you can
chat -- with fancy line editing, no less -- with everyone else
on your server through the console. It works exactly like you'd
expect, and requires no documentation. Although right now, if you
type a line that just happens to be too long (100 characters or
so), you'll get disconnected.

But why would you do a thing like that?

Commands
--------

Commands are prefixed with // -- chat packets starting with that
prefix will be removed from the stream and considered by mcmap.

* `//goto x z`: teleport into coordinates (x, z).
* `//jump ...`: control the jump list (run without arguments for more
  information).
* `//coords`: print your current coordinates.
* `//slap name`: transport you to a magical world of faeries and unicorns.
* `//save [directory]`: save the seen chunks to disk in the Minecraft
  world format.

The teleporting works by first moving the player directly up to height
y=128, then moving to (x, 128, z).  Passing through solid blocks is
not possible, so teleporting will only work if you have clear view of
the sky.  Additionally you'll fall down from height 128, which is
likely to prove fatal if you're playing on a server where health is
on.

(Also, don't teleport into some ridiculously far-away coordinates.
The server seems to generate all the terrain on your flight-path, and
will probably crash.)

Building the Win32 port
=======================

Explode the following files into `win/glib/` under the source root:

* http://ftp.gnome.org/pub/gnome/binaries/win32/glib/2.26/glib-dev_2.26.1-1_win32.zip
* http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/zlib-dev_1.2.5-2_win32.zip
* http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/gettext-runtime-dev_0.18.1.1-2_win32.zip

Also explode this into `win/SDL-1.2.14/` (the `SDL-1.2.14` part is
already in the tarball):

* http://www.libsdl.org/release/SDL-devel-1.2.14-mingw32.tar.gz

You also need the mingw stuff: on Ubuntu 10.10, the packages
`gcc-mingw32` and `mingw32-runtime`.

Then with some luck `make -f Makefile.win` will build a `mcmap.exe`.

To actually run it, you need the SDL/glib/zlib DLL files.  Start with:

* http://ftp.gnome.org/pub/gnome/binaries/win32/glib/2.26/glib_2.26.1-1_win32.zip
* http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/gettext-runtime_0.18.1.1-2_win32.zip
* http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/zlib_1.2.5-2_win32.zip

Explode those somewhere and collect all .dll files from the `bin/`
subdirectory.  Also take the `SDL.dll` from:

*  http://www.libsdl.org/release/SDL-1.2.14-win32.zip

These, placed in the same directory as `mcmap.exe`, should be enough.
