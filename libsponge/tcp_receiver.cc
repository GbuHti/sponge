#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    if (seg.header().syn) {
        _syn = true;
        _ISN = seg.header().seqno;
    }
    if(_syn) {
        uint64_t absSeqno = unwrap(seg.header().seqno, _ISN, _checkPoint);
        uint64_t index = seg.header().syn ? absSeqno : absSeqno - 1;
        DEBUG_LOG("seqno=%u | absSeqno=%lu | payload size=%zu\n", seg.header().seqno.raw_value(), absSeqno, seg.payload().size());
        _reassembler.push_substring(seg.payload().copy(), index, seg.header().fin);
        _checkPoint = unwrap(ackno().value_or(static_cast<WrappingInt32>(0)),  _ISN, _checkPoint);
        /* 不可以直接使用seg.header().fin */
        _fin = _reassembler.stream_out().input_ended();
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const
{
    if (_syn) {
        if (_fin) {
            return _ISN + _reassembler.stream_out().bytes_written() + 2;
        }
        return _ISN + _reassembler.stream_out().bytes_written() + 1;
    }
    return nullopt;
}

size_t TCPReceiver::window_size() const
{
    return _reassembler.stream_out().remaining_capacity();
}
