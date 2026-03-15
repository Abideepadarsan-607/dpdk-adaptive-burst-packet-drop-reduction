/*
 * dpdk_baseline.c
 * DPDK Baseline: Real-time PPS monitoring + Burst Detection
 * Major Project — DPDK Adaptive Burst-Aware Packet Drop Reduction
 * Authors: Abideepadarsan SK, Vikkesh P
 * SRM IST, 2026
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
#define BURST_THRESHOLD  200000   /* PPS threshold for burst detection */

static volatile int keep_running = 1;

static void signal_handler(int sig) {
    (void)sig;
    keep_running = 0;
}

int main(int argc, char **argv)
{
    struct rte_mempool *mbuf_pool;
    struct rte_mbuf   *bufs[BURST_SIZE];
    uint16_t port_id  = 0;
    uint64_t hz, last_tsc;
    uint64_t rx_counter = 0;
    uint64_t total_rx   = 0;
    uint64_t total_tx   = 0;

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

    printf("\nDPDK Baseline — PPS Monitor + Burst Detection\n");
    printf("Burst threshold : %d PPS\n", BURST_THRESHOLD);
    printf("Waiting for traffic...\n\n");

    hz       = rte_get_timer_hz();
    last_tsc = rte_get_timer_cycles();

    /* ── Main RX Loop ──────────────────────────────────────── */
    while (keep_running) {
        uint16_t nb_rx = rte_eth_rx_burst(port_id, 0, bufs, BURST_SIZE);

        if (nb_rx > 0) {
            rx_counter += nb_rx;
            total_rx   += nb_rx;

            /* Forward all received packets */
            uint16_t sent = rte_eth_tx_burst(port_id, 0, bufs, nb_rx);
            total_tx += sent;

            /* Free any unsent packets */
            for (int i = sent; i < nb_rx; i++)
                rte_pktmbuf_free(bufs[i]);
        }

        /* ── PPS Calculation (every 1 second) ─────────────── */
        uint64_t now = rte_get_timer_cycles();
        if (now - last_tsc >= hz) {
            uint64_t pps = rx_counter;
            printf("PPS = %lu", pps);

            if (pps >= BURST_THRESHOLD)
                printf("  <-- 🚨 BURST DETECTED! PPS = %lu\n", pps);
            else
                printf("\n");

            rx_counter = 0;
            last_tsc   = now;
        }
    }

    /* ── Final Statistics ──────────────────────────────────── */
    printf("\n=== Final Statistics ===\n");
    printf("RX Packets  : %lu\n", total_rx);
    printf("TX Packets  : %lu\n", total_tx);
    printf("Dropped     : %lu\n", total_rx - total_tx);

    rte_eth_dev_stop(port_id);
    rte_eth_dev_close(port_id);
    return 0;
}
