#include "upi_mesh/crypto/hybrid_crypto.hpp"
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <stdexcept>
#include <sstream>
#include <iomanip>

namespace upi::crypto
{

    std::string HybridCryptoService::sha256(const std::string &input)
    {
        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256(reinterpret_cast<const unsigned char *>(input.data()), input.size(), hash);

        std::stringstream ss;
        for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
        {
            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
        }
        return ss.str();
    }

    KeyPair HybridCryptoService::generate_rsa_keypair()
    {
        EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
        if (!ctx)
            throw std::runtime_error("Failed to create EVP_PKEY_CTX");

        if (EVP_PKEY_keygen_init(ctx) <= 0)
            throw std::runtime_error("Failed to init keygen");
        if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048) <= 0)
            throw std::runtime_error("Failed to set key length");

        EVP_PKEY *pkey = nullptr;
        if (EVP_PKEY_keygen(ctx, &pkey) <= 0)
            throw std::runtime_error("Failed to generate RSA key");

        // Extract Public Key PEM
        BIO *pub_bio = BIO_new(BIO_s_mem());
        PEM_write_bio_PUBKEY(pub_bio, pkey);
        char *pub_buf = nullptr;
        long pub_len = BIO_get_mem_data(pub_bio, &pub_buf);
        std::string pub_key(pub_buf, pub_len);

        // Extract Private Key PEM
        BIO *priv_bio = BIO_new(BIO_s_mem());
        PEM_write_bio_PKCS8PrivateKey(priv_bio, pkey, nullptr, nullptr, 0, nullptr, nullptr);
        char *priv_buf = nullptr;
        long priv_len = BIO_get_mem_data(priv_bio, &priv_buf);
        std::string priv_key(priv_buf, priv_len);

        BIO_free(pub_bio);
        BIO_free(priv_bio);
        EVP_PKEY_free(pkey);
        EVP_PKEY_CTX_free(ctx);

        return {pub_key, priv_key};
    }

} // namespace upi::crypto