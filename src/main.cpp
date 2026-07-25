#include "upi_mesh/service/idempotency.hpp"
#include "upi_mesh/crypto/hybrid_crypto.hpp"
#include "upi_mesh/db/database.hpp"
#include <crow.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <chrono>

int main()
{
    crow::SimpleApp app;
    upi::service::IdempotencyService idempotency_cache;

    CROW_LOG_INFO << "Initializing SQLite Database...";
    upi::db::Database db("upi_ledger.db");
    db.init_schema();
    db.seed_accounts();

    CROW_LOG_INFO << "Generating Server RSA-2048 Keypair...";
    auto keys = upi::crypto::HybridCryptoService::generate_rsa_keypair();

    CROW_ROUTE(app, "/api/bridge/ingest").methods(crow::HTTPMethod::POST)([&idempotency_cache, &keys, &db](const crow::request &req)
                                                                          {
        try {
            auto body = nlohmann::json::parse(req.body);
            if (!body.contains("ciphertext") || !body.contains("packetId")) {
                return crow::response(400, "Missing required fields");
            }
            
            std::string ciphertext = body["ciphertext"];
            std::string packet_hash = upi::crypto::HybridCryptoService::sha256(ciphertext);
            
            if (!idempotency_cache.claim_hash(packet_hash)) {
                CROW_LOG_WARNING << "DUPLICATE DROPPED: " << packet_hash;
                return crow::response(409, nlohmann::json{{"outcome", "DUPLICATE_DROPPED"}, {"packetHash", packet_hash}}.dump());
            }
            
            std::string decrypted_json_str = upi::crypto::HybridCryptoService::decrypt_payload(ciphertext, keys.private_key_pem);
            auto decrypted_payload = nlohmann::json::parse(decrypted_json_str);
            
            std::string sender = decrypted_payload["sender"];
            std::string receiver = decrypted_payload["receiver"];
            int amount = decrypted_payload["amount"];

            if (db.settle_payment(sender, receiver, amount, packet_hash)) {
                CROW_LOG_INFO << "PAYMENT SETTLED: " << amount << " from " << sender << " to " << receiver;
                return crow::response(200, nlohmann::json{{"outcome", "SETTLED"}, {"packetHash", packet_hash}}.dump());
            } else {
                return crow::response(400, nlohmann::json{{"outcome", "REJECTED"}, {"reason", "Insufficient funds"}}.dump());
            }
            
        } catch (const std::exception& e) {
            return crow::response(400, nlohmann::json{{"outcome", "INVALID"}, {"reason", e.what()}}.dump());
        } });

    CROW_ROUTE(app, "/api/accounts").methods(crow::HTTPMethod::GET)([&db]()
                                                                    { return crow::response(200, db.get_all_accounts_json()); });

    CROW_ROUTE(app, "/api/demo/generate").methods(crow::HTTPMethod::GET)([&keys]()
                                                                         {
        std::string nonce = "uuid-" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        std::string payload = R"({"sender":"alice@upi","receiver":"bob@upi","amount":500,"nonce":")" + nonce + R"("})";
        
        auto encrypted = upi::crypto::HybridCryptoService::encrypt_payload(payload, keys.public_key_pem);
        nlohmann::json res = {
            {"packetId", nonce},
            {"ciphertext", encrypted.ciphertext_base64}
        };
        return crow::response(200, res.dump()); });

    CROW_LOG_INFO << "Starting Offline UPI Mesh backend on http://localhost:8080";
    app.port(8080).multithreaded().run();

    return 0;
}