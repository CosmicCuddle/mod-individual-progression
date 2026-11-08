// F14 REVIEW-ONLY deterministic interleavings of optional actor-publication
// generation lease. The real IP producer is checked separately by source CI.
// No AzerothCore objects, actual bot logins, SQL or client calls here.
#include <atomic>
#include <cstdint>
#include <iostream>
#include <limits>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>

class PublicationModel
{
public:
    struct Observed
    {
        std::uint64_t generation = 0;
        bool enabled = false;
    };

    Observed BeginLoginRead() const
    {
        std::shared_lock<std::shared_mutex> lock(_lock);
        return {_generation.load(std::memory_order_acquire),
                _enabled.load(std::memory_order_acquire)};
    }

    void BeginReload()
    {
        std::unique_lock<std::shared_mutex> lock(_lock);
        _enabled.store(false, std::memory_order_release);
        auto const generation = _generation.load(std::memory_order_relaxed);
        if (generation != std::numeric_limits<std::uint64_t>::max())
            _generation.store(generation + 1, std::memory_order_release);
        _published = false;
    }

    void FinishReload(bool ipEnabled)
    {
        std::unique_lock<std::shared_mutex> lock(_lock);
        if (_generation.load(std::memory_order_acquire) ==
            std::numeric_limits<std::uint64_t>::max())
            return;
        _enabled.store(ipEnabled, std::memory_order_release);
    }

    bool PublishVerifiedLogin(Observed const& observed)
    {
        std::unique_lock<std::shared_mutex> lock(_lock);
        if (_enabled.load(std::memory_order_acquire) &&
            observed.enabled &&
            observed.generation == _generation.load(std::memory_order_acquire))
        {
            _published = true;
            return true;
        }
        return false;
    }

    bool HasPublication() const
    {
        std::shared_lock<std::shared_mutex> lock(_lock);
        return _published && _enabled.load(std::memory_order_acquire);
    }

    void ForTestSetGeneration(std::uint64_t generation)
    {
        std::unique_lock<std::shared_mutex> lock(_lock);
        _generation.store(generation, std::memory_order_release);
    }

private:
    mutable std::shared_mutex _lock;
    std::atomic<bool> _enabled{false};
    std::atomic<std::uint64_t> _generation{0};
    bool _published = false;
};

static void Assert(bool condition, char const* test, std::uint64_t& checks)
{
    ++checks;
    if (!condition)
        throw std::runtime_error(test);
}

int main()
{
    std::uint64_t checks = 0;
    PublicationModel model;
    Assert(!model.BeginLoginRead().enabled, "default OFF", checks);
    model.BeginReload();
    model.FinishReload(true);

    // Ordinary verified login after a successful config load may publish.
    auto const first = model.BeginLoginRead();
    Assert(first.enabled, "fresh generation armed", checks);
    Assert(model.PublishVerifiedLogin(first), "same-generation login", checks);
    Assert(model.HasPublication(), "current publication available", checks);

    // A previously verified login can be paused before it takes the lock.
    // The old gate-only implementation would accept this after re-enable.
    auto const staleBeforeReload = model.BeginLoginRead();
    model.BeginReload();
    Assert(!model.HasPublication(), "begin evicts", checks);
    auto const startedDuringReload = model.BeginLoginRead();
    Assert(!startedDuringReload.enabled, "disabled during reload", checks);
    model.FinishReload(true);
    Assert(!model.PublishVerifiedLogin(staleBeforeReload),
           "pre-reload writer cannot resurrect stale identity", checks);
    Assert(!model.PublishVerifiedLogin(startedDuringReload),
           "during-reload writer cannot become freshly verified", checks);
    Assert(!model.HasPublication(), "stale and during-reload entries absent", checks);

    auto const freshAfterReload = model.BeginLoginRead();
    Assert(model.PublishVerifiedLogin(freshAfterReload),
           "only fresh post-reload verification", checks);
    model.BeginReload();
    model.FinishReload(false);
    Assert(!model.PublishVerifiedLogin(freshAfterReload),
           "IP disabled cannot publish", checks);
    Assert(!model.HasPublication(), "disabled reader refuses", checks);

    // An old login cannot become valid after an OFF->ON cycle.
    model.BeginReload();
    model.FinishReload(true);
    Assert(!model.PublishVerifiedLogin(freshAfterReload),
           "OFF->ON revocation durable", checks);
    Assert(model.PublishVerifiedLogin(model.BeginLoginRead()),
           "new verified login can publish", checks);

    // Generation exhaustion is permanently OFF, never wraps to old token.
    model.ForTestSetGeneration(std::numeric_limits<std::uint64_t>::max() - 1);
    model.BeginReload();
    model.FinishReload(true);
    Assert(!model.BeginLoginRead().enabled,
           "generation saturation prevents rearm", checks);
    Assert(!model.PublishVerifiedLogin(model.BeginLoginRead()),
           "generation saturation refuses writer", checks);
    model.BeginReload();
    model.FinishReload(true);
    Assert(!model.BeginLoginRead().enabled,
           "repeated reload cannot escape exhaustion", checks);
    Assert(!model.HasPublication(), "no publication after exhaustion", checks);

    std::cout << "F14 RELOAD GENERATION MODEL " << checks
              << "/" << checks << " passed (no real gameplay)\n";
}
