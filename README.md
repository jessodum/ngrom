# New GROM - Simple Genesis ROM SMD to BIN converter
Based on the GROM 0.75 source code by Bart Trzynadlowski, 2000, which I found [here](https://www.zophar.net/utilities/segautil/grom.html) (Zophar's Domain).

## Why New GROM?
_Here's the story of why I created this upgraded version of GROM:_

I had a couple of Genesis (Mega Drive) ROMs in SMD format that I just couldn't get to behave in any emulator.  All of my other ROMs were in the BIN format and they worked just fine.  Doing some Googling led me to Bart's GROM code (see the link above), which appealed to me for its small size and simplicity.  After downloading, compiling, and running it on one of those SMD ROM files... I got a segmentation fault!

Ok, well, the source code is from the turn of the century and written in some quick and dirty C with some (IMO) unsafe coding practices.  I'm a C++ guy, so if I'm going to fix this code, I'm going to re-write it in a way I feel comfortable with.

Thus NGROM is born and raised.

(BTW, once I completed my rewrite, it worked perfectly on converting my SMD ROMs to BIN, which run great).

And now, I'm sharing my code with you! 😉

## Compiling
**Disclaimer:** I've only compiled on my system (Linux x86_64 / Fedora 39).  It works on my machine 😄.

I.e. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND...

Since it's only one source file and the Makefile is simple enough, you can probably adjust it on your own if it doesn't work for you out of the box.  Since the Makefile is was written specifically for (my) Linux, it very much _will not_ work out of the box for, say, Windows.  See the [dependencies](#dependencies) below for what you'll need to do your own compiling.

### Dependencies
- **C++ compiler** (e.g., g++)
- **Qt5 Core** libs and dev (headers) packages*
- _(Optional)_ **GNU make**

*Note: I like Qt's `QCommandLineParser`, thus the need for the Qt5 Core library.  The `QFileInfo` class came in handy, too.  This utility is still just a command line executable, not a GUI.

## Potential Enhancements
- Reverse conversion (BIN to SMD)
- GitHub Actions to compile release for multiple platforms
