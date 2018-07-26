# DMA Guide

Direct memory access (DMA) is a feature that allows hardware to access memory without CPU
intervention. A driver provides a device-visible memory address to the hardware to enable it
to perform DMA.

Zircon drivers generally operate on virtual addresses, which represent memory mapped into
the driver process's address space, or handles to VMOs, which may or may not be mapped. The
system memory device is addressed by physical addresses, and the Zircon kernel's VM system
is responsible for managing the mapping between these types of addresses.

Devices that are capable of DMA operate on device addresses. On some systems they are the
same as the physical address. On more complex systems, a piece of hardware called the IOMMU
restricts the memory visible to devices, and provide facilities to map between physical
addresses and device addresses for each device or subset of devices. The Zircon kernel is
also responsible for managing this mapping.

## Bus Transaction Initiator

The [Bus Transaction Initiator (BTI) kernel object](../objects/bus_transaction_initiator.md)
abstracts the model and provides API to obtain device-visible addresses from VMOs.

The driver obtains a handle to the BTI that manages its device by calling an upstream
device's protocol API. For example, **pci_get_bti()** returns a handle to a PCI device's
BTI. The equivalent API for a platform device is **pdev_get_api()**.

The driver calls [**zx_bti_pin()**](../syscalls/bti_pin.md) on a VMO to grant the device
access to the VMO's pages and to obtain addresses to those pages that are suitable for use
by the device. By default, each address returned represent a single page, but larger
chunks may be requested. See the documentation for **zx_bti_pin()** for more details.

The permissions specified by the *options* argument are from the perspective of the
device. For example, for a block device write operation, the device reads from memory
page and therefore the driver specifies *ZX_BTI_PERM_READ*, and vice versa for block
device read operations.

**zx_bti_pin()** returns a Pinned Memory Token upon success. The driver must call
[**zx_pmt_unpin()**](../syscalls/pmt_unpin.md) when the device is done with the
memory transaction to unpin and revoke access to the memory pages by the device.

## Cache Coherency

Some architectures have caches that are not DMA-coherent. On these systems, the driver
must flush or clean the cache to ensure no stale data is being accessed. For a VMO,
the operation is implemented by the cache operations in
[**zx_vmo_op_range()**](../syscalls/vmo_op_range.md). [**zx_cache_flush()**] implements
the same operations on a virtual address range.

Before a device memory read, the cache must be made consistent by flushing the cache.
Before a device memory write, the cache must be invalidated, so the process does not
read stale data from the cache.

# Contiguous Buffers

# VMAR
