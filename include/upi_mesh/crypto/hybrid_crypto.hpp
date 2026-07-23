// cryptography header

#ifndef UPI_HYBRID_CRYPTO_HPP
#define UPI_HYBRID_CRYPTO_HPP

#include <string>
#include <vector>
#include <memory>
#include <openssl/evp.h>

namespace upi::crypto
{

    struct KeyPair
    {
        std::string public_key_pem;
        std::string private_key_pem;
    };

    struct EncryptedPacket
    {
        std::string ciphertext_base64;
        std::string packet_hash;
    };

    class HybridCryptoService
    {
    public:
        static KeyPair generate_rsa_keypair();

        // RSA-OAEP + AES-256-GCM Encryption
        static EncryptedPacket encrypt_payload(const std::string &json_payload, const std::string &public_key_pem);

        // Decryption
        static std::string decrypt_payload(const std::string &ciphertext_base64, const std::string &private_key_pem);

        // Compute SHA-256 hash of ciphertext
        static std::string sha256(const std::string &input);
    };
}

#endif// UPI_HYBRID_CRYPTO_HPP