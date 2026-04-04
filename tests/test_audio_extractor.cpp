#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

// Test utilities
namespace {

// Create a minimal valid WAV file for testing
void create_test_wav(const std::string& path, int duration_sec = 1) {
    int sample_rate = 16000;
    int channels    = 1;
    int num_samples = sample_rate * duration_sec;
    uint32_t data_size  = num_samples * 2;
    uint32_t file_size  = 36 + data_size;

    std::ofstream f(path, std::ios::binary);
    
    auto w32 = [&](uint32_t v){ f.write((char*)&v, 4); };
    auto w16 = [&](uint16_t v){ f.write((char*)&v, 2); };

    f.write("RIFF",4); w32(file_size);
    f.write("WAVE",4); f.write("fmt ",4); w32(16);
    w16(1); w16(channels); w32(sample_rate); w32(sample_rate*2);
    w16(2); w16(16);
    f.write("data",4); w32(data_size);

    // Write silence
    std::vector<int16_t> zeros(num_samples, 0);
    f.write((char*)zeros.data(), data_size);
}

}

TEST(WavFileTest, CreateAndVerify) {
    auto path = std::filesystem::temp_directory_path() / "test.wav";
    create_test_wav(path.string(), 2);
    
    EXPECT_TRUE(std::filesystem::exists(path));
    EXPECT_GT(std::filesystem::file_size(path), 44u); // > header only
    
    // Verify RIFF header
    std::ifstream f(path, std::ios::binary);
    char buf[4];
    f.read(buf, 4);
    EXPECT_EQ(std::string(buf, 4), "RIFF");
    
    std::filesystem::remove(path);
}

TEST(WavFileTest, CorrectDuration) {
    auto path = std::filesystem::temp_directory_path() / "test2.wav";
    create_test_wav(path.string(), 5);
    
    // Expected size: 44 (header) + 5*16000*2 (data) = 160044
    EXPECT_EQ(std::filesystem::file_size(path), 44u + 5*16000*2);
    
    std::filesystem::remove(path);
}
