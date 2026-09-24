#include "pages/music/music_view_model.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <utility>

namespace {
std::string lowercase(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return std::tolower(c); });
    return value;
}
}  // namespace

MusicViewModel::MusicViewModel() = default;
MusicViewModel::MusicViewModel(std::unique_ptr<AudioBackend> backend, std::vector<MusicTrack> tracks)
    : model_(std::move(backend), std::move(tracks))
{
}
void MusicViewModel::publish()
{
    revision_.set(revision_.value() == INT32_MAX ? 0 : revision_.value() + 1);
}
void MusicViewModel::poll()
{
    model_.poll();
    publish();
}
void MusicViewModel::play(std::size_t index)
{
    model_.play_track(index);
    publish();
}
void MusicViewModel::toggle_playback()
{
    model_.toggle_playback();
    publish();
}
void MusicViewModel::next()
{
    model_.next();
    publish();
}
void MusicViewModel::previous()
{
    model_.previous();
    publish();
}
void MusicViewModel::seek(int permille)
{
    model_.seek(model_.duration_seconds() * std::clamp(permille, 0, 1000) / 1000.0);
    publish();
}
void MusicViewModel::set_volume(int value)
{
    model_.set_volume(value);
    publish();
}
void MusicViewModel::toggle_shuffle()
{
    model_.toggle_shuffle();
    publish();
}
void MusicViewModel::cycle_repeat()
{
    model_.cycle_repeat();
    publish();
}
void MusicViewModel::toggle_favorite()
{
    model_.toggle_favorite();
    publish();
}
void MusicViewModel::rescan()
{
    model_.rescan();
    publish();
}
void MusicViewModel::set_favorites_only(bool value)
{
    favorites_only_ = value;
    publish();
}
void MusicViewModel::set_query(std::string value)
{
    query_ = std::move(value);
    publish();
}
std::vector<std::size_t> MusicViewModel::visible_tracks() const
{
    std::vector<std::size_t> result;
    const auto query = lowercase(query_);
    for (std::size_t i = 0; i < model_.tracks().size(); ++i) {
        const auto &track = model_.tracks()[i];
        if (favorites_only_ && !track.favorite) continue;
        if (!query.empty() &&
            lowercase(track.title + " " + track.artist + " " + track.album).find(query) == std::string::npos)
            continue;
        result.push_back(i);
    }
    return result;
}
std::string MusicViewModel::format_time(double seconds)
{
    if (!std::isfinite(seconds) || seconds < 0) seconds = 0;
    const auto total = static_cast<int>(std::min(seconds, 359999.0));
    char buffer[24];
    std::snprintf(buffer, sizeof(buffer), "%d:%02d", total / 60, total % 60);
    return buffer;
}
