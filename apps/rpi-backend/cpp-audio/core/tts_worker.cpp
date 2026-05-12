#include "tts_worker.hpp"
#include <iostream>
#include <sstream>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <vector>
#include <fstream>

// Forward declarations for piper (may not be available)
// Stub implementations - will be replaced with actual calls when library is available
extern "C" {
    // Piper TTS forward declarations - simplified interface
    struct piper_speaker;
    typedef struct piper_speaker piper_speaker_t;

    inline piper_speaker_t* piper_load_speaker(const char* voice_path) { return nullptr; }
    inline void piper_free_speaker(piper_speaker_t* speaker) {}
    inline int piper_synthesize(piper_speaker_t* speaker, const char* text, int16_t** out_samples, int* out_len) { return -1; }
}

struct TTSWorker::PiperContext {
    piper_speaker_t* speaker = nullptr;
    std::string voice_path_;

    PiperContext(const std::string& voice_path) : voice_path_(voice_path) {
        speaker = piper_load_speaker(voice_path.c_str());
    }

    ~PiperContext() {
        if (speaker) {
            piper_free_speaker(speaker);
        }
    }

    bool is_valid() const { return speaker != nullptr; }

    // Native synthesis via piper API — writes WAV and returns path
    std::string synthesize(const std::string& text) {
        if (!speaker) return "";

        int16_t* samples = nullptr;
        int sample_count = 0;
        int ret = piper_synthesize(speaker, text.c_str(), &samples, &sample_count);
        if (ret != 0 || !samples || sample_count <= 0) return "";

        // Write to temporary WAV file
        std::string out_path = "/tmp/mia_tts_" + std::to_string(std::hash<std::string>{}(text)) + ".wav";
        std::ofstream wav(out_path, std::ios::binary);
        if (!wav) return "";

        // Simple WAV header (16-bit mono 22050 Hz)
        const int sample_rate = 22050;
        const int data_size = sample_count * 2;
        const int file_size = 36 + data_size;
        wav.write("RIFF", 4);
        wav.write(reinterpret_cast<const char*>(&file_size), 4);
        wav.write("WAVEfmt ", 8);
        int chunk_size = 16; wav.write(reinterpret_cast<const char*>(&chunk_size), 4);
        int16_t audio_fmt = 1; wav.write(reinterpret_cast<const char*>(&audio_fmt), 2);
        int16_t channels = 1; wav.write(reinterpret_cast<const char*>(&channels), 2);
        wav.write(reinterpret_cast<const char*>(&sample_rate), 4);
        int byte_rate = sample_rate * 2; wav.write(reinterpret_cast<const char*>(&byte_rate), 4);
        int16_t block_align = 2; wav.write(reinterpret_cast<const char*>(&block_align), 2);
        int16_t bits = 16; wav.write(reinterpret_cast<const char*>(&bits), 2);
        wav.write("data", 4);
        wav.write(reinterpret_cast<const char*>(&data_size), 4);
        wav.write(reinterpret_cast<const char*>(samples), data_size);
        wav.close();

        return out_path;
    }
};

TTSWorker::TTSWorker() {
    worker_thread_ = std::thread(&TTSWorker::worker_thread_func, this);
}

TTSWorker::~TTSWorker() {
    stop();
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

bool TTSWorker::initialize(const std::string& model_path, const std::string& voice_path) {
    model_path_ = model_path;
    voice_path_ = voice_path;
    return load_model(model_path, voice_path);
}

void TTSWorker::synthesize_speech(const std::string& text,
                                 OnResultCallback on_result,
                                 OnErrorCallback on_error) {
    TTSTRequest request{text, on_result, on_error};

    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        request_queue_.push(request);
    }

    queue_cv_.notify_one();
}

void TTSWorker::stop() {
    should_stop_ = true;
    queue_cv_.notify_all();
}

void TTSWorker::worker_thread_func() {
    while (!should_stop_) {
        TTSTRequest request;

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this]() {
                return !request_queue_.empty() || should_stop_;
            });

            if (should_stop_) break;

            if (!request_queue_.empty()) {
                request = request_queue_.front();
                request_queue_.pop();
            } else {
                continue;
            }
        }

        processing_ = true;
        bool success = process_tts(request);
        processing_ = false;

        if (!success && request.on_error) {
            request.on_error("TTS synthesis failed");
        }
    }
}

bool TTSWorker::process_tts(const TTSTRequest& request) {
    // Try native piper integration first
    if (piper_ctx_ && piper_ctx_->is_valid()) {
        std::string audio_path = piper_ctx_->synthesize(request.text);
        if (!audio_path.empty()) {
            if (request.on_result) {
                request.on_result(audio_path);
            }
            return true;
        }
        // Native failed, fall through to CLI
    }

    // Fall back to piper CLI
    std::string audio_path = run_piper_cli(request.text);
    if (!audio_path.empty()) {
        // Read the generated audio file
        std::ifstream audio_file(audio_path, std::ios::binary);
        if (audio_file) {
            std::vector<char> audio_data((std::istreambuf_iterator<char>(audio_file)),
                                        std::istreambuf_iterator<char>());
            audio_file.close();

            // Clean up the temporary file
            std::filesystem::remove(audio_path);

            // Convert to base64 or return raw data
            // For now, return the file path
            if (request.on_result) {
                request.on_result(audio_path);
            }
            return true;
        }
    }

    return false;
}

bool TTSWorker::load_model(const std::string& model_path, const std::string& voice_path) {
    try {
        // Attempt to load the piper voice model for native synthesis
        if (!voice_path.empty() && std::filesystem::exists(voice_path)) {
            piper_ctx_ = std::make_unique<PiperContext>(voice_path);
            if (piper_ctx_->is_valid()) {
                std::cout << "Piper model loaded natively: " << voice_path << std::endl;
                return true;
            }
            // Native load failed — clear context, allow CLI fallback
            std::cerr << "Piper native load failed, will use CLI fallback" << std::endl;
            piper_ctx_.reset();
        }
        return true; // Allow CLI fallback even without native model
    } catch (const std::exception& e) {
        std::cerr << "Failed to load piper model: " << e.what() << std::endl;
        return false;
    }
}

std::string TTSWorker::run_piper_cli(const std::string& text) {
    // Create temporary text file
    std::string text_file = "/tmp/tts_input.txt";
    std::ofstream text_out(text_file);
    if (!text_out) {
        return "";
    }
    text_out << text;
    text_out.close();

    // Generate output audio path
    std::string audio_file = "/tmp/tts_output.wav";

    // Build piper command
    std::string cmd = "piper";
    if (!model_path_.empty()) {
        cmd += " --model " + model_path_;
    }
    cmd += " --output_file " + audio_file;

    // Execute command
    FILE* pipe = popen(cmd.c_str(), "w");
    if (!pipe) {
        std::filesystem::remove(text_file);
        return "";
    }

    // Write text to piper stdin
    fprintf(pipe, "%s\n", text.c_str());
    int status = pclose(pipe);

    // Clean up text file
    std::filesystem::remove(text_file);

    if (status == 0 && std::filesystem::exists(audio_file)) {
        return audio_file;
    }

    return "";
}