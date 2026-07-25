#ifndef UPI_DATABASE_HPP
#define UPI_DATABASE_HPP

#include <string>
#include <sqlite3.h>
#include <stdexcept>
#include <mutex>

namespace upi::db {

class Database {
private:
    sqlite3* db_;
    std::mutex db_mutex_;

    void execute_query(const std::string& query);

public:
    Database(const std::string& db_path = "upi_ledger.db");
    ~Database();

    void init_schema();
    void seed_accounts();
    bool settle_payment(const std::string& sender, const std::string& receiver, int amount, const std::string& packet_hash);
    
    std::string get_all_accounts_json(); 
};

} // namespace upi::db

#endif // UPI_DATABASE_HPP