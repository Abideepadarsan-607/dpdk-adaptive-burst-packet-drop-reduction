/*
 * dpdk_validator.c
 * DPDK Packet Validation — Ethernet + IP + UDP header checks
 * Major Project — DPDK Adaptive Burst-Aware Packet Drop Reduction
 * Authors: Abideepadarsan SK, Vikkesh P
 * SRM IST, 2026
 *
 * Validates incoming packets:
 *   1. Ethernet frame length check (min 64 bytes, max 1518 bytes)
 *   2. EtherType check (IPv4 = 0x0800 only)
 *   3. IP version check (must be IPv4)
 *   4. IP header length check (>= 20 bytes)
 *   5. UDP/ICMP protocol check
 *   Drops malformed or unknown packets silently.
 */

#include <stdint.h>
#include <stdio.h>
#include <signal.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include <rte_cycles.h>

#define NUM_MBUFS        8192
#define MBUF_CACHE_SIZE  250
#define RX_RING_SIZE     1024
#define TX_RING_SIZE     1024
#define BURST_SIZE       32
#define BURST_THRESHOLD  200000

static volatile int keep_running = 1;

static void signal_handler(int sig) {
    (void)sig;
    keep_running = 0;
}

/*
 * validate_packet()
 * Returns 1 if packet is valid, 0 if it should be dropped.
 */
static int validate_packet(struct rte_mbuf *pkt)
{
    /* 1. Minimum Ethernet frame length */
    if (pkt->pkt_len < 64 || pkt->pkt_len > 1518)
        return 0;

    /* 2. Ethernet header */
    struct rte_ether_hdr *eth_hdr =
        rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);

    /* Only accept IPv4 (EtherType 0x0800) */
    if (rte_be_to_cpu_16(eth_hdr->ether_type) != RTE_ETHER_TYPE_IPV4)
        return 0;

    /* 3. IPv4 header */
    struct rte_ipv4_hdr *ip_hdr =
        (struct rte_ipv4_hdr *)(eth_hdr + 1);

    /* IP version must be 4 */
    if ((ip_hdr->version_ihl >> 4) != 4)
        return 0;

    /* IP header length must be at least 20 bytes */
    uint8_t ihl = (ip_hdr->version_ihl & 0x0F) * 4;
    if (ihl < 20)
        return 0;

    /* 4. Accept only UDP (17) or ICMP (1) */
    if (ip_hdr->next_proto_id != IPPROTO_UDP &&
        ip_hdr->next_proto_id != IPPROTO_ICMP)
        return 0;

    return 1; /* Packet is valid */
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
    uint64_t invalid    = 0;
    int      burst_active = 0;

    if (rte_eal_init(argc, argv) < 0)
        rte_exit(EXIT_FAILURE, "EAL init failed\n");

    signal(SIGINT,  signal_handler);
    signal(SIGTERM, signal_handler);

    mbuf_pool = rte_pktmbuf_pool_create(
        "MBUF_POOL", NUM_MBUFS, MBUF_CACHE_SIZE, 0,
        RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

    if (!mbuf_pool)
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

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
    printf("  DPDK Validator + Adaptive Control    \n");
    printf("========================================\n");
    printf("Accepts : IPv4 UDP/ICMP only\n");
    printf("Drops   : malformed, non-IPv4, short frames\n");
    printf("Burst   : 50%% adaptive drop above %d PPS\n\n", BURST_THRESHOLD);

    hz       = rte_get_timer_hz();
    last_tsc = rte_get_timer_cycles();

    while (keep_running)
    {
        uint16_t nb_rx = rte_eth_rx_burst(port_id, 0, bufs, BURST_SIZE);

        if (nb_rx > 0)
        {
            struct rte_mbuf *valid_bufs[BURST_SIZE];
            uint16_t valid_count = 0;

            /* Step 1: Validate each packet */
            for (int i = 0; i < nb_rx; i++) {
                if (validate_packet(bufs[i])) {
                    valid_bufs[valid_count++] = bufs[i];
                } else {
                    rte_pktmbuf_free(bufs[i]);
                    invalid++;
                }
            }

            total_rx += nb_rx;
            rx_counter += valid_count;

            uint16_t start_index = 0;

            /* Step 2: Adaptive dropping during burst */
            if (burst_active && valid_count > 0) {
                uint16_t drop_count = valid_count / 2;
                for (int i = 0; i < drop_count; i++) {
                    rte_pktmbuf_free(valid_bufs[i]);
                    dropped++;
                }
                start_index = drop_count;
            }

            /* Step 3: Forward remaining valid packets */
            uint16_t to_send = valid_count - start_index;
            if (to_send > 0) {
                uint16_t sent = rte_eth_tx_burst(
                    port_id, 0, &valid_bufs[start_index], to_send);
                total_tx += sent;
                for (int i = start_index + sent; i < valid_count; i++)
                    rte_pktmbuf_free(valid_bufs[i]);
            }
        }

        uint64_t now = rte_get_timer_cycles();
        if (now - last_tsc >= hz)
        {
            uint64_t pps = rx_counter;
            printf("PPS = %lu  |  Invalid = %lu  |  Dropped = %lu\n",
                   pps, invalid, dropped);

            if (pps >= BURST_THRESHOLD) {
                if (!burst_active) {
                    printf("🚨 BURST DETECTED — Adaptive drop activated\n\n");
                    burst_active = 1;
                }
            } else {
                burst_active = 0;
            }

            rx_counter = 0;
            last_tsc   = now;
        }
    }

    printf("\n========================================\n");
    printf("          Final Statistics              \n");
    printf("========================================\n");
    printf("Total RX        : %lu\n", total_rx);
    printf("Total TX        : %lu\n", total_tx);
    printf("Invalid Packets : %lu\n", invalid);
    printf("Dropped (burst) : %lu\n", dropped);
    printf("Total Loss      : %lu\n", total_rx - total_tx);

    rte_eth_dev_stop(port_id);
    rte_eth_dev_close(port_id);
    return 0;
}
