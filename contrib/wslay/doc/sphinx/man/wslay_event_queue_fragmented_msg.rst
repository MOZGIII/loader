.. highlight:: c

wslay_event_queue_fragmented_msg
================================

SYNOPSIS
--------

#include <wslay/wslay.h>

.. c:function:: int wslay_event_queue_fragmented_msg(wslay_event_context_ptr ctx, const struct wslay_event_fragmented_msg *arg)

DESCRIPTION
-----------

:c:func:`wslay_event_queue_fragmented_msg` queues a fragmented message
specified in *arg*.
The *struct wslay_event_fragmented_msg* is defined as::

  union wslay_event_msg_source {
      int   fd;
      void *data;
  };

  struct wslay_event_fragmented_msg {
      uint8_t                             opcode;
      union wslay_event_msg_source        source;
      wslay_event_fragmented_msg_callback read_callback;
  };

.. c:type:: typedef ssize_t (*wslay_event_fragmented_msg_callback)(wslay_event_context_ptr ctx, uint8_t *buf, size_t len, const union wslay_event_msg_source *source, int *eof, void *user_data)

The *opcode* member is the opcode of the message.
The *source* member is an union and nomally it contains a "source" to
generate message data.
The *read_callback* is a callback function called by
:c:func:`wslay_event_send` to read message data from *source*.
The implementation of :c:type:`wslay_event_fragmented_msg_callback` must
store at most *len* bytes of data to *buf* and return the number of stored
bytes. If all data is read (i.e., EOF), set *\*eof* to 1.
If no data can be generated at the moment, return 0.
If there is an error, return -1 and
set error code ``WSLAY_ERR_CALLBACK_FAILURE``
using :c:func:`wslay_event_set_error`.

This function supports non-control messages only. For control frames,
use :c:func:`wslay_event_queue_msg` or :c:func:`wslay_event_queue_close`.

This function just queues a message and does not send it.
:c:func:`wslay_event_send` function call sends these queued messages.

RETURN VALUE
------------

:c:func:`wslay_event_queue_fragmented_msg` returns 0 if it succeeds, or returns
the following negative error codes:

**WSLAY_ERR_NO_MORE_MSG**
  Could not queue given message. The one of
  possible reason is that close control frame has been
  queued/sent and no further queueing message is not allowed.

**WSLAY_ERR_INVALID_ARGUMENT**
  The given message is invalid.

**WSLAY_ERR_NOMEM**
  Out of memory.

SEE ALSO
--------

:c:func:`wslay_event_queue_msg`,
:c:func:`wslay_event_queue_close`
