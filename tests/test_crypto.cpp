#include "upi_mesh/crypto/hybrid_crypto.hpp"
#include <iostream>

int main() {
    try {
        std::cout << "[+] Generating RSA 2048 Keypair via OpenSSL...\n";
        auto keys = upi::crypto::HybridCryptoService::generate_rsa_keypair();
        
        std::cout << "[+] Public Key generated successfully:\n" << keys.public_key_pem.substr(0, 100) << "...\n\n";

        std::string sample = "UPI_OFFLINE_PAYMENT_PAYLOAD";
        std::string hash = upi::crypto::HybridCryptoService::sha256(sample);
        std::cout << "[+] SHA-256 Hash check: " << hash << "\n";

        std::cout << "\n[SUCCESS] Cryptography key generator compiled and working properly!\n";
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}