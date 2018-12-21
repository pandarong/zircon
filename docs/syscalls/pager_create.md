# zx_pager_create

## NAME

<!-- Updated by update-docs-from-abigen, do not edit. -->

pager_create - create a new pager object

## SYNOPSIS

<!-- Updated by update-docs-from-abigen, do not edit. -->

```
#include <zircon/syscalls.h>

zx_status_t zx_pager_create(uint32_t options, zx_handle_t* out);
```

## DESCRIPTION

`zx_pager_create()` creates a new pager object.

## RIGHTS

<!-- Updated by update-docs-from-abigen, do not edit. -->

TODO(ZX-2399)

## RETURN VALUE

`zx_pager_create()` returns ZX_OK on success, or one of the following error codes on failure.

## ERRORS

**ZX_ERR_INVALID_ARGS** *out* is an invalid pointer or NULL or *options* is
any value other than 0.

**ZX_ERR_NO_MEMORY** failure due to lack of memory.

## SEE ALSO

 - [`zx_pager_create_vmo()`]
 - [`zx_pager_detach_vmo()`]
 - [`zx_pager_vmo_op()`]

<!-- References updated by update-docs-from-abigen, do not edit. -->

[`zx_pager_create_vmo()`]: pager_create_vmo.md
[`zx_pager_detach_vmo()`]: pager_detach_vmo.md
[`zx_pager_vmo_op()`]: pager_vmo_op.md
