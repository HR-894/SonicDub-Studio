#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "core/segment.h"
#include "stages/translator.h"
#include "stages/tts_engine.h"
#include "core/thread_pool.h"

using namespace vd;
using namespace testing;

// ── Mock Translator ───────────────────────────────────
class MockTranslator : public ITranslator {
public:
    MOCK_METHOD(std::string, translate, 
        (const std::string&, const std::string&, const std::string&), (override));
    std::string name() const override { return "MockTranslator"; }
};

// ── Mock TTS ─────────────────────────────────────────
class MockTTS : public ITTSEngine {
public:
    MOCK_METHOD(std::vector<uint8_t>, synthesize,
        (const std::string&, const std::string&, const std::string&), (override));
    std::string name() const override { return "MockTTS"; }
};

// ── Segment Tests ─────────────────────────────────────
TEST(SegmentTest, DurationCalculation) {
    Segment seg;
    seg.start_ms = 1000;
    seg.end_ms   = 3500;
    EXPECT_EQ(seg.duration_ms(), 2500);
}

TEST(SegmentTest, DefaultState) {
    Segment seg;
    EXPECT_EQ(seg.state, Segment::State::Raw);
    EXPECT_EQ(seg.id, 0u);
}

// ── Thread Pool Tests ─────────────────────────────────
TEST(ThreadPoolTest, BasicSubmit) {
    ThreadPool pool(4);
    std::atomic<int> counter{0};
    std::vector<std::future<void>> futures;

    for (int i = 0; i < 20; ++i) {
        futures.push_back(pool.submit([&]{ ++counter; }));
    }

    for (auto& f : futures) f.wait();
    EXPECT_EQ(counter.load(), 20);
}

TEST(ThreadPoolTest, ReturnValue) {
    ThreadPool pool(2);
    auto f = pool.submit([]{ return 42; });
    EXPECT_EQ(f.get(), 42);
}

TEST(ThreadPoolTest, ExceptionPropagation) {
    ThreadPool pool(2);
    auto f = pool.submit([]() -> int { throw std::runtime_error("test error"); return 0; });
    EXPECT_THROW(f.get(), std::runtime_error);
}

// ── Translator Mock Tests ─────────────────────────────
TEST(TranslatorTest, MockTranslation) {
    MockTranslator translator;
    EXPECT_CALL(translator, translate("Hello", "en", "hi"))
        .WillOnce(Return("नमस्ते"));
    
    auto result = translator.translate("Hello", "en", "hi");
    EXPECT_EQ(result, "नमस्ते");
}

TEST(TranslatorTest, EmptyTextHandling) {
    MockTranslator translator;
    EXPECT_CALL(translator, translate("", _, _))
        .WillOnce(Return(""));
    
    EXPECT_EQ(translator.translate("", "en", "hi"), "");
}

// ── TTS Mock Tests ────────────────────────────────────
TEST(TTSTest, MockSynthesize) {
    MockTTS tts;
    std::vector<uint8_t> fake_audio = {0x52,0x49,0x46,0x46}; // "RIFF"
    
    EXPECT_CALL(tts, synthesize("नमस्ते", "hi", "hi-IN-Standard-A"))
        .WillOnce(Return(fake_audio));
    
    auto audio = tts.synthesize("नमस्ते", "hi", "hi-IN-Standard-A");
    EXPECT_EQ(audio.size(), 4u);
    EXPECT_EQ(audio[0], 0x52);
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
