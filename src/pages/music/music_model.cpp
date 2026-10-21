#include "pages/music/music_model.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <set>
#include <sstream>
#include <utility>

namespace {
namespace fs                      = std::filesystem;
constexpr std::size_t track_limit = 2048;

std::string lowercase(std::string value)
{
    for (auto& c : value) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return value;
}

bool audio_file(const fs::path& path)
{
    const auto ext = lowercase(path.extension().string());
    return ext == ".mp3" || ext == ".wav" || ext == ".flac" || ext == ".m4a" || ext == ".aac" || ext == ".ogg" ||
           ext == ".opus" || ext == ".aiff" || ext == ".aif" || ext == ".wma" || ext == ".ac3" || ext == ".mp4" ||
           ext == ".ts";
}

double wav_duration(const fs::path& path)
{
    if (lowercase(path.extension().string()) != ".wav") return 0;
    std::ifstream file(path, std::ios::binary);
    char header[12];
    if (!file.read(header, sizeof(header)) || std::string(header, 4) != "RIFF" || std::string(header + 8, 4) != "WAVE")
        return 0;
    auto number = [](const unsigned char* data) {
        return static_cast<std::uint32_t>(data[0]) | (static_cast<std::uint32_t>(data[1]) << 8) |
               (static_cast<std::uint32_t>(data[2]) << 16) | (static_cast<std::uint32_t>(data[3]) << 24);
    };
    std::uint32_t byte_rate = 0;
    for (int chunks = 0; chunks < 64 && file; ++chunks) {
        unsigned char chunk[8];
        if (!file.read(reinterpret_cast<char*>(chunk), sizeof(chunk))) break;
        const auto length = number(chunk + 4);
        const std::string kind(reinterpret_cast<char*>(chunk), 4);
        if (kind == "data" && byte_rate) return static_cast<double>(length) / byte_rate;
        if (kind == "fmt " && length >= 16) {
            unsigned char format[16];
            if (!file.read(reinterpret_cast<char*>(format), sizeof(format))) break;
            byte_rate = number(format + 8);
            file.seekg(static_cast<std::streamoff>(length - 16 + (length & 1)), std::ios::cur);
        } else {
            file.seekg(static_cast<std::streamoff>(length) + (length & 1), std::ios::cur);
        }
    }
    return 0;
}

std::vector<MusicTrack> scan_path(const std::string& root, const std::shared_ptr<std::atomic_bool>& cancelled = {})
{
    std::vector<MusicTrack> tracks;
    std::error_code error;
    fs::recursive_directory_iterator entry(root, fs::directory_options::skip_permission_denied, error), end;
    for (; !error && entry != end && tracks.size() < track_limit; entry.increment(error)) {
        if (cancelled && cancelled->load()) break;
        const auto path     = entry->path();
        const auto filename = path.filename().string();
        if ((!filename.empty() && filename.front() == '.') || entry.depth() >= 8) {
            entry.disable_recursion_pending();
            if (!filename.empty() && filename.front() == '.') continue;
        }
        if (!entry->is_regular_file(error) || error || !audio_file(path)) {
            error.clear();
            continue;
        }
        const auto absolute = fs::weakly_canonical(path, error);
        if (error) {
            error.clear();
            continue;
        }
        MusicTrack track;
        track.path           = absolute.string();
        track.id             = track.path;
        track.title          = path.stem().string();
        track.artist         = "";
        const auto separator = track.title.find(" - ");
        if (separator != std::string::npos && separator > 0 && separator + 3 < track.title.size()) {
            track.artist = track.title.substr(0, separator);
            track.title.erase(0, separator + 3);
        }
        track.album            = path.parent_path().filename().string();
        track.duration_seconds = wav_duration(path);
        tracks.push_back(std::move(track));
    }
    std::sort(tracks.begin(), tracks.end(), [](const MusicTrack& a, const MusicTrack& b) {
        const auto left = lowercase(a.title), right = lowercase(b.title);
        return left == right ? a.path < b.path : left < right;
    });
    for (std::size_t i = 0; i < tracks.size(); ++i) tracks[i].artwork = static_cast<unsigned>(i % 3);
    return tracks;
}

std::vector<MusicTrack> sample_tracks()
{
    std::vector<std::string> directories;
#ifdef LILYGO_MUSIC_ASSET_DIR
    directories.emplace_back(std::string(LILYGO_MUSIC_ASSET_DIR) + "/audio");
#endif
#ifdef LILYGO_MUSIC_SOURCE_ASSET_DIR
    directories.emplace_back(std::string(LILYGO_MUSIC_SOURCE_ASSET_DIR) + "/audio");
#endif
    for (const auto& directory : directories) {
        auto tracks = scan_path(directory);
        if (tracks.empty()) continue;
        for (auto& track : tracks) {
            const auto stem = lowercase(fs::path(track.path).stem().string());
            if (stem == "soft-landing") {
                track.title   = "Soft Landing";
                track.artist  = "LILYGO Sessions";
                track.album   = "Little Moments";
                track.artwork = 0;
            } else if (stem == "blue-hour") {
                track.title   = "Blue Hour";
                track.artist  = "LILYGO Sessions";
                track.album   = "Little Moments";
                track.artwork = 1;
            } else if (stem == "afterglow") {
                track.title   = "Afterglow";
                track.artist  = "LILYGO Sessions";
                track.album   = "Little Moments";
                track.artwork = 2;
            } else {
                track.artist  = "Espressif ADF";
                track.album   = "Two Channel Audio";
                track.artwork = static_cast<unsigned>(track.artwork % 3);
            }
            track.sample = true;
        }
        std::stable_sort(tracks.begin(), tracks.end(), [](const MusicTrack& a, const MusicTrack& b) {
            const auto rank = [](const MusicTrack& track) {
                const auto stem = lowercase(fs::path(track.path).stem().string());
                if (stem == "soft-landing") return 0;
                if (stem == "blue-hour") return 1;
                if (stem == "afterglow") return 2;
                return 3;
            };
            const auto left = rank(a), right = rank(b);
            return left == right ? lowercase(a.title) < lowercase(b.title) : left < right;
        });
        return tracks;
    }
    return {};
}

std::string preference_file()
{
    if (const char* config = std::getenv("XDG_CONFIG_HOME"); config && *config)
        return std::string(config) + "/lilygo-ui-music/preferences";
    if (const char* user_home = std::getenv("HOME"); user_home && *user_home)
        return std::string(user_home) + "/.config/lilygo-ui-music/preferences";
    return {};
}

}  // namespace

MusicModel::MusicModel()
    : backend_(make_mpv_audio_backend()), preferences_enabled_(true), preferences_path_(preference_file())
{
    apply_preferences();
    rescan();
}

MusicModel::MusicModel(std::unique_ptr<AudioBackend> backend, std::vector<MusicTrack> initial_tracks)
    : backend_(std::move(backend)), tracks_(std::move(initial_tracks))
{
    if (!backend_) backend_ = make_mpv_audio_backend();
}

MusicModel::~MusicModel()
{
    if (scan_cancelled_) scan_cancelled_->store(true);
    if (scan_.valid()) scan_.wait();
    backend_->stop();
}

const std::vector<MusicTrack>& MusicModel::tracks() const noexcept
{
    return tracks_;
}
const MusicTrack* MusicModel::current_track() const noexcept
{
    return current_index_ < tracks_.size() ? &tracks_[current_index_] : nullptr;
}
std::size_t MusicModel::current_index() const noexcept
{
    return current_index_;
}
bool MusicModel::playing() const noexcept
{
    return playback_.playing;
}
bool MusicModel::loading() const noexcept
{
    return playback_.loading;
}
bool MusicModel::scanning() const noexcept
{
    return scanning_;
}
bool MusicModel::shuffle() const noexcept
{
    return shuffle_;
}
RepeatMode MusicModel::repeat_mode() const noexcept
{
    return repeat_;
}
int MusicModel::volume() const noexcept
{
    return volume_;
}
double MusicModel::position_seconds() const noexcept
{
    return playback_.position_seconds;
}
double MusicModel::duration_seconds() const noexcept
{
    if (playback_.duration_seconds > 0) return playback_.duration_seconds;
    return current_track() ? current_track()->duration_seconds : 0;
}
const std::string& MusicModel::error() const noexcept
{
    return playback_.error.empty() ? scan_error_ : playback_.error;
}
std::string MusicModel::library_label() const
{
    if (scanning_) return "Searching for music";
    if (tracks_.empty()) return "No music yet";
    return std::all_of(tracks_.begin(), tracks_.end(), [](const MusicTrack& track) { return track.sample; })
               ? "Sample Music"
               : "";
}

void MusicModel::poll()
{
    if (scanning_ && scan_.valid() && scan_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        const auto previous_path = current_track() ? current_track()->path : std::string{};
        auto result              = scan_.get();
        tracks_                  = std::move(result.tracks);
        scan_error_              = std::move(result.error);
        scanning_                = false;
        current_index_           = 0;
        bool preserved           = false;
        for (std::size_t i = 0; i < tracks_.size(); ++i) {
            tracks_[i].favorite =
                std::find(favorite_paths_.begin(), favorite_paths_.end(), tracks_[i].path) != favorite_paths_.end();
            if (tracks_[i].path == previous_path) {
                current_index_ = i;
                preserved      = true;
            }
        }
        if (!previous_path.empty() && !preserved) {
            backend_->stop();
            playback_ = {};
        }
        history_.clear();
        rebuild_shuffle();
    }
    const auto state = backend_->snapshot();
    if (current_track() && state.path == current_track()->path) {
        playback_ = state;
        if (state.duration_seconds > 0) tracks_[current_index_].duration_seconds = state.duration_seconds;
        if (state.finished && !finished_handled_) {
            finished_handled_ = true;
            advance(true);
        }
    }
}

void MusicModel::start_track(std::size_t index, bool add_history)
{
    if (index >= tracks_.size()) return;
    if (add_history && index != current_index_) history_.push_back(current_index_);
    if (history_.size() > track_limit) history_.erase(history_.begin());
    current_index_    = index;
    finished_handled_ = false;
    pause_requested_  = false;
    backend_->load(tracks_[index].path, volume_);
    playback_ = backend_->snapshot();
}

void MusicModel::play_track(std::size_t index)
{
    if (index >= tracks_.size()) return;
    start_track(index, true);
    rebuild_shuffle();
}

void MusicModel::toggle_playback()
{
    if (!current_track()) return;
    if (!playback_.loaded && !playback_.loading) {
        start_track(current_index_, false);
        return;
    }
    pause_requested_ = !pause_requested_;
    backend_->set_paused(pause_requested_);
    playback_.playing = playback_.loaded && !pause_requested_;
}

void MusicModel::advance(bool automatic)
{
    if (tracks_.empty()) return;
    if (automatic && repeat_ == RepeatMode::one) {
        start_track(current_index_, false);
        return;
    }
    std::size_t index = current_index_;
    if (shuffle_) {
        if (shuffle_queue_.empty()) {
            if (automatic && repeat_ == RepeatMode::off) return;
            rebuild_shuffle();
        }
        if (!shuffle_queue_.empty()) {
            index = shuffle_queue_.back();
            shuffle_queue_.pop_back();
        } else if (automatic && repeat_ == RepeatMode::off)
            return;
    } else {
        if (automatic && current_index_ + 1 >= tracks_.size() && repeat_ == RepeatMode::off) return;
        index = (current_index_ + 1) % tracks_.size();
    }
    start_track(index, true);
}

void MusicModel::next()
{
    advance(false);
}
void MusicModel::previous()
{
    if (tracks_.empty()) return;
    if (position_seconds() > 3 && playback_.loaded) {
        seek(0);
        return;
    }
    if (shuffle_ && !history_.empty()) {
        const auto index = history_.back();
        history_.pop_back();
        start_track(index, false);
    } else {
        start_track(current_index_ == 0 ? tracks_.size() - 1 : current_index_ - 1, false);
    }
}

void MusicModel::seek(double seconds)
{
    if (!current_track() || !playback_.loaded || !std::isfinite(seconds)) return;
    const auto duration = duration_seconds();
    seconds             = std::max(0.0, seconds);
    if (duration > 0) seconds = std::min(seconds, std::max(0.0, duration - 0.05));
    backend_->seek(seconds);
    playback_.position_seconds = seconds;
}

void MusicModel::set_volume(int volume)
{
    volume_ = std::clamp(volume, 0, 100);
    backend_->set_volume(volume_);
    save_preferences();
}

void MusicModel::rebuild_shuffle()
{
    shuffle_queue_.clear();
    for (std::size_t i = 0; i < tracks_.size(); ++i)
        if (i != current_index_) shuffle_queue_.push_back(i);
    std::shuffle(shuffle_queue_.begin(), shuffle_queue_.end(), random_);
}

void MusicModel::toggle_shuffle()
{
    shuffle_ = !shuffle_;
    history_.clear();
    rebuild_shuffle();
    save_preferences();
}
void MusicModel::cycle_repeat()
{
    repeat_ = repeat_ == RepeatMode::off   ? RepeatMode::all
              : repeat_ == RepeatMode::all ? RepeatMode::one
                                           : RepeatMode::off;
    save_preferences();
}
void MusicModel::toggle_favorite()
{
    if (!current_track()) return;
    auto& track      = tracks_[current_index_];
    track.favorite   = !track.favorite;
    const auto entry = std::find(favorite_paths_.begin(), favorite_paths_.end(), track.path);
    if (track.favorite && entry == favorite_paths_.end())
        favorite_paths_.push_back(track.path);
    else if (!track.favorite && entry != favorite_paths_.end())
        favorite_paths_.erase(entry);
    save_preferences();
}

std::vector<MusicTrack> MusicModel::scan_directory(const std::string& path)
{
    return scan_path(path);
}

MusicModel::ScanResult MusicModel::scan_library(const std::shared_ptr<std::atomic_bool>& cancelled)
{
    ScanResult result;
    std::vector<std::string> roots;
    const char* override_path = std::getenv("LILYGO_MUSIC_DIR");
    const bool overridden     = override_path != nullptr;
    if (overridden)
        roots.emplace_back(override_path);
    else {
        if (const char* user_home = std::getenv("HOME"); user_home && *user_home)
            roots.emplace_back(std::string(user_home) + "/Music");
        roots.insert(roots.end(), {"/root/music", "/mnt/sdcard/music", "/mnt/music", "/media/music"});
    }
    std::set<std::string> seen;
    for (const auto& root : roots) {
        if (cancelled->load()) return result;
        auto tracks = scan_path(root, cancelled);
        for (auto& track : tracks) {
            if (seen.insert(track.path).second) result.tracks.push_back(std::move(track));
            if (result.tracks.size() >= track_limit) break;
        }
        if (result.tracks.size() >= track_limit) break;
    }
    if (result.tracks.empty() && !overridden && !cancelled->load()) result.tracks = sample_tracks();
    if (overridden) {
        std::error_code error;
        if (!fs::is_directory(roots.front(), error)) result.error = "Music folder not found. Check LILYGO_MUSIC_DIR.";
    }
    std::sort(result.tracks.begin(), result.tracks.end(), [](const MusicTrack& a, const MusicTrack& b) {
        if (a.sample && b.sample) return a.artwork < b.artwork;
        return lowercase(a.title) < lowercase(b.title);
    });
    return result;
}

void MusicModel::rescan()
{
    if (scanning_) return;
    scanning_ = true;
    scan_error_.clear();
    scan_cancelled_      = std::make_shared<std::atomic_bool>(false);
    const auto cancelled = scan_cancelled_;
    scan_                = std::async(std::launch::async, [cancelled] {
        try {
            return scan_library(cancelled);
        } catch (const std::exception&) {
            return ScanResult{{}, "Unable to read the music folder"};
        }
    });
}

void MusicModel::apply_preferences()
{
    std::ifstream file(preferences_path_);
    std::string kind;
    while (file >> kind) {
        if (kind == "volume") {
            int value;
            if (file >> value) volume_ = std::clamp(value, 0, 100);
        } else if (kind == "shuffle") {
            int value;
            if (file >> value) shuffle_ = value != 0;
        } else if (kind == "repeat") {
            int value;
            if (file >> value && value >= 0 && value <= 2) repeat_ = static_cast<RepeatMode>(value);
        } else if (kind == "favorite") {
            std::string path;
            if (file >> std::quoted(path)) favorite_paths_.push_back(std::move(path));
        } else {
            std::string ignored;
            std::getline(file, ignored);
        }
    }
}

void MusicModel::save_preferences() const
{
    if (!preferences_enabled_ || preferences_path_.empty()) return;
    std::error_code error;
    fs::create_directories(fs::path(preferences_path_).parent_path(), error);
    if (error) return;
    const auto temporary = preferences_path_ + ".tmp";
    std::ofstream file(temporary, std::ios::trunc);
    file << "volume " << volume_ << "\nshuffle " << shuffle_ << "\nrepeat " << static_cast<int>(repeat_) << '\n';
    for (const auto& path : favorite_paths_) file << "favorite " << std::quoted(path) << '\n';
    file.close();
    if (file) fs::rename(temporary, preferences_path_, error);
}
