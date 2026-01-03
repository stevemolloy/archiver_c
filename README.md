# MAX-IV archiver access

## Quick start

You will need a working postgresql installation.  Either use your OS's usual package installation method, or grab it [from here](https://www.postgresql.org/download/),

Build the project according to the instructions below.  Then set the `ARCHIVER_PASS` environment variable to the correct value (speak to me, or your nearest DB admin).

Finally, call the executable as follows.

```console
$ ./bin/archiver --help
```

```console
$ ./bin/archiver --start 2024-09-27T14:00:00 --end 2024-09-27T14:00:10 --file datafile .*r1.*dcct.*inst.*

```
### Linux
Build the executable from your console/terminal
```console
$ mkdir build
$ cd build
$ cmake ..
$ cmake --build
```

### Windows
I am not a Windows user, but I have had success building and running this within a Windows VM.

To do so I opened the folder in MS Visual Studio (note, **not** Visual Studio Code -- just Visual Studio).  It figured out the relevant configuration and allowed the project to be built.  You will get a few warnings about deprecated functions (for example `snprintf`) but these can be ignored.

After building you will have a working executable somewhere in the `out` folder.

### MacOS
I have no idea.  If you know, edit this file and make a pull request.

