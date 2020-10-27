#include "arp.h"

#include "interface.h"
#include "send.h"


std::map<uint8_t*,uint8_t*> arp_table;
std::mutex arp_table_mutex;

std::vector<uint8_t*> inflight_arp_req;
std::mutex inflight_arp_req_mutex;


void arp_table_insert(uint8_t* proto_addr,
                      uint8_t* hw_addr)
{
    std::map<uint8_t*,uint8_t*>::iterator it;

    arp_table_mutex.lock();
    uint8_t* key = (uint8_t *)malloc(sizeof(uint8_t)*PROTO_ADDR_SIZE);
    uint8_t* value = (uint8_t *)malloc(sizeof(uint8_t)*MAC_ADDR_SIZE);
    memcpy(key, proto_addr, PROTO_ADDR_SIZE);
    memcpy(value, hw_addr, MAC_ADDR_SIZE);
    bool found;
    for (it = arp_table.begin(); it != arp_table.end(); ++it) {
        found = true;
        for (int i = 0; i < PROTO_ADDR_SIZE; ++i) {
            if (it->first[i] != proto_addr[i]) {
                found = false;
                break;
            }
        }
        if (found) break;
    }
    if (!found) {
        arp_table[key] = value;
    } else {
        it->second = value;
    }
    arp_table_mutex.unlock();
}


uint8_t* arp_table_get(uint8_t* proto_addr)
{
    std::map<uint8_t*,uint8_t*>::iterator it;

    arp_table_mutex.lock();
    bool found;
    for (it = arp_table.begin(); it != arp_table.end(); ++it) {
        found = true;
        for (int i = 0; i < PROTO_ADDR_SIZE; ++i) {
            if (it->first[i] != proto_addr[i]) {
                found = false;
                break;
            }
        }
        if (found) break;
    }
    if (!found) {
        arp_table_mutex.unlock();
        return NULL;
    } else {
        arp_table_mutex.unlock();
        return it->second;
    }
}


void arp_table_delete(uint8_t* proto_addr)
{
    std::map<uint8_t*,uint8_t*>::iterator it;

    arp_table_mutex.lock();
    bool found;
    for (it = arp_table.begin(); it != arp_table.end(); ++it) {
        found = true;
        for (int i = 0; i < PROTO_ADDR_SIZE; ++i) {
            if (it->first[i] != proto_addr[i]) {
                found = false;
                break;
            }
        }
        if (found) {
            free(it->first);
            free(it->second);
            arp_table.erase(it);
            break;
        }
    }
    arp_table_mutex.unlock();
}


void arp_table_view()
{
    std::map<uint8_t*,uint8_t*>::iterator it;

    arp_table_mutex.lock();
    fprintf(stdout, "ARP Table:\n");
    fprintf(stdout, "------------------------------------------\n");
    for (it = arp_table.begin(); it != arp_table.end(); ++it) {
        int i;
        for (i = 0; i < PROTO_ADDR_SIZE-1; ++i) {
            fprintf(stdout, "%d.", it->first[i]);
        }
        fprintf(stdout, "%d -> ", it->first[i]);

        for (i = 0; i < HW_ADDR_SIZE-1; ++i) {
            fprintf(stdout, "%02X:", it->second[i]);
        }
        fprintf(stdout, "%02X\n", it->second[i]);
    }
    fprintf(stdout, "------------------------------------------\n");
    arp_table_mutex.unlock();
}


void arp_table_clear()
{
    std::map<uint8_t*,uint8_t*>::iterator it;

    arp_table_mutex.lock();
    for (it = arp_table.begin(); it != arp_table.end(); ++it) {
        arp_table.erase(it);
    }
    arp_table_mutex.unlock();
}


void insert_arp_req(uint8_t* proto_addr)
{
    inflight_arp_req_mutex.lock();
    uint8_t* key = (uint8_t *)malloc(sizeof(uint8_t)*PROTO_ADDR_SIZE);
    memcpy(key, proto_addr, PROTO_ADDR_SIZE);
    inflight_arp_req.push_back(key);
    inflight_arp_req_mutex.unlock();
}


void delete_arp_req(uint8_t* proto_addr)
{
    std::vector<uint8_t*>::iterator it;

    inflight_arp_req_mutex.lock();
    bool found;
    for (it = inflight_arp_req.begin(); it != inflight_arp_req.end(); ++it) {
        found = true;
        for (int i = 0; i < PROTO_ADDR_SIZE; ++i) {
            if ((*it)[i] != proto_addr[i]) {
                found = false;
                break;
            }
        }
        if (found) {
            free(*it);
            inflight_arp_req.erase(it);
            break;
        }
    }
    inflight_arp_req_mutex.unlock();
}


bool is_arp_req_inflight(uint8_t* proto_addr)
{
    std::vector<uint8_t*>::iterator it;

    inflight_arp_req_mutex.lock();
    bool found;
    for (it = inflight_arp_req.begin(); it != inflight_arp_req.end(); ++it) {
        found = true;
        for (int i = 0; i < PROTO_ADDR_SIZE; ++i) {
            if ((*it)[i] != proto_addr[i]) {
                found = false;
                break;
            }
        }
        if (found) {
            inflight_arp_req_mutex.unlock();
            return true;
        }
    }
    inflight_arp_req_mutex.unlock();

    return false;
}


void view_arp_req_inflight()
{
    std::vector<uint8_t*>::iterator it;

    inflight_arp_req_mutex.lock();
    fprintf(stdout, "Inflight ARP Requests:\n");
    fprintf(stdout, "------------------------------------------\n");
    for (it = inflight_arp_req.begin(); it != inflight_arp_req.end(); ++it) {
        int i;
        for (i = 0; i < PROTO_ADDR_SIZE-1; ++i) {
            fprintf(stdout, "%d.", (*it)[i]);
        }
        fprintf(stdout, "%d\n", (*it)[i]);
    }
    fprintf(stdout, "------------------------------------------\n");
    inflight_arp_req_mutex.unlock();
}


void arp_tx(uint16_t hw_type,
            uint16_t proto_type,
            uint8_t hw_addr_len,
            uint8_t proto_addr_len,
            uint16_t opcode,
            uint8_t sender_hw_addr[HW_ADDR_SIZE],
            uint8_t sender_proto_addr[PROTO_ADDR_SIZE],
            uint8_t target_hw_addr[HW_ADDR_SIZE],
            uint8_t target_proto_addr[PROTO_ADDR_SIZE],
            void* pkt_ptr)
{
    arp_hdr_t* arp_hdr = (arp_hdr_t *)pkt_ptr;

    arp_hdr->hw_type = htons(hw_type);

    arp_hdr->proto_type = htons(proto_type);

    arp_hdr->hw_addr_len = hw_addr_len;

    arp_hdr->proto_addr_len = proto_addr_len;

    arp_hdr->opcode = htons(opcode);

    memcpy(arp_hdr->sender_hw_addr, sender_hw_addr, HW_ADDR_SIZE);

    memcpy(arp_hdr->sender_proto_addr, sender_proto_addr, PROTO_ADDR_SIZE);

    memcpy(arp_hdr->target_hw_addr, target_hw_addr, HW_ADDR_SIZE);

    memcpy(arp_hdr->target_proto_addr, target_proto_addr, PROTO_ADDR_SIZE);
}


void arp_rx(const char* interface_name,
            void* pkt_ptr)
{
    interface_t* interface = get_interface_by_name(interface_name);

    arp_hdr_t* arp_hdr = (arp_hdr_t *)pkt_ptr;

    switch (ntohs(arp_hdr->opcode)) {
        case ARP_REQUEST:
            arp_table_insert(arp_hdr->sender_proto_addr, arp_hdr->sender_hw_addr);

            for (int i = 0; i < PROTO_ADDR_SIZE; ++i) {
                if (arp_hdr->target_proto_addr[i] != interface->ip.ip[i]) {
                    return;
                }
            }

            mac_addr_t dst_mac;
            ip_addr_t dst_ip;
            memcpy(dst_mac.mac, arp_hdr->sender_hw_addr, HW_ADDR_SIZE);
            memcpy(dst_ip.ip, arp_hdr->sender_proto_addr, PROTO_ADDR_SIZE);

            /* send ARP Reply packet */
            send_arp_packet(interface_name, ARP_REPLY, dst_mac, dst_ip);
            break;

        case ARP_REPLY:
            arp_table_insert(arp_hdr->sender_proto_addr, arp_hdr->sender_hw_addr);
            delete_arp_req(arp_hdr->sender_proto_addr);
            break;

        default:
            fprintf(stderr, "arp_rx: unrecognized opcode; packet dropped\n");
    }
}
