# DPDK-Based Adaptive Burst-Aware Packet Drop Reduction

![DPDK](https://img.shields.io/badge/Framework-DPDK-blue)
![OS](https://img.shields.io/badge/OS-Kali%20Linux-red)
![Lang](https://img.shields.io/badge/Language-C-lightgrey)
![Type](https://img.shields.io/badge/Type-Major%20Project-green)
![Status](https://img.shields.io/badge/Status-Completed-brightgreen)

## Project Overview

This major project implements a **high-speed adaptive packet processing system** using the Data Plane Development Kit (DPDK). The system detects burst traffic in real time and applies adaptive control mechanisms to reduce packet drops — a critical problem in modern data centers, cloud platforms, and 5G networks.

---

## Problem Statement

In traditional Linux networking:
- Packets pass through the **kernel network stack** (multiple layers)
- Each packet causes a **CPU interrupt**, wasting processing time
- During **burst traffic**, NIC buffers overflow → packets are dropped
- No intelligent mechanism exists to detect or respond to bursts in real time

## Our Solution

Using DPDK's **Poll Mode Driver (PMD)**:
- Bypass the kernel network stack entirely
- Poll the NIC directly from user space — no interrupts
- Monitor Packets Per Second (PPS) in real time
- Detect burst traffic when PPS exceeds threshold
- Apply **adaptive packet dropping** (50% drop during burst)
- Log final statistics: RX, TX, and dropped packets

---

## Key Results Achieved

| Metric | Value |
|---|---|
| Peak RX Rate | ~500,000+ PPS |
| Burst Detection Threshold | 200,000 PPS |
| Traffic Generated | 2,500,000 packets per run |
| Throughput | ~616 Mbps (tcpreplay topspeed) |
| Dropped Packets (adaptive) | ~50% during detected burst |
| Final RX (baseline test) | 1,720,850 packets |

---

## System Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    Traffic Generator                     │
│              (tcpreplay / nping / ping -f)               │
└───────────────────────────┬─────────────────────────────┘
                            │ UDP/ICMP Packets
                            ▼
┌─────────────────────────────────────────────────────────┐
│                   TAP Interface (dtap0)                  │
│              Virtual Ethernet — DPDK controlled          │
└───────────────────────────┬─────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────┐
│                    DPDK PMD (Poll Mode)                  │
│         Bypasses kernel network stack entirely           │
│  ┌─────────────┐  ┌──────────────┐  ┌───────────────┐  │
│  │  RX Queue   │  │  Burst Detect│  │  Adaptive Drop│  │
│  │  (1024 desc)│→ │  PPS Monitor │→ │  50% on burst │  │
│  └─────────────┘  └──────────────┘  └───────────────┘  │
│                          │                               │
│                    ┌─────▼──────┐                        │
│                    │  TX Queue  │                        │
│                    │  Forward   │                        │
│                    └────────────┘                        │
└─────────────────────────────────────────────────────────┘
                            │
                            ▼
                   Final Statistics
              (RX / TX / Dropped Packets)
```

---

## Project Structure

```
dpdk-major-project/
├── README.md
├── src/
│   ├── dpdk_baseline.c         ← Baseline: RX + PPS monitoring + burst detection
│   ├── dpdk_adaptive.c         ← Adaptive: burst detection + packet dropping
│   ├── dpdk_validator.c        ← Packet validation (Ethernet/IP/UDP check)
│   └── Makefile
├── scripts/
│   ├── setup_hugepages.sh      ← Setup hugepages after every reboot
│   ├── bind_dpdk.sh            ← Bind NIC to vfio-pci for DPDK
│   ├── run_baseline.sh         ← Run baseline test
│   ├── run_adaptive.sh         ← Run adaptive test
│   └── generate_traffic.sh     ← Generate burst traffic
├── results/
│   ├── baseline_results.md     ← Baseline test output + analysis
│   └── adaptive_results.md     ← Adaptive test output + analysis
├── docs/
│   ├── ieee_paper.md           ← Full IEEE-style research paper
│   └── project_report.md       ← Detailed project report
└── presentation/
    └── slides_outline.md       ← Presentation structure
```

---

## Setup & Run

### Step 1 — Hugepages (after every reboot)
```bash
sudo bash scripts/setup_hugepages.sh
```

### Step 2 — Run Baseline Test (Terminal 1)
```bash
sudo ./src/dpdk_baseline -l 0-3 -n 4 --vdev=net_tap0
```

### Step 3 — Generate Traffic (Terminal 2)
```bash
sudo bash scripts/generate_traffic.sh
```

### Step 4 — Run Adaptive Test (Terminal 1)
```bash
sudo ./src/dpdk_adaptive -l 0-3 -n 4 --vdev=net_tap0
```

---

## Technologies Used

| Technology | Purpose |
|---|---|
| DPDK (Data Plane Development Kit) | High-speed packet processing framework |
| Poll Mode Driver (PMD) | Bypass kernel interrupts, direct NIC polling |
| TAP Interface (dtap0) | Virtual NIC used in lab environment |
| Hugepages | Large memory pages required by DPDK |
| tcpreplay | High-speed pcap traffic generator |
| nping (Nmap suite) | UDP packet burst generator |
| C Language | All DPDK application code |
| Kali Linux | Development and testing platform |
| vfio-pci | DPDK-compatible kernel module for NIC binding |

---

## Authors

**Abideepadarsan S K** — B.Tech CSE, SRM IST (2026)  
**Vikkesh P** — B.Tech CSE, SRM IST (2026)
