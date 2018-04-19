# Resource

## NAME

resource - Hierarchical resource rights and accounting

## SYNOPSIS

A resource is an immutable object that is used to validate access to syscalls
that create objects backed by address space, or permit access to address space.
These include [vm objects](vm_object.md), [interrupts](interrupts.md), and x86
ioports.

## DESCRIPTION

Resources are used to gate access to specific regions of address space and are
required to create VMOs and IRQs, as well as accessing x86 ioports.

A resource object consists of a single resource *kind*, and an inclusive *base
address* and *length* tuple specifying the accessible range within the address
space. These objects are immutable after creation. Valid *kind*  values are
ROOT, MMIO, IOPORT, and IRQ. Resources are hierarchical in that new resources
can only be created through use of the root resource. An initial root resource
is created by the kernel during boot and handed off to userspace's userboot
process.

Resource allocations can be either *shared* or *exclusive*. A shared resource
grants the permission to access the given address space, but does not reserve
that address space exclusively for the owner of the resource. An exclusive
resource grants access to the region to only the holder of tee exclusive
resource.  Exclusive and shared resource ranges may not overlap.

Resources are lifecycle tracked and upon the last handle being closed will be
freed. In the case of exclusive resources this means the given address range
will be released back to the allocator for the given *kind* of resource. Objects
created through a resource do not hold a reference to a the resource and thus do
not keep it alive.

## NOTES

Resources are typically private to the DDK and platform bus drivers. Presently,
this means ACPI and pdev hold the root resource respectively and hand out more
fine-grained resources to other drivers.

## SYSCALLS

[interrupt_create](../syscalls/interrupt_create.md),
[ioports_requeat](../syscalls/ioports_request.md),
[resource_create](../syscalls/resource_create.md),
[vmo_create_physical](../syscalls/vmo_create_physical.md)
