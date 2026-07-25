#include "upi_mesh/db/database.hpp"
#include <iostream>

namespace upi::db
{

    Database::Database(const std::string &db_path)
    {
        if (sqlite3_open(db_path.c_str(), &db_) != SQLITE_OK)
        {
            throw std::runtime_error("Failed to open SQLite database");
        }
    }

    Database::~Database()
    {
        sqlite3_close(db_);
    }

    void Database::execute_query(const std::string &query)
    {
        char *err_msg = nullptr;
        if (sqlite3_exec(db_, query.c_str(), nullptr, nullptr, &err_msg) != SQLITE_OK)
        {
            std::string error = err_msg;
            sqlite3_free(err_msg);
            throw std::runtime_error("SQL Error: " + error);
        }
    }

    void Database::init_schema()
    {
        std::lock_guard<std::mutex> lock(db_mutex_);

        execute_query(R"(
        CREATE TABLE IF NOT EXISTS accounts (
            vpa TEXT PRIMARY KEY,
            balance INTEGER NOT NULL CHECK (balance >= 0)
        );
    )");

        execute_query(R"(
        CREATE TABLE IF NOT EXISTS transactions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            sender_vpa TEXT NOT NULL,
            receiver_vpa TEXT NOT NULL,
            amount INTEGER NOT NULL,
            packet_hash TEXT UNIQUE NOT NULL,
            settled_at DATETIME DEFAULT CURRENT_TIMESTAMP
        );
    )");
    }

    void Database::seed_accounts()
    {
        std::lock_guard<std::mutex> lock(db_mutex_);
        execute_query("INSERT OR IGNORE INTO accounts (vpa, balance) VALUES ('alice@upi', 1000);");
        execute_query("INSERT OR IGNORE INTO accounts (vpa, balance) VALUES ('bob@upi', 1000);");
    }

    bool Database::settle_payment(const std::string &sender, const std::string &receiver, int amount, const std::string &packet_hash)
    {
        std::lock_guard<std::mutex> lock(db_mutex_);

        execute_query("BEGIN EXCLUSIVE TRANSACTION;");

        try
        {
            std::string debit_sql = "UPDATE accounts SET balance = balance - " + std::to_string(amount) + " WHERE vpa = '" + sender + "';";

            char *err_msg = nullptr;
            if (sqlite3_exec(db_, debit_sql.c_str(), nullptr, nullptr, &err_msg) != SQLITE_OK)
            {
                sqlite3_free(err_msg);
                throw std::runtime_error("Insufficient funds or invalid sender");
            }

            std::string credit_sql = "UPDATE accounts SET balance = balance + " + std::to_string(amount) + " WHERE vpa = '" + receiver + "';";
            execute_query(credit_sql);

            std::string ledger_sql = "INSERT INTO transactions (sender_vpa, receiver_vpa, amount, packet_hash) VALUES ('" +
                                     sender + "', '" + receiver + "', " + std::to_string(amount) + ", '" + packet_hash + "');";
            execute_query(ledger_sql);

            execute_query("COMMIT;");
            return true;
        }
        catch (const std::exception &e)
        {
            // Rollback
            execute_query("ROLLBACK;");
            return false;
        }
    }

    std::string Database::get_all_accounts_json()
    {
        std::lock_guard<std::mutex> lock(db_mutex_);
        sqlite3_stmt *stmt;
        std::string json = "[";

        if (sqlite3_prepare_v2(db_, "SELECT vpa, balance FROM accounts", -1, &stmt, nullptr) == SQLITE_OK)
        {
            bool first = true;
            while (sqlite3_step(stmt) == SQLITE_ROW)
            {
                if (!first)
                    json += ",";
                std::string vpa = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
                int balance = sqlite3_column_int(stmt, 1);
                json += R"({"vpa":")" + vpa + R"(","balance":)" + std::to_string(balance) + "}";
                first = false;
            }
            sqlite3_finalize(stmt);
        }
        json += "]";
        return json;
    }

} // namespace upi::db