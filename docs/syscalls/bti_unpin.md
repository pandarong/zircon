# zx_bti_unpin

## NAME

bti_unpin - unpin pages and revoke device access to them

## SYNOPSIS

```
#include <zircon/syscalls.h>

zx_status_t zx_bti_unpin(zx_handle_t bti, uint64_t* addrs, uint32_t addrs_len);
```

## DESCRIPTION

**bti_unpin**() unpins pages that were previously pinned by **zx_bti_pin**(),
and revokes the access that was granted by the pin call.

*addrs* and *addrs_len* must refer to an array with exactly the same elements
that were returned by a call to **zx_btin_pin**().

## RETURN VALUE

On success, **bti_unpin**() returns ZX_OK.
In the event of failure, a negative error value is returned.

## ERRORS

**ZX_ERR_BAD_HANDLE**  *bti* is not a valid handle.

**ZX_ERR_WRONG_TYPE**  *bti* is not a BTI handle.

**ZX_ERR_ACCESS_DENIED** *bti* does not have the *ZX_RIGHT_MAP*.

**ZX_ERR_INVALID_ARGS**  The requested range was not previously returned by
**zx_bti_pin**().

## SEE ALSO

[bti_create](bti_create.md),
[bti_pin](bti_pin.md).
