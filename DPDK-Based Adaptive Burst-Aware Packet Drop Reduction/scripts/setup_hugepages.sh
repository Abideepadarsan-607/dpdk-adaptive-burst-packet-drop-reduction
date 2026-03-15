#!/bin/bash
# setup_hugepages.sh
# Run this after every reboot before starting DPDK
# Usage: sudo bash setup_hugepages.sh

echo "============================================"
echo "  DPDK Hugepage Setup"
echo "============================================"

# Allocate 1024 x 2MB hugepages = 2GB
echo 1024 | sudo tee /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages

# Mount hugetlbfs if not already mounted
if ! mountpoint -q /dev/hugepages; then
    sudo mkdir -p /dev/hugepages
    sudo mount -t hugetlbfs nodev /dev/hugepages
    echo "[+] Hugepages mounted at /dev/hugepages"
else
    echo "[+] Hugepages already mounted"
fi

# Verify
HUGEPAGES=$(cat /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages)
FREE=$(cat /sys/kernel/mm/hugepages/hugepages-2048kB/free_hugepages)

echo ""
echo "Total hugepages : $HUGEPAGES"
echo "Free hugepages  : $FREE"
echo ""

if [ "$FREE" -gt "0" ]; then
    echo "[OK] Hugepages ready. You can now run DPDK."
else
    echo "[FAIL] No free hugepages. Check system memory."
fi

# Load vfio-pci module
echo ""
echo "Loading kernel modules..."
sudo modprobe vfio-pci
sudo modprobe vfio
echo "[+] vfio-pci loaded"
echo "============================================"
