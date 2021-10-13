#include "tcp_receiver.hh"

#include <iostream>
// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    TCPHeader header = seg.header();

    // syn handler
    if (header.syn) {
        if (_syn)
            return;
        _isn = header.seqno.raw_value();
        _syn = true;
    }
    // fin handler
    if (_syn && header.fin) {
        _fin = true;
    }

    // push to _reassembler
    size_t index;
    if (header.syn) {
        index = 0;
    } else {
        index = unwrap(header.seqno, WrappingInt32(_isn), _check_point) - 1;
    }
    // std::cout << "index : " << index << std::endl;
    // std::cout << "data : " << seg.payload().copy() << std::endl;
    // std::cout << "fin : " << header.fin << std::endl;
    _reassembler.push_substring(seg.payload().copy(), index, header.fin);
    // std::cout << "bytes_written: " << stream_out().bytes_written() << std::endl;
    // std::cout << "index : "
    // calculate _check_point
    _check_point = stream_out().bytes_written();
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (not _syn) {
        return {};
    }
    if (!stream_out().input_ended()) {
        // syn(1) + data
        // std::cout << "not" << std::endl;
        return wrap(stream_out().bytes_written() + 1, WrappingInt32(_isn));
    } else {
        // syn(1) + data + eof(1)
        // std::cout << "stream_out().bytes_written() " << stream_out().bytes_written() << std::endl;
        return wrap(stream_out().bytes_written() + 2, WrappingInt32(_isn));
    }
}

size_t TCPReceiver::window_size() const { return _capacity - stream_out().buffer_size(); }
