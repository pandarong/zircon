# Pager

## NAME

pager - Mechanism for userspace paging

## SYNOPSIS

Pagers provide a mechanism for a userspace process to provide demand paging for VMOs.

## DESCRIPTION

A pager object allows a userspace pager service (typically a filesystem) to create VMOs that serve
as in-memory caches of some external data source. The pager service is responsible for supplying
data to the VMOs to fulfill page requests from the kernel based on accesses to the VMOs. The kernel
does not do prefetching - it is the responsibility of the pager service to implement any applicable
prefetching.

It is possible for a single pager to simultaniously back multiple vmos. It is also possible for
multiple independent pager objects to exist simultaniously.

Creating a pager is not a privileged operation. However, the default behavior of syscalls which
operate on VMOs is to fail if the operation would require blocking on IPC back to a userspace
process so, applications generally need to be aware of when they are operating on pager-backed
VMOs. This means that  that services which provide pager-backed VMOs to clients should be explicit
about doing so as part of their API.

TODO(stevensd): Update description once writeback is implemented.

## SEE ALSO

+ [vm_object](vm_object.md) - Virtual Memory Objects

## SYSCALLS

+ [pager_create](../syscalls/pager_create.md) - create a new pager object
+ [pager_create_vmo](../syscalls/pager_create_vmo.md) - create a vmo backed by a pager
+ [pager_detach_vmo](../syscalls/pager_detach_vmo.md) - detaches a pager from a vmo
+ [pager_vmo_op](../syscalls/pager_vmo_op.md) - perform a pager operation on a vmo
