# UPI Offline Mesh — C++ Core

A high-performance C++ backend demonstrating **mesh-routed deferred settlement** for offline payments. 

This system proves that a payment can be encrypted on a mobile device without internet, hopped across untrusted intermediary devices via Bluetooth, and safely ingested into a banking ledger the moment any device in the chain regains connectivity.

## The Tech Stack
* **Backend:** C++20, Crow (Async HTTP), ASIO
* **Cryptography:** OpenSSL 3.x (`libcrypto`)
* **Ledger:** Embedded SQLite3 
* **Frontend:** Vanilla JS/HTML/CSS 
* **Build System:** CMake, Ninja, MSYS2/GCC

## ⚡ Core Engineering Challenges Solved

### 1. Untrusted Intermediaries (Hybrid Cryptography)
A random stranger's phone carries your transaction. To prevent tampering, the payload is secured using TLS-style hybrid encryption.
* A fresh AES-256-GCM key encrypts the payment JSON (providing authenticated encryption to catch bit-flipping).
* The AES key is wrapped in the server's RSA-2048 public key using OAEP padding. 
* Untrusted nodes route opaque Base64 blobs. Only the backend holds the private key.

### 2. The Duplicate Storm (Thread-Safe Idempotency)
If three bridge phones carry the same packet and hit the server simultaneously, the sender must only be debited once. 
* The ingestion pipeline computes a SHA-256 hash of the ciphertext immediately.
* A highly concurrent C++ `std::mutex` lock ensures only the very first thread successfully claims the hash in the `unordered_map` cache.
* Duplicates are short-circuited and dropped with a `409 Conflict` before CPU cycles are wasted on RSA decryption.

### 3. ACID-Compliant Ledger
Money cannot be created out of thin air. The settlement engine wraps the sender debit, receiver credit, and ledger insertion inside a strict SQLite `BEGIN EXCLUSIVE TRANSACTION`. If a user has insufficient funds, the entire operation safely rolls back.

## The Dashboard
The project features a custom, high-contrast dashboard to visualize the mesh in real-time. Moving away from standard, convention-heavy admin tables, the frontend utilizes an unconventional, visually striking design language to create a high-impact command center. You can inject packets, trigger duplicate storms, and watch the C++ server ingest and deduplicate requests live.

## Global System Design & Scaling Roadmap

While this repository contains the core C++ microservice, it is architected to slot into a globally distributed production environment. 

* **Edge Delivery (CDNs):** To ensure the high-fidelity UI loads instantly worldwide, the static frontend assets are deployed to a Content Delivery Network (CDN). This guarantees sub-20ms load times globally, while dynamic API calls are tunneled back to the core backend servers.
* **Geographic Latency Mitigation:** In a real-world scenario, a bridge node might upload from the United States while another uploads the exact same packet simultaneously from India. To handle deduplication across global latency differences, the local C++ `std::mutex` idempotency cache will be upgraded to a globally synchronized **Redis Cluster**. 
* **Distributed `SETNX`:** By leveraging the `hiredis` C++ client, the ingestion pipeline will issue atomic `SETNX` (Set if Not Exists) commands to Redis, guaranteeing that even across continents, a packet hash can only be claimed exactly once.

## How to Build and Run (Windows/MinGW)

**Prerequisites:** MSYS2 with UCRT64 (GCC, CMake, Ninja, OpenSSL, SQLite3).

```powershell
# 1. Clone and enter the build directory
mkdir build && cd build

# 2. Configure and Compile
cmake -G "Ninja" ..
ninja

# 3. Start the Server
.\upi_mesh_server.exe