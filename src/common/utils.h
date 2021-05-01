// yabridge: a Wine VST bridge
// Copyright (C) 2020-2021 Robbert van der Helm
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <optional>

#ifdef __WINE__
#include "../wine-host/boost-fix.h"
#endif
#include <boost/filesystem.hpp>

/**
 * The interval in seconds between synchronizing the Wine plugin host's audio
 * thread scheduling priority with the host's audio thread.
 *
 * @relates Vst2Bridge::last_audio_thread_priority_synchronization
 */
constexpr time_t audio_thread_priority_synchronization_interval = 10;

/**
 * When the `hide_daw` compatibility option is enabled, we'll report this
 * instead of the actual DAW's name. This can be useful when plugins are
 * hardcoded to behave differently in certain DAWs, and when that different
 * behaviour causes issues under Wine. An example of such a plugin is AAS
 * Chromaphone 3 when run under Bitwig.
 */
constexpr char product_name_override[] = "Get yabridge'd";
/**
 * When the `hide_daw` compatibility option is enabled, we'll report this
 * instead of the actual vendor's name in a VST2 plugin.
 */
constexpr char vendor_name_override[] = "yabridge";

// The cannonical overloading template for `std::visitor`, not sure why this
// isn't part of the standard library
template <class... Ts>
struct overload : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
overload(Ts...) -> overload<Ts...>;

/**
 * Return the path to the directory for story temporary files. This will be
 * `$XDG_RUNTIME_DIR` if set, and `/tmp` otherwise.
 */
boost::filesystem::path get_temporary_directory();

/**
 * Get the current thread's scheduling priority if the thread is using
 * `SCHED_FIFO`. Returns a nullopt of the calling thread is not under realtime
 * scheduling.
 */
std::optional<int> get_realtime_priority();

/**
 * Set the scheduling policy to `SCHED_FIFO` with priority 5 for this process.
 * We explicitly don't do this for wineserver itself since from my testing that
 * can actually increase latencies.
 *
 * @param sched_fifo If true, set the current process/thread's scheudling policy
 *   to `SCHED_FIFO`. Otherwise reset it back to `SCHWED_OTHER`.
 * @param priority The scheduling priority to use. The exact value usually
 *   doesn't really matter unless there are a lot of other active `SCHED_FIFO`
 *   background tasks. We'll use 5 as a default, but we'll periodically copy the
 *   priority set by the host on the audio threads.
 *
 * @return Whether the operation was successful or not. This will fail if the
 *   user does not have the privileges to set realtime priorities.
 */
bool set_realtime_priority(bool sched_fifo, int priority = 5);

/**
 * Check whether a process with the given PID is still active (and not a
 * zombie).
 */
bool pid_running(pid_t pid);

/**
 * A RAII wrapper that will temporarily enable the FTZ flag so that denormals
 * are automatically flushed to zero, returning to whatever the flag was
 * previously when it drops out of scope.
 */
class ScopedFlushToZero {
   public:
    ScopedFlushToZero();
    ~ScopedFlushToZero();

    ScopedFlushToZero(const ScopedFlushToZero&) = delete;
    ScopedFlushToZero& operator=(const ScopedFlushToZero&) = delete;

    ScopedFlushToZero(ScopedFlushToZero&&);
    ScopedFlushToZero& operator=(ScopedFlushToZero&&);

   private:
    /**
     * The previous FTZ mode. When we use this on the Wine side, this should
     * always be disabled. But, we'll make sure to do it correctly anyhow so we
     * don't accidentally end up disabling FTZ somewhere where it should be
     * enabled.
     */
    std::optional<unsigned int> old_ftz_mode;
};

/**
 * A helper to temporarily cache a value. Calling `ScopedValueCache::set(x)`
 * will return a guard object. When `ScopedValueCache::get()` is called while
 * this guard object is active, then `x` is returned. Otherwise a nullopt will
 * be returned.
 *
 * @note This class provides no thread safety guarantees. If thread safety is
 *   needed, then you should use mutexes around the getter and the setter.
 */
template <typename T>
class ScopedValueCache {
   public:
    ScopedValueCache() {}

    ScopedValueCache(const ScopedValueCache&) = delete;
    ScopedValueCache& operator=(const ScopedValueCache&) = delete;

    // Moving is impossible because of the guard
    ScopedValueCache(ScopedValueCache&&) = delete;
    ScopedValueCache& operator=(ScopedValueCache&&) = delete;

    /**
     * Return the cached value, if we're currently caching a value. Will return
     * a null pointer when this is not the case.
     */
    const T* get() const { return value ? &*value : nullptr; }

    /**
     * A guard that will reset the cached value on the `ScopedValueCache` when
     * it drops out of scope.
     */
    class Guard {
       public:
        Guard(std::optional<T>& cached_value) : cached_value(cached_value) {}
        ~Guard() {
            if (is_active) {
                cached_value.get().reset();
            }
        }

        Guard(const Guard&) = delete;
        Guard& operator=(const Guard&) = delete;

        Guard(Guard&& o) : cached_value(std::move(o.cached_value)) {
            o.is_active = false;
        }
        Guard& operator=(Guard&& o) {
            cached_value = std::move(o.cached_value);
            o.is_active = false;

            return *this;
        }

       private:
        bool is_active = true;
        std::reference_wrapper<std::optional<T>> cached_value;
    };

    /**
     * Temporarily cache `new_value`. This value will be cached as long as the
     * returned guard is in scope. This guard should not outlive the
     * `ScopedValueCache` object.
     *
     * @param new_value The cached value to store.
     *
     * @throw std::runtime_error When we are already caching a value.
     */
    Guard set(T new_value) {
        value = std::move(new_value);

        return Guard(value);
    }

   private:
    /**
     * The current value, if `set()` has been called and the guard is still
     * active.
     */
    std::optional<T> value;
};
