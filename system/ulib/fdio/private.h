// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <fuchsia/io/c/fidl.h>
#include <lib/fdio/limits.h>
#include <lib/fdio/vfs.h>
#include <errno.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <threads.h>
#include <zircon/types.h>

typedef struct fdio fdio_t;
typedef struct fdio_namespace fdio_ns_t;

// FDIO provides open/close/read/write io over various transports
// via the fdio_t interface abstraction.
//
// The PIPE protocol uses message ports as simple, no-flow-control
// io pipes with a maximum message size of ZX_PIPE_SIZE.
//
// The REMOTEIO protocol uses message ports to implement simple
// synchronous remoting of read/write/close operations.
//
// The NULL protocol absorbs writes and is never readable.

typedef struct fdio_ops {
    ssize_t (*read)(fdio_t* io, void* data, size_t len);
    ssize_t (*read_at)(fdio_t* io, void* data, size_t len, off_t offset);
    ssize_t (*write)(fdio_t* io, const void* data, size_t len);
    ssize_t (*write_at)(fdio_t* io, const void* data, size_t len, off_t offset);
    off_t (*seek)(fdio_t* io, off_t offset, int whence);
    zx_status_t (*misc)(fdio_t* io, uint32_t op, int64_t off, uint32_t maxreply,
                        void* data, size_t len);
    zx_status_t (*close)(fdio_t* io);
    zx_status_t (*open)(fdio_t* io, const char* path, uint32_t flags,
                        uint32_t mode, fdio_t** out);
    zx_status_t (*clone)(fdio_t* io, zx_handle_t* out_handles, uint32_t* out_types);
    zx_status_t (*unwrap)(fdio_t* io, zx_handle_t* out_handles, uint32_t* out_types);
    void (*wait_begin)(fdio_t* io, uint32_t events, zx_handle_t* handle,
                       zx_signals_t* signals);
    void (*wait_end)(fdio_t* io, zx_signals_t signals, uint32_t* events);
    ssize_t (*ioctl)(fdio_t* io, uint32_t op, const void* in_buf, size_t in_len,
                     void* out_buf, size_t out_len);
    ssize_t (*posix_ioctl)(fdio_t* io, int req, va_list va);
    zx_status_t (*get_vmo)(fdio_t* io, int flags, zx_handle_t* out);
    zx_status_t (*get_token)(fdio_t* io, zx_handle_t* out);
    zx_status_t (*get_attr)(fdio_t* io, fuchsia_io_NodeAttributes* out);
    zx_status_t (*set_attr)(fdio_t* io, uint32_t flags, const fuchsia_io_NodeAttributes* attr);
    zx_status_t (*sync)(fdio_t* io);
    zx_status_t (*readdir)(fdio_t* io, void* ptr, size_t max, size_t* actual);
    zx_status_t (*rewind)(fdio_t* io);
    zx_status_t (*unlink)(fdio_t* io, const char* path, size_t len);
    zx_status_t (*truncate)(fdio_t* io, off_t off);
    zx_status_t (*rename)(fdio_t* io, const char* src, size_t srclen,
                          zx_handle_t dst_token, const char* dst, size_t dstlen);
    zx_status_t (*link)(fdio_t* io, const char* src, size_t srclen,
                        zx_handle_t dst_token, const char* dst, size_t dstlen);
    zx_status_t (*get_flags)(fdio_t* io, uint32_t* out_flags);
    zx_status_t (*set_flags)(fdio_t* io, uint32_t flags);
    ssize_t (*recvfrom)(fdio_t* io, void* data, size_t len, int flags,
                        struct sockaddr* restrict addr, socklen_t* restrict addrlen);
    ssize_t (*sendto)(fdio_t* io, const void* data, size_t len, int flags,
                      const struct sockaddr* addr, socklen_t addrlen);
    ssize_t (*recvmsg)(fdio_t* io, struct msghdr* msg, int flags);
    ssize_t (*sendmsg)(fdio_t* io, const struct msghdr* msg, int flags);
    zx_status_t (*shutdown)(fdio_t* io, int how);
} fdio_ops_t;

// fdio_t ioflag values
#define IOFLAG_CLOEXEC              (1 << 0)
#define IOFLAG_SOCKET               (1 << 1)
#define IOFLAG_EPOLL                (1 << 2)
#define IOFLAG_WAITABLE             (1 << 3)
#define IOFLAG_SOCKET_CONNECTING    (1 << 4)
#define IOFLAG_SOCKET_CONNECTED     (1 << 5)
#define IOFLAG_NONBLOCK             (1 << 6)

// The subset of fdio_t per-fd flags queryable via fcntl.
// Static assertions in unistd.c ensure we aren't colliding.
#define IOFLAG_FD_FLAGS IOFLAG_CLOEXEC

typedef struct fdio {
    fdio_ops_t* ops;
    uint32_t magic;
    atomic_int_fast32_t refcount;
    int32_t dupcount;
    uint32_t ioflag;
} fdio_t;

// Lifecycle notes:
//
// Upon creation, fdio objects have a refcount of 1.
// fdio_acquire() and fdio_release() are used to upref
// and downref, respectively.  Upon downref to 0,
// fdio_free() is called, which poisons the object and
// free()s it.
//
// The close hook must be called before free and should
// only be called once.  In normal use, fdio objects are
// accessed through the fdio_fdtab, and when close is
// called they are removed from the fdtab and the reference
// that the fdtab itself is holding is released, at which
// point they will be free()'d unless somebody is holding
// a ref due to an ongoing io transaction, which will
// certainly fail doe to underlying handles being closed
// at which point a downref will happen and destruction
// will follow.
//
// dupcount tracks how many fdtab entries an fdio object
// is in.  close() reduces the dupcount, and only actually
// closes the underlying object when it reaches zero.

#define FDIO_MAGIC 0x4f49584d // FDIO

zx_status_t fdio_close(fdio_t* io);
zx_status_t fdio_wait(fdio_t* io, uint32_t events, zx_time_t deadline,
                      uint32_t* out_pending);

// Creates a pipe backed by a socket.
//
// Takes ownership of |socket|.
fdio_t* fdio_pipe_create(zx_handle_t socket);

// Wraps a socket with an fdio_t using socketpair io.
fdio_t* fdio_socketpair_create(zx_handle_t h);

// Creates an |fdio_t| for a VMO file.
//
// * |control| is an handle to the control channel for the VMO file.
// * |vmo| is the VMO that contains the contents of the file.
// * |offset| is the index of the first byte of the file in the VMO.
// * |length| is the number of bytes in the file.
// * |seek| is the initial seek offset within the file (i.e., relative to
//   |offset| within the underlying VMO).
//
// Always consumes |h| and |vmo|.
fdio_t* fdio_vmofile_create(zx_handle_t control, zx_handle_t vmo,
                            zx_off_t offset, zx_off_t length, zx_off_t seek);

// Creates an |fdio_t| from a Zircon socket object.
//
// Examines |socket| and determines whether to create a pipe, stream socket, or
// datagram socket.
//
// Always consumes |socket|.
zx_status_t fdio_from_socket(zx_handle_t socket, fdio_t** out_io);

// Creates an |fdio_t| from a Zircon channel object.
//
// The |channel| must implement the |fuchsia.io.Node| protocol. Uses the
// |Describe| method from the |fuchsia.io.Node| protocol to determine whether to
// create a remoteio or a vmofile.
//
// Always consumes |channel|.
zx_status_t fdio_from_channel(zx_handle_t channel, fdio_t** out_io);

// Wraps a socket with an fdio_t using socket io.
fdio_t* fdio_socket_create_stream(zx_handle_t s, int flags);
fdio_t* fdio_socket_create_datagram(zx_handle_t s, int flags);

// creates a message port and pair of simple io fdio_t's
int fdio_pipe_pair(fdio_t** a, fdio_t** b);

void fdio_free(fdio_t* io);

fdio_t* fdio_ns_open_root(fdio_ns_t* ns);

// io will be consumed by this and must not be shared
void fdio_chdir(fdio_t* io, const char* path);

// Wraps an arbitrary handle with a fdio_t that works with wait hooks.
// Takes ownership of handle unless shared_handle is true.
fdio_t* fdio_waitable_create(zx_handle_t h, zx_signals_t signals_in,
                             zx_signals_t signals_out, bool shared_handle);

// unsupported / do-nothing hooks shared by implementations
zx_status_t fdio_default_get_token(fdio_t* io, zx_handle_t* out);
zx_status_t fdio_default_set_attr(fdio_t* io, uint32_t flags, const fuchsia_io_NodeAttributes* attr);
zx_status_t fdio_default_sync(fdio_t* io);
zx_status_t fdio_default_readdir(fdio_t* io, void* ptr, size_t max, size_t* actual);
zx_status_t fdio_default_rewind(fdio_t* io);
zx_status_t fdio_default_unlink(fdio_t* io, const char* path, size_t len);
zx_status_t fdio_default_truncate(fdio_t* io, off_t off);
zx_status_t fdio_default_rename(fdio_t* io, const char* src, size_t srclen,
                                zx_handle_t dst_token, const char* dst, size_t dstlen);
zx_status_t fdio_default_link(fdio_t* io, const char* src, size_t srclen,
                              zx_handle_t dst_token, const char* dst, size_t dstlen);
zx_status_t fdio_default_get_flags(fdio_t* io, uint32_t* out_flags);
zx_status_t fdio_default_set_flags(fdio_t* io, uint32_t flags);
ssize_t fdio_default_read(fdio_t* io, void* _data, size_t len);
ssize_t fdio_default_read_at(fdio_t* io, void* _data, size_t len, off_t offset);
ssize_t fdio_default_write(fdio_t* io, const void* _data, size_t len);
ssize_t fdio_default_write_at(fdio_t* io, const void* _data, size_t len, off_t offset);
ssize_t fdio_default_recvfrom(fdio_t* io, void* _data, size_t len, int flags,
                              struct sockaddr* restrict addr,
                              socklen_t* restrict addrlen);
ssize_t fdio_default_sendto(fdio_t* io, const void* _data, size_t len,
                            int flags, const struct sockaddr* addr,
                            socklen_t addrlen);
ssize_t fdio_default_recvmsg(fdio_t* io, struct msghdr* msg, int flags);
ssize_t fdio_default_sendmsg(fdio_t* io, const struct msghdr* msg, int flags);
off_t fdio_default_seek(fdio_t* io, off_t offset, int whence);
zx_status_t fdio_default_get_attr(fdio_t* io, fuchsia_io_NodeAttributes* out);
zx_status_t fdio_default_misc(fdio_t* io, uint32_t op, int64_t off,
                              uint32_t arg, void* data, size_t len);
zx_status_t fdio_default_close(fdio_t* io);
zx_status_t fdio_default_open(fdio_t* io, const char* path, uint32_t flags,
                              uint32_t mode, fdio_t** out);
zx_status_t fdio_default_clone(fdio_t* io, zx_handle_t* handles, uint32_t* types);
ssize_t fdio_default_ioctl(fdio_t* io, uint32_t op, const void* in_buf,
                           size_t in_len, void* out_buf, size_t out_len);
void fdio_default_wait_begin(fdio_t* io, uint32_t events, zx_handle_t* handle,
                             zx_signals_t* _signals);
void fdio_default_wait_end(fdio_t* io, zx_signals_t signals, uint32_t* _events);
zx_status_t fdio_default_unwrap(fdio_t* io, zx_handle_t* handles, uint32_t* types);
zx_status_t fdio_default_shutdown(fdio_t* io, int how);
ssize_t fdio_default_posix_ioctl(fdio_t* io, int req, va_list va);
zx_status_t fdio_default_get_vmo(fdio_t* io, int flags, zx_handle_t* out);

typedef struct {
    mtx_t lock;
    mtx_t cwd_lock;
    bool init;
    mode_t umask;
    fdio_t* root;
    fdio_t* cwd;
    // fdtab contains either NULL, or a reference to fdio_reserved_io, or a
    // valid fdio_t pointer. fdio_reserved_io must never be returned for
    // operations.
    fdio_t* fdtab[FDIO_MAX_FD];
    fdio_ns_t* ns;
    char cwd_path[PATH_MAX];
} fdio_state_t;

extern fdio_state_t __fdio_global_state;

#define fdio_lock (__fdio_global_state.lock)
#define fdio_root_handle (__fdio_global_state.root)
#define fdio_cwd_handle (__fdio_global_state.cwd)
#define fdio_cwd_lock (__fdio_global_state.cwd_lock)
#define fdio_cwd_path (__fdio_global_state.cwd_path)
#define fdio_fdtab (__fdio_global_state.fdtab)
#define fdio_root_init (__fdio_global_state.init)
#define fdio_root_ns (__fdio_global_state.ns)


// Enable low level debug chatter, which requires a kernel that
// doesn't check the resource argument to zx_debuglog_create()
//
// The value is the default debug level (0 = none)
// The environment variable FDIODEBUG will override this on fdio init
//
// #define FDIO_LLDEBUG 1

#ifdef FDIO_LLDEBUG
void fdio_lldebug_printf(unsigned level, const char* fmt, ...);
#define LOG(level, fmt...) fdio_lldebug_printf(level, fmt)
#else
#define LOG(level, fmt...) do {} while (0)
#endif

void fdio_set_debug_level(unsigned level);

// Enable intrusive allocation debugging
//
//#define FDIO_ALLOCDEBUG

static inline void fdio_acquire(fdio_t* io) {
    LOG(6, "fdio: acquire: %p\n", io);
    atomic_fetch_add(&io->refcount, 1);
}

static inline void fdio_release(fdio_t* io) {
    LOG(6, "fdio: release: %p\n", io);
    if (atomic_fetch_sub(&io->refcount, 1) == 1) {
        fdio_free(io);
    }
}

#ifdef FDIO_ALLOCDEBUG
void* fdio_alloc(size_t sz);
#else
static inline void* fdio_alloc(size_t sz) {
    void* ptr = calloc(1, sz);
    LOG(5, "fdio: io: alloc: %p\n", ptr);
    return ptr;
}
#endif

// Returns an fd number greater than or equal to |starting_fd|, following the
// same rules as fdio_bind_fd. If there are no free file descriptors, -1 is
// returned and |errno| is set to EMFILE. The returned |fd| is bound to
// fdio_reserved_io that has no ops table, and must not be consumed outside of
// fdio, nor allowed to be used for operations.
int fdio_reserve_fd(int starting_fd);

// Assign the given |io| to the reserved |fd|. If |fd| is not reserved, then -1
// is returned and errno is set to EINVAL.
int fdio_assign_reserved(int fd, fdio_t *io);

// Unassign the reservation at |fd|. If |fd| does not resolve to a reservation
// then -1 is returned and errno is set to EINVAL, otherwise |fd| is returned.
int fdio_release_reserved(int fd);