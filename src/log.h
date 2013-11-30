#ifndef LOG_H_
#define LOG_H_

/** @file
 * @brief Logs and error handling.
 */

#include <cstdarg>
#include <memory>

/// Logging macro, for convenience.
#define LOG (::Logger::glog)


class Logger
{
 public:
  Logger() {}
  virtual ~Logger() {}

  void flog(const char* fmt, ...);
  void vlog(const char* fmt, va_list ap);
  virtual void log(const char* msg) = 0;

  /// Global logging method.
  static void glog(const char* fmt, ...);

  /** @brief Set global logger.
   * @note The given pointer is owned by the Logger class.
   */
  static void setLogger(Logger* logger);

 private:
  /// Global logger instance.
  static std::unique_ptr<Logger> logger_;
};


/// Default logger.
class FileLogger: public Logger
{
 public:
  FileLogger();
  FileLogger(const char* filename);
  virtual ~FileLogger();

  /** @brief Change or close log file of default logging.
   *
   * Close current log file (if any, and is not \e stderr) and open a new one.
   * If \e filename is \c -, stderr will be used.
   * If \e filename is \e NULL, no log file will be opened (useful to only
   * close current log file).
   */
  void setFile(const char* filename);

  virtual void log(const char* msg);

 private:
  FILE* fp_;
};


#endif
