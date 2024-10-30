# MAX-IV archiver access

## Quick start

First build `libpq`
```console
$ cd thirdparty/postgresql-17.0/
$ ./configure
$ cd src/interfaces/libpq
$ make
```
Then, back at the top-level of the project.

```console
$ make
```

Set the ´ARCHIVER_PASS´ environment variable to the correct value (speak to me, or your nearest DB admin).

Finally, call the executable.


```console
$ ./bin/archiver --start 2024-09-27T14:00:00 --end 2024-09-27T14:00:10 .*r1.*dcct.*inst.*
```
