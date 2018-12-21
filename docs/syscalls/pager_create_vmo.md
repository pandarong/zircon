# zx_pager_create_vmo

## NAME

<!-- Updated by update-docs-from-abigen, do not edit. -->

pager_create_vmo - create a vmo backed by a pager

## SYNOPSIS

<!-- Updated by update-docs-from-abigen, do not edit. -->

```
#include <zircon/syscalls.h>

zx_status_t zx_pager_create_vmo(zx_handle_t pager,
                                zx_handle_t port,
                                uint64_t key,
                                uint64_t size,
                                uint32_t options,
                                zx_handle_t* out);
```

## DESCRIPTION

Creates a VMO backed by a pager object. *size* will be rounded up to the next page size
boundary, and *options* must be zero.

On success, the returned vmo has the same rights as a vmo created with `[zx_vmo_create()]`, as well
as having the same behavior with respect to **ZX_VMO_ZERO_CHILDREN**.

Page requests will be delivered to *port* when certain conditions are met. Those packets will have
*type* set to **ZX_PKT_TYPE_PAGE_REQUEST** and *key* set to the value provided to
`zx_pager_create_vmo()`. The packet's union is of type `zx_packet_page_request_t`:

```
typedef struct zx_packet_page_request {
    uint16_t command;
    uint16_t flags;
    uint32_t reserved0;
    uint64_t offset;
    uint64_t length;
    uint64_t reserved1;
} zx_packet_page_request_t;
```

*offset* and *length* are always page-aligned, and *flags* is always 0. The trigger and meaning of
the packet depends on *command*, which can take one of the following values:

**ZX_PAGER_VMO_READ**: Sent when an application accesses a vacant page in a pager's VMO. The
pager service should populate the range [offset, offset + length) in the registered vmo with
the **ZX_PAGER_SUPPLY_PAGES** [`zx_pager_vmo_op()`].

**ZX_PAGER_VMO_COMPLETE**: Sent when no more pager requests will be sent for the corresponding
VMO, either because of [`zx_pager_detach_vmo()`] or because no references to the VMO remain.

If *pager* is closed, then no more packets will be delivered to *port*, and any future accesses to
the returned vmo will behave as if [`zx_pager_detach_vmo()`] had been called (except no
**ZX_PAGER_VMO_COMPLETE** complete message will be delivered).

## RIGHTS

<!-- Updated by update-docs-from-abigen, do not edit. -->

*pager* must be of type **ZX_OBJ_TYPE_PAGER**.

*port* must be of type **ZX_OBJ_TYPE_PORT** and have **ZX_RIGHT_WRITE**.

## RETURN VALUE

`zx_pager_create()` returns ZX_OK on success, or one of the following error codes on failure.

## ERRORS

**ZX_ERR_INVALID_ARGS** *out* is an invalid pointer or NULL, or *options* is any value other than
0.

**ZX_ERR_BAD_HANDLE** *pager* or *port* is not a valid handle.

**ZX_ERR_ACCESS_DENIED** *port* does not have **ZX_RIGHT_WRITE**.

**ZX_ERR_WRONG_TYPE** *pager* is not a pager handle or *port* is not a port handle.

**ZX_ERR_OUT_OF_RANGE** the aligned size cannot be represented as a `uint64_t`

**ZX_ERR_NO_MEMORY**  Failure due to lack of memory.

## SEE ALSO

 - [`zx_pager_detach_vmo()`]
 - [`zx_pager_vmo_op()`]
 - [`zx_port_wait()`]

<!-- References updated by update-docs-from-abigen, do not edit. -->

[`zx_pager_detach_vmo()`]: pager_detach_vmo.md
[`zx_pager_vmo_op()`]: pager_vmo_op.md
[`zx_port_wait()`]: port_wait.md
