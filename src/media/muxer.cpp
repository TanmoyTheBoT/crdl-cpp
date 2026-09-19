#include <crdl/media/muxer.h>
#include <crdl/utils/logger.h>
#include <fstream>
#include <sstream>
#include <regex>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace crdl {

Result<std::filesystem::path> Muxer::mux_to_mkv(
    const std::filesystem::path& video_file,
    const std::vector<AudioTrack>& audio_tracks,
    const std::vector<std::filesystem::path>& subtitle_files,
    const std::filesystem::path& chapter_file,
    const std::filesystem::path& output_file) {

    LOG_INFO("Muxing files into: {}", output_file.string());

    // Validate inputs
    if (!std::filesystem::exists(video_file)) {
        return Result<std::filesystem::path>(ErrorCode::FileNotFound,
            "Video file not found: " + video_file.string());
    }

    std::vector<std::string> args = {"mkvmerge", "-o", output_file.string(), video_file.string()};

    // Add audio tracks
    for (size_t i = 0; i < audio_tracks.size(); ++i) {
        const auto& track = audio_tracks[i];

        if (!std::filesystem::exists(track.file_path)) {
            LOG_WARN("Audio file not found: {}", track.file_path.string());
            continue;
        }

        args.push_back("--language");
        args.push_back("0:" + track.language_code);

        args.push_back("--track-name");
        args.push_back("0:" + track.language_code);

        args.push_back("--default-track");
        args.push_back(track.is_default ? "0:yes" : "0:no");

        args.push_back("--audio-tracks");
        args.push_back("0");

        args.push_back(track.file_path.string());
    }

    // Add subtitles
    for (const auto& sub_file : subtitle_files) {
        if (!std::filesystem::exists(sub_file)) continue;

        // Extract language from filename (e.g., "episode.en-US.ass")
        std::string lang = "und";
        std::string filename = sub_file.filename().string();
        std::regex lang_regex("\\.([a-z]{2}-[A-Z]{2})\\.");
        std::smatch match;
        if (std::regex_search(filename, match, lang_regex)) {
            lang = match[1].str();
        }

        args.push_back("--language");
        args.push_back("0:" + lang);

        args.push_back("--track-name");
        args.push_back("0:" + lang);

        args.push_back("--default-track");
        args.push_back("0:no");

        args.push_back("--forced-track");
        args.push_back("0:no");

        args.push_back(sub_file.string());
    }

    // Add chapters if available
    if (!chapter_file.empty() && std::filesystem::exists(chapter_file)) {
        args.push_back("--chapters");
        args.push_back(chapter_file.string());
    }

    // Execute mkvmerge
    int result = execute_command(args);

    // mkvmerge returns 0 or 1 (warnings) on success
    if (result != 0 && result != 1) {
        return Result<std::filesystem::path>(ErrorCode::MuxingFailed,
            "mkvmerge failed with code: " + std::to_string(result));
    }

    LOG_INFO("Muxing completed successfully");

    // Clean up temp files
    try {
        std::filesystem::remove(video_file);
        for (const auto& track : audio_tracks) {
            if (std::filesystem::exists(track.file_path)) {
                std::filesystem::remove(track.file_path);
            }
        }
        for (const auto& sub_file : subtitle_files) {
            if (std::filesystem::exists(sub_file)) {
                std::filesystem::remove(sub_file);
            }
        }
        if (!chapter_file.empty() && std::filesystem::exists(chapter_file)) {
            std::filesystem::remove(chapter_file);
        }
    } catch (const std::exception& e) {
        LOG_WARN("Failed to clean up temp files: {}", e.what());
    }

    return Result<std::filesystem::path>(output_file);
}

Result<std::filesystem::path> Muxer::create_chapter_file(
    const std::vector<Chapter>& chapters,
    const std::filesystem::path& output_path) {

    if (chapters.empty()) {
        return Result<std::filesystem::path>(ErrorCode::InvalidArgument, "No chapters provided");
    }

    try {
        std::ofstream file(output_path);
        if (!file.is_open()) {
            return Result<std::filesystem::path>(ErrorCode::Unknown, "Cannot create chapter file");
        }

        file << "<?xml version=\"1.0\"?>\n";
        file << "<Chapters>\n";
        file << "  <EditionEntry>\n";

        for (size_t i = 0; i < chapters.size(); ++i) {
            const auto& chapter = chapters[i];

            // Convert seconds to HH:MM:SS.nnn
            int hours = static_cast<int>(chapter.time_seconds / 3600);
            int minutes = static_cast<int>((chapter.time_seconds - hours * 3600) / 60);
            double seconds = chapter.time_seconds - hours * 3600 - minutes * 60;

            std::ostringstream time_str;
            time_str << std::setfill('0') << std::setw(2) << hours << ":"
                    << std::setw(2) << minutes << ":"
                    << std::setw(6) << std::fixed << std::setprecision(3) << seconds;

            file << "    <ChapterAtom>\n";
            file << "      <ChapterUID>" << (i + 1) << "</ChapterUID>\n";
            file << "      <ChapterTimeStart>" << time_str.str() << "</ChapterTimeStart>\n";
            file << "      <ChapterDisplay>\n";
            file << "        <ChapterString>" << chapter.title << "</ChapterString>\n";
            file << "        <ChapterLanguage>eng</ChapterLanguage>\n";
            file << "      </ChapterDisplay>\n";
            file << "    </ChapterAtom>\n";
        }

        file << "  </EditionEntry>\n";
        file << "</Chapters>\n";
        file.close();

        return Result<std::filesystem::path>(output_path);

    } catch (const std::exception& e) {
        return Result<std::filesystem::path>(ErrorCode::Unknown,
            std::string("Chapter file creation failed: ") + e.what());
    }
}

Result<void> Muxer::clean_subtitle_file(const std::filesystem::path& subtitle_file) {
    try {
        std::ifstream infile(subtitle_file);
        if (!infile.is_open()) {
            return Result<void>(ErrorCode::FileNotFound, "Cannot open subtitle file");
        }

        std::vector<std::string> lines;
        std::string line;
        while (std::getline(infile, line)) {
            lines.push_back(line);
        }
        infile.close();

        // Remove empty dialogue lines
        std::regex empty_pattern("^Dialogue:.*,,,.*$");
        auto new_end = std::remove_if(lines.begin(), lines.end(),
            [&empty_pattern](const std::string& l) {
                return std::regex_match(l, empty_pattern);
            });

        if (new_end != lines.end()) {
            std::ofstream outfile(subtitle_file);
            for (auto it = lines.begin(); it != new_end; ++it) {
                outfile << *it << "\n";
            }
            outfile.close();
            LOG_INFO("Cleaned subtitle file: {}", subtitle_file.string());
        }

        return Result<void>(ErrorCode::Success);

    } catch (const std::exception& e) {
        return Result<void>(ErrorCode::Unknown,
            std::string("Subtitle cleaning failed: ") + e.what());
    }
}

bool Muxer::is_available() {
    return execute_command({"mkvmerge", "--version"}) == 0;
}

int Muxer::execute_command(const std::vector<std::string>& args) {
#ifdef _WIN32
    std::string cmdline;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) cmdline += " ";
        if (args[i].find(' ') != std::string::npos) {
            cmdline += "\"" + args[i] + "\"";
        } else {
            cmdline += args[i];
        }
    }

    STARTUPINFOA si = {sizeof(si)};
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    PROCESS_INFORMATION pi = {};

    if (!CreateProcessA(nullptr, const_cast<char*>(cmdline.c_str()),
                       nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi)) {
        return -1;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exit_code;
    GetExitCodeProcess(pi.hProcess, &exit_code);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return static_cast<int>(exit_code);
#else
    pid_t pid = fork();
    if (pid == 0) {
        std::vector<char*> argv;
        for (const auto& arg : args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);

        execvp(argv[0], argv.data());
        _exit(127);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }
    return -1;
#endif
}

} // namespace crdl
