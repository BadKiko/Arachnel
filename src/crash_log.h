#pragma once

#include <QString>

namespace arachnel {

bool isCrashDialogMode(int argc, char* argv[]);
void markApplicationShuttingDown();
void installCrashLogging();
void logRunStarted(int argc, char* argv[]);
void logRunFinished(int exitCode);

bool hasPendingCrashReport();
QString pendingCrashSummary();
QString pendingCrashDetails();
QString pendingCrashReportPath();
QString pendingCrashIssueUrl();
void dismissPendingCrashReport();
void openPendingCrashIssue();
void revealPendingCrashReport();

} // namespace arachnel
