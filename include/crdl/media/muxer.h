#pragma once

#include "../core/types.h"
#include <filesystem>
#include <vector>

namespace crdl {

struct AudioTrack {
    std::filesystem::path file_path;
    std::string language_code;
    bool is_default{false};
};

class Muxer {
public:
    Muxer() = default;
    ~Muxer() = default;

    // Mux video, audio, subtitles, and chapters into MKV
    Result<std::filesystem::path> mux_to_mkv(
        const std::filesystem::path& video_file,
        const std::vector<AudioTrack>& audio_tracks,
        const std::vector<std::filesystem::path>& subtitle_files,
        const std::filesystem::path& chapter_file,
        const std::filesystem::path& output_file
    );

    // Generate chapter file from chapter data
    Result<std::filesystem::path> create_chapter_file(
        const std::vector<Chapter>& chapters,
        const std::filesystem::path& output_path
    );

    // Clean subtitle file (remove empty lines)
    Result<void> clean_subtitle_file(const std::filesystem::path& subtitle_file);

    // Check if mkvmerge is available
    static bool is_available();

private:
    std::vector<std::string> build_mkvmerge_command(
        const std::filesystem::path& video_file,
        const std::vector<AudioTrack>& audio_tracks,
        const std::vector<std::filesystem::path>& subtitle_files,
        const std::filesystem::path& chapter_file,
        const std::filesystem::path& output_file
    );

    static int execute_command(const std::vector<std::string>& args);
};

} // namespace crdl
