#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _retransmission_timeout(retx_timeout) {}

uint64_t TCPSender::bytes_in_flight() const { return _next_seqno - _ack_seqno; }

void TCPSender::fill_window() {
    if (!_syn_sent) {
        TCPSegment segment;
        segment.header().syn = true;
        segment.header().seqno = wrap(_next_seqno, _isn);
        _segments_out.push(segment);
        _segments_not_ack.push(segment);

        _next_seqno += 1;
        _syn_sent = true;

        if (!_time_running) {
            _time_running = true;
            _timer = 0;
        }
    } else {
        if (!_stream.buffer_size() && !_stream.eof()) {
            return;
        }
        // Lab4 behavior: if incoming_seg.length_in_sequence_space() is not zero, send ack.
        uint64_t remain = _window_size - bytes_in_flight();
        while (remain != 0 && !_fin_sent) {
            remain = read_and_send(remain);
            if (!_time_running) {
                _time_running = true;
                _timer = 0;
            }
        }
    }
}

size_t TCPSender::read_and_send(size_t window_size) {
    string payload = _stream.read(min(window_size, TCPConfig::MAX_PAYLOAD_SIZE));

    TCPSegment segment;
    segment.header().seqno = wrap(_next_seqno, _isn);
    segment.payload() = move(payload);
    _next_seqno += segment.length_in_sequence_space();

    if (_stream.eof() && (segment.length_in_sequence_space() < _window_size)) {
        segment.header().fin = true;
        _next_seqno += 1;
        _fin_sent = true;
    }

    if (segment.length_in_sequence_space() == 0)
        return 0;

    _segments_out.push(segment);
    _segments_not_ack.push(segment);

    return _window_size - bytes_in_flight();
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    if (window_size == 0) {
        _window_size = 1;
        _no_space_window = true;
    } else {
        _window_size = window_size;
        _no_space_window = false;
    }

    uint64_t ack = unwrap(ackno, _isn, _ack_seqno);
    if (ack > _next_seqno || ack <= _ack_seqno)
        return;
    if (ack > _ack_seqno) {
        _ack_seqno = ack;
    }

    while (!_segments_not_ack.empty()) {
        size_t length = _segments_not_ack.front().length_in_sequence_space();
        size_t index = unwrap(_segments_not_ack.front().header().seqno, _isn, _ack_seqno);

        if (index + length - 1 <= _ack_seqno - 1) {
            _segments_not_ack.pop();
        } else {
            break;
        }
    }

    _retransmission_timeout = _initial_retransmission_timeout;
    _consecutive_retransmission = 0;

    if (!_segments_not_ack.empty()) {
        _time_running = true;
        _timer = 0;
    }

    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (!_time_running)
        return;
    _timer += ms_since_last_tick;
    if (_timer >= _retransmission_timeout && !_segments_not_ack.empty()) {
        if (!_no_space_window) {
            _retransmission_timeout *= 2;
        }

        _segments_out.push(_segments_not_ack.front());
        _consecutive_retransmission++;

        _time_running = true;
        _timer = 0;
    }

    if (_segments_not_ack.empty()) {
        _time_running = false;
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmission; }

void TCPSender::send_empty_segment() {
    TCPSegment segment;
    segment.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(segment);
}