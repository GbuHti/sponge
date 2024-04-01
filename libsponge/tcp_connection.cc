#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const
{
    return _sender.stream_in().remaining_capacity();
}

size_t TCPConnection::bytes_in_flight() const
{
    return _sender.bytes_in_flight();
}

size_t TCPConnection::unassembled_bytes() const
{
    return _receiver.unassembled_bytes();
}

size_t TCPConnection::time_since_last_segment_received() const { return _linger_time; }

void TCPConnection::segment_received(const TCPSegment &seg)
{
    _linger_time = 0;
    if (seg.header().rst) {
        ERROR_LOG("sport=%d, dport=%d\n", seg.header().sport, seg.header().dport);
        _receiver.stream_out().set_error(__FUNCTION__, __LINE__);
        _sender.stream_in().set_error(__FUNCTION__, __LINE__);
        UpdateStatus();
        return;
    }

    /* receiver handle this segment first */
    _receiver.segment_received(seg);

    /* then sender handle this segmnet */
    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
        /* 关于何时启动_sender */
        if ((_sender.get_tcp_sender_status() != SYN) || (seg.header().syn)) {
            _sender.fill_window();
        }
    }
    if (seg.length_in_sequence_space() != 0) {
        _sender.fill_window();
        if (_sender.segments_out().empty()) {
            TCPSegment segment{};
            _sender.segments_out().push(segment);
        }
    } else {
        if (_receiver.ackno().has_value() and seg.header().seqno == _receiver.ackno().value() - 1) {
            _sender.send_empty_segment();
        }
    }
    transfer_segment();
    UpdateStatus();
}

bool TCPConnection::active() const
{
    return _stat != Stat::CLOSE;
}

void TCPConnection::complete_segment(TCPSegment &segment) const {
    segment.header().ack = _receiver.ackno().has_value();
    segment.header().ackno = _receiver.ackno().value_or(static_cast<WrappingInt32>(0));
    if (_receiver.window_size() > numeric_limits<uint16_t>::max()) {
        segment.header().win = numeric_limits<uint16_t>::max();
    } else {
        segment.header().win = _receiver.window_size();
    }
}

size_t TCPConnection::write(const string &data) {
    size_t bytesWriten = _sender.stream_in().write(data);
    _sender.fill_window();
    DEBUG_LOG("[WRITE][DATA] data=%s\n", data.c_str());
    transfer_segment();
    UpdateStatus();
    return bytesWriten;
}

size_t TCPConnection::transfer_segment() {
    size_t cnt = 0;
    while(!_sender.segments_out().empty()) {
        cnt++;
        TCPSegment segment = _sender.segments_out().front();
        _sender.segments_out().pop();
        complete_segment(segment);
        DEBUG_LOG("[SEND][DATA] seq=%u syn=%d fin=%d payload length=%zu ackno=%u window_size=%d\n",
                  segment.header().seqno.raw_value(),
                  segment.header().syn, segment.header().fin,
                  segment.payload().str().size(),
                  segment.header().ackno.raw_value(),
                  segment.header().win);
        _segments_out.push(segment);
    }
    return cnt;
}

void TCPConnection::UpdateStatus()
{
    Stat oldStatus = _stat;
    switch(_stat) {
        case Stat::NORMAL:
            // 理论上两个条件一定不会在同一时刻发生
            if (_receiver.stream_out().input_ended()) {
                _stat = Stat::RECV_DONE;
            }
            if (_sender.get_tcp_sender_status() == FIN) {
                _stat = Stat::SEND_FIN;
            }
            if (_sender.stream_in().error() && _receiver.stream_out().error()) {
                _linger_after_streams_finish = false;
                _stat = Stat::CLOSE;
            }
            break;
        case Stat::SEND_FIN:
            if (_sender.get_tcp_sender_status() == FIN_ACKED) {
                _stat = Stat::SEND_FIN_ACK;
            }
            if (_sender.stream_in().error() && _receiver.stream_out().error()) {
                _linger_after_streams_finish = false;
                _stat = Stat::CLOSE;
            }
            break;
        case Stat::SEND_FIN_ACK:
            if (_receiver.stream_out().input_ended()) {
                _stat = Stat::LINGER;
            }
            if (_sender.stream_in().error() && _receiver.stream_out().error()) {
                _linger_after_streams_finish = false;
                _stat = Stat::CLOSE;
            }
            break;
        case Stat::RECV_DONE:
            _linger_after_streams_finish = false;
            if (_sender.get_tcp_sender_status() == FIN) {
                _stat = Stat::PASSIVE_CLOSE;
            }
            if (_sender.stream_in().error() && _receiver.stream_out().error()) {
                _linger_after_streams_finish = false;
                _stat = Stat::CLOSE;
            }
            break;
        case Stat::LINGER:
            if (_linger_time >= 10 * _cfg.rt_timeout) {
                _linger_after_streams_finish = false;
                _stat = Stat::CLOSE;
            }
            if (_sender.stream_in().error() && _receiver.stream_out().error()) {
                _linger_after_streams_finish = false;
                _stat = Stat::CLOSE;
            }
            break;
        case Stat::PASSIVE_CLOSE:
            if (_sender.get_tcp_sender_status() == FIN_ACKED) {
                _stat = Stat::CLOSE;
            }
            if (_sender.stream_in().error() && _receiver.stream_out().error()) {
                _linger_after_streams_finish = false;
                _stat = Stat::CLOSE;
            }
            break;
        case Stat::CLOSE:
            break;
        default:
            WARN_LOG("Unsupport status=%d\n", static_cast<int>(_stat));
    }
    if (oldStatus != _stat) {
        DEBUG_LOG("TCP_Connection status changed, from %s to %s\n",
                  print_stat_string(oldStatus).c_str(),
                  print_stat_string(_stat).c_str());
    }
}

string TCPConnection::print_stat_string(Stat s)
{
    switch(s) {
        CASE_STR(Stat::NORMAL);
        CASE_STR(Stat::SEND_FIN);
        CASE_STR(Stat::SEND_FIN_ACK);
        CASE_STR(Stat::RECV_DONE);
        CASE_STR(Stat::PASSIVE_CLOSE);
        CASE_STR(Stat::LINGER);
        CASE_STR(Stat::CLOSE);
        default:
            break;
    }
    return "UNKNOW STATUS";
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick)
{
    _linger_time += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    static_cast<void>(transfer_segment());
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        if (!_segments_out.empty()) {
            _segments_out.pop();
        }
        TCPSegment segment{};
        segment.header().rst = true;
        _segments_out.push(segment);
        _sender.stream_in().set_error(__FUNCTION__, __LINE__);
        _receiver.stream_out().set_error(__FUNCTION__, __LINE__);
    }
    UpdateStatus();
    //cout << "sender status: " << _sender.get_tcp_sender_status() << "receiver status: " << _receiver.stream_out().input_ended() << endl;
}

void TCPConnection::end_input_stream()
{
    _sender.stream_in().end_input();
    _sender.fill_window();
    transfer_segment();
    UpdateStatus();
}

void TCPConnection::connect()
{
    _sender.fill_window();
    transfer_segment();
    UpdateStatus();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
