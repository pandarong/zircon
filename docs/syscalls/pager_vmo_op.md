# zx_pager_vmo_op

## NAME

<!-- Updated by update-docs-from-abigen, do not edit. -->

pager_vmo_op - perform a pager operation on a vmo

## SYNOPSIS

<!-- Updated by update-docs-from-abigen, do not edit. -->

```
#include <zircon/syscalls.h>

zx_status_t zx_pager_vmo_op(zx_handle_t pager,
                            zx_handle_t pager_vmo,
                            uint32_t op,
                            uint64_t offset,
                            uint64_t length,
                            zx_handle_t aux_vmo,
                            uint64_t aux_offset);
```

## DESCRIPTION

Performs various pager operations on *pager_vmo*. The operation depends on *op*, which can have
the following values.

**ZX_PAGER_SUPPLY_PAGES** - Moves the pages of *aux_vmo* in the range [*aux_offset*, *aux_offset* +
*length*) to *pager_vmo* in the range [*offset*, *offset* + *length*). Any pages in *pager_vmo* in
the specified range will not be replaced; instead the corresponding pages from *aux_vmo* will be
freed. *aux_vmo* must have been created by [`zx_vmo_create()`], must have no clones or mappings, and
must be fully committed with no pinned pages in the specified range. After this operation, the
specified region of *aux_vmo* will be fully decommitted.

## RIGHTS

<!-- Updated by update-docs-from-abigen, do not edit. -->

*pager* must be of type **ZX_OBJ_TYPE_PAGER**.

*pager_vmo* must be of type **ZX_OBJ_TYPE_VMO**.

*aux_vmo* must be of type **ZX_OBJ_TYPE_VMO**.

## RETURN VALUE

`zx_pager_create()` returns ZX_OK on success, or one of the following error codes on failure. On
failure of **ZX_PAGER_SUPPLY_PAGES**, the specified range of *aux_vmo* may be either untouched
or fully decommitted.

## ERRORS

**ZX_ERR_BAD_HANDLE** *pager* or *pager_vmo* is not a valid handle, or *aux_vmo* is required but
not a valid handle.

**ZX_ERR_WRONG_TYPE** *pager* is not a pager handle, *pager_vmo* is not a vmo handle, or
*aux_vmo* is required but not a vmo handle.

**ZX_ERR_INVALID_ARGS**  *pager_vmo* is not a vmo created from *pager*, *op* is not a valid
opcode, *offset* or *size* is not page aligned, or *aux_offset* is needed but not page aligned.

**ZX_ERR_ACCESS_DENIED** *aux_vmo* is required but does not have **ZX_RIGHT_WRITE** and
**ZX_RIGHT_READ**.

**ZX_ERR_BAD_STATE** *aux_vmo* is not in a state where it can supply the required pages.

**ZX_ERR_OUT_OF_RANGE** the specified range in *pager_vmo* or *aux_vmo* is invalid.

**ZX_ERR_NO_MEMORY** failure due to lack of memory.

## SEE ALSO

 - [`zx_pager_create_vmo()`]

<!-- References updated by update-docs-from-abigen, do not edit. -->

[`zx_pager_create_vmo()`]: pager_create_vmo.md
[`zx_vmo_create()`]: vmo_create.md
