#ifndef UPI_IDEMPOTENCY_HPP
#define UPI_IDEMPOTENCY_HPP

#include <string>
#include <unordered_map>
#include <shared_mutex>
#include <chrono>

namespace upi::service
{

    class IdempotencyService
    {
    private:
        std::unordered_map<std::string, std::chrono::system_clock::time_point> seen_hashes_;
        mutable std::shared_mutex mutex_;

    public:
        IdempotencyService() = default;

        bool claim_hash(const std::string &hash);

        size_t size() const;

        void clear();
    };

}

#endif // UPI_IDEMPOTENCY_HPP