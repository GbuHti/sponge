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
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();

    EthernetFrame frame {};
    frame.header().src = _ethernet_address;
    auto matchedEa = _table.find(next_hop_ip);
    if ( (matchedEa != _table.end()) && ((_current_time - matchedEa->second._last_mapping_time) <= 30*1000U)) {
        frame.header().dst = matchedEa->second._ethernet_address;
        frame.header().type = EthernetHeader::TYPE_IPv4;
        frame.payload().append(dgram.serialize());
        _frames_out.push(frame);
    } else {
        _cached_dgram[next_hop_ip].push(dgram);
        if ((_current_time - _last_send_time_of_arp > 5000U) || (_isFirstTime)) {
            frame.header().dst = ETHERNET_BROADCAST;
            frame.header().type = EthernetHeader::TYPE_ARP;
            ARPMessage arpMessage{};
            arpMessage.opcode = ARPMessage::OPCODE_REQUEST;
            arpMessage.sender_ethernet_address = _ethernet_address;
            arpMessage.sender_ip_address = _ip_address.ipv4_numeric();
            arpMessage.target_ethernet_address = {};
            arpMessage.target_ip_address = next_hop_ip;
            frame.payload().append(arpMessage.serialize());
            _frames_out.push(frame);
            _last_send_time_of_arp = _current_time;
            _isFirstTime = false;
        }
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame)
{
    if ((frame.header().dst != ETHERNET_BROADCAST) && (frame.header().dst != _ethernet_address)) {
        return {};
    }
    switch(frame.header().type) {
        case EthernetHeader::TYPE_ARP : {
            ARPMessage arpMessage{};
            auto ret = arpMessage.parse(frame.payload());
            if (ret != ParseResult::NoError) {
                ERROR_LOG("Failed to parse ARP frame, ret=%d", ret);
            }
            _table[arpMessage.sender_ip_address] = {_current_time, arpMessage.sender_ethernet_address};
            if (arpMessage.target_ip_address != _ip_address.ipv4_numeric()) {
                ERROR_LOG("Illegal ARP=%s", arpMessage.to_string().c_str());
                return {};
            }
            if (arpMessage.opcode == ARPMessage::OPCODE_REQUEST) {
                arpMessage.opcode = ARPMessage::OPCODE_REPLY;
                arpMessage.target_ethernet_address = arpMessage.sender_ethernet_address;
                arpMessage.target_ip_address = arpMessage.sender_ip_address;
                arpMessage.sender_ethernet_address = _ethernet_address;
                arpMessage.sender_ip_address = _ip_address.ipv4_numeric();
                EthernetFrame frameArp {};
                frameArp.header().dst = frame.header().src;
                frameArp.header().src = _ethernet_address;
                frameArp.header().type = frame.header().type;
                frameArp.payload().append(arpMessage.serialize());
                _frames_out.push(frameArp);
            } else if (arpMessage.opcode == ARPMessage::OPCODE_REPLY) {
                // 将缓存没有发出去的InternetDatagram发送出去
                while(!_cached_dgram[arpMessage.sender_ip_address].empty()) {
                    EthernetFrame stashedFrame {};
                    stashedFrame.header().dst = arpMessage.sender_ethernet_address;
                    stashedFrame.header().src = arpMessage.target_ethernet_address;
                    stashedFrame.header().type = EthernetHeader::TYPE_IPv4;
                    stashedFrame.payload().append(_cached_dgram[arpMessage.sender_ip_address].front().serialize());
                    _cached_dgram[arpMessage.sender_ip_address].pop();
                    _frames_out.push(stashedFrame);
                }
            }
            return {};
        }
        case EthernetHeader::TYPE_IPv4: {
            IPv4Datagram dgram {};
            auto res = dgram.parse(frame.payload());
            if (res != ParseResult::NoError) {
                ERROR_LOG("Failed to parse IPv4 frame, ret=%d", res);
                return {};
            }
            return dgram;
        }
        default:
            ERROR_LOG("Unknown Ethernet type=%u", frame.header().type);
            return {};
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) { _current_time += ms_since_last_tick;
}
