#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

Interval::Interval(const string &str, size_t index, uint32_t firstUnacceptable, bool eof){

    _interval[0] = index;
    _interval[1] = (index + str.size() - 1 < firstUnacceptable) ? index + str.size() - 1 : firstUnacceptable - 1;
    _s = str.substr(0, _interval[1] - _interval[0] + 1);
    _eof = (index + str.size() < firstUnacceptable) ? eof : false;
}

Interval::Interval(const vector<uint32_t> &interval, uint32_t firstUnacceptable, bool eof): _eof(eof) {
    _interval[0] = interval[0];
    _interval[1] = (interval[1] < firstUnacceptable) ? interval[1] : firstUnacceptable - 1;
    _s = string(_interval[1]-_interval[0] + 1, 0);
    _eof = (interval[1] < firstUnacceptable) ? eof : false;
}

StreamReassembler::StreamReassembler(const size_t capacity): _output(capacity), _capacity(capacity) {}

void insertNewInterval(vector<Interval> &intervals, Interval newInterval) {
    for (auto it = intervals.begin(); it != intervals.end(); it++) {
        if (newInterval._interval[0] < it->_interval[0]) {
            intervals.insert(it, newInterval);
            return;
        }
    }
    intervals.insert(intervals.end(), newInterval);
}

//void clearOldInterval(vector<vector<int>> &intervals) {
//    for (auto it = crossBlock_.rbegin(); it != crossBlock_.rend(); it++) {
//        intervals.erase(intervals.begin() + *it);
//    }
//}

string getPostionStr(StreamReassembler::Position position)
{
    switch(position) {
        case StreamReassembler::Position::LL:
            return "LL";
        case StreamReassembler::Position::LM:
            return "LM";
        case StreamReassembler::Position::MM:
            return "MM";
        case StreamReassembler::Position::MR:
            return "MR";
        case StreamReassembler::Position::RR:
            return "RR";
        case StreamReassembler::Position::LR:
            return "LR";
        default:
            return "Unrecognized";
    }
}

StreamReassembler::Position getPosition(vector<uint32_t> &interval, vector<uint32_t> &newInterval) {
    if (interval[0] < newInterval[0]) {
        if (interval[1] < newInterval[0] - 1) {
            return StreamReassembler::Position::LL;
        } else if (interval[1] >= newInterval[0] - 1 && interval[1] <= newInterval[1] + 1) {
            return StreamReassembler::Position::LM;
        } else {
            return StreamReassembler::Position::LR;
        }
    } else if (interval[0] >= newInterval[0] - 1 && interval[0] <= newInterval[1] + 1) {
        if (interval[1] >= newInterval[0] - 1 && interval[1] <= newInterval[1] + 1) {
            return StreamReassembler::Position::MM;
        } else {
            return StreamReassembler::Position::MR;
        }
    } else {
        return StreamReassembler::Position::RR;
    }
}

vector<uint32_t> GetUpdatedInterval(vector<Interval> &intervals, vector<uint32_t> &newInterval) {
    int64_t mostLeft = -1;
    int64_t mostRight = -1;
    bool stop = false;
    for (uint64_t i = 0; i < intervals.size(); i++) {
        StreamReassembler::Position position = getPosition(intervals[i]._interval, newInterval);
        if (position == StreamReassembler::Position::LM) {
            mostLeft = intervals[i]._interval[0];
        } else if (position == StreamReassembler::Position::MR) {
            mostRight = intervals[i]._interval[1];
        } else if (position == StreamReassembler::Position::MM) {
        } else if (position == StreamReassembler::Position::LR) {
            stop = true;
        }
    }

    if (stop) {
        return {};
    }

    if (mostLeft == -1 && mostRight == -1) {
        return newInterval;
    } else if (mostLeft == -1 && mostRight != -1) {
        return {newInterval[0], static_cast<uint32_t>(mostRight)};
    } else if (mostLeft != -1 && mostRight == -1) {
        return {static_cast<uint32_t>(mostLeft), newInterval[1]};
    } else if (mostLeft != -1 && mostRight != -1) {
        return {static_cast<uint32_t>(mostLeft),static_cast<uint32_t>(mostRight)};
    }
    return {};
}


void displayVector(vector<uint32_t> &interval)
{
    cout << "[" << interval[0] << "," << interval[1] << "]" << endl;
}

void displayIntervals(vector<Interval> &intervals)
{
    for (auto it : intervals) {
        displayVector(it._interval);
        cout << it._s << endl;
    }
}

//! If exist one, it must be the first one.
vector<Interval>::iterator OneIntervalContainFirstUnassemble(vector<Interval> &intervals, uint32_t firstUnAssembled)
{
    for (auto it = intervals.begin(); it != intervals.end(); it++) {
        if (firstUnAssembled >= it->_interval[0]) {
            return it;
        }
    }
    return intervals.end();
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    size_t firstUnacceptable = _firstUnassembled + _output.remaining_capacity();
    if (index >= firstUnacceptable || (data.size() < 1 && eof == false)) {
        return;
    }
    Interval newInterval(data, index, firstUnacceptable, eof);
    vector<uint32_t> interval = GetUpdatedInterval(_intervals, newInterval._interval);
    Interval updatedInterval(interval, firstUnacceptable, eof);
    updatedInterval._s.replace(newInterval._interval[0] - updatedInterval._interval[0], newInterval._s.size(), newInterval._s);
    // Lookup Intervals
    auto it = _intervals.begin();
    while(it != _intervals.end()) {
        if (getPosition(it->_interval, updatedInterval._interval) == StreamReassembler::Position::MM) {
            size_t pos = it->_interval[0] - updatedInterval._interval[0];
            updatedInterval._s.replace(pos, it->_s.size(), it->_s);
            if (it->_eof == true) {
                updatedInterval._eof = true;
            }
            it = _intervals.erase(it);
        } else {
            it++;
        }
    }

    // Update Intervals
    insertNewInterval(_intervals, updatedInterval);

    vector<Interval>::iterator theOne = OneIntervalContainFirstUnassemble(_intervals, _firstUnassembled);
    if ( theOne != _intervals.end()) {
        _output.write(theOne->_s.substr(_firstUnassembled - theOne->_interval[0], theOne->_interval[1] - _firstUnassembled + 1));
        if (theOne->_eof == true) {
            _output.end_input();
        }
        _firstUnassembled = theOne->_interval[1] + 1;
        _intervals.erase(theOne);

    }
    (void)eof;
}


size_t StreamReassembler::unassembled_bytes() const {
    size_t cnt = 0;
    for (auto it : _intervals) {
        cnt += it._s.size();
    }
    return cnt;
}

bool StreamReassembler::empty() const {
    if (unassembled_bytes() == 0) {
        return true;
    }
    return false;
}

