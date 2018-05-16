// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <stdint.h>
#include <zircon/compiler.h>
#include <zircon/syscalls.h>

namespace zx {

// TODO(maniscalco): Document these classes.

class duration {
public:
    constexpr duration() = default;

    explicit constexpr duration(zx_duration_t value)
        : value_(value) {}

    static constexpr duration infinite() { return duration(ZX_TIME_INFINITE); }

    constexpr zx_duration_t get() const { return value_; }

    duration operator+(duration other) const {
        zx_duration_t x = 0;
        if (unlikely(add_overflow(value_, other.value_, &x))) {
            if (x >= 0) {
                return duration(INT64_MIN);
            } else {
                return duration(ZX_TIME_INFINITE);
            }
        }
        return duration(x);
    }

    duration operator-(duration other) const {
        zx_duration_t x = 0;
        if (unlikely(sub_overflow(value_, other.value_, &x))) {
            if (x >= 0) {
                return duration(INT64_MIN);
            } else {
                return duration(ZX_TIME_INFINITE);
            }
        }
        return duration(x);
    }

    duration operator*(int64_t multiplier) const {
        zx_duration_t x = 0;
        if (unlikely(mul_overflow(value_, multiplier, &x))) {
            if ((value_ > 0 && multiplier > 0) || (value_ < 0 && multiplier < 0)) {
                return duration(ZX_TIME_INFINITE);
            } else {
                return duration(INT64_MIN);
            }
        }
        return duration(x);
    }

    constexpr duration operator/(int64_t divisor) const {
        return duration(value_ / divisor);
    }

    constexpr duration operator%(duration divisor) const {
        return duration(value_ % divisor.value_);
    }

    constexpr int64_t operator/(duration other) const {
        return value_ / other.value_;
    }

    duration& operator+=(duration other) {
        if (unlikely(add_overflow(value_, other.value_, &value_))) {
            if (value_ >= 0) {
                value_ = INT64_MIN;
            } else {
                value_ = ZX_TIME_INFINITE;
            }
        }
        return *this;
    }

    duration& operator-=(duration other) {
        if (unlikely(sub_overflow(value_, other.value_, &value_))) {
            if (value_ >= 0) {
                value_ = INT64_MIN;
            } else {
                value_ = ZX_TIME_INFINITE;
            }
        }
        return *this;
    }

    duration& operator*=(int64_t multiplier) {
        zx_duration_t x = 0;
        if (unlikely(mul_overflow(value_, multiplier, &x))) {
            if ((value_ > 0 && multiplier > 0) || (value_ < 0 && multiplier < 0)) {
                value_ = ZX_TIME_INFINITE;
                return *this;
            } else {
                value_ = INT64_MIN;
                return *this;
            }
        }
        value_ = x;
        return *this;
    }

    duration& operator/=(int64_t divisor) {
        value_ /= divisor;
        return *this;
    }

    constexpr bool operator==(duration other) const { return value_ == other.value_; }
    constexpr bool operator!=(duration other) const { return value_ != other.value_; }
    constexpr bool operator<(duration other) const { return value_ < other.value_; }
    constexpr bool operator<=(duration other) const { return value_ <= other.value_; }
    constexpr bool operator>(duration other) const { return value_ > other.value_; }
    constexpr bool operator>=(duration other) const { return value_ >= other.value_; }

    constexpr int64_t to_nsecs() const { return value_; }

    constexpr int64_t to_usecs() const { return value_ / ZX_USEC(1); }

    constexpr int64_t to_msecs() const { return value_ / ZX_MSEC(1); }

    constexpr int64_t to_secs() const { return value_ / ZX_SEC(1); }

    constexpr int64_t to_mins() const { return value_ / ZX_MIN(1); }

    constexpr int64_t to_hours() const { return value_ / ZX_HOUR(1); }

private:
    zx_duration_t value_ = 0;
};

class ticks {
public:
    constexpr ticks() = default;

    explicit constexpr ticks(zx_ticks_t value) : value_(value) {}

    // Constructs a tick object for the current tick counter in the system.
    static ticks now() { return ticks(zx_ticks_get()); }

    // Returns the number of ticks contained within one second.
    static ticks per_second() { return ticks(zx_ticks_per_second()); }

    // Acquires the number of ticks contained within this object.
    constexpr zx_ticks_t get() const { return value_; }

    constexpr ticks operator+(ticks other) const {
        return ticks(value_ + other.value_);
    }

    constexpr ticks operator-(ticks other) const {
        return ticks(value_ - other.value_);
    }

    constexpr ticks operator*(uint64_t multiplier) const {
        return ticks(value_ * multiplier);
    }

    constexpr ticks operator/(uint64_t divisor) const {
        return ticks(value_ / divisor);
    }

    constexpr uint64_t operator/(ticks other) const {
        return value_ / other.value_;
    }

    ticks& operator+=(ticks other) {
        value_ += other.value_;
        return *this;
    }

    ticks& operator-=(ticks other) {
        value_ -= other.value_;
        return *this;
    }

    ticks& operator*=(uint64_t multiplier) {
        value_ *= multiplier;
        return *this;
    }

    ticks& operator/=(uint64_t divisor) {
        value_ /= divisor;
        return *this;
    }

    constexpr bool operator==(ticks other) const { return value_ == other.value_; }
    constexpr bool operator!=(ticks other) const { return value_ != other.value_; }
    constexpr bool operator<(ticks other) const { return value_ < other.value_; }
    constexpr bool operator<=(ticks other) const { return value_ <= other.value_; }
    constexpr bool operator>(ticks other) const { return value_ > other.value_; }
    constexpr bool operator>=(ticks other) const { return value_ >= other.value_; }

private:
    zx_ticks_t value_ = 0;
};

class time {
public:
    constexpr time() = default;

    explicit constexpr time(zx_time_t value)
        : value_(value) {}

    static constexpr time infinite() { return time(ZX_TIME_INFINITE); }

    constexpr zx_time_t get() const { return value_; }

    zx_time_t* get_address() { return &value_; }

    duration operator-(time other) const {
        zx_duration_t x = 0;
        if (unlikely(sub_overflow(value_, other.value_, &x))) {
            if (x >= 0) {
                return duration(INT64_MIN);
            } else {
                return duration(ZX_TIME_INFINITE);
            }
        }
        return duration(x);
    }

    time operator+(duration delta) const {
        zx_time_t x = 0;
        if (unlikely(add_overflow(value_, delta.get(), &x))) {
            if (x >= 0) {
                return time(INT64_MIN);
            } else {
                return time(ZX_TIME_INFINITE);
            }
        }
        return time(x);
    }

    time operator-(duration delta) const {
        zx_time_t x = 0;
        if (unlikely(sub_overflow(value_, delta.get(), &x))) {
            if (x >= 0) {
                return time(INT64_MIN);
            } else {
                return time(ZX_TIME_INFINITE);
            }
        }
        return time(x);
    }

    time& operator+=(duration delta) {
        if (unlikely(add_overflow(value_, delta.get(), &value_))) {
            if (value_ >= 0) {
                value_ = INT64_MIN;
            } else {
                value_ = ZX_TIME_INFINITE;
            }
        }
        return *this;
    }

    time& operator-=(duration delta) {
        if (unlikely(sub_overflow(value_, delta.get(), &value_))) {
            if (value_ >= 0) {
                value_ = INT64_MIN;
            } else {
                value_ = ZX_TIME_INFINITE;
            }
        }
        return *this;
    }

    constexpr bool operator==(time other) const { return value_ == other.value_; }
    constexpr bool operator!=(time other) const { return value_ != other.value_; }
    constexpr bool operator<(time other) const { return value_ < other.value_; }
    constexpr bool operator<=(time other) const { return value_ <= other.value_; }
    constexpr bool operator>(time other) const { return value_ > other.value_; }
    constexpr bool operator>=(time other) const { return value_ >= other.value_; }

private:
    zx_time_t value_ = 0;
};

namespace clock {

static inline time get(zx_clock_t clock_id) {
    return time(zx_clock_get(clock_id));
}

} // namespace clock

constexpr inline duration nsec(int64_t n) { return duration(ZX_NSEC(n)); }

constexpr inline duration usec(int64_t n) { return duration(ZX_USEC(n)); }

constexpr inline duration msec(int64_t n) { return duration(ZX_MSEC(n)); }

constexpr inline duration sec(int64_t n) { return duration(ZX_SEC(n)); }

constexpr inline duration min(int64_t n) { return duration(ZX_MIN(n)); }

constexpr inline duration hour(int64_t n) { return duration(ZX_HOUR(n)); }

inline zx_status_t nanosleep(zx::time deadline) {
    return zx_nanosleep(deadline.get());
}

inline time deadline_after(zx::duration nanoseconds) {
    return time(zx_deadline_after(nanoseconds.get()));
}

} // namespace zx
