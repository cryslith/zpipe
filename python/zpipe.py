from subprocess import Popen, PIPE
from io import DEFAULT_BUFFER_SIZE
from threading import Thread, Lock


class ReadUntil(object):
    '''
    Input stream wrapper to facilitate reading until a sentinel value is
    seen
    '''

    def __init__(self, reader, readsize=DEFAULT_BUFFER_SIZE):
        '''
        reader: a BufferedIOBase stream to wrap
        readsize: the number of bytes to read into the buffer at once
        '''
        self.reader = reader
        self.buffer = []
        self.readsize = readsize

    def _more(self, c=None):
        '''
        read more data into the buffer, returning the new location of c
        if it appears in the new data
        '''
        curlen = len(self.buffer)
        m = self.reader.read1(self.readsize)
        if not m:
            raise EOFError
        self.buffer.extend(m)
        if c is not None:
            for i, b in enumerate(m):
                if b == c:
                    return curlen + i

    def readuntil(self, c, strip=True):
        '''
        Read until c is found, returning the resulting data.  If strip
        is false, include the delimiter if one was found.
        '''
        for i, b in enumerate(self.buffer):
            if b == c:
                break
        else:
            i = None
        while i is None:
            try:
                i = self._more(c)
            except EOFError:
                return self.read(len(self.buffer))
        result = self.read(i + 1)
        if strip:
            result = result.rstrip(bytes([c]))
        return result

    def read(self, n):
        '''Read and return n bytes.'''
        while n > len(self.buffer):
            try:
                self._more()
            except EOFError:
                result = self.buffer[:]
                self.buffer = []
                return result
        result = self.buffer[:n]
        self.buffer = self.buffer[n:]
        return bytes(result)


class ZNotice(object):
    '''A libzephyr ZNotice'''
    def __init__(self, charset, sender, cls, instance, recipient,
                 opcode, auth, message, time=None):
        '''
        Arguments:
        charset -- One of the bytestrings b'UNKNOWN', b'UTF-8', or
                   b'ISO-8859-1'
        sender -- A bytestring b'<username>@<realm>' or None
        cls -- bytestring (class)
        instance -- bytestring
        recipient -- bytestring b'<username>@<realm>' or None
        opcode -- bytestring
        auth -- boolean
        message -- bytestring
        time -- a bytestring b'<sec>:<usec>' where <sec> is the number
                of seconds since the epoch and usec is the number of
                microseconds since the last second
        '''
        self.charset = charset
        self.sender = sender
        self.cls = cls
        self.instance = instance
        self.recipient = recipient
        self.opcode = opcode
        self.auth = auth
        self.message = message
        self.time = time

    def to_zephyrgram(self):
        '''
        Decode the notice into a Zephyrgram, possibly resulting in
        decoding errors.
        '''
        encoding = {b'UTF-8': 'utf-8',
                    b'ISO-8859-1': 'latin-1'}.get(self.charset, 'ascii')
        if self.time is None:
            time = None
        else:
            secs, usecs = [int(x) for x in self.time.split(b':')]
            time = secs + usecs / 1000000
        return Zephyrgram(self.sender.decode(encoding),
                          self.cls.decode(encoding),
                          self.instance.decode(encoding),
                          self.recipient.decode(encoding),
                          self.opcode.decode(encoding),
                          self.auth,
                          [b.decode(encoding) for b in
                           self.message.split(b'\x00')],
                          time=time)

    def _to_zpipe(self):
        '''Serialize the notice for sending to a zpipe.'''
        return (b''.join(key + b'\x00' + value + b'\x00'
                         for key, value in
                         [(b'charset', self.charset),
                          (b'sender', self.sender),
                          (b'class', self.cls),
                          (b'instance', self.instance),
                          (b'recipient', self.recipient),
                          (b'opcode', self.opcode),
                          (b'auth', b'1' if self.auth else b'0'),
                          (b'message_length',
                           str(len(self.message)).encode())]
                         if value is not None) +
                b'\x00' + self.message)

    @classmethod
    def _from_zpipe(C, zp):
        '''Read a notice from an open zpipe.'''
        d = {}
        while True:
            key = zp.zpipe_out.readuntil(0)
            if not key:
                break
            value = zp.zpipe_out.readuntil(0)
            d[key] = value

        charset = d[b'charset']
        time = d[b'timestamp']
        sender = d[b'sender']
        cls = d[b'class']
        instance = d[b'instance']
        recipient = d[b'recipient']
        opcode = d[b'opcode']
        auth = int(d[b'auth'])
        message_len = int(d[b'message_length'])

        message = zp.zpipe_out.read(message_len)
        return C(charset, sender, cls, instance,
                 recipient, opcode, auth, message,
                 time=time)


class Zephyrgram(object):
    def __init__(self, sender, cls, instance, recipient,
                 opcode, auth, fields, time=None):
        '''
        Arguments:
        sender -- A string '<username>@<realm>' or None
        cls -- string (class)
        instance -- string
        recipient -- string '<username>@<realm>' or None
        opcode -- string
        auth -- boolean
        fields -- list string
        time -- number of seconds since the epoch
        '''
        self.time = time
        self.sender = sender
        self.cls = cls
        self.instance = instance
        self.recipient = recipient
        self.opcode = opcode
        self.auth = auth
        self.fields = fields

    def __repr__(self):
        return ('{}({!r}, {!r}, {!r}, {!r}, {!r}, {!r}, {!r}, '
                'time={time!r})'.format(type(self).__name__,
                                        self.sender,
                                        self.cls,
                                        self.instance,
                                        self.recipient,
                                        self.opcode,
                                        self.auth,
                                        self.fields,
                                        time=self.time))

    def to_znotice(self):
        '''Encode the zephyrgram into a ZNotice.'''
        if self.time is None:
            time = None
        else:
            time = b':'.join(str(int(x)).encode() for x in
                             [self.time, self.time % 1 * 1000000])

        return ZNotice(b'UTF-8',
                       None if self.sender is None else
                       self.sender.encode(),
                       self.cls.encode(),
                       self.instance.encode(),
                       None if self.recipient is None else
                       self.recipient.encode(),
                       self.opcode.encode(),
                       self.auth,
                       b'\x00'.join(f.encode() for f in self.fields),
                       time=time)


class ZPipe(object):
    def __init__(self, args, handler, raw=False):
        '''
        Create and open a zpipe.

        Arguments:
        args -- A list of arguments with which to invoke the zpipe
                program.  The first should be a path to the zpipe
                binary.
        handler -- A function to be called with each incoming zephyr
        raw -- Call the handler with raw ZNotice objects instead of
               Zephyrgrams
        '''
        self.zpipe = Popen(args, stdin=PIPE, stdout=PIPE)
        self.zpipe_out = ReadUntil(self.zpipe.stdout)
        target = self._zpipe_listen_notice if raw else self._zpipe_listen_zgram
        self.stdout_thread = Thread(target=target, args=(handler,))
        self.stdout_thread.start()
        self.zephyr_closed = False
        self.closed = False
        self.stdin_lock = Lock()

    def __enter__(self):
        return self

    def __exit__(self, *err):
        self.close()

    def zwrite_notice(self, notice):
        '''Send a ZNotice to the zpipe.'''
        if self.closed:
            raise ValueError('zwrite to closed zpipe')
        with self.stdin_lock:
            self.zpipe.stdin.write(b'command\x00zwrite\x00\x00')
            self.zpipe.stdin.write(notice._to_zpipe())
            self.zpipe.stdin.flush()

    def zwrite(self, zgram):
        '''Send a Zephyrgram to the zpipe.'''
        self.zwrite_notice(zgram.to_znotice())

    def subscribe(self, cls, instance='*', recipient='*', unsub=False):
        '''Add a subscription.'''
        if self.zephyr_closed:
            raise ValueError('zephyr closed')
        if isinstance(cls, str):
            cls = cls.encode()
        if isinstance(instance, str):
            instance = instance.encode()
        if isinstance(recipient, str):
            recipient = recipient.encode()
        with self.stdin_lock:
            self.zpipe.stdin.write(b'command\x00')
            self.zpipe.stdin.write(b'unsubscribe' if unsub else b'subscribe')
            self.zpipe.stdin.write(b'\x00\x00')
            self.zpipe.stdin.write(
                b''.join(key + b'\x00' + value + b'\x00'
                         for key, value in
                         [(b'class', cls),
                          (b'instance', instance),
                          (b'recipient', recipient)]))
            self.zpipe.stdin.write(b'\x00')
            self.zpipe.stdin.flush()

    def unsubscribe(self, *args):
        '''Remove a subscription.'''
        self.subscribe(*args, unsub=True)

    def close_zephyr(self):
        '''Close the zpipe's output stream.'''
        if self.zephyr_closed:
            return
        with self.stdin_lock:
            self.zpipe.stdin.write(b'command\x00close_zephyr\x00\x00')
            self.zpipe.stdin.flush()
        self.stdout_thread.join()
        self.zephyr_closed = True

    def close(self):
        '''Close the zpipe.'''
        if self.closed:
            return
        self.close_zephyr()
        with self.stdin_lock:
            self.zpipe.stdin.close()
        self.closed = True

    def _zpipe_listen_notice(self, notice_handler):
        while True:
            d = {}
            while True:
                key = self.zpipe_out.readuntil(0)
                if not key:
                    break
                value = self.zpipe_out.readuntil(0)
                d[key] = value
            try:
                typ = d[b'type']
            except KeyError:
                # no type = EOF
                return
            if typ == b'notice':
                t = Thread(target=notice_handler,
                           args=(self, ZNotice._from_zpipe(self)))
                t.start()
                continue
            # unknown type - ignore
            self.zpipe_out.read(int(d[b'length']))

    def _zpipe_listen_zgram(self, zgram_handler):
        def notice_handler(s, notice):
            try:
                zgram = notice.to_zephyrgram()
            except ValueError:
                # ignore decoding errors
                return
            zgram_handler(s, zgram)
        self._zpipe_listen_notice(notice_handler)


__all__ = ['ZNotice', 'Zephyrgram', 'ZPipe']
