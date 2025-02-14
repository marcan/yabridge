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

#include "utils.h"

#include <sched.h>
#include <xmmintrin.h>
#include <boost/process/environment.hpp>

namespace bp = boost::process;
namespace fs = boost::filesystem;

using namespace std::literals::string_view_literals;

/**
 * If this environment variable is set to `1`, then we won't enable the watchdog
 * timer. This is only necessary when running the Wine process under a different
 * namespace than the host.
 */
constexpr char disable_watchdog_timer_env_var[] = "YABRIDGE_NO_WATCHDOG";

fs::path get_temporary_directory() {
    bp::environment env = boost::this_process::environment();
    if (!env["XDG_RUNTIME_DIR"].empty()) {
        return env["XDG_RUNTIME_DIR"].to_string();
    } else {
        return fs::temp_directory_path();
    }
}

std::optional<int> get_realtime_priority() noexcept {
    sched_param current_params{};
    if (sched_getparam(0, &current_params) == 0 &&
        current_params.sched_priority > 0) {
        return current_params.sched_priority;
    } else {
        return std::nullopt;
    }
}

bool set_realtime_priority(bool sched_fifo, int priority) noexcept {
    sched_param params{.sched_priority = (sched_fifo ? priority : 0)};
    return sched_setscheduler(0, sched_fifo ? SCHED_FIFO : SCHED_OTHER,
                              &params) == 0;
}

std::optional<rlim_t> get_rttime_limit() noexcept {
    rlimit limits{};
    if (getrlimit(RLIMIT_RTTIME, &limits) == 0) {
        return limits.rlim_cur;
    } else {
        return std::nullopt;
    }
}

bool is_watchdog_timer_disabled() {
    // This is safe because we're not storing the pointer anywhere and the
    // environment doesn't get modified anywhere
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    const char* disable_watchdog_env = getenv(disable_watchdog_timer_env_var);

    return disable_watchdog_env && disable_watchdog_env == "1"sv;
}

bool pid_running(pid_t pid) {
    // With regular individually hosted plugins we can simply check whether the
    // process is still running, however Boost.Process does not allow you to do
    // the same thing for a process that's not a direct child if this process.
    // When using plugin groups we'll have to manually check whether the PID
    // returned by the group host process is still active. We sadly can't use
    // `kill()` for this as that provides no way to distinguish between active
    // processes and zombies, and a terminated group host process will always be
    // left as a zombie process. If the process is active, then
    // `/proc/<pid>/{cwd,exe,root}` will be valid symlinks.
    try {
        fs::canonical("/proc/" + std::to_string(pid) + "/exe");
        return true;
    } catch (const fs::filesystem_error&) {
        return false;
    }
}

ScopedFlushToZero::ScopedFlushToZero() noexcept {
    old_ftz_mode = _MM_GET_FLUSH_ZERO_MODE();
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
}

ScopedFlushToZero::~ScopedFlushToZero() noexcept {
    if (old_ftz_mode) {
        _MM_SET_FLUSH_ZERO_MODE(*old_ftz_mode);
    }
}

ScopedFlushToZero::ScopedFlushToZero(ScopedFlushToZero&& o) noexcept
    : old_ftz_mode(std::move(o.old_ftz_mode)) {
    o.old_ftz_mode.reset();
}

ScopedFlushToZero& ScopedFlushToZero::operator=(
    ScopedFlushToZero&& o) noexcept {
    old_ftz_mode = std::move(o.old_ftz_mode);
    o.old_ftz_mode.reset();

    return *this;
}
