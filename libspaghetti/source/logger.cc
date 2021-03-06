#include "spaghetti/logger.h"

namespace spaghetti::log {

Logger g_loggerConsole{};
Logger g_loggerFile{};

void init()
{
  if (!g_loggerConsole) g_loggerConsole = spdlog::stdout_color_mt("console");
  if (!g_loggerFile) g_loggerFile = spdlog::basic_logger_mt("file", "spaghetti.log");

  spdlog::set_pattern("[%Y.%m.%d %H:%M:%S.%e] [%n] [%L] %v");

  g_loggerConsole->set_level(spdlog::level::debug);
}

Loggers get()
{
  return { g_loggerConsole, g_loggerFile };
}

} // namespace spaghetti::log
