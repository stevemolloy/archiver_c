# MAX-IV archiver access

## Quick start

You will need a working postgresql installation.  Either use your OS's usual package installation method, or grab it [from here](https://www.postgresql.org/download/),

Set the `ARCHIVER_PASS` environment variable to the correct value (speak to me, or your nearest DB admin).

Finally, call the executable.

```console
$ ./bin/archiver --help
```

## Example call

```console
$ ./bin/archiver --start 2024-09-27T14:00:00 --end 2024-09-27T14:00:10 .*r1.*dcct.*inst.*
```
