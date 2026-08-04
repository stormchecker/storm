#include "storm/utility/threads.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <thread>

#include "storm/io/file.h"

namespace storm::utility {

namespace detail {
static uint64_t num_threads = 0u;

uint64_t tryReadFromSlurm() {
    auto val = std::getenv("SLURM_CPUS_PER_TASK");
    if (val != nullptr) {
        char* end;
        auto i = std::strtoul(val, &end, 10);
        if (val != end) {
            STORM_LOG_WARN("Detected thread limitation via Slurm (max. " << i << " threads.)");
            return static_cast<uint64_t>(i);
        }
    }
    return 0;
}

uint64_t tryReadFromCgroups() {
    std::string const filename = "/sys/fs/cgroup/cpu.max";
    if (storm::io::fileExistsAndIsReadable(filename)) {
        std::ifstream inputFileStream;
        storm::io::openFile(filename, inputFileStream);
        std::string contents;
        storm::io::getline(inputFileStream, contents);
        storm::io::closeFile(inputFileStream);

        auto pos1 = contents.data();
        char* pos2;
        double quota = std::strtod(pos1, &pos2);
        if (pos1 != pos2 && quota > 0.0) {
            pos1 = pos2;
            double period = std::strtod(pos1, &pos2);
            if (pos1 != pos2 && period > 0.0) {
                auto res = static_cast<uint64_t>(std::ceil(quota / period));
                STORM_LOG_WARN("Detected thread limitation via cgroup (max. " << res << " threads, quota=" << quota << ", period=" << period << ").");
                return res;
            }
        }
    }
    return 0u;
}

}  // namespace detail

uint64_t getNumberOfThreads() {
    if (detail::num_threads == 0) {
        // try to obtain a sensible number of threads we can use
        auto numHardwareThreads = std::max(1u, std::thread::hardware_concurrency());
        auto numSlurmThreads = detail::tryReadFromSlurm();
        auto numCgroupsThreads = detail::tryReadFromCgroups();
        detail::num_threads = numHardwareThreads;
        for (auto i : {numSlurmThreads, numCgroupsThreads}) {
            if (i > 0 && i < detail::num_threads) {
                detail::num_threads = i;
            }
        }
    }
    return detail::num_threads;
}
}  // namespace storm::utility