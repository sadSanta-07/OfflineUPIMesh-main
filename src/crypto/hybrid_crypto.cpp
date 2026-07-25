#include "upi_mesh/crypto/hybrid_crypto.hpp"
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <vector>

namespace upi::crypto
{

    // Helper: Extract OpenSSL error stack messages
    static std::string get_openssl_error()
    {
        BIO *bio = BIO_new(BIO_s_mem());
        ERR_print_errors(bio);
        char *buf = nullptr;
        long len = BIO_get_mem_data(bio, &buf);
        std::string err_msg(buf, len);
        BIO_free(bio);
        return err_msg.empty() ? "Unknown OpenSSL Error" : err_msg;
    }

    // Helper: Encode binary vector to Base64
    static std::string base64_encode(const std::vector<unsigned char> &data)
    {
        BIO *bio = BIO_new(BIO_f_base64());
        BIO *mem = BIO_new(BIO_s_mem());
        BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
        bio = BIO_push(bio, mem);

        BIO_write(bio, data.data(), static_cast<int>(data.size()));
        BIO_flush(bio);

        BUF_MEM *bufferPtr;
        BIO_get_mem_ptr(bio, &bufferPtr);
        std::string result(bufferPtr->data, bufferPtr->length);
        BIO_free_all(bio);
        return result;
    }

    // Helper: Decode Base64 to binary vector
    static std::vector<unsigned char> base64_decode(const std::string &base64_str)
    {
        BIO *bio = BIO_new(BIO_f_base64());
        BIO *mem = BIO_new_mem_buf(base64_str.data(), static_cast<int>(base64_str.size()));
        BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
        bio = BIO_push(bio, mem);

        std::vector<unsigned char> buffer(base64_str.size());
        int decoded_total = 0;
        int len = 0;

        while ((len = BIO_read(bio, buffer.data() + decoded_total, static_cast<int>(buffer.size() - decoded_total))) > 0)
        {
            decoded_total += len;
        }
        BIO_free_all(bio);

        if (decoded_total <= 0)
            throw std::runtime_error("Base64 decode failed or empty output");
        buffer.resize(decoded_total);
        return buffer;
    }

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
            throw std::runtime_error("Failed to create EVP_PKEY_CTX: " + get_openssl_error());

        if (EVP_PKEY_keygen_init(ctx) <= 0)
            throw std::runtime_error("Failed to init keygen: " + get_openssl_error());
        if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048) <= 0)
            throw std::runtime_error("Failed to set key length: " + get_openssl_error());

        EVP_PKEY *pkey = nullptr;
        if (EVP_PKEY_keygen(ctx, &pkey) <= 0)
            throw std::runtime_error("Failed to generate RSA key: " + get_openssl_error());

        BIO *pub_bio = BIO_new(BIO_s_mem());
        PEM_write_bio_PUBKEY(pub_bio, pkey);
        char *pub_buf = nullptr;
        long pub_len = BIO_get_mem_data(pub_bio, &pub_buf);
        std::string pub_key(pub_buf, pub_len);

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

    EncryptedPacket HybridCryptoService::encrypt_payload(const std::string &json_payload, const std::string &public_key_pem)
    {
        // 1. Generate fresh AES-256 Key
        std::vector<unsigned char> aes_key(32);
        std::vector<unsigned char> iv(12);
        if (RAND_bytes(aes_key.data(), 32) != 1 || RAND_bytes(iv.data(), 12) != 1)
        {
            throw std::runtime_error("Failed to generate secure random AES key/IV");
        }

        // 2. Encrypt Payload using AES-256-GCM
        EVP_CIPHER_CTX *cipher_ctx = EVP_CIPHER_CTX_new();
        if (!cipher_ctx)
            throw std::runtime_error("Failed to create CIPHER_CTX");

        if (EVP_EncryptInit_ex(cipher_ctx, EVP_aes_256_gcm(), nullptr, aes_key.data(), iv.data()) != 1)
        {
            EVP_CIPHER_CTX_free(cipher_ctx);
            throw std::runtime_error("EVP_EncryptInit_ex failed: " + get_openssl_error());
        }

        std::vector<unsigned char> ciphertext(json_payload.size() + EVP_CIPHER_block_size(EVP_aes_256_gcm()));
        int out_len = 0;
        if (EVP_EncryptUpdate(cipher_ctx, ciphertext.data(), &out_len,
                              reinterpret_cast<const unsigned char *>(json_payload.data()),
                              static_cast<int>(json_payload.size())) != 1)
        {
            EVP_CIPHER_CTX_free(cipher_ctx);
            throw std::runtime_error("EVP_EncryptUpdate failed: " + get_openssl_error());
        }
        int cipher_len = out_len;

        if (EVP_EncryptFinal_ex(cipher_ctx, ciphertext.data() + out_len, &out_len) != 1)
        {
            EVP_CIPHER_CTX_free(cipher_ctx);
            throw std::runtime_error("EVP_EncryptFinal_ex failed: " + get_openssl_error());
        }
        cipher_len += out_len;
        ciphertext.resize(cipher_len);

        // Get GCM Authentication Tag
        std::vector<unsigned char> gcm_tag(16);
        if (EVP_CIPHER_CTX_ctrl(cipher_ctx, EVP_CTRL_GCM_GET_TAG, 16, gcm_tag.data()) != 1)
        {
            EVP_CIPHER_CTX_free(cipher_ctx);
            throw std::runtime_error("Failed to get GCM Tag: " + get_openssl_error());
        }
        EVP_CIPHER_CTX_free(cipher_ctx);

        // 3. Encrypt AES key with RSA Public Key
        BIO *pub_bio = BIO_new_mem_buf(public_key_pem.data(), static_cast<int>(public_key_pem.size()));
        EVP_PKEY *rsa_key = PEM_read_bio_PUBKEY(pub_bio, nullptr, nullptr, nullptr);
        BIO_free(pub_bio);
        if (!rsa_key)
            throw std::runtime_error("Failed to parse RSA public key: " + get_openssl_error());

        EVP_PKEY_CTX *rsa_ctx = EVP_PKEY_CTX_new(rsa_key, nullptr);
        EVP_PKEY_encrypt_init(rsa_ctx);
        EVP_PKEY_CTX_set_rsa_padding(rsa_ctx, RSA_PKCS1_OAEP_PADDING);
        EVP_PKEY_CTX_set_rsa_oaep_md(rsa_ctx, EVP_sha256());
        EVP_PKEY_CTX_set_rsa_mgf1_md(rsa_ctx, EVP_sha256());

        size_t encrypted_key_len = 0;
        EVP_PKEY_encrypt(rsa_ctx, nullptr, &encrypted_key_len, aes_key.data(), aes_key.size());
        std::vector<unsigned char> encrypted_aes_key(encrypted_key_len);

        if (EVP_PKEY_encrypt(rsa_ctx, encrypted_aes_key.data(), &encrypted_key_len, aes_key.data(), aes_key.size()) <= 0)
        {
            EVP_PKEY_CTX_free(rsa_ctx);
            EVP_PKEY_free(rsa_key);
            throw std::runtime_error("RSA key encryption failed: " + get_openssl_error());
        }
        EVP_PKEY_CTX_free(rsa_ctx);
        EVP_PKEY_free(rsa_key);

        // 4. Combine into final wire packet buffer:
        std::vector<unsigned char> wire_buffer;
        wire_buffer.insert(wire_buffer.end(), encrypted_aes_key.begin(), encrypted_aes_key.end());
        wire_buffer.insert(wire_buffer.end(), iv.begin(), iv.end());
        wire_buffer.insert(wire_buffer.end(), gcm_tag.begin(), gcm_tag.end());
        wire_buffer.insert(wire_buffer.end(), ciphertext.begin(), ciphertext.end());

        std::string b64_ciphertext = base64_encode(wire_buffer);
        std::string packet_hash = sha256(b64_ciphertext);

        return {b64_ciphertext, packet_hash};
    }

    std::string HybridCryptoService::decrypt_payload(const std::string &ciphertext_base64, const std::string &private_key_pem)
    {
        std::vector<unsigned char> wire_buffer = base64_decode(ciphertext_base64);

        // Min len check
        if (wire_buffer.size() < 284)
        {
            throw std::runtime_error("Invalid packet length: Payload too short");
        }

        auto encrypted_key_begin = wire_buffer.begin();
        auto iv_begin = encrypted_key_begin + 256;
        auto tag_begin = iv_begin + 12;
        auto ciphertext_begin = tag_begin + 16;

        std::vector<unsigned char> encrypted_aes_key(encrypted_key_begin, iv_begin);
        std::vector<unsigned char> iv(iv_begin, tag_begin);
        std::vector<unsigned char> gcm_tag(tag_begin, ciphertext_begin);
        std::vector<unsigned char> ciphertext(ciphertext_begin, wire_buffer.end());

        // 1. Decrypt AES Key using RSA Private Key
        BIO *priv_bio = BIO_new_mem_buf(private_key_pem.data(), static_cast<int>(private_key_pem.size()));
        EVP_PKEY *rsa_key = PEM_read_bio_PrivateKey(priv_bio, nullptr, nullptr, nullptr);
        BIO_free(priv_bio);
        if (!rsa_key)
            throw std::runtime_error("Failed to parse RSA private key: " + get_openssl_error());

        EVP_PKEY_CTX *rsa_ctx = EVP_PKEY_CTX_new(rsa_key, nullptr);
        EVP_PKEY_decrypt_init(rsa_ctx);
        EVP_PKEY_CTX_set_rsa_padding(rsa_ctx, RSA_PKCS1_OAEP_PADDING);
        EVP_PKEY_CTX_set_rsa_oaep_md(rsa_ctx, EVP_sha256());
        EVP_PKEY_CTX_set_rsa_mgf1_md(rsa_ctx, EVP_sha256());

        // Step A: Determine maximum output buffer required by OpenSSL
        size_t aes_key_len = 0;
        if (EVP_PKEY_decrypt(rsa_ctx, nullptr, &aes_key_len, encrypted_aes_key.data(), encrypted_aes_key.size()) <= 0)
        {
            EVP_PKEY_CTX_free(rsa_ctx);
            EVP_PKEY_free(rsa_key);
            throw std::runtime_error("Failed to query RSA decrypt buffer size: " + get_openssl_error());
        }

        // Step B: Allocate buffer and perform decryption
        std::vector<unsigned char> aes_key(aes_key_len);
        if (EVP_PKEY_decrypt(rsa_ctx, aes_key.data(), &aes_key_len, encrypted_aes_key.data(), encrypted_aes_key.size()) <= 0)
        {
            EVP_PKEY_CTX_free(rsa_ctx);
            EVP_PKEY_free(rsa_key);
            throw std::runtime_error("RSA key decryption failed: " + get_openssl_error());
        }
        aes_key.resize(aes_key_len);

        EVP_PKEY_CTX_free(rsa_ctx);
        EVP_PKEY_free(rsa_key);

        // 2. Decrypt Ciphertext using AES-256-GCM
        EVP_CIPHER_CTX *cipher_ctx = EVP_CIPHER_CTX_new();
        if (EVP_DecryptInit_ex(cipher_ctx, EVP_aes_256_gcm(), nullptr, aes_key.data(), iv.data()) != 1)
        {
            EVP_CIPHER_CTX_free(cipher_ctx);
            throw std::runtime_error("EVP_DecryptInit_ex failed: " + get_openssl_error());
        }

        std::vector<unsigned char> plaintext(ciphertext.size());
        int out_len = 0;
        if (EVP_DecryptUpdate(cipher_ctx, plaintext.data(), &out_len, ciphertext.data(), static_cast<int>(ciphertext.size())) != 1)
        {
            EVP_CIPHER_CTX_free(cipher_ctx);
            throw std::runtime_error("EVP_DecryptUpdate failed: " + get_openssl_error());
        }
        int plain_len = out_len;

        // Set expected GCM Authentication
        if (EVP_CIPHER_CTX_ctrl(cipher_ctx, EVP_CTRL_GCM_SET_TAG, 16, gcm_tag.data()) != 1)
        {
            EVP_CIPHER_CTX_free(cipher_ctx);
            throw std::runtime_error("Failed to set GCM Tag: " + get_openssl_error());
        }

        // Finalize decryption
        if (EVP_DecryptFinal_ex(cipher_ctx, plaintext.data() + out_len, &out_len) <= 0)
        {
            EVP_CIPHER_CTX_free(cipher_ctx);
            throw std::runtime_error("GCM Authentication failed! Packet was tampered with.");
        }
        plain_len += out_len;
        EVP_CIPHER_CTX_free(cipher_ctx);

        return std::string(reinterpret_cast<char *>(plaintext.data()), plain_len);
    }

} // namespace upi::crypto