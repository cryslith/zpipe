# zpipe.py

`zpipe.py` provides Python 3 bindings for interfacing with zpipe.  See
the autogenerated documentation for details, and the `zpipe_example.py`
example file.  Typical usage:

    def handler(zp, zgram):
        ...

    with ZPipe(['/path/to/zpipe'], handler) as zp:
        zp.subscribe('some_class')
        ...
        zp.zwrite(Zephyrgram(...))
