#pragma once

namespace arachnel {

void installCrashLogging();
void logRunStarted(int argc, char* argv[]);
void logRunFinished(int exitCode);

} // namespace arachnel
