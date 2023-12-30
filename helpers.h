#ifndef HELPERS_H
#define HELPERS_H

#include <string>
#include <vector>

#ifdef _DEBUG
void logDebugHelper(const char * format, ...);
#define logDebug(...) logDebugHelper(__VA_ARGS__)
#else
#define logDebug(...)
#endif

bool fatalError(const char * format, ...);
void split(const std::string & s, char delim, std::vector<std::string> & elems);
void trim(std::string & s);
std::string getExeAdjacentFile(const std::string & filename);
bool runOnStartup(bool enabled);
bool readEntireFile(const std::string & filename, std::string & contents);
bool writeEntireFile(const std::string & filename, const std::string & contents);

#endif
