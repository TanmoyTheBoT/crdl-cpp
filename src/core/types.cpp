#include <crdl/core/types.h>
#include <crdl/utils/string_utils.h>
#include <sstream>
#include <iomanip>

namespace crdl {

std::string EpisodeMetadata::to_filename(const std::string& quality, const std::string& release_group) const {
    std::ostringstream ss;

    // Series.S##E##.Title.Quality.CR.WEB-DL.AAC2.0.H.264-GROUP
    std::string series = series_title.empty() ? "Crunchyroll" : series_title;
    ss << string_utils::sanitize_filename(series) << ".";
    ss << "S" << std::setfill('0') << std::setw(2) << season_number;
    ss << "E" << std::setfill('0') << std::setw(2) << episode_number;

    if (!title.empty() && title != "Episode") {
        ss << "." << string_utils::sanitize_filename(title);
    }

    ss << "." << quality << ".CR.WEB-DL.AAC2.0.H.264-" << release_group;

    return ss.str();
}

} // namespace crdl
