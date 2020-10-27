#include "send.h"

#include "interface.h"
#include "eth.h"
#include "twz.h"
#include "arp.h"
#include "ipv4.h"
#include "udp.h"


static bool arp_check(const char* interface_name,
                      ip_addr_t dst_ip)
{
    uint8_t* dst_mac_addr;
    if ((dst_mac_addr = arp_table_get(dst_ip.ip)) == NULL) {
        if (!is_arp_req_inflight(dst_ip.ip)) {
            /* send ARP Request packet */
            mac_addr_t mac = (mac_addr_t) {.mac = {0}};
            send_arp_packet(interface_name, ARP_REQUEST, mac, dst_ip);

            insert_arp_req(dst_ip.ip);
        }

        return false;
    }

    return true;
}


void send_arp_packet(const char* interface_name,
                     uint16_t opcode,
                     mac_addr_t dst_mac,
                     ip_addr_t dst_ip)
{
    /* calculate the size of packet buffer */
    int pkt_size = ETH_HDR_SIZE + ARP_HDR_SIZE;

    /* create packet buffer object to store packet that will the transfered */
    void* pkt_ptr = allocate_packet_buffer_object(pkt_size);

    /* add ARP header */
    char* arp_ptr = (char *)pkt_ptr;
    arp_ptr += ETH_HDR_SIZE;
    interface_t* interface = get_interface_by_name(interface_name);
    arp_tx(1, IPV4, HW_ADDR_SIZE, PROTO_ADDR_SIZE, opcode,
            interface->mac.mac, interface->ip.ip,
            dst_mac.mac, dst_ip.ip, arp_ptr);

    /* add Ethernet header */
    mac_addr_t broadcast_mac = (mac_addr_t) {
        .mac = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}
    };
    eth_tx(interface_name, (opcode == ARP_REQUEST) ? broadcast_mac : dst_mac,
            ARP, pkt_ptr, pkt_size);
}


int send_udp_packet(const char* interface_name,
                    object_id_t object_id,
                    uint8_t twz_op,
                    ip_addr_t dst_ip,
                    uint16_t src_port,
                    uint16_t dst_port,
                    char* payload)
{
    if (arp_check(interface_name, dst_ip) == false) {
        return EARP_WAIT;
    }

    bool has_twz_hdr = true;
    if (twz_op == NOOP) has_twz_hdr = false;

    /* calculate the size of packet buffer */
    uint16_t payload_size = 0;
    if (payload) {
        payload_size = strlen(payload);
    }
    int pkt_size;
    if (has_twz_hdr) {
        pkt_size = ETH_HDR_SIZE + TWZ_HDR_SIZE + IP_HDR_SIZE
            + UDP_HDR_SIZE + payload_size;
    } else {
        pkt_size = ETH_HDR_SIZE + IP_HDR_SIZE + UDP_HDR_SIZE + payload_size;
    }
    if (pkt_size > MAX_ETH_FRAME_SIZE) {
        return EMAX_FRAME_SIZE;
    }

    /* create packet buffer object to store packet that will the transfered */
    void* pkt_ptr = allocate_packet_buffer_object(pkt_size);

    /* add payload */
    if (payload) {
        char* payload_ptr = (char *)pkt_ptr;
        payload_ptr += (pkt_size - payload_size);
        strcpy(payload_ptr, payload);
    }

    /* add UDP header */
    char* udp_ptr = (char *)pkt_ptr;
    udp_ptr += (pkt_size - UDP_HDR_SIZE - payload_size);
    udp_tx(src_port, dst_port, udp_ptr, (UDP_HDR_SIZE + payload_size));

    /* add IPv4 header */
    char* ip_ptr = (char *)pkt_ptr;
    ip_ptr += (pkt_size - IP_HDR_SIZE - UDP_HDR_SIZE - payload_size);
    ip_tx(interface_name, dst_ip, UDP, ip_ptr,
            (IP_HDR_SIZE + UDP_HDR_SIZE + payload_size));

    if (has_twz_hdr) {
        /* add Twizzler header */
        char* twz_ptr = (char *)pkt_ptr;
        twz_ptr += (pkt_size - TWZ_HDR_SIZE - IP_HDR_SIZE
                - UDP_HDR_SIZE - payload_size);
        twz_tx(object_id, twz_op, IPV4, twz_ptr);
    }

    /* add Ethernet header */
    uint8_t* dst_mac_addr = arp_table_get(dst_ip.ip);
    mac_addr_t dst_mac;
    memcpy(dst_mac.mac, dst_mac_addr, MAC_ADDR_SIZE);
    eth_tx(interface_name, dst_mac, (has_twz_hdr) ? TWIZZLER : IPV4,
            pkt_ptr, pkt_size);

    return 0;
}
