/*
 * dpdk_adaptive.c
 * DPDK Adaptive Burst Control — Real PPS + Adaptive Packet Dropping
 * Major Project — DPDK Adaptive Burst-Aware Packet Drop Reduction
 * Authors: Abideepadarsan SK, Vikkesh P
 * SRM IST, 2026
 *
 * How it works:
 *  1. Polls NIC using DPDK PMD (no kernel interrupts)
 *  2. Measures real PPS every second
 *  3. On burst detection (PPS >= BURST_THRESHOLD):
 *       - Drops 50% of incoming packets immediately
 *       - Forwards remaining 50%
 *  4. On normal traffic (PPS < BURST_THRESHOLD):
 *       - Forwards all packets
 *  5. Prints final RX / TX / Dropped statistics
 */

#include <stdint.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_mbuf.h>

#define NUM_MBUFS        8192
#define MBUF_CACHE_SIZE  250
#define RX_RING_SIZE     1024
#define TX_RING_SIZE     1024
#define BURST_SIZE       32
#define BURST_THRESHOLD  200000    /* PPS above this = burst */

static volatile int keep_running = 1;

static void signal_handler(int sig) {
    (void)sig;
    keep_running = 0;
}

int main(int argc, char **argv)
{
    struct rte_mempool *mbuf_pool;
    struct rte_mbuf   *bufs[BURST_SIZE];
    uint16_t port_id   = 0;
    uint64_t hz, last_tsc;
    uint64_t rx_counter = 0;
    uint64_t total_rx   = 0;
    uint64_t total_tx   = 0;
    uint64_t dropped    = 0;
    int      burst_active = 0;

    /* ── EAL Init ──────────────────────────────────────────── */
    if (rte_eal_init(argc, argv) < 0)
        rte_exit(EXIT_FAILURE, "EAL init failed\n");

    signal(SIGINT,  signal_handler);
    signal(SIGTERM, signal_handler);

    /* ── Memory Pool ───────────────────────────────────────── */
    mbuf_pool = rte_pktmbuf_pool_create(
        "MBUF_POOL", NUM_MBUFS, MBUF_CACHE_SIZE, 0,
        RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

    if (mbuf_pool == NULL)
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

    /* ── Port Configuration ────────────────────────────────── */
    struct rte_eth_conf port_conf = {0};

    if (rte_eth_dev_configure(port_id, 1, 1, &port_conf) < 0)
        rte_exit(EXIT_FAILURE, "Port configure failed\n");

    if (rte_eth_rx_queue_setup(port_id, 0, RX_RING_SIZE,
        rte_eth_dev_socket_id(port_id), NULL, mbuf_pool) < 0)
        rte_exit(EXIT_FAILURE, "RX queue setup failed\n");

    if (rte_eth_tx_queue_setup(port_id, 0, TX_RING_SIZE,
        rte_eth_dev_socket_id(port_id), NULL) < 0)
        rte_exit(EXIT_FAILURE, "TX queue setup failed\n");

    if (rte_eth_dev_start(port_id) < 0)
        rte_exit(EXIT_FAILURE, "Port start failed\n");

    printf("\n========================================\n");
    printf("  DPDK Adaptive Burst Control Started  \n");
    printf("========================================\n");
    printf("Burst Threshold : %d PPS\n", BURST_THRESHOLD);
    printf("Drop Strategy   : 50%% drop during burst\n");
    printf("Waiting for traffic...\n\n");

    hz       = rte_get_timer_hz();
    last_tsc = rte_get_timer_cycles();

    /* ── Main Adaptive Loop ────────────────────────────────── */
    while (keep_running)
    {
        uint16_t nb_rx = rte_eth_rx_burst(port_id, 0, bufs, BURST_SIZE);

        if (nb_rx > 0)
        {
            rx_counter += nb_rx;
            total_rx   += nb_rx;

            uint16_t start_index = 0;

            /* ── Adaptive Packet Dropping ──────────────────── */
            if (burst_active)
            {
                /*
                 * Drop 50% of packets during burst.
                 * Strategy: free the first half immediately,
                 * forward the second half.
                 */
                uint16_t drop_count = nb_rx / 2;
                for (int i = 0; i < drop_count; i++) {
                    rte_pktmbuf_free(bufs[i]);
                    dropped++;
                }
                start_index = drop_count;
            }

            /* ── Forward Remaining Packets ─────────────────── */
            uint16_t to_send = nb_rx - start_index;
            uint16_t sent    = rte_eth_tx_burst(
                port_id, 0, &bufs[start_index], to_send);
            total_tx += sent;

            /* Free any packets that TX could not send */
            for (int i = start_index + sent; i < nb_rx; i++)
                rte_pktmbuf_free(bufs[i]);
        }

        /* ── PPS Calculation (every 1 second) ─────────────── */
        uint64_t now = rte_get_timer_cycles();
        if (now - last_tsc >= hz)
        {
            uint64_t pps = rx_counter;
            printf("PPS = %lu\n", pps);

            if (pps >= BURST_THRESHOLD)
            {
                if (!burst_active) {
                    printf("🚨 BURST DETECTED\n");
                    printf("Adaptive Control Activated — dropping 50%% of packets\n\n");
                    burst_active = 1;
                }
            }
            else
            {
                if (burst_active) {
                    printf("[INFO] Burst ended — resuming full forwarding\n\n");
                }
                burst_active = 0;
            }

            rx_counter = 0;
            last_tsc   = now;
        }
    }

    /* ── Final Statistics ──────────────────────────────────── */
    printf("\n========================================\n");
    printf("          Final Statistics              \n");
    printf("========================================\n");
    printf("RX Packets      : %lu\n",   total_rx);
    printf("TX Packets      : %lu\n",   total_tx);
    printf("Dropped Packets : %lu\n",   dropped);
    printf("Drop Rate       : %.2f%%\n",
        total_rx > 0 ? (dropped * 100.0 / total_rx) : 0.0);

    rte_eth_dev_stop(port_id);
    rte_eth_dev_close(port_id);
    return 0;
}
