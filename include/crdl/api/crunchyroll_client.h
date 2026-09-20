#pragma once

#include "../core/types.h"
#include "../core/config.h"
#include "../utils/http_client.h"
#include <mutex>
#include <atomic>

namespace crdl {

class CrunchyrollClient {
public:
    explicit CrunchyrollClient(const Config& config);
    ~CrunchyrollClient();

    // Authentication
    Result<void> login();
    Result<void> logout();
    bool is_logged_in() const;

    // Content discovery
    Result<Series> get_series(const std::string& series_id);
    Result<std::vector<Season>> get_seasons(const std::string& series_id);
    Result<std::vector<Episode>> get_episodes(const std::string& season_id);
    Result<Episode> get_episode(const std::string& episode_id);

    // Streaming
    Result<StreamInfo> get_streams(const std::string& episode_id, const std::string& guid);
    Result<void> delete_stream(const std::string& guid, const std::string& token);

    // Download
    Result<void> download_episode(const std::string& episode_id,
                                  const std::vector<std::string>& audio_langs = {"ja-JP"});
    Result<void> download_season(const std::string& season_id,
                                const std::vector<std::string>& audio_langs = {"ja-JP"});
    Result<void> download_series(const std::string& series_id,
                                const std::vector<std::string>& audio_langs = {"ja-JP"});

    // Profile
    Result<std::string> get_profile();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

// Series, Season, Episode classes
class Series {
public:
    std::string id;
    std::string title;
    std::string description;
    std::vector<Season> seasons;

    Series() = default;
    Series(std::string id, std::string title)
        : id(std::move(id)), title(std::move(title)) {}
};

class Season {
public:
    std::string id;
    std::string title;
    int season_number{1};
    std::string series_id;
    std::vector<Episode> episodes;

    Season() = default;
    Season(std::string id, int number)
        : id(std::move(id)), season_number(number) {}
};

class Episode {
public:
    std::string id;
    std::string title;
    std::string series_id;
    std::string series_title;
    std::string season_id;
    int season_number{1};
    int episode_number{1};
    std::vector<Version> versions;
    std::vector<Chapter> chapters;

    EpisodeMetadata to_metadata() const;

    Episode() = default;
    Episode(std::string id, std::string title)
        : id(std::move(id)), title(std::move(title)) {}
};

} // namespace crdl
