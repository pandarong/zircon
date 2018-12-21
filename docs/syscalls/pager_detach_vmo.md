# zx_pager_detach_vmo

## NAME

<!-- Updated by update-docs-from-abigen, do not edit. -->

pager_detach_vmo - detaches a vmo from a pager

## SYNOPSIS

<!-- Updated by update-docs-from-abigen, do not edit. -->

```
#include <zircon/syscalls.h>

zx_status_t zx_pager_detach_vmo(zx_handle_t pager, zx_handle_t vmo);
```

## DESCRIPTION

Detaching *vmo* from *pager* causes the kernel to stop queuing page requests for the vmo. Subsequent
accesses which would have generated page requests will instead fail.

Note that there may still be pending page requests that need to be handled. The pager service
should continue to service requests until a **ZX_PAGER_VMO_COMPLETE** request is recieved.

## RIGHTS

<!-- Updated by update-docs-from-abigen, do not edit. -->

*pager* must be of type **ZX_OBJ_TYPE_PAGER**.

*vmo* must be of type **ZX_OBJ_TYPE_VMO**.

## RETURN VALUE

`zx_pager_detach_vmo()` returns ZX_OK on success, or one of the following error codes on failure.

## ERRORS

**ZX_ERR_BAD_HANDLE** *pager* or *vmo* is not a valid handle.

**ZX_ERR_WRONG_TYPE** *pager* is not a pager handle or *vmo* is not a vmo handle.

**ZX_ERR_INVALID_ARGS**  *vmo* is not a vmo created from *pager*.

## SEE ALSO

 - [`zx_pager_create_vmo()`]

<!-- References updated by update-docs-from-abigen, do not edit. -->

[`zx_pager_create_vmo()`]: pager_create_vmo.md
