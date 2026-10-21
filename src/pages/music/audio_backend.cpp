#include "pages/music/audio_backend.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cctype>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <fcntl.h>
#include <mutex>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <string>
#include <sys/socket.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

extern char** environ;

namespace {

std::string preferred_audio_device()
{
    if (const char* configured = std::getenv("LILYGO_MUSIC_AUDIO_DEVICE"); configured && *configured) return configured;
#if defined(__linux__)
    std::ifstream cards("/proc/asound/cards");
    std::string line;
    while (std::getline(cards, line)) {
        if (line.find("CM0ES8389") != std::string::npos) return "alsa/hw:CARD=CM0ES8389,DEV=0";
    }
#endif
    return {};
}

// The IPC messages we consume have scalar top-level fields. Walk complete JSON
// values so text nested in an error or data object cannot be mistaken for keys.
std::string json_field(const std::string& json, const std::string& key)
{
    std::size_t pos = 0;
    auto whitespace = [&] {
        while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) ++pos;
    };
    auto quoted = [&]() {
        std::string value;
        if (pos >= json.size() || json[pos++] != '"') return value;
        while (pos < json.size()) {
            char c = json[pos++];
            if (c == '"') break;
            if (c == '\\' && pos < json.size()) c = json[pos++];
            value += c;
        }
        return value;
    };
    whitespace();
    if (pos >= json.size() || json[pos++] != '{') return {};
    while (pos < json.size()) {
        whitespace();
        if (pos >= json.size()) break;
        if (json[pos] == '}') break;
        if (json[pos] != '"') return {};
        const auto name = quoted();
        whitespace();
        if (pos >= json.size() || json[pos++] != ':') return {};
        whitespace();
        if (pos >= json.size()) return {};
        if (json[pos] == '"') {
            auto value = quoted();
            if (name == key) return value;
        } else {
            const auto begin = pos;
            int depth        = 0;
            while (pos < json.size()) {
                const char c = json[pos];
                if (c == '"') {
                    quoted();
                    continue;
                }
                if (depth == 0 && (c == ',' || c == '}')) break;
                if (c == '{' || c == '[') ++depth;
                if (c == '}' || c == ']') --depth;
                ++pos;
            }
            if (name == key) return json.substr(begin, pos - begin);
        }
        whitespace();
        if (pos >= json.size() || json[pos] != ',') break;
        ++pos;
    }
    return {};
}

double json_number(const std::string& text)
{
    char* end           = nullptr;
    const double result = std::strtod(text.c_str(), &end);
    return end != text.c_str() && std::isfinite(result) ? std::max(0.0, result) : 0;
}

class MpvAudioBackend final : public AudioBackend {
public:
    MpvAudioBackend(std::string executable, bool null_audio)
        : executable_(std::move(executable)), null_audio_(null_audio), worker_([this] { run(); })
    {
    }

    ~MpvAudioBackend() override
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            quitting_ = true;
        }
        changed_.notify_one();
        worker_.join();
    }

    void load(const std::string& path, int volume) override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ++generation_;
        state_         = {};
        state_.path    = path;
        state_.loading = true;
        commands_.clear();
        commands_.push_back({Command::load, path, static_cast<double>(std::clamp(volume, 0, 100)), generation_});
        changed_.notify_one();
    }

    void set_paused(bool paused) override
    {
        enqueue(Command::pause, paused ? 1 : 0);
    }
    void seek(double seconds) override
    {
        if (std::isfinite(seconds)) enqueue(Command::seek, std::max(0.0, seconds));
    }
    void set_volume(int volume) override
    {
        enqueue(Command::volume, std::clamp(volume, 0, 100));
    }
    void stop() override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ++generation_;
        state_ = {};
        commands_.clear();
        commands_.push_back({Command::stop, {}, 0, generation_});
        changed_.notify_one();
    }

    AudioSnapshot snapshot() const override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return state_;
    }

private:
    enum class Command { load, pause, seek, volume, stop };
    struct Request {
        Command type;
        std::string path;
        double value;
        std::uint64_t generation;
    };

    void enqueue(Command type, double value)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // Coalesce slider motion so a slow device cannot build an unbounded queue.
        commands_.erase(std::remove_if(commands_.begin(), commands_.end(),
                                       [type](const Request& request) { return request.type == type; }),
                        commands_.end());
        commands_.push_back({type, {}, value, generation_});
        changed_.notify_one();
    }

    void fail(const std::string& error)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (active_generation_ != generation_) return;
        state_.loading = false;
        state_.playing = false;
        state_.loaded  = false;
        state_.error   = error;
    }

    bool send_command(const std::string& command)
    {
        if (socket_ < 0) return false;
        const std::string line = "{\"command\":" + command + "}\n";
        std::size_t written    = 0;
        while (written < line.size()) {
#ifdef MSG_NOSIGNAL
            constexpr int flags = MSG_NOSIGNAL;
#else
            constexpr int flags = 0;
#endif
            const auto count = ::send(socket_, line.data() + written, line.size() - written, flags);
            if (count > 0) {
                written += static_cast<std::size_t>(count);
                continue;
            }
            if (count < 0 && errno == EINTR) continue;
            if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                struct pollfd pending{socket_, POLLOUT, 0};
                if (::poll(&pending, 1, 100) > 0) continue;
            }
            fail("Unable to connect to the audio player");
            return false;
        }
        return true;
    }

    void close_player()
    {
        if (socket_ >= 0) {
            send_command("[\"quit\"]");
            ::close(socket_);
            socket_ = -1;
        }
        if (pid_ > 0) {
            for (int i = 0; i < 15; ++i) {
                const auto result = ::waitpid(pid_, nullptr, WNOHANG);
                if (result == pid_ || (result < 0 && errno == ECHILD)) {
                    pid_ = -1;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            if (pid_ > 0) {
                ::kill(pid_, SIGKILL);
                while (::waitpid(pid_, nullptr, 0) < 0 && errno == EINTR) {
                }
                pid_ = -1;
            }
        }
        input_.clear();
    }

    void start_player(const Request& request)
    {
        close_player();
        active_generation_ = request.generation;
        int sockets[2];
        if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0) {
            fail("Unable to create an audio connection");
            return;
        }
        // dup2 installs the IPC socket at a stable child descriptor. Ensure its
        // source is different even when this process has closed standard FDs.
        const int child_fd = ::fcntl(sockets[1], F_DUPFD, 10);
        ::close(sockets[1]);
        if (child_fd < 0) {
            ::close(sockets[0]);
            fail("Unable to create an audio connection");
            return;
        }
        ::fcntl(sockets[0], F_SETFD, FD_CLOEXEC);
        ::fcntl(child_fd, F_SETFD, FD_CLOEXEC);
        posix_spawn_file_actions_t actions;
        posix_spawn_file_actions_init(&actions);
        posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
        posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
        posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
        posix_spawn_file_actions_adddup2(&actions, child_fd, 3);
        posix_spawn_file_actions_addclose(&actions, child_fd);
        std::vector<std::string> args{executable_,
                                      "--no-config",
                                      "--no-video",
                                      "--audio-display=no",
                                      "--no-terminal",
                                      "--input-default-bindings=no",
                                      "--input-ipc-client=fd://3",
                                      "--volume=" + std::to_string(static_cast<int>(request.value))};
        const auto audio_device = preferred_audio_device();
        if (!audio_device.empty()) args.emplace_back("--audio-device=" + audio_device);
        // The CM0 ES8389 is clocked from a 19.2 MHz MCLK and its kernel
        // driver only exposes a 48 kHz clock coefficient.  Let mpv resample
        // source files such as 44.1 kHz music before ALSA opens the device;
        // otherwise the driver rejects the stream with -EINVAL.
        if (audio_device.find("CM0ES8389") != std::string::npos)
        {
            args.emplace_back("--audio-samplerate=48000");
            args.emplace_back("--audio-format=s16");
            args.emplace_back("--audio-channels=stereo");
        }
        if (null_audio_) args.emplace_back("--ao=null");
        args.emplace_back("--");
        args.push_back(request.path);
        std::vector<char*> argv;
        for (auto& arg : args) argv.push_back(arg.data());
        argv.push_back(nullptr);
        const int result = ::posix_spawnp(&pid_, executable_.c_str(), &actions, nullptr, argv.data(), environ);
        posix_spawn_file_actions_destroy(&actions);
        ::close(child_fd);
        if (result != 0) {
            ::close(sockets[0]);
            pid_ = -1;
            fail(result == ENOENT ? "mpv not found. Install it and try again." : "Unable to start the audio player");
            return;
        }
        socket_ = sockets[0];
        ::fcntl(socket_, F_SETFL, ::fcntl(socket_, F_GETFL) | O_NONBLOCK);
#ifdef SO_NOSIGPIPE
        const int enabled = 1;
        ::setsockopt(socket_, SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled));
#endif
        send_command("[\"observe_property\",1,\"time-pos\"]");
        send_command("[\"observe_property\",2,\"duration\"]");
        send_command("[\"observe_property\",3,\"pause\"]");
        started_ = std::chrono::steady_clock::now();
    }

    void handle_message(const std::string& message)
    {
        const auto event = json_field(message, "event");
        std::lock_guard<std::mutex> lock(mutex_);
        if (active_generation_ != generation_) return;
        if (event == "file-loaded") {
            state_.loading = false;
            state_.loaded  = true;
            state_.playing = !paused_;
        } else if (event == "property-change") {
            const auto name = json_field(message, "name");
            const auto data = json_field(message, "data");
            if (name == "time-pos" && data != "null") state_.position_seconds = json_number(data);
            if (name == "duration" && data != "null") state_.duration_seconds = json_number(data);
            if (name == "pause" && (data == "true" || data == "false")) {
                paused_        = data == "true";
                state_.playing = state_.loaded && !paused_;
            }
        } else if (event == "end-file") {
            state_.loading    = false;
            state_.playing    = false;
            state_.loaded     = false;
            const auto reason = json_field(message, "reason");
            state_.finished   = reason == "eof";
            if (reason == "error") state_.error = "Unable to play this file. Check the file or audio device.";
        }
    }

    void receive()
    {
        if (socket_ < 0) return;
        char buffer[4096];
        for (;;) {
            const auto count = ::recv(socket_, buffer, sizeof(buffer), 0);
            if (count <= 0) break;
            input_.append(buffer, static_cast<std::size_t>(count));
            std::size_t newline;
            while ((newline = input_.find('\n')) != std::string::npos) {
                handle_message(input_.substr(0, newline));
                input_.erase(0, newline + 1);
            }
            if (input_.size() > 65536) {
                input_.clear();
                fail("The audio player returned invalid data");
            }
        }
    }

    void run()
    {
        for (;;) {
            std::deque<Request> pending;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                changed_.wait_for(lock, std::chrono::milliseconds(20),
                                  [this] { return quitting_ || !commands_.empty(); });
                if (quitting_) break;
                pending.swap(commands_);
            }
            for (const auto& request : pending) {
                if (request.type == Command::load) {
                    paused_ = false;
                    start_player(request);
                } else if (request.type == Command::stop) {
                    close_player();
                    active_generation_ = request.generation;
                } else if (request.generation == active_generation_) {
                    if (request.type == Command::pause)
                        send_command(std::string("[\"set_property\",\"pause\",") +
                                     (request.value ? "true]" : "false]"));
                    if (request.type == Command::seek)
                        send_command("[\"seek\"," + std::to_string(request.value) + ",\"absolute+exact\"]");
                    if (request.type == Command::volume)
                        send_command("[\"set_property\",\"volume\"," + std::to_string(request.value) + "]");
                }
            }
            receive();
            if (pid_ > 0) {
                int status = 0;
                if (::waitpid(pid_, &status, WNOHANG) == pid_) {
                    receive();
                    pid_ = -1;
                    if (socket_ >= 0) {
                        ::close(socket_);
                        socket_ = -1;
                    }
                    const auto current = snapshot();
                    if (!current.finished && current.error.empty()) fail("Playback stopped unexpectedly. Try again.");
                } else if (std::chrono::steady_clock::now() - started_ > std::chrono::seconds(15)) {
                    const auto current = snapshot();
                    if (current.loading) {
                        fail("Audio read timed out. Check the file.");
                        close_player();
                    }
                }
            }
        }
        close_player();
    }

    std::string executable_;
    bool null_audio_;
    mutable std::mutex mutex_;
    std::condition_variable changed_;
    std::deque<Request> commands_;
    AudioSnapshot state_;
    bool quitting_                   = false;
    std::uint64_t generation_        = 0;
    std::uint64_t active_generation_ = 0;
    pid_t pid_                       = -1;
    int socket_                      = -1;
    bool paused_                     = false;
    std::string input_;
    std::chrono::steady_clock::time_point started_;
    std::thread worker_;
};

}  // namespace

std::unique_ptr<AudioBackend> make_mpv_audio_backend(const std::string& executable, bool null_audio)
{
    return std::make_unique<MpvAudioBackend>(executable, null_audio);
}
