#pragma once
#include <cstdint>
#include <cstdio>
#include <deque>
#include <string>
extern std::uint32_t testNow;
inline std::uint32_t millis() { return testNow; }
inline void delay(unsigned ms) { testNow += ms; }
struct TestSerial {
    std::deque<char> input;
    std::string output;
    void begin(unsigned) {}
    int available() { return int(input.size()); }
    char read() { const char c = input.front(); input.pop_front(); return c; }
    void write(char c) { output += c; }
    void println(const char *text = "") { output += text; output += '\n'; }
    template<class... Args> void printf(const char *format, Args... args) {
        char buffer[512]; std::snprintf(buffer, sizeof(buffer), format, args...); output += buffer;
    }
    void send(const std::string &text) { for (const char c : text + "\n") input.push_back(c); }
};
extern TestSerial Serial;
struct TestEsp {
    unsigned getSketchSize() { return 42; }
    std::string getSketchMD5() { return "stub"; }
};
static TestEsp ESP;
