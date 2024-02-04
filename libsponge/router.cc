#include "router.hh"

#include <iostream>

using namespace std;

// Dummy implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
//! \param[in] interface_num The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
         << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";

    _router_table.push_back({route_prefix, prefix_length, next_hop, interface_num});
}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    const uint32_t dst_ip_addr = dgram.header().dst;
    auto max_matched_entry = _router_table.end();
    // 开始查询
    for (auto router_entry_iter = _router_table.begin(); router_entry_iter != _router_table.end();
         router_entry_iter++) {
        // 如果前缀匹配匹配长度为 0，或者前缀匹配相同
        if (router_entry_iter->prefix_length == 0 ||
            (router_entry_iter->route_prefix ^ dst_ip_addr) >> (32 - router_entry_iter->prefix_length) == 0) {
            // 如果条件符合，则更新最匹配的条目
            if (max_matched_entry == _router_table.end() ||
                max_matched_entry->prefix_length < router_entry_iter->prefix_length)
                max_matched_entry = router_entry_iter;
        }
    }
    // 将数据包 TTL 减去1
    // 如果存在最匹配的，并且数据包仍然存活，则将其转发
    if (max_matched_entry != _router_table.end() && dgram.header().ttl-- > 1) {
        const optional<Address> next_hop = max_matched_entry->next_hop;
        AsyncNetworkInterface &interface = _interfaces[max_matched_entry->interface_idx];
        if (next_hop.has_value())
            interface.send_datagram(dgram, next_hop.value());
        else
            interface.send_datagram(dgram, Address::from_ipv4_numeric(dst_ip_addr));
    }
    // 其他情况下则丢弃该数据包
}

void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    for (auto &interface : _interfaces) {
        auto &queue = interface.datagrams_out();
        while (not queue.empty()) {
            route_one_datagram(queue.front());
            queue.pop();
        }
    }
}