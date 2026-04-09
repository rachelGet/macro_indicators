#include "logs.h"
#include <ctime>
#include <iostream>
#include <sstream>

using namespace std;

Logger::Logger(const string& filename)
{
    logFile.open(filename, ios::app);
    if (!logFile.is_open()) {
        cerr << "Error opening log file." << endl;
    }
}

Logger::~Logger() { logFile.close(); }

void Logger::log(const string& message)
{
    time_t now = time(0);
    tm* timeinfo = localtime(&now);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp),
             "%Y-%m-%d %H:%M:%S", timeinfo);

    ostringstream logEntry;
    logEntry << "[" << timestamp << "] " << message << endl;

    cout << logEntry.str();

    if (logFile.is_open()) {
        logFile << logEntry.str();
        logFile.flush();
    }
}
