// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Anton Soderqvist

#ifndef VIRASA_CORE_LOGGER_H
#define VIRASA_CORE_LOGGER_H

#include <cstdint>

#include "quill/Logger.h"

namespace virasa
{

/**
 * @brief Minimum severity level for log messages.
 *
 * Values are ordered from most verbose (Trace) to least verbose (Critical).
 * Off disables logging entirely when used as a sink minimum level.
 */
enum class LogLevel : uint8_t
{
	Trace = 0,
	Debug,
	Info,
	Warning,
	Error,
	Critical,
	Off
};

/**
 * @brief Configuration for the logging system.
 *
 * Holds all parameters needed by Logger::Initialize. Pointer members do not
 * own the strings they reference; the caller must keep referenced strings
 * live for the duration of any Initialize call that receives this struct.
 */
struct LogConfig
{
public:
	/// Minimum level for the console sink.
	LogLevel consoleLevel = LogLevel::Info;
	/// Minimum level for the file sink.
	LogLevel fileLevel = LogLevel::Trace;
	/// Path for the log file, or nullptr to disable the file sink.
	const char* logFilePath = nullptr;
	/// Custom log pattern, or nullptr to use the default pattern.
	const char* pattern = nullptr;
	/// Whether to enable ANSI color output on the console sink.
	bool enableColors = true;
};

/**
 * @brief Static-only logging facade backed by the Quill logging library.
 *
 * Logger is a non-instantiable class providing a thread-safe static API for
 * initializing, querying, and tearing down the logging system. All methods
 * are static; the class cannot be constructed, copied, or moved.
 */
class Logger final
{
	public:
	/**
	 * @brief Initialize the logging system with the supplied configuration.
	 *
	 * Starts the Quill backend, creates a console sink, and optionally a file
	 * sink. Idempotent: if already initialized, returns without any side effect.
	 *
	 * @param config Configuration to apply. Defaults to a default-constructed
	 *               LogConfig (Info console, Trace file, no file, colors on).
	 */
	static void Initialize(LogConfig config = LogConfig{});

	/**
	 * @brief Flush all pending messages and tear down the logging system.
	 *
	 * Safe to call when not initialized; in that case it is a no-op.
	 * After this call IsInitialized() returns false.
	 */
	static void Shutdown();

	/**
	 * @brief Return a named logger, auto-initializing if necessary.
	 *
	 * If the logging system is not yet initialized, triggers automatic
	 * initialization with a default-constructed LogConfig before returning
	 * the logger. The returned pointer is valid until the next Shutdown call.
	 *
	 * @param module_name Null-terminated name for the logger. Must not be null.
	 * @return Non-null pointer to the named quill::Logger.
	 */
	[[nodiscard]] static quill::Logger* GetLogger(const char* module_name);

	/**
	 * @brief Block until all buffered messages have been written to their sinks.
	 *
	 * Safe to call when not initialized; in that case it is a no-op.
	 */
	static void Flush();

	/**
	 * @brief Report whether the logging system is currently initialized.
	 *
	 * Purely observational — does not trigger initialization or modify state.
	 *
	 * @return true if initialized, false otherwise.
	 */
	[[nodiscard]] static bool IsInitialized();

	Logger() = delete;
	~Logger() = delete;
	Logger(const Logger&) = delete;
	Logger& operator=(const Logger&) = delete;
	Logger(Logger&&) = delete;
	Logger& operator=(Logger&&) = delete;
};

} // namespace virasa

#endif // VIRASA_CORE_LOGGER_H
