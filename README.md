# zpipe

`zpipe` is a program for interacting with zephyr over unix pipes.  It
listens on standard input for zephyr notices to be sent, and outputs
received notices on standard output.

# Input/output syntax

Most input and output follows the pattern

    <key>\x00<value>\x00<key>\x00<value>... ...<key>\x00<value>\x00\x00

where key-value pairs can be freely reordered.  This syntax will be
referred to as a key-value list.  Note that keys and values cannot
contain null bytes, and that the empty key is not permitted (though the
empty value is).

# Input

The following syntax is accepted on standard input:

    <command keys><command body>

where `<command keys>` is a key-value list accepting only the single
mandatory key `command`.  Valid commands are `zwrite`, `subscribe`,
`unsubscribe`, and `close_zephyr`.

## zwrite

The command `zwrite` is for sending a single zephyr notice.  Its body
takes the syntax

    <zwrite keys><message>

where `<zwrite keys>` is a key-value list accepting the following keys:

* `charset`: The character set with which the strings in the notice are
  encoded.  Valid values are "UTF-8", "ISO-8859-1", and "UNKNOWN".  The
  default is "UTF-8".
* `sender`: The sender of the notice.  Defaults to the result of calling
  `ZGetSender()`, which is usually the username of the current user.
* `class`: The zephyr class; defaults to "MESSAGE"
* `instance`: The zephyr instance; defaults to "personal"
* `recipient`: The recipient; defaults to the empty string.
* `opcode`: The zephyr opcode; defaults to the empty string.
* `auth`: Whether the message should be authenticated.  Valid values are
  "0" (for unauthenticated) and "1" (for authenticated).  Defaults to
  "1".
* `message_length`: The length of the message in bytes (decimal).
  Defaults to "0".

It is an error to supply neither `class` nor `recipient`.  `message`
must have the length specified in `message_length`.

## subscribe and unsubscribe

The commands `subscribe` and `unsubscribe` add and remove triples to and
from the subscription list.  Zephyr notices sent to triples on the
subscription list will be printed on standard output.  The body of these
commands is a single key-value list, which accepts the following keys:

* `class` (mandatory)
* `instance` (defaults to "*")
* `recipient` (defaults to "*")

Overlapping triples are treated separately.  Programs should avoid
subscribing to the same triple twice or unsubscribing from a triple to
which they have not previously subscribed, but it is not an error to do
so.

## close_zephyr

The command `close_zephyr` causes all subscriptions to be removed, and
standard output to be closed.  It is an error to issue the commands
`subscribe`, `unsubscribe`, or `close_zephyr` after this occurs.
`zpipe` exits normally only when this command has been issued, and it
receives EOF on standard input.  The body of this command must be empty.

# Output

The following syntax is emitted on standard output:

    <output keys><output body>

where `<output keys>` is a key-value list containing the following keys:

* `type`: the type of output; current existing types are `notice`
* `length`: the length of the output body

Programs should ignore outputs whose types they do not understand.

## notice

The output type `notice` is emitted when a zephyr notice is received on
one of the subscribed triples.  Its body is of the form:

    <notice keys><message>

Where `<notice keys>` is a key-value list containing at least the
following keys:

* `charset`: The character set with which the strings in the notice are
  encoded.  Valid values are "UTF-8", "ISO-8859-1", and "UNKNOWN".  Note
  that there is no guarantee that the strings in the notice are valid in
  the given charset.
* `sender`: The sender of the notice.
* `class`: The zephyr class on which the notice was received
* `instance`
* `recipient`
* `opcode`
* `auth`: Whether the message was authenticated.  Valid values are "0"
  (for unauthenticated) and "1" (for authenticated).
* `message_length`: The length of the message in bytes (decimal).

The above keys will always be present, but it is possible that other
keys will be present also.  Programs should ignore unexpected keys.
`message` will have the length specified in `message_length`.

# Authorship

`zpipe` copyright 2017 ikdc <ikdc@mit.edu>

`zpipe` is provided under the terms of the MIT license; see the LICENSE
file for details.

# Related work

`tzc` (Trivial Zephyr Client) is a similar program with an interface
designed around S-expressions, originally designed to work with emacs.
