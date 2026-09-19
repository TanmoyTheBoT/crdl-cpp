#pragma once

#include "../core/types.h"
#include "../core/config.h"
#include "../drm/widevine_cdm.h"
#include <filesystem>
#include <functional>

namespace crdl {

struct DownloadProgress {
    size_t bytes_downloaded{0};
    size_t total_bytes{0};
    int percentage{0};
    std::string current_file;
};

using ProgressCallback = std::function<void(const DownloadProgress&)>;

class Downloader {
public:
    explicit Downloader(const Config& config);
    ~Downloader();

    // Download video stream using N_m3u8DL-RE
    Result<std::filesystem::path> download_video(
        const StreamInfo& stream_info,
        const EpisodeMetadata& metadata,
        const std::vector<DRMKey>& keys,
        Quality quality,
        const std::string& access_token = ""
    );

    // Download audio stream
    Result<std::filesystem::path> download_audio(
        const StreamInfo& stream_info,
        const EpisodeMetadata& metadata,
        const std::vector<DRMKey>& keys,
        const std::string& audio_lang,
        const std::string& access_token = ""
    );

    // Download subtitles
    Result<std::vector<std::filesystem::path>> download_subtitles(
        const std::map<std::string, Subtitle>& subtitles,
        const EpisodeMetadata& metadata,
        const std::filesystem::path& output_dir
    );

    // Set progress callback
    void set_progress_callback(ProgressCallback callback);

    // Check if required tools are available
    static Result<void> check_dependencies();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace crdl
