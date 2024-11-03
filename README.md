# MAX-IV archiver access

## Quick start

You will need a working postgresql installation.  Either use your OS's usual package installation method, or grab it [from here](https://www.postgresql.org/download/),

Build the executable from your console/terminal
```console
$ make
```

Set the `ARCHIVER_PASS` environment variable to the correct value (speak to me, or your nearest DB admin).

Finally, call the executable.

```console
$ ./bin/archiver --help
```

## Example call

```console
$ ./bin/archiver --start 2024-09-27T14:00:00 --end 2024-09-27T14:00:10 --file datafile .*r1.*dcct.*inst.*
```
# Building on Windows

I have succeeded in building this with [MSYS2](https://www.msys2.org/).

If your installation of MSYS2 is completely fresh, then you will need to do the following (in the MSYS2 environment).  This might need you to restart your MSYS2 environment.

```console
pacman -Syyu
```

Execute that command repeatedly until you get a message similar to, `Nothing else to do`.

In the MYSYS2 environment, execute the following to install the dependencies:

```console
pacman -S make
pacman -S mingw-w64-x86_64-gcc
pacman -S mingw-w64-x86_64-postgresql
```

If you want to be able to do git-related things from MSYS2, you should also install git.

```console
pacman -S git
```

Add the following lines to the `~/.bashrc` file.

```console
export PATH=$PATH:/c/msys64/mingw64/bin
export ARCHIVER_PASS=_GET_THIS_PASSWORD_FROM_ME_OR_YOUR_FRIENDLY_DB_ADMIN_
```

Restart MSYS2 to make sure these variables are active.

At this stage you have everything you need to build it.  Clone the repository, cd into the directory, and run `make`.

