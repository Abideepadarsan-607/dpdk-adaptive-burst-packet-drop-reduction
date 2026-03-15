# Presentation Slides — DPDK Adaptive Burst Control

## Slide 1 — Title
**DPDK-Based Adaptive Burst-Aware Packet Drop Reduction**
- Presented by: Abideepadarsan SK, Vikkesh P
- B.Tech CSE — SRM IST, Kattankulathur, 2026
- Major Project Review

---

## Slide 2 — Problem Statement
**The Problem: Packet Drops During Burst Traffic**
- Modern networks face sudden traffic bursts
- Linux kernel stack: interrupt-driven, slow, multiple memory copies
- During bursts: NIC buffer overflows → packets dropped
- No real-time adaptive control in traditional systems

*(Diagram: Traditional Kernel Stack vs DPDK)*

---

## Slide 3 — What is DPDK?
**Data Plane Development Kit**
- Framework for high-speed packet processing in USER SPACE
- Bypasses the Linux kernel NETWORK STACK
- Uses Poll Mode Drivers (PMD) — no CPU interrupts
- Processes millions of packets per second
- Used in: Telecom, 5G, Data Centers, Cloud Routers

---

## Slide 4 — DPDK vs Traditional Networking
| Feature | Linux Kernel Stack | DPDK |
|---|---|---|
| Packet arrival | CPU interrupt | Poll loop |
| Processing | Kernel layers | User space direct |
| Memory copy | Multiple copies | Zero-copy |
| Throughput | ~100k PPS | 500k–10M PPS |
| Latency | High (µs) | Low (ns) |

---

## Slide 5 — System Architecture
*(Architecture diagram from README)*

Components:
1. Traffic Generator (tcpreplay)
2. TAP Interface (dtap0) — virtual NIC
3. DPDK PMD — polls NIC directly
4. Burst Detection Engine — PPS counter
5. Adaptive Drop Controller — 50% drop on burst
6. TX Queue — forward valid packets

---

## Slide 6 — DPDK Key Components
**What DPDK needs to run:**
- **Hugepages** — 2MB large memory pages (reduces TLB misses)
- **mbuf Pool** — pre-allocated packet buffers
- **RX Queue** — receive ring buffer (1024 descriptors)
- **TX Queue** — transmit ring buffer
- **vfio-pci** — kernel module to bind NIC to DPDK
- **PMD** — Poll Mode Driver, replaces kernel NIC driver

---

## Slide 7 — Proposed Algorithm
```
Every second:
  Count packets received (PPS)

  IF PPS >= 200,000:
      BURST DETECTED
      Drop 50% of incoming packets
      Forward remaining 50%

  ELSE:
      Forward all packets

Print: RX / TX / Dropped / Drop Rate
```

---

## Slide 8 — Packet Validation
**How we validate incoming packets:**
1. Frame length check (64–1518 bytes)
2. EtherType check (IPv4 = 0x0800 only)
3. IP version check (must be IPv4)
4. IP header length (>= 20 bytes)
5. Protocol check (UDP or ICMP only)

Malformed / unknown packets → dropped before forwarding pipeline

---

## Slide 9 — Experimental Setup
| Component | Detail |
|---|---|
| Hardware | 8-core laptop, 8GB RAM |
| OS | Kali Linux |
| DPDK Mode | TAP virtual interface (dtap0) |
| Traffic Tool | tcpreplay at topspeed |
| Hugepages | 1024 x 2MB = 2GB |
| Burst Threshold | 200,000 PPS |

---

## Slide 10 — Results
**Baseline (no adaptive control):**
- Peak PPS: 447,776
- Total RX: 1,720,850 | TX: 1,720,850 | Dropped: 0

**Adaptive (with burst control):**
- Peak PPS: 531,755
- Total RX: 2,500,000 | TX: ~1,250,000
- Dropped: ~1,250,000 (50% during burst)
- Drop Rate: ~50% ✅

---

## Slide 11 — Why Not Just Increase Ring Buffer?
**Professor Question: "Why not just make the buffer bigger?"**

Answer:
- Larger ring buffer only **delays** packet loss
- During a **sustained burst**, even a 10x larger buffer fills up
- Adaptive dropping **actively controls** the forwarding rate
- Maintains **sustainable throughput** regardless of burst duration
- Like a pressure valve vs a larger tank

---

## Slide 12 — Conclusion & Future Work
**Conclusion:**
- Successfully implemented real-time burst detection at 500k+ PPS
- Adaptive control reduces congestion without system instability
- DPDK PMD eliminates kernel overhead completely

**Future Work:**
- Machine learning-based dynamic threshold adjustment
- Multi-core parallel processing (10 Gbps+ support)
- Per-flow selective dropping policies
- SDN controller integration
