# qcomix

Qt-based comic viewer

## Building from source

You need Qt 5, QuaZip, CMake and a recent C++ compiler.
Then execute `cmake .` and `make` in the source directory. When it is done compiling, you can optionally install it with `make install`.

## Configuration

Copy default.conf from this repo to `~/.config/qcomix/` and edit it to your liking (there are comments explaining every option).

On Windows, the location is `C:\Users\YourUser\AppData\Local\qcomix\`.

The thumbnail cache is usually in `~/.cache/qcomix/` (which is `C:\Users\YourUser\AppData\Local\qcomix\cache` on Windows).

You can have multiple config files on these locations and select between them with the `---profile myprofilename` command line switch (do not include the `.conf` extension). 