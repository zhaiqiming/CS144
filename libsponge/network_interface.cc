#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
    _macAddressTable[_ip_address.ipv4_numeric()] = _ethernet_address;
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    EthernetFrame ethernetFrame;
    EthernetHeader ethernetHeader;
    if (_macAddressTable.count(next_hop_ip)) {
        ethernetHeader.dst = _macAddressTable[next_hop_ip];
        ethernetHeader.src = _ethernet_address;
        ethernetHeader.type = EthernetHeader::TYPE_IPv4;

        ethernetFrame.payload() = dgram.serialize();
        ethernetFrame.header() = ethernetHeader;

        _frames_out.push(ethernetFrame);
    } else {
        _internetDatagram.push(dgram);
        _next_hop_ip.push(next_hop_ip);
        if (_arpRequestTimeStamp.count(next_hop_ip))
            return;

        // ethernetHeader
        ethernetHeader.dst = ETHERNET_BROADCAST;
        ethernetHeader.src = _ethernet_address;
        ethernetHeader.type = EthernetHeader::TYPE_ARP;
        ethernetFrame.header() = ethernetHeader;

        // ARP body
        ARPMessage message;
        message.opcode = ARPMessage::OPCODE_REQUEST;
        message.sender_ip_address = _ip_address.ipv4_numeric();
        message.target_ip_address = next_hop_ip;
        message.sender_ethernet_address = _ethernet_address;
        ethernetFrame.payload() = Buffer(message.serialize());

        _frames_out.push(ethernetFrame);
        _arpRequestTimeStamp[next_hop_ip] = _time;
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if (frame.header().dst != _ethernet_address && frame.header().dst != ETHERNET_BROADCAST) {
        return nullopt;
    } else if (frame.header().type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram rec;
        if (rec.parse(frame.payload()) == ParseResult::NoError) {
            uint32_t ipAddress = rec.header().src;
            _macAddressTable[ipAddress] = frame.header().src;
            _macAddressTimeStamp[ipAddress] = _time;
            return rec;
        }
    } else if (frame.header().type == EthernetHeader::TYPE_ARP) {
        ARPMessage recarp;
        if (recarp.parse(frame.payload()) != ParseResult::NoError) {
            return {};
        }

        uint32_t sender_ip_address = recarp.sender_ip_address;
        EthernetAddress sender_ethernet_address = recarp.sender_ethernet_address;
        _macAddressTable[sender_ip_address] = sender_ethernet_address;
        _macAddressTimeStamp[sender_ip_address] = _time;

        if (recarp.opcode == ARPMessage::OPCODE_REQUEST && _macAddressTable.count(recarp.target_ip_address)) {
            ARPMessage arpme;
            arpme.opcode = ARPMessage::OPCODE_REPLY;
            arpme.sender_ip_address = recarp.target_ip_address;
            arpme.sender_ethernet_address = _macAddressTable[recarp.target_ip_address];
            arpme.target_ethernet_address = sender_ethernet_address;
            arpme.target_ip_address = sender_ip_address;

            EthernetHeader ethernetHeader;
            ethernetHeader.dst = sender_ethernet_address;
            ethernetHeader.src = _ethernet_address;
            ethernetHeader.type = EthernetHeader::TYPE_ARP;
            EthernetFrame ethernetFrame;
            ethernetFrame.header() = ethernetHeader;
            ethernetFrame.payload() = Buffer(arpme.serialize());

            _frames_out.push(ethernetFrame);
        } else {
            _macAddressTable[recarp.target_ip_address] = recarp.target_ethernet_address;
            _macAddressTimeStamp[recarp.target_ip_address] = _time;
        }
        try_to_send();
    }
    return nullopt;
}

void NetworkInterface::try_to_send() {
    size_t num = _next_hop_ip.size();
    for (size_t i = 0; i < num; i++) {
        InternetDatagram dgra = _internetDatagram.front();
        uint32_t dip = _next_hop_ip.front();
        _next_hop_ip.pop();
        _internetDatagram.pop();

        if (_macAddressTable.count(dip)) {
            EthernetHeader ethernetHeader;
            ethernetHeader.dst = _macAddressTable[dip];
            ethernetHeader.src = _ethernet_address;
            ethernetHeader.type = EthernetHeader::TYPE_IPv4;

            EthernetFrame sende;
            sende.payload() = dgra.serialize();
            sende.header() = ethernetHeader;
            _frames_out.push(sende);
        } else {
            _internetDatagram.push(dgra);
            _next_hop_ip.push(dip);
        }
    }
}

void NetworkInterface::tick(const size_t ms_since_last_tick) {
    _time += ms_since_last_tick;
    for (auto it = _macAddressTimeStamp.begin(); it != _macAddressTimeStamp.end();) {
        if (it->second + 30000 <= _time) {
            _macAddressTable.erase(it->first);
            _macAddressTimeStamp.erase(it++);
        } else {
            it++;
        }
    }
    for (auto it = _arpRequestTimeStamp.begin(); it != _arpRequestTimeStamp.end();) {
        if (it->second + 5000 <= _time) {
            _arpRequestTimeStamp.erase(it++);
        } else {
            it++;
        }
    }
}
