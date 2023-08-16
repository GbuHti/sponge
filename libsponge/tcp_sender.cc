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
    , _stream(capacity) {
    _retransmission_timeout = _initial_retransmission_timeout;
}

size_t TCPSender::bytes_in_flight() const
{
    if (_segments_outstanding.empty()) {
        return 0;
    } else {
        return _segments_outstanding.back().length_in_sequence_space() +
               (_segments_outstanding.back().header().seqno -
               _segments_outstanding.front().header().seqno);
    }
}

void TCPSender::fill_window()
{
    cout << "------- eof=" << _stream.eof() << " buff_size=" << _stream.buffer_size() << endl;
    while(((_stream.buffer_size() > 0) || (_next_seqno == 0) || (_stream.eof())) && (_windowSize > 0)) {
        size_t maxDataLenReadFromByteStream{};
        if (_stream.buffer_size() < _windowSize) {
            maxDataLenReadFromByteStream = _stream.buffer_size();
        } else {
            maxDataLenReadFromByteStream = _windowSize;
        }
        if (maxDataLenReadFromByteStream > TCPConfig::MAX_PAYLOAD_SIZE) {
            maxDataLenReadFromByteStream = TCPConfig::MAX_PAYLOAD_SIZE;
        }

        cout << "--- maxDataLenReadFromByteStream=" << maxDataLenReadFromByteStream << endl;
        TCPSegment tcpSegment;
        tcpSegment.payload() = _stream.read(maxDataLenReadFromByteStream);
        tcpSegment.header().sport = 0;
        tcpSegment.header().dport = 0;
        tcpSegment.header().seqno = wrap(_next_seqno, _isn);
        tcpSegment.header().ackno = WrappingInt32{0};
        tcpSegment.header().syn = (_next_seqno == 0);
        tcpSegment.header().fin = _stream.eof();
        tcpSegment.header().win = false;
        tcpSegment.header().ack = false;
        tcpSegment.header().doff = 0;
        tcpSegment.header().cksum = 0;

        _segments_out.push(tcpSegment);
        _segments_outstanding.push(tcpSegment);
        _windowSize -= tcpSegment.length_in_sequence_space();
        _next_seqno += tcpSegment.length_in_sequence_space();
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size)
{
    uint64_t absAckno = unwrap(ackno, _isn, _next_seqno);
    while(!_segments_outstanding.empty()) {
        TCPSegment segment = _segments_outstanding.front();
        WrappingInt32 seqno = segment.header().seqno + segment.length_in_sequence_space();
        uint64_t absSeqno = unwrap(seqno, _isn, _next_seqno);
        cout << "absAckno=" << absAckno << " absSeqnno=" << absSeqno << endl;
        if (absSeqno <= absAckno) {
            _segments_outstanding.pop();
            _consecutive_retransmissions = 0;
            _retransmission_timeout = _initial_retransmission_timeout;
        } else {
            break;
        }
    }
    size_t newWindow = absAckno + window_size - _next_seqno;
    _windowSize = (newWindow > 0) ? newWindow : 0;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick)
{
    _timeInFlight += ms_since_last_tick;
    if ((_timeInFlight - _lastTimeout) >= _retransmission_timeout) {
        _lastTimeout = _timeInFlight;
        _retransmission_timeout *= 2;
        _consecutive_retransmissions++;
        if (!_segments_outstanding.empty()) {
            TCPSegment tcpSegment = _segments_outstanding.front();
            _segments_out.push(tcpSegment);
        }
    }
}

unsigned int TCPSender::consecutive_retransmissions() const
{
    return _consecutive_retransmissions;
}

void TCPSender::send_empty_segment() {}
