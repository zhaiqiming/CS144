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
void DUMMY_CODE(Targs &&... /* unused */) {}

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

    _dataItem.emplace_back(DataItem{route_prefix, prefix_length, next_hop, interface_num});
}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    if (dgram.header().ttl <= 1) {
        return;
    }
    dgram.header().ttl--;
    // int tem = -1, temlen = -1;
    DataItem target;
    int match_length = -1;
    const uint32_t dst = dgram.header().dst;
    for(const auto item : _dataItem) {
        if(item._prefix_length == 0 && match_length == -1) {
            match_length = 0;
            target = item;
        } else if(dst >> (32 - item._prefix_length) == item._route_prefix >> (32 - item._prefix_length) && match_length < item._prefix_length) {
            match_length = item._prefix_length;
            target = item;
        }
    }
    if(match_length == -1) {
        return ;
    }
    if(target._next_hop != nullopt) {
        interface(target._interface_num).send_datagram(dgram, *(target._next_hop));
    } else {
        interface(target._interface_num).send_datagram(dgram,  Address::from_ipv4_numeric(dst));
    }
    // for (long unsigned int i = 0; i < _dataItem.size(); i++) {
    //     uint8_t rs = 32 - _pl[i];
    //     if ((rs == 32 || dster >> rs == _rp[i] >> rs) && temlen < 32 - rs) {
    //         temlen = 32 - rs;
    //         tem = i;
    //     }
    // }
    // if (tem < 0) {
    //     return;
    // }
    // if (_nh[tem])
    //     interface(_in[tem]).send_datagram(dgram, *(_nh[tem]));
    // else
    //     interface(_in[tem]).send_datagram(dgram, Address::from_ipv4_numeric(dster));
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
