1. System Architecture Evolution
[LOCAL PROTOTYPE]
Client Request ──► Crow Web Server ──► C++ std::mutex Cache ──► Local SQLite

[GLOBAL PRODUCTION]
Client Request ──► CDN Edge (Cloudflare) ──► Anycast Load Balancer
                                                   │
                        ┌──────────────────────────┴──────────────────────────┐
                        ▼                                                     ▼
              C++ Microservice Instance 1                           C++ Microservice Instance 2
                        │                                                     │
                        └──────────────────────────┬──────────────────────────┘
                                                   │
                                                   ▼
                                     Global Redis Cluster (SETNX)
                                                   │
                                                   ▼
                                     PostgreSQL (Primary + Replicas)

