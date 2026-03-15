#!/bin/bash
# generate_traffic.sh
# Generates burst traffic into dtap0 for DPDK testing
# Usage: sudo bash generate_traffic.sh

IFACE="dtap0"
PCAP="sample.pcap"

echo "============================================"
echo "  DPDK Burst Traffic Generator"
echo "============================================"

# Check interface exists
if ! ip link show "$IFACE" &>/dev/null; then
    echo "[ERROR] Interface $IFACE not found."
    echo "Make sure dpdk_baseline or dpdk_adaptive is running first."
    exit 1
fi

# Bring interface up
sudo ip link set "$IFACE" up
sudo ip addr add 10.10.10.2/24 dev "$IFACE" 2>/dev/null

echo "[+] Interface $IFACE is UP"
echo ""

# Check pcap file
if [ ! -f "$PCAP" ]; then
    echo "[!] sample.pcap not found. Creating one with tcpdump..."
    sudo timeout 5 tcpdump -i "$IFACE" -w "$PCAP" -c 25 2>/dev/null &
    sleep 1
    ping -c 5 10.10.10.1 -I "$IFACE" &>/dev/null
    wait
    echo "[+] sample.pcap created"
fi

echo "Starting burst traffic generation..."
echo "Press Ctrl+C to stop."
echo ""

# Method 1: tcpreplay at topspeed (best for high burst)
sudo tcpreplay \
    --intf1="$IFACE" \
    --topspeed \
    --loop=100000 \
    "$PCAP"

echo ""
echo "[+] Traffic generation complete."
echo "============================================"
