#include "upi_mesh/service/idempotency.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <cassert>

int main() {
    try {
        std::cout << "[+] Initializing Idempotency Service...\n";
        upi::service::IdempotencyService cache;

        std::string mock_packet_hash = "3820099561d7984dd48636d0152fc815327856eee78edca30f08d704494a8733";

        std::atomic<int> success_count{0};
        std::atomic<int> duplicate_count{0};

        const int num_threads = 10;
        std::vector<std::thread> threads;

        std::cout << "[+] Spawning " << num_threads << " concurrent bridge threads targeting the SAME packet hash...\n";

        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&cache, &mock_packet_hash, &success_count, &duplicate_count]() {
                if (cache.claim_hash(mock_packet_hash)) {
                    success_count++;
                } else {
                    duplicate_count++;
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        std::cout << " -> First-claimer Settlements : " << success_count.load() << "\n";
        std::cout << " -> Rejected Duplicates       : " << duplicate_count.load() << "\n";
        std::cout << " -> Total Cache Size          : " << cache.size() << "\n";

        assert(success_count.load() == 1);
        assert(duplicate_count.load() == num_threads - 1);
        assert(cache.size() == 1);

        std::cout << "\n=======================================================\n";
        std::cout << "  CONCURRENCY TEST PASSED (THREAD-SAFE IDEMPOTENCY)   \n";
        std::cout << "=======================================================\n";

    } catch (const std::exception& e) {
        std::cerr << "Concurrency Test Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}