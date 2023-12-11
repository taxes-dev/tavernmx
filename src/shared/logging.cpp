#include <memory>
#include <vector>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include "tavernmx/logging.h"

namespace {
    std::vector<std::shared_ptr<spdlog::logger>> s_loggers{};
}

namespace tavernmx {
    void configure_logging(spdlog::level::level_enum level, const std::optional<std::string> &log_file) {
        spdlog::drop_all();
        s_loggers.clear();
        s_loggers.push_back(spdlog::stdout_color_mt("console"));
    	if (log_file) {
    		s_loggers.push_back(spdlog::basic_logger_mt("file", *log_file));
    	}
        spdlog::set_pattern("%Y-%m-%d %H:%M:%S.%e [%t][%^%l%$] %v");
        spdlog::set_level(level);
    }

	std::vector<std::shared_ptr<spdlog::logger>> & get_loggers() {
	    return s_loggers;
    }
}