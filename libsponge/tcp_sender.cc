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

void TCPSender::fill_window_helper(TCPSegment *tcpSegment, size_t size) {
    cout << "fill_window_helper size=" << size << endl;
    tcpSegment->payload() = _stream.read(size);
    tcpSegment->header().sport = 0;
    tcpSegment->header().dport = 0;
    tcpSegment->header().seqno = wrap(_next_seqno, _isn);
    tcpSegment->header().ackno = WrappingInt32{0};
    //tcpSegment->header().syn = (_next_seqno == 0);
    tcpSegment->header().syn = false;
    //tcpSegmentheader().fin = _stream.eof() && _stream.buffer_empty() && hasSpaceLeft;
    tcpSegment->header().fin = false;
    tcpSegment->header().win = false;
    tcpSegment->header().ack = false;
    tcpSegment->header().doff = 0;
    tcpSegment->header().cksum = 0;

    //_segments_out.push(tcpSegment);
    //_segments_outstanding.push(tcpSegment);
}

// 在没有数据发送的情况下, 但有window, 只发送syn 或者 fin
// 在有数据发送的情况下，没有window, 要发一个字节的payload.
size_t TCPSender::get_available_payload_len(size_t window) {
    size_t maxDataLenReadFromByteStream{};
    if (_stream.buffer_size() < window) {
        maxDataLenReadFromByteStream = _stream.buffer_size();
    } else {
        maxDataLenReadFromByteStream = window;
    }
    if (maxDataLenReadFromByteStream > TCPConfig::MAX_PAYLOAD_SIZE) {
        maxDataLenReadFromByteStream = TCPConfig::MAX_PAYLOAD_SIZE;
    }
    return maxDataLenReadFromByteStream;
}

void TCPSender::fill_window()
{
    bool zeroWindow = false;
    bool bufferEmpty = false;
    switch (_current_stat) {
        case SYN:
        {
            cout << "fill_window: in state SYN" << endl;
            TCPSegment tcpSegment{};
            fill_window_helper(&tcpSegment, 0);
            tcpSegment.header().syn = true;
            _segments_out.push(tcpSegment);
            _segments_outstanding.push(tcpSegment);
            _next_seqno += tcpSegment.length_in_sequence_space();
            set_tcp_sender_status(PAYLOAD);
            break;
        }
        case PAYLOAD:
            cout << "fill_window: In state PAYLOAD, windowsize=" << _windowSize << endl;
            while(_windowSize > 0) {
                if (_stream.buffer_size() > 0) {
                    TCPSegment tcpSegment{};
                    size_t availableDataLen = get_available_payload_len(_windowSize);
                    fill_window_helper(&tcpSegment, availableDataLen);
                    _windowSize -= tcpSegment.length_in_sequence_space();
                    if (_windowSize > 0 && _stream.buffer_empty() && _stream.eof()) {
                        tcpSegment.header().fin = true;
                        _windowSize -= tcpSegment.length_in_sequence_space();
                        _segments_out.push(tcpSegment);
                        _segments_outstanding.push(tcpSegment);
                        _next_seqno += tcpSegment.length_in_sequence_space();
                        set_tcp_sender_status(FIN);
                        break;
                    }
                    _segments_out.push(tcpSegment);
                    _segments_outstanding.push(tcpSegment);
                    _next_seqno += tcpSegment.length_in_sequence_space();
                } else if (_stream.eof()) {
                    TCPSegment tcpSegment{};
                    fill_window_helper(&tcpSegment, 0);
                    tcpSegment.header().fin = true;
                    _windowSize -= tcpSegment.length_in_sequence_space();
                    _next_seqno += tcpSegment.length_in_sequence_space();
                    _segments_out.push(tcpSegment);
                    _segments_outstanding.push(tcpSegment);
                    set_tcp_sender_status(FIN);
                    break;
                } else {
                    break;
                }
            }
            break;
        case WAIT_FOR_WINDOW:
        {
            cout << "fill_wiodows: In state WAIT_FOR_WINDOW" << endl;
            if (!_isEntry) {
                break;
            }
            _isEntry = false;
            if (_stream.buffer_size() > 0) {
                TCPSegment tcpSegment{};
                fill_window_helper(&tcpSegment, 1);
                _segments_out.push(tcpSegment);
                _segments_outstanding.push(tcpSegment);
                _next_seqno += tcpSegment.length_in_sequence_space();
            } else if (_stream.eof()) {
                TCPSegment tcpSegment{};
                fill_window_helper(&tcpSegment, 1);
                tcpSegment.header().fin = true;
                _segments_out.push(tcpSegment);
                _segments_outstanding.push(tcpSegment);
                _next_seqno += tcpSegment.length_in_sequence_space();
            } else {
                cout << "somethin wrong!!" << endl;
            }
            _retransmission_timeout = _initial_retransmission_timeout;
            break;
        }
        case FIN:
            set_tcp_sender_status(FIN);
            cout << "In status FIN" << endl;
            break;
        default:
            cout << "Error: unsupport status=" << _current_stat << endl;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size)
{
    uint64_t absAckno = unwrap(ackno, _isn, _next_seqno);
    if (absAckno > _next_seqno) {
        /* Impossible ackno (beyond next seqno) is ignored */
        cout << "receive impossible ackno=" << absAckno << " next_seq=" << _next_seqno << endl;
        return;
    }
    //bool ackFlag = false;
    while(!_segments_outstanding.empty()) {
        TCPSegment segment = _segments_outstanding.front();
        WrappingInt32 seqno = segment.header().seqno + segment.length_in_sequence_space();
        uint64_t absSeqno = unwrap(seqno, _isn, _next_seqno);
        cout << "ack_received: absAckno=" << absAckno  << " WinSize=" << window_size << " absSeqnno=" << absSeqno << endl;
        if (absSeqno <= absAckno) {
            _segments_outstanding.pop();
            _consecutive_retransmissions = 0;
            _retransmission_timeout = _initial_retransmission_timeout;
            _lastTimeout = _timeInFlight;
            //ackFlag = true;
        } else {
            break;
        }
    }
    size_t newWindow = absAckno + window_size - _next_seqno;
    _windowSize = newWindow;
    if (_windowSize > 0) {
        cout << "ack_received set state to PAYLOAD" << endl;
        set_tcp_sender_status(PAYLOAD);
    } else if (_windowSize == 0 && window_size == 0){
        cout << "ack_received set state to WAIT_FOR_WINDOW" << endl;
        set_tcp_sender_status(WAIT_FOR_WINDOW);
    } else {
        /* do nothing */
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick)
{
    _timeInFlight += ms_since_last_tick;
    if ((_timeInFlight - _lastTimeout) >= _retransmission_timeout) {
        _lastTimeout = _timeInFlight;
        if (_current_stat != WAIT_FOR_WINDOW) {
            _retransmission_timeout *= 2;
        }
        _consecutive_retransmissions++;
        if (!_segments_outstanding.empty()) {
            TCPSegment tcpSegment = _segments_outstanding.front();
            cout << "resend seq=" << tcpSegment.header().seqno << " payload=" << tcpSegment.payload().str() << endl;
            _segments_out.push(tcpSegment);
        }
    }
}

unsigned int TCPSender::consecutive_retransmissions() const
{
    return _consecutive_retransmissions;
}

void TCPSender::send_empty_segment() {}
