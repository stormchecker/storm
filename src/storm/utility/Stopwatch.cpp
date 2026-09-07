#include "Stopwatch.h"

namespace storm {
namespace utility {

Stopwatch::Stopwatch(bool startNow)
    : accumulatedTime(std::chrono::nanoseconds::zero()), isStopped(true), startOfCurrentMeasurement(std::chrono::nanoseconds::zero()) {
    if (startNow) {
        start();
    }
}

Stopwatch::SecondType Stopwatch::getTimeInSeconds() const {
    auto time = accumulatedTime;
    if (!this->stopped()) {
        time += std::chrono::high_resolution_clock::now() - startOfCurrentMeasurement;
    }
    return std::chrono::duration_cast<std::chrono::seconds>(time).count();
}

Stopwatch::MilisecondType Stopwatch::getTimeInMilliseconds() const {
    auto time = accumulatedTime;
    if (!this->stopped()) {
        time += std::chrono::high_resolution_clock::now() - startOfCurrentMeasurement;
    }
    return std::chrono::duration_cast<std::chrono::milliseconds>(time).count();
}

Stopwatch::NanosecondType Stopwatch::getTimeInNanoseconds() const {
    return accumulatedTime.count();
}

void Stopwatch::addToTime(std::chrono::nanoseconds timeNanoseconds) {
    accumulatedTime += timeNanoseconds;
}

void Stopwatch::add(Stopwatch const& other) {
    STORM_LOG_WARN_COND(other.stopped(), "Expected stopped watch.");
    accumulatedTime += other.accumulatedTime;
}

void Stopwatch::stop() {
    STORM_LOG_WARN_COND(!isStopped, "Stopwatch is already paused.");
    isStopped = true;
    accumulatedTime += std::chrono::high_resolution_clock::now() - startOfCurrentMeasurement;
}

void Stopwatch::start() {
    STORM_LOG_WARN_COND(isStopped, "Stopwatch is already running.");
    isStopped = false;
    startOfCurrentMeasurement = std::chrono::high_resolution_clock::now();
}

void Stopwatch::reset() {
    accumulatedTime = std::chrono::nanoseconds::zero();
    isStopped = true;
}

void Stopwatch::restart() {
    reset();
    start();
}

bool Stopwatch::stopped() const {
    return isStopped;
}

std::ostream& operator<<(std::ostream& out, Stopwatch const& stopwatch) {
    char oldFillChar = out.fill('0');
    out << stopwatch.getTimeInSeconds() << "." << std::setw(3) << (stopwatch.getTimeInMilliseconds() % 1000) << "s";
    out.fill(oldFillChar);
    return out;
}

}  // namespace utility
}  // namespace storm
