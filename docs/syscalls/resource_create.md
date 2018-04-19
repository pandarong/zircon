# zx_resource_create

## NAME

resource_create - create a resource object

## SYNOPSIS

```
#include <zircon/syscalls.h>

zx_status_t zx_resource_create(zx_handle_t src_obj,
                               uint32_t kind,
                               uint64_t base,
                               size_t len,
                               uint32_t options,
                               const char* name,
                               size_t name_size,
                               zx_handle_t out_handle) {


```

## DESCRIPTION

**resource_create**() creates an resource object for use with other DDK
syscalls. Resources are typically handled out to bus drivers and rarely need to
be interacted with directly by drivers using driver protocols. Resource objects
provide access to the range *base* to *base*+*len* within the address space
defined by *kind*. Two special values for *kind* exist: **ZX_RSRC_KIND_ROOT**
and **ZX_SRC_KIND_HYPERVISOR**. These resources have no range associated with
them and are used as a privilege check.

*src_obj* must be a handle to a resource of *kind* **ZX_RSRC_KIND_ROOT**.

*kind* must be one of **ZX_RSRC_KIND_MMIO**, **ZX_RSRC_KIND_IRQ**,
**ZX_RSRC_KIND_IOPORT** (x86 only), **ZX_RSRC_KIND_ROOT**, or
**ZX_RSRC_KIND_HYPERVISOR**. The latter two must not be paired with non-zero
values for *base* and *len*, as they do not use an address space range.

For other kinds of resources *base* may be zero, but *len* must be non-zero.

*options* must be zero, or **ZX_RSRC_FLAG_EXCLUSIVE**. If
**ZX_RSRC_FLAG_EXCLUSIVE** is provided then the syscall will attempt to
exclusively reserve the requested address space region, preventing other
resources creation from overlapping with it as long as it exists.

*name* and *name_size* are optional, but the largest value allowed for
*name_size* is **ZX_MAX_NAME_LENGTH**. This name is provided for debugging /
tool use only and is not used by the kernel.

On success, a valid resource handle is returned in *out_handle*.

## RETURN VALUE

**resource_create**() returns **ZX_OK** on success. In the event of failure, a
negative error value is returned.

## ERRORS

**ZX_ERR_BAD_HANDLE** the *src_obj* handle is invalid

**ZX_ERR_WRONG_TYPE** the *src_obj* handle is not a resource handle.

**ZX_ERR_ACCESS_DENIED** The *src_obj* handle is not a resource of *kind* ZX_RSRC_KIND_ROOT.

**ZX_ERR_INVALID_ARGS** *kind* is invalid, *options* contains an invalid flag,
*name* is an invalid pointer, or *kind* is ZX_RSRC_KIND_ROOT or
ZX_RSRC_KIND_HYPERVISOR but *base* and *len* are not 0.

**ZX_ERR_NO_MEMORY** Failure due to lack of memory. There is no good way for
userspace to handle this (unlikely) error. In a future build this error will no
longer occur.

## SEE ALSO

[handle_close](handle_close.md),
[interrupt_create](interrupt_create.md),
[ioports_requeat](ioports_request.md),
[vmo_create_physical](vmo_create_physical.md)

