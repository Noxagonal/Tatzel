#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

struct sqlite3;

namespace sample {

/// @brief Owned project record transferred between the database worker and a UI loop.
struct Project
{
	std::int64_t id = 0; ///< Persistent identifier; zero means a new project.
	std::string name; ///< Human-readable project name.
	std::string description; ///< Plain-text project description.
	bool archived = false; ///< Whether the project is hidden from active views.
	int version = 0; ///< Optimistic concurrency revision.
};

/// @brief Owned task record; no view or database storage is borrowed.
struct Task
{
	std::int64_t id = 0; ///< Persistent identifier; zero means a new task.
	std::int64_t project_id = 0; ///< Owning project identifier.
	std::string title; ///< Required task title.
	std::string description; ///< Plain-text notes.
	std::string status = "todo"; ///< todo, doing, or done.
	std::string priority = "normal"; ///< low, normal, or high.
	bool archived = false; ///< Whether the task is archived.
	int version = 0; ///< Optimistic concurrency revision.
};

/// @brief Complete immutable-by-convention workspace snapshot delivered to each page.
struct Snapshot
{
	std::vector<Project> projects; ///< All active and archived projects.
	std::vector<Task> tasks; ///< All active and archived tasks.
	std::vector<std::string> activity; ///< Most recent 30 committed actions.
	bool compact = false; ///< Shared compact-list preference.
	bool show_completed = true; ///< Shared default for completed tasks.
};

/// @brief SQLite repository confined to one worker after construction.
/// @details Owns its connection. All writes use transactions, bound parameters,
/// validation, and optimistic version checks. Construction seeds only a new store.
class Repository
{
public:
	/// @brief Opens or creates a workspace and initializes its schema and first dataset.
	/// @param path SQLite filename; its parent directory must exist.
	/// @pre No other thread uses this instance.
	/// @post A valid connection and schema are owned.
	/// @throws tatzel::Exception If opening, schema initialization, or seeding fails.
	explicit Repository( const std::string& path );

	/// @brief Prevents duplicate ownership of the SQLite connection.
	/// @param other Source repository.
	/// @pre None.
	/// @post Copy construction is unavailable.
	Repository( const Repository& other ) = delete;

	/// @brief Prevents duplicate ownership of the SQLite connection.
	/// @param other Source repository.
	/// @return No value; assignment is unavailable.
	/// @pre None.
	/// @post Copy assignment is unavailable.
	auto operator=( const Repository& other ) -> Repository& = delete;

	/// @brief Closes the database connection.
	/// @pre Worker use has finished.
	/// @post SQLite resources are released.
	~Repository();

	/// @brief Loads an internally consistent workspace snapshot.
	/// @return Owned records, preferences, and recent activity.
	/// @pre Called on the worker; no transaction is active.
	/// @post Database state is unchanged.
	auto Load() -> Snapshot;

	/// @brief Creates or updates a task using its expected version.
	/// @param task Owned task values; zero ID creates a new task.
	/// @pre Called on the worker.
	/// @post A valid task and its activity entry are committed together.
	/// @throws tatzel::Exception If validation or optimistic concurrency checks fail.
	auto SaveTask( const Task& task ) -> void;

	/// @brief Creates or updates a project using its expected version.
	/// @param project Owned project values; zero ID creates a new project.
	/// @pre Called on the worker.
	/// @post A valid project and its activity entry are committed together.
	/// @throws tatzel::Exception If validation or optimistic concurrency checks fail.
	auto SaveProject( const Project& project ) -> void;

	/// @brief Permanently deletes a task using its expected version.
	/// @param task Existing task identity and version.
	/// @pre Called on the worker.
	/// @post The task is deleted and an activity entry is committed.
	/// @throws tatzel::Exception If the task changed or no longer exists.
	auto DeleteTask( const Task& task ) -> void;

	/// @brief Stores shared workspace display preferences.
	/// @param compact Whether task rows use compact spacing.
	/// @param show_completed Whether completed tasks appear by default.
	/// @pre Called on the worker.
	/// @post Both preferences are committed together.
	auto SavePreferences(
		bool compact,
		bool show_completed
	) -> void;

	/// @brief Replaces workspace records with the sample dataset in one transaction.
	/// @pre Called on the worker after the user confirms reset.
	/// @post New identifiers prevent old open drafts from addressing replacement records.
	auto Reset() -> void;

private:
	/// @brief Executes fixed internal SQL.
	/// @param sql Complete SQL with no user-provided interpolation.
	/// @pre The connection is open and confined to this thread.
	/// @post Statements have completed or an exception has been raised.
	auto Execute( const char* sql ) -> void;

	/// @brief Adds an activity message to the current transaction.
	/// @param message Plain text copied into SQLite.
	/// @pre A write transaction is active.
	/// @post The message is stored if the surrounding transaction commits.
	auto Record( std::string_view message ) -> void;

	/// @brief Inserts the initial projects and tasks in the current transaction.
	/// @pre Project and task tables are empty; a transaction is active.
	/// @post The dataset is inserted with fresh monotonically increasing IDs.
	auto Seed() -> void;

	sqlite3* database = nullptr;
};

}
