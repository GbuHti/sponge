#include "byte_stream.hh"
#include "fsm_stream_reassembler_harness.hh"
#include "stream_reassembler.hh"
#include "util.hh"

#include <exception>
#include <iostream>

using namespace std;

int main() {
    try {
        {
            cout << "===== 1" << endl;
            // Overlapping assembled (unread) section
            const size_t cap = {1000};
            ReassemblerTestHarness test{cap};

            test.execute(SubmitSegment{"a", 0});
            test.execute(SubmitSegment{"ab", 0});

            test.execute(BytesAssembled(2));
            test.execute(BytesAvailable("ab"));
        }

        {
            cout << "===== 2" << endl;
            // Overlapping assembled (read) section
            const size_t cap = {1000};
            ReassemblerTestHarness test{cap};

            test.execute(SubmitSegment{"a", 0});
            test.execute(BytesAvailable("a"));

            test.execute(SubmitSegment{"ab", 0});
            test.execute(BytesAvailable("b"));
            test.execute(BytesAssembled(2));
        }

        {
            cout << "===== 3" << endl;
            // Overlapping unassembled section, resulting in assembly
            const size_t cap = {1000};
            ReassemblerTestHarness test{cap};

            test.execute(SubmitSegment{"b", 1});
            test.execute(BytesAvailable(""));

            test.execute(SubmitSegment{"ab", 0});
            test.execute(BytesAvailable("ab"));
            test.execute(UnassembledBytes{0});
            test.execute(BytesAssembled(2));
        }
        {
            cout << "===== 4" << endl;
            // Overlapping unassembled section, not resulting in assembly
            const size_t cap = {1000};
            ReassemblerTestHarness test{cap};

            test.execute(SubmitSegment{"b", 1});
            test.execute(BytesAvailable(""));

            test.execute(SubmitSegment{"bc", 1});
            test.execute(BytesAvailable(""));
            test.execute(UnassembledBytes{2});
            test.execute(BytesAssembled(0));
        }
        {
            cout << "===== 5" << endl;
            // Overlapping unassembled section, not resulting in assembly
            const size_t cap = {1000};
            ReassemblerTestHarness test{cap};

            test.execute(SubmitSegment{"c", 2});
            test.execute(BytesAvailable(""));

            test.execute(SubmitSegment{"bcd", 1});
            test.execute(BytesAvailable(""));
            test.execute(UnassembledBytes{3});
            test.execute(BytesAssembled(0));
        }

        {
            cout << "===== 6" << endl;
            // Overlapping multiple unassembled sections
            const size_t cap = {1000};
            ReassemblerTestHarness test{cap};

            test.execute(SubmitSegment{"b", 1});
            test.execute(SubmitSegment{"d", 3});
            test.execute(BytesAvailable(""));

            test.execute(SubmitSegment{"bcde", 1});
            test.execute(BytesAvailable(""));
            test.execute(BytesAssembled(0));
            test.execute(UnassembledBytes(4));
        }

        {
            cout << "===== 7" << endl;
            // Submission over existing
            const size_t cap = {1000};
            ReassemblerTestHarness test{cap};

            test.execute(SubmitSegment{"c", 2});
            test.execute(SubmitSegment{"bcd", 1});

            test.execute(BytesAvailable(""));
            test.execute(BytesAssembled(0));
            test.execute(UnassembledBytes(3));

            test.execute(SubmitSegment{"a", 0});
            test.execute(BytesAvailable("abcd"));
            test.execute(BytesAssembled(4));
            test.execute(UnassembledBytes(0));
        }

        {
            cout << "===== 8" << endl;
            // Submission within existing
            const size_t cap = {1000};
            ReassemblerTestHarness test{cap};

            test.execute(SubmitSegment{"bcd", 1});
            test.execute(SubmitSegment{"c", 2});

            test.execute(BytesAvailable(""));
            test.execute(BytesAssembled(0));
            test.execute(UnassembledBytes(3));

            test.execute(SubmitSegment{"a", 0});
            test.execute(BytesAvailable("abcd"));
            test.execute(BytesAssembled(4));
            test.execute(UnassembledBytes(0));
        }

    } catch (const exception &e) {
        cerr << "Exception: " << e.what() << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
