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
            ReassemblerTestHarness test{65000};

            test.execute(SubmitSegment{"b", 1});

            test.execute(BytesAssembled(0));
            test.execute(BytesAvailable(""));
            test.execute(NotAtEof{});
        }

        {
            cout << "===== 2" << endl;
            ReassemblerTestHarness test{65000};

            test.execute(SubmitSegment{"b", 1});
            test.execute(SubmitSegment{"a", 0});

            test.execute(BytesAssembled(2));
            test.execute(BytesAvailable("ab"));
            test.execute(NotAtEof{});
        }

        {
            cout << "===== 3" << endl;
            ReassemblerTestHarness test{65000};

            test.execute(SubmitSegment{"b", 1}.with_eof(true));

            test.execute(BytesAssembled(0));
            test.execute(BytesAvailable(""));
            test.execute(NotAtEof{});

            test.execute(SubmitSegment{"a", 0});

            test.execute(BytesAssembled(2));
            test.execute(BytesAvailable("ab"));
            test.execute(AtEof{});
        }

        {
            cout << "===== 4" << endl;
            ReassemblerTestHarness test{65000};

            test.execute(SubmitSegment{"b", 1});
            test.execute(SubmitSegment{"ab", 0});

            test.execute(BytesAssembled(2));
            test.execute(BytesAvailable("ab"));
            test.execute(NotAtEof{});
        }

        {
            cout << "===== 5" << endl;
            ReassemblerTestHarness test{65000};

            test.execute(SubmitSegment{"b", 1});
            test.execute(BytesAssembled(0));
            test.execute(BytesAvailable(""));
            test.execute(NotAtEof{});

            test.execute(SubmitSegment{"d", 3});
            test.execute(BytesAssembled(0));
            test.execute(BytesAvailable(""));
            test.execute(NotAtEof{});

            test.execute(SubmitSegment{"c", 2});
            test.execute(BytesAssembled(0));
            test.execute(BytesAvailable(""));
            test.execute(NotAtEof{});

            test.execute(SubmitSegment{"a", 0});

            test.execute(BytesAssembled(4));
            test.execute(BytesAvailable("abcd"));
            test.execute(NotAtEof{});
        }

        {
            cout << "===== 6" << endl;
            ReassemblerTestHarness test{65000};

            test.execute(SubmitSegment{"b", 1});
            test.execute(BytesAssembled(0));
            test.execute(BytesAvailable(""));
            test.execute(NotAtEof{});

            test.execute(SubmitSegment{"d", 3});
            test.execute(BytesAssembled(0));
            test.execute(BytesAvailable(""));
            test.execute(NotAtEof{});

            test.execute(SubmitSegment{"abc", 0});

            test.execute(BytesAssembled(4));
            test.execute(BytesAvailable("abcd"));
            test.execute(NotAtEof{});
        }

        {
            cout << "===== 7" << endl;
            ReassemblerTestHarness test{65000};

            test.execute(SubmitSegment{"b", 1});
            test.execute(BytesAssembled(0));
            test.execute(BytesAvailable(""));
            test.execute(NotAtEof{});

            test.execute(SubmitSegment{"d", 3});
            test.execute(BytesAssembled(0));
            test.execute(BytesAvailable(""));
            test.execute(NotAtEof{});

            test.execute(SubmitSegment{"a", 0});
            test.execute(BytesAssembled(2));
            test.execute(BytesAvailable("ab"));
            test.execute(NotAtEof{});

            test.execute(SubmitSegment{"c", 2});
            test.execute(BytesAssembled(4));
            test.execute(BytesAvailable("cd"));
            test.execute(NotAtEof{});

            test.execute(SubmitSegment{"", 4}.with_eof(true));
            test.execute(BytesAssembled(4));
            test.execute(BytesAvailable(""));
            test.execute(AtEof{});
        }
    } catch (const exception &e) {
        cerr << "Exception: " << e.what() << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
