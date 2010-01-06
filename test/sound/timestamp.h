#include <cxxtest/TestSuite.h>

#include "sound/timestamp.h"

class TimestampTestSuite : public CxxTest::TestSuite
{
	public:
	void test_diff_add() {
		const Audio::Timestamp a(10000, 1000);
		const Audio::Timestamp b(10001, 1000);
		const Audio::Timestamp c(10002, 1000);

		TS_ASSERT_EQUALS(a.frameDiff(b), -1);
		TS_ASSERT_EQUALS(b.frameDiff(a), 1);
		TS_ASSERT_EQUALS(c.frameDiff(a), 2);
		TS_ASSERT_EQUALS(b.addFrames(2000).frameDiff(a), 2001);
		TS_ASSERT_EQUALS(a.frameDiff(b), -1);
		TS_ASSERT_EQUALS(b.frameDiff(a), 1);
		TS_ASSERT_EQUALS(c.frameDiff(a), 2);
		TS_ASSERT_EQUALS(b.addFrames(2000).frameDiff(a.addFrames(-1000)), 3001);
		TS_ASSERT_EQUALS(a.frameDiff(b), -1);
		TS_ASSERT_EQUALS(b.frameDiff(a), 1);
		TS_ASSERT_EQUALS(c.frameDiff(a), 2);
	}

	void test_ticks() {
		const Audio::Timestamp a(1234, 60);
		const Audio::Timestamp b(5678, 60);

		TS_ASSERT_EQUALS(a.msecs(), (uint32)1234);
		TS_ASSERT_EQUALS(b.msecs(), (uint32)5678);

		TS_ASSERT_EQUALS(a.msecsDiff(b), 1234 - 5678);
		TS_ASSERT_EQUALS(b.msecsDiff(a), 5678 - 1234);

		TS_ASSERT_EQUALS(a.frameDiff(b), (1234 - 5678) * 60 / 1000);
		TS_ASSERT_EQUALS(b.frameDiff(a), (5678 - 1234) * 60 / 1000);

		TS_ASSERT_EQUALS(a.addFrames(1).msecs(), (uint32)(1234 + 1000 * 1/60));
		TS_ASSERT_EQUALS(a.addFrames(59).msecs(), (uint32)(1234 + 1000 * 59/60));
		TS_ASSERT_EQUALS(a.addFrames(60).msecs(), (uint32)(1234 + 1000 * 60/60));

		// As soon as we go back even by only one frame, the msec value
		// has to drop by at least one.
		TS_ASSERT_EQUALS(a.addFrames(-1).msecs(), (uint32)(1234 - 1000 * 1/60 - 1));
		TS_ASSERT_EQUALS(a.addFrames(-59).msecs(), (uint32)(1234 - 1000 * 59/60 - 1));
		TS_ASSERT_EQUALS(a.addFrames(-60).msecs(), (uint32)(1234 - 1000 * 60/60));
	}

	void test_more_add_diff() {
		const Audio::Timestamp c(10002, 1000);

		for (int i = -10000; i < 10000; i++) {
			int v = c.addFrames(i).frameDiff(c);
			TS_ASSERT_EQUALS(v, i);
		}
	}


	void test_diff_with_conversion() {
		const Audio::Timestamp a = Audio::Timestamp(10, 1000).addFrames(20);
		const Audio::Timestamp b = Audio::Timestamp(10, 1000/5).addFrames(20/5);
		const Audio::Timestamp c = Audio::Timestamp(10, 1000*2).addFrames(20*2);

		TS_ASSERT_EQUALS(a.frameDiff(a), 0);
		TS_ASSERT_EQUALS(a.frameDiff(b), 0);
		TS_ASSERT_EQUALS(a.frameDiff(c), 0);

		TS_ASSERT_EQUALS(b.frameDiff(a), 0);
		TS_ASSERT_EQUALS(b.frameDiff(b), 0);
		TS_ASSERT_EQUALS(b.frameDiff(c), 0);

		TS_ASSERT_EQUALS(c.frameDiff(a), 0);
		TS_ASSERT_EQUALS(c.frameDiff(b), 0);
		TS_ASSERT_EQUALS(c.frameDiff(c), 0);
	}


	void test_convert() {
		const Audio::Timestamp a = Audio::Timestamp(10, 1000).addFrames(20);
		const Audio::Timestamp b = Audio::Timestamp(10, 1000/5).addFrames(20/5);
		const Audio::Timestamp c = Audio::Timestamp(10, 1000*2).addFrames(20*2);

		TS_ASSERT_EQUALS(a.convertToFramerate(1000/5), b);
		TS_ASSERT_EQUALS(a.convertToFramerate(1000*2), c);
	}

	void test_equals() {
		const Audio::Timestamp a = Audio::Timestamp(500, 1000);
		Audio::Timestamp b = Audio::Timestamp(0, 1000);
		Audio::Timestamp c = Audio::Timestamp(0, 100);

		TS_ASSERT(a != b);
		TS_ASSERT(a != c);
		TS_ASSERT(b == c);

		b = b.addFrames(500);
		c = c.addFrames(50);

		TS_ASSERT(a == b);
		TS_ASSERT(a == c);
		TS_ASSERT(b == c);
	}


	void test_framerate() {
		const Audio::Timestamp a = Audio::Timestamp(500, 1000);
		const Audio::Timestamp b = Audio::Timestamp(500, 67);
		const Audio::Timestamp c = Audio::Timestamp(500, 100);
		const Audio::Timestamp d = Audio::Timestamp(500, 44100);


		TS_ASSERT_EQUALS(a.getFramerate(), 1000);
		TS_ASSERT_EQUALS(b.getFramerate(), 67);
		TS_ASSERT_EQUALS(c.getFramerate(), 100);
		TS_ASSERT_EQUALS(d.getFramerate(), 44100);
	}
};
