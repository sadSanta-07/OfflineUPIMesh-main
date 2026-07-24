#include "upi_mesh/service/idempotency.hpp"

namespace upi::service {

bool IdempotencyService::claim_hash(const std::string& hash) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto [it, inserted] = seen_hashes_.emplace(hash, std::chrono::system_clock::now());
    
    return inserted;
}

size_t IdempotencyService::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return seen_hashes_.size();
}

void IdempotencyService::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    seen_hashes_.clear();
}

} // namespace upi::service