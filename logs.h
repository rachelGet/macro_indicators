#ifndef LOGS_H
#define LOGS_H

#include <string>
#include <fstream>

class Logger {
public:
    Logger(const std::string& filename);
    ~Logger();
    void log(const std::string& message);
private:
    std::ofstream logFile;
};

extern Logger* globalLogger;

#endif
