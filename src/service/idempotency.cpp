#include "upi_mesh/service/idempotency.hpp"
#include <mutex>

namespace upi::service {

bool IdempotencyService::claim_hash(const std::string& hash) {
    std::unique_lock<std::shared_mutex> lock(mutex_);


    auto [it, inserted] = seen_hashes_.emplace(hash, std::chrono::system_clock::now());
    
    return inserted;
}

size_t IdempotencyService::size() const {
    // Acquire
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return seen_hashes_.size();
}

void IdempotencyService::clear() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    seen_hashes_.clear();
}

} // namespace upi::service