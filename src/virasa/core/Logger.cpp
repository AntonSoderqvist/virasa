// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Anton Soderqvist

#include "virasa/core/Logger.h"

#include <atomic>
#include <cassert>
#include <mutex>
#include <string>
#include <unordered_map>

#include "quill/Backend.h"
#include "quill/Frontend.h"
#include "quill/LogMacros.h"
#include "quill/sinks/ConsoleSink.h"
#include "quill/sinks/FileSink.h"

namespace virasa
{

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace
{

/// Convert our LogLevel to Quill's LogLevel.
[[nodiscard]] quill::LogLevel ToQuillLevel(LogLevel level) noexcept
{
	switch (level)
	{
		case LogLevel::Trace:
			return quill::LogLevel::TraceL3;
		case LogLevel::Debug:
			return quill::LogLevel::Debug;
		case LogLevel::Info:
			return quill::LogLevel::Info;
		case LogLevel::Warning:
			return quill::LogLevel::Warning;
		case LogLevel::Error:
			return quill::LogLevel::Error;
		case LogLevel::Critical:
			return quill::LogLevel::Critical;
		case LogLevel::Off:
			return quill::LogLevel::None;
		default:
			return quill::LogLevel::Info;
	}
}

// ---------------------------------------------------------------------------
// Module-level state (all guarded by g_mutex)
// ---------------------------------------------------------------------------

std::mutex g_mutex;
bool g_initialized = false;
std::unordered_map<std::string, quill::Logger*> g_loggerCache;
std::vector<std::shared_ptr<quill::Sink>> g_sinks;

/// Internal implementation — must be called with g_mutex held.
void InitializeImpl(LogConfig config)
{
	if (g_initialized)
		return;

	// Start the Quill backend if it is not already running.
	if (!quill::Backend::is_running())
	{
		quill::BackendOptions backendOptions;
		quill::Backend::start(backendOptions);
	}

	// Build the console sink.
	quill::ConsoleSinkConfig consoleSinkConfig;
	consoleSinkConfig.set_colour_mode(config.enableColors
		? quill::ConsoleSinkConfig::ColourMode::Automatic
		: quill::ConsoleSinkConfig::ColourMode::Never);

	std::vector<std::shared_ptr<quill::Sink>> sinks;

	auto consoleSink = quill::Frontend::create_or_get_sink<quill::ConsoleSink>("console_sink", consoleSinkConfig);
	consoleSink->set_log_level_filter(ToQuillLevel(config.consoleLevel));
	sinks.push_back(consoleSink);

	// Build the file sink if a path was supplied.
	if (config.logFilePath != nullptr)
	{
		quill::FileSinkConfig fileSinkConfig;
		fileSinkConfig.set_open_mode('w');

		auto fileSink = quill::Frontend::create_or_get_sink<quill::FileSink>(
			config.logFilePath, fileSinkConfig);
		fileSink->set_log_level_filter(ToQuillLevel(config.fileLevel));
		sinks.push_back(fileSink);
	}

	// Store the sinks so GetLogger can attach them to new loggers.
	g_sinks = std::move(sinks);

	g_initialized = true;
}

/// Internal implementation — must be called with g_mutex held.
void ShutdownImpl()
{
	if (!g_initialized)
		return;

	// Flush synchronously.
	for (auto* logger : quill::Frontend::get_all_loggers())
		logger->flush_log();

	// Clear the logger cache.
	g_loggerCache.clear();
	g_sinks.clear();

	g_initialized = false;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Logger static method definitions
// ---------------------------------------------------------------------------

void Logger::Initialize(const LogConfig& config)
{
	std::lock_guard<std::mutex> lock(g_mutex);
	InitializeImpl(config);
}

void Logger::Shutdown()
{
	std::lock_guard<std::mutex> lock(g_mutex);
	ShutdownImpl();
}

quill::Logger* Logger::GetLogger(const char* module_name)
{
	assert(module_name != nullptr);

	std::lock_guard<std::mutex> lock(g_mutex);

	// Auto-initialize with defaults if needed.
	if (!g_initialized)
		InitializeImpl(LogConfig{});

	// Return cached logger if available.
	auto it = g_loggerCache.find(module_name);
	if (it != g_loggerCache.end())
		return it->second;

	// Create a new logger and cache it.
	quill::Logger* logger = quill::Frontend::create_or_get_logger(module_name, g_sinks);
	g_loggerCache.emplace(module_name, logger);
	return logger;
}

void Logger::Flush()
{
	std::lock_guard<std::mutex> lock(g_mutex);
	if (!g_initialized)
		return;
	for (auto* logger : quill::Frontend::get_all_loggers())
		logger->flush_log();
}

bool Logger::IsInitialized()
{
	std::lock_guard<std::mutex> lock(g_mutex);
	return g_initialized;
}

} // namespace virasa
