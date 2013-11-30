#include <cstdio>
#include <cstring>
#include <cerrno>
#include <stdexcept>
#include "log.h"


std::unique_ptr<Logger> Logger::logger_;

void Logger::flog(const char* fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  this->vlog(fmt, ap);
  va_end(ap);
}

void Logger::vlog(const char* fmt, va_list ap)
{
  va_list ap2;
  va_copy(ap2,ap);
  int n = vsnprintf(NULL, 0, fmt, ap2) + 1;
  va_end(ap2);
  char msg[n];
  vsnprintf(msg, n, fmt, ap);
  this->log(msg);
}

void Logger::glog(const char* fmt, ...)
{
  if(!logger_) {
    return;
  }
  va_list ap;
  va_start(ap, fmt);
  logger_->vlog(fmt, ap);
  va_end(ap);
}

void Logger::setLogger(Logger* logger)
{
  logger_.reset(logger);
}


FileLogger::FileLogger(): fp_(stderr) {}

FileLogger::FileLogger(const char* filename)
{
  this->setFile(filename);
}

FileLogger::~FileLogger()
{
  this->setFile(NULL); // close log file
}


void FileLogger::setFile(const char* filename)
{
  if(fp_ != NULL && fp_ != stderr && fp_ != stdout) {
    ::fclose(fp_);
  }
  fp_ = NULL;

  if(filename == NULL) {
    return;
  }

  if(filename[0] == '-' && filename[1] == '\0') {
    fp_ = stderr;
  } else {
    fp_ = ::fopen(filename, "w");
    if(fp_ == NULL) {
      throw std::runtime_error(::strerror(errno));
    }
  }
}


void FileLogger::log(const char* msg)
{
  if(fp_ == NULL) {
    return;
  }
  ::fprintf(fp_, "%s\n", msg);
  ::fflush(fp_);
}

