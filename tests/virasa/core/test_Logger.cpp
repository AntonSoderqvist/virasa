#include <atomic>
#include <cstdint>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

#include "virasa/core/Logger.h"

// ---------------------------------------------------------------------------
// Helper: ensure Logger is shut down before and after each test so tests are
// independent.  We cannot use a fixture with a non-trivial constructor because
// Logger itself is non-instantiable; instead we use free helpers called at the
// top/bottom of each test that needs a clean slate.
// ---------------------------------------------------------------------------
namespace
{

void EnsureShutdown()
{
	if (virasa::Logger::IsInitialized())
	{
		virasa::Logger::Shutdown();
	}
}

} // namespace

// ---------------------------------------------------------------------------
// log_level_enum_values_in_declared_order
// ---------------------------------------------------------------------------
TEST(Logger, test_log_level_enum_values_in_declared_order)
{
	using virasa::LogLevel;

	// Underlying type must be uint8_t.
	static_assert(std::is_same_v<std::underlying_type_t<LogLevel>, uint8_t>,
		"LogLevel underlying type must be uint8_t");

	// Trace is explicitly 0.
	EXPECT_EQ(static_cast<uint8_t>(LogLevel::Trace), uint8_t{0});

	// Values are strictly ordered from most verbose to least verbose.
	EXPECT_LT(static_cast<uint8_t>(LogLevel::Trace), static_cast<uint8_t>(LogLevel::Debug));
	EXPECT_LT(static_cast<uint8_t>(LogLevel::Debug), static_cast<uint8_t>(LogLevel::Info));
	EXPECT_LT(static_cast<uint8_t>(LogLevel::Info), static_cast<uint8_t>(LogLevel::Warning));
	EXPECT_LT(static_cast<uint8_t>(LogLevel::Warning), static_cast<uint8_t>(LogLevel::Error));
	EXPECT_LT(static_cast<uint8_t>(LogLevel::Error), static_cast<uint8_t>(LogLevel::Critical));
	EXPECT_LT(static_cast<uint8_t>(LogLevel::Critical), static_cast<uint8_t>(LogLevel::Off));
}

// ---------------------------------------------------------------------------
// log_config_holds_logger_configuration_fields
// ---------------------------------------------------------------------------
TEST(Logger, test_log_config_holds_logger_configuration_fields)
{
	using virasa::LogConfig;
	using virasa::LogLevel;

	// Default-constructed LogConfig must satisfy all documented defaults.
	LogConfig cfg{};

	EXPECT_EQ(cfg.consoleLevel, LogLevel::Info);
	EXPECT_EQ(cfg.fileLevel, LogLevel::Trace);
	EXPECT_EQ(cfg.logFilePath, nullptr);
	EXPECT_EQ(cfg.pattern, nullptr);
	EXPECT_EQ(cfg.enableColors, true);

	// LogConfig must be copyable.
	LogConfig copy = cfg;
	EXPECT_EQ(copy.consoleLevel, cfg.consoleLevel);
	EXPECT_EQ(copy.fileLevel, cfg.fileLevel);
	EXPECT_EQ(copy.logFilePath, cfg.logFilePath);
	EXPECT_EQ(copy.pattern, cfg.pattern);
	EXPECT_EQ(copy.enableColors, cfg.enableColors);

	// LogConfig must be movable.
	LogConfig moved = std::move(copy);
	EXPECT_EQ(moved.consoleLevel, LogLevel::Info);

	// Members can be mutated.
	cfg.consoleLevel = LogLevel::Warning;
	cfg.fileLevel = LogLevel::Debug;
	cfg.enableColors = false;
	EXPECT_EQ(cfg.consoleLevel, LogLevel::Warning);
	EXPECT_EQ(cfg.fileLevel, LogLevel::Debug);
	EXPECT_EQ(cfg.enableColors, false);
}

// ---------------------------------------------------------------------------
// logger_is_non_instantiable_static_api
// ---------------------------------------------------------------------------
TEST(Logger, test_logger_is_non_instantiable_static_api)
{
	using virasa::Logger;

	// Logger must not be default-constructible.
	static_assert(
		!std::is_default_constructible_v<Logger>, "Logger must not be default-constructible");

	// Logger must not be copy-constructible or copy-assignable.
	static_assert(!std::is_copy_constructible_v<Logger>, "Logger must not be copy-constructible");
	static_assert(!std::is_copy_assignable_v<Logger>, "Logger must not be copy-assignable");

	// Logger must not be move-constructible or move-assignable.
	static_assert(!std::is_move_constructible_v<Logger>, "Logger must not be move-constructible");
	static_assert(!std::is_move_assignable_v<Logger>, "Logger must not be move-assignable");

	// Logger must be final.
	static_assert(std::is_final_v<Logger>, "Logger must be final");

	// All five public members must be static (verified by calling them without
	// an instance — this compiles only if they are static).
	// We don't actually call them here to avoid side-effects; the static_assert
	// checks above are sufficient for the structural guarantee.
	// A compile-time check: take the address of each static member function.
	[[maybe_unused]] auto pInit = &Logger::Initialize;
	[[maybe_unused]] auto pShutdown = &Logger::Shutdown;
	[[maybe_unused]] auto pGetLogger = &Logger::GetLogger;
	[[maybe_unused]] auto pFlush = &Logger::Flush;
	[[maybe_unused]] auto pIsInitialized = &Logger::IsInitialized;
}

// ---------------------------------------------------------------------------
// initialize_starts_logging_system_and_is_idempotent
// ---------------------------------------------------------------------------
TEST(Logger, test_initialize_starts_logging_system_and_is_idempotent)
{
	using virasa::LogConfig;
	using virasa::Logger;

	EnsureShutdown();

	// Before initialization IsInitialized must return false.
	EXPECT_FALSE(Logger::IsInitialized());

	// Initialize with default config.
	Logger::Initialize();
	EXPECT_TRUE(Logger::IsInitialized());

	// Second Initialize call must be idempotent — no crash, state stays true.
	Logger::Initialize();
	EXPECT_TRUE(Logger::IsInitialized());

	// Initialize with an explicit LogConfig must also be idempotent.
	LogConfig cfg;
	cfg.consoleLevel = virasa::LogLevel::Warning;
	Logger::Initialize(cfg);
	EXPECT_TRUE(Logger::IsInitialized());

	EnsureShutdown();
}

// ---------------------------------------------------------------------------
// shutdown_flushes_and_tears_down_logging_system
// ---------------------------------------------------------------------------
TEST(Logger, test_shutdown_flushes_and_tears_down_logging_system)
{
	using virasa::Logger;

	EnsureShutdown();

	// Shutdown on an uninitialized system must be safe (no crash).
	Logger::Shutdown();
	EXPECT_FALSE(Logger::IsInitialized());

	// Initialize, then shut down.
	Logger::Initialize();
	ASSERT_TRUE(Logger::IsInitialized());

	Logger::Shutdown();
	EXPECT_FALSE(Logger::IsInitialized());

	// A subsequent Initialize must succeed after Shutdown.
	Logger::Initialize();
	EXPECT_TRUE(Logger::IsInitialized());

	EnsureShutdown();
}

// ---------------------------------------------------------------------------
// get_logger_returns_non_null_and_auto_initializes
// ---------------------------------------------------------------------------
TEST(Logger, test_get_logger_returns_non_null_and_auto_initializes)
{
	using virasa::Logger;

	EnsureShutdown();
	ASSERT_FALSE(Logger::IsInitialized());

	// GetLogger must auto-initialize and return a non-null pointer.
	quill::Logger* logger = Logger::GetLogger("test_module");
	ASSERT_NE(logger, nullptr);

	// After auto-initialization IsInitialized must return true.
	EXPECT_TRUE(Logger::IsInitialized());

	EnsureShutdown();

	// GetLogger after explicit Initialize must also return non-null.
	Logger::Initialize();
	quill::Logger* logger2 = Logger::GetLogger("test_module_2");
	ASSERT_NE(logger2, nullptr);

	EnsureShutdown();
}

// ---------------------------------------------------------------------------
// get_logger_caches_by_module_name
// ---------------------------------------------------------------------------
TEST(Logger, test_get_logger_caches_by_module_name)
{
	using virasa::Logger;

	EnsureShutdown();
	Logger::Initialize();

	// Two calls with the same name must return the same pointer.
	quill::Logger* first = Logger::GetLogger("cached_module");
	quill::Logger* second = Logger::GetLogger("cached_module");
	ASSERT_NE(first, nullptr);
	ASSERT_NE(second, nullptr);
	EXPECT_EQ(first, second);

	// Different names must not collide.
	quill::Logger* other = Logger::GetLogger("other_module");
	ASSERT_NE(other, nullptr);
	EXPECT_NE(first, other);

	// After Shutdown + re-Initialize the cache is cleared; a new pointer is
	// returned for the same name (it may or may not be the same address, but
	// the contract says a *new* instance is created — we can only verify
	// non-null and that the system is functional again).
	Logger::Shutdown();
	Logger::Initialize();
	quill::Logger* afterReinit = Logger::GetLogger("cached_module");
	ASSERT_NE(afterReinit, nullptr);

	EnsureShutdown();
}

// ---------------------------------------------------------------------------
// flush_blocks_until_pending_messages_written
// ---------------------------------------------------------------------------
TEST(Logger, test_flush_blocks_until_pending_messages_written)
{
	using virasa::Logger;

	// Flush on an uninitialized system must be safe (no crash).
	EnsureShutdown();
	Logger::Flush(); // must not crash or block indefinitely
	EXPECT_FALSE(Logger::IsInitialized());

	// Flush on an initialized system must return (not deadlock).
	Logger::Initialize();
	ASSERT_TRUE(Logger::IsInitialized());
	Logger::Flush(); // must complete without hanging
	EXPECT_TRUE(Logger::IsInitialized());

	EnsureShutdown();
}

// ---------------------------------------------------------------------------
// is_initialized_reports_initialization_state
// ---------------------------------------------------------------------------
TEST(Logger, test_is_initialized_reports_initialization_state)
{
	using virasa::Logger;

	EnsureShutdown();

	// Must be false before any initialization.
	EXPECT_FALSE(Logger::IsInitialized());

	// Calling IsInitialized must not trigger initialization.
	EXPECT_FALSE(Logger::IsInitialized());
	EXPECT_FALSE(Logger::IsInitialized());

	Logger::Initialize();
	EXPECT_TRUE(Logger::IsInitialized());

	// Calling IsInitialized must not change state.
	EXPECT_TRUE(Logger::IsInitialized());

	Logger::Shutdown();
	EXPECT_FALSE(Logger::IsInitialized());

	EnsureShutdown();
}

// ---------------------------------------------------------------------------
// logger_api_is_thread_safe
// ---------------------------------------------------------------------------
TEST(Logger, test_logger_api_is_thread_safe)
{
	using virasa::Logger;

	EnsureShutdown();
	Logger::Initialize();
	ASSERT_TRUE(Logger::IsInitialized());

	constexpr int kThreadCount = 8;
	constexpr int kIterations = 50;
	std::atomic<int> nullCount{0};

	// Spawn threads that concurrently call GetLogger, IsInitialized, and Flush.
	std::vector<std::thread> threads;
	threads.reserve(kThreadCount);
	for (int t = 0; t < kThreadCount; ++t)
	{
		threads.emplace_back(
			[&, t]()
			{
				for (int i = 0; i < kIterations; ++i)
				{
					// Alternate between two module names to exercise cache contention.
					const char* name =
						(i % 2 == 0) ? "thread_module_a" : "thread_module_b";
					quill::Logger* logger = Logger::GetLogger(name);
					if (logger == nullptr)
					{
						nullCount.fetch_add(1, std::memory_order_relaxed);
					}
					Logger::IsInitialized();
					if (t == 0 && i % 10 == 0)
					{
						Logger::Flush();
					}
				}
			});
	}

	for (auto& th : threads)
	{
		th.join();
	}

	// No GetLogger call should have returned null.
	EXPECT_EQ(nullCount.load(), 0);

	// Concurrent Initialize calls (idempotency under concurrency).
	std::vector<std::thread> initThreads;
	initThreads.reserve(kThreadCount);
	for (int t = 0; t < kThreadCount; ++t)
	{
		initThreads.emplace_back(
			[]()
			{
				for (int i = 0; i < 10; ++i)
				{
					Logger::Initialize();
				}
			});
	}
	for (auto& th : initThreads)
	{
		th.join();
	}
	EXPECT_TRUE(Logger::IsInitialized());

	EnsureShutdown();
}
