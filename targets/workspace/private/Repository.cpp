#include "Repository.hpp"

#include <sqlite3.h>
#include <algorithm>
#include <cctype>
#include <memory>
#include <utility>

import UI.Diagnostics;

namespace {

/// @brief RAII prepared statement; its connection is borrowed from Repository.
class Statement
{
public:
	/// @brief Prepares fixed SQL for execution.
	/// @param database Live connection on the current worker.
	/// @param sql Fixed SQL text.
	/// @pre database is open.
	/// @post A statement is owned or an exception is raised.
	Statement(
		sqlite3* database,
		const char* sql
	) :
		database{ database }
	{
		if( sqlite3_prepare_v2( database, sql, -1, &statement, nullptr ) != SQLITE_OK )
			throw tatzel::Exception{ sqlite3_errmsg( database ) };
	}

	/// @brief Prevents duplicate prepared-statement ownership.
	/// @param other Source statement.
	/// @pre None.
	/// @post Copy construction is unavailable.
	Statement( const Statement& other ) = delete;

	/// @brief Prevents duplicate prepared-statement ownership.
	/// @param other Source statement.
	/// @return No value; assignment is unavailable.
	/// @pre None.
	/// @post Copy assignment is unavailable.
	auto operator=( const Statement& other ) -> Statement& = delete;

	/// @brief Finalizes the prepared statement.
	/// @pre Its connection remains alive.
	/// @post SQLite statement resources are released.
	~Statement() { sqlite3_finalize( statement ); }

	/// @brief Binds an owned copy of text to a parameter.
	/// @param index One-based parameter index.
	/// @param value Text copied by SQLite.
	/// @pre The statement has not started stepping.
	/// @post The parameter is bound or an exception is raised.
	auto Text(
		int index,
		std::string_view value
	) -> void
	{
		if( sqlite3_bind_text( statement, index, value.data(), static_cast<int>( value.size() ), SQLITE_TRANSIENT ) != SQLITE_OK )
			throw tatzel::Exception{ sqlite3_errmsg( database ) };
	}

	/// @brief Binds an integer parameter.
	/// @param index One-based parameter index.
	/// @param value Integer value.
	/// @pre The statement has not started stepping.
	/// @post The parameter is bound or an exception is raised.
	auto Integer(
		int index,
		std::int64_t value
	) -> void
	{
		if( sqlite3_bind_int64( statement, index, value ) != SQLITE_OK )
			throw tatzel::Exception{ sqlite3_errmsg( database ) };
	}

	/// @brief Advances to the next row or completes a write.
	/// @return Whether a row is available.
	/// @pre Bound parameters match the fixed SQL.
	/// @post The statement is positioned at a row or has finished.
	auto Step() -> bool
	{
		auto result = sqlite3_step( statement );
		if( result != SQLITE_ROW && result != SQLITE_DONE ) throw tatzel::Exception{ sqlite3_errmsg( database ) };
		return result == SQLITE_ROW;
	}

	/// @brief Copies a text column from the current row.
	/// @param index Zero-based column index.
	/// @return Owned text, empty for SQL NULL.
	/// @pre Step returned true.
	/// @post Statement state is unchanged.
	auto GetText( int index ) const -> std::string
	{
		auto* value = sqlite3_column_text( statement, index );
		return value ? reinterpret_cast<const char*>( value ) : "";
	}

	/// @brief Reads an integer column from the current row.
	/// @param index Zero-based column index.
	/// @return Column value.
	/// @pre Step returned true.
	/// @post Statement state is unchanged.
	auto GetInteger( int index ) const -> std::int64_t { return sqlite3_column_int64( statement, index ); }

private:
	sqlite3* database;
	sqlite3_stmt* statement = nullptr;
};

/// @brief Ensures a required title contains visible text and fits the storage policy.
/// @param value User-provided title or name.
/// @pre None.
/// @post No state is changed; invalid input raises an exception.
auto ValidateTitle( std::string_view value ) -> void
{
	if( value.size() > 160 || std::ranges::none_of( value, []( unsigned char character ) { return !std::isspace( character ); } ) )
		throw tatzel::Exception{ "Enter a shorter name with some visible text." };
}

/// @brief Rejects stale writes after executing a conditional modification.
/// @param database Connection on which the statement completed.
/// @pre A conditional update or delete has just run.
/// @post No state changes; zero matches raises a useful conflict message.
auto CheckChanged( sqlite3* database ) -> void
{
	if( sqlite3_changes( database ) != 1 )
		throw tatzel::Exception{ "This item changed or was removed in another window. Your draft is preserved; reopen the item to load its latest version." };
}

}

sample::Repository::Repository( const std::string& path )
{
	if( sqlite3_open( path.c_str(), &database ) != SQLITE_OK )
	{
		auto message = std::string{ sqlite3_errmsg( database ) };
		sqlite3_close( database );
		database = nullptr;
		throw tatzel::Exception{ message };
	}
	try
	{
		sqlite3_busy_timeout( database, 3000 );
		Execute( "PRAGMA foreign_keys=ON; PRAGMA journal_mode=WAL;"
			"CREATE TABLE IF NOT EXISTS projects(id INTEGER PRIMARY KEY AUTOINCREMENT,name TEXT NOT NULL,description TEXT NOT NULL,archived INTEGER NOT NULL DEFAULT 0,version INTEGER NOT NULL DEFAULT 1);"
			"CREATE TABLE IF NOT EXISTS tasks(id INTEGER PRIMARY KEY AUTOINCREMENT,project_id INTEGER NOT NULL REFERENCES projects(id),title TEXT NOT NULL,description TEXT NOT NULL,status TEXT NOT NULL CHECK(status IN ('todo','doing','done')),priority TEXT NOT NULL CHECK(priority IN ('low','normal','high')),archived INTEGER NOT NULL DEFAULT 0,version INTEGER NOT NULL DEFAULT 1);"
			"CREATE INDEX IF NOT EXISTS tasks_project ON tasks(project_id);"
			"CREATE TABLE IF NOT EXISTS activity(id INTEGER PRIMARY KEY AUTOINCREMENT,message TEXT NOT NULL);"
			"CREATE TABLE IF NOT EXISTS preferences(id INTEGER PRIMARY KEY CHECK(id=1),compact INTEGER NOT NULL,show_completed INTEGER NOT NULL);" );
		auto query = Statement{ database, "SELECT count(*) FROM preferences" };
		query.Step();
		if( query.GetInteger( 0 ) == 0 ) Reset();
	}
	catch( ... )
	{
		sqlite3_close( database );
		database = nullptr;
		throw;
	}
}

sample::Repository::~Repository() { sqlite3_close( database ); }

auto sample::Repository::Execute( const char* sql ) -> void
{
	if( sqlite3_exec( database, sql, nullptr, nullptr, nullptr ) != SQLITE_OK )
		throw tatzel::Exception{ sqlite3_errmsg( database ) };
}

auto sample::Repository::Load() -> Snapshot
{
	auto result = Snapshot{};
	Execute( "BEGIN" );
	try
	{
		auto projects = Statement{ database, "SELECT id,name,description,archived,version FROM projects ORDER BY id" };
		while( projects.Step() ) result.projects.push_back( { projects.GetInteger( 0 ), projects.GetText( 1 ), projects.GetText( 2 ), projects.GetInteger( 3 ) != 0, static_cast<int>( projects.GetInteger( 4 ) ) } );
		auto tasks = Statement{ database, "SELECT id,project_id,title,description,status,priority,archived,version FROM tasks ORDER BY id DESC" };
		while( tasks.Step() ) result.tasks.push_back( { tasks.GetInteger( 0 ), tasks.GetInteger( 1 ), tasks.GetText( 2 ), tasks.GetText( 3 ), tasks.GetText( 4 ), tasks.GetText( 5 ), tasks.GetInteger( 6 ) != 0, static_cast<int>( tasks.GetInteger( 7 ) ) } );
		auto activity = Statement{ database, "SELECT message FROM activity ORDER BY id DESC LIMIT 30" };
		while( activity.Step() ) result.activity.push_back( activity.GetText( 0 ) );
		auto preferences = Statement{ database, "SELECT compact,show_completed FROM preferences WHERE id=1" };
		if( preferences.Step() )
		{
			result.compact = preferences.GetInteger( 0 ) != 0;
			result.show_completed = preferences.GetInteger( 1 ) != 0;
		}
		Execute( "COMMIT" );
		return result;
	}
	catch( ... ) { Execute( "ROLLBACK" ); throw; }
}

auto sample::Repository::Record( std::string_view message ) -> void
{
	auto statement = Statement{ database, "INSERT INTO activity(message) VALUES(?)" };
	statement.Text( 1, message );
	statement.Step();
	Execute( "DELETE FROM activity WHERE id NOT IN (SELECT id FROM activity ORDER BY id DESC LIMIT 200)" );
}

auto sample::Repository::SaveTask( const Task& task ) -> void
{
	ValidateTitle( task.title );
	if( task.description.size() > 10000 ) throw tatzel::Exception{ "The description is too long. Please shorten it." };
	if( task.status != "todo" && task.status != "doing" && task.status != "done" ) throw tatzel::Exception{ "Choose a valid task status." };
	if( task.priority != "low" && task.priority != "normal" && task.priority != "high" ) throw tatzel::Exception{ "Choose a valid priority." };
	Execute( "BEGIN IMMEDIATE" );
	try
	{
		auto project = Statement{ database, "SELECT archived FROM projects WHERE id=?" };
		project.Integer( 1, task.project_id );
		if( !project.Step() || project.GetInteger( 0 ) != 0 ) throw tatzel::Exception{ "Choose an active project. Restore an archived project before editing its tasks." };
		auto statement = Statement{ database, task.id == 0
			? "INSERT INTO tasks(project_id,title,description,status,priority,archived) VALUES(?,?,?,?,?,?)"
			: "UPDATE tasks SET project_id=?,title=?,description=?,status=?,priority=?,archived=?,version=version+1 WHERE id=? AND version=?" };
		statement.Integer( 1, task.project_id );
		statement.Text( 2, task.title );
		statement.Text( 3, task.description );
		statement.Text( 4, task.status );
		statement.Text( 5, task.priority );
		statement.Integer( 6, task.archived );
		if( task.id ) { statement.Integer( 7, task.id ); statement.Integer( 8, task.version ); }
		statement.Step();
		CheckChanged( database );
		Record( std::string{ task.id ? "Updated task · " : "Created task · " } + task.title );
		Execute( "COMMIT" );
	}
	catch( ... ) { Execute( "ROLLBACK" ); throw; }
}

auto sample::Repository::SaveProject( const Project& project ) -> void
{
	ValidateTitle( project.name );
	if( project.description.size() > 10000 ) throw tatzel::Exception{ "The description is too long. Please shorten it." };
	Execute( "BEGIN IMMEDIATE" );
	try
	{
		auto statement = Statement{ database, project.id == 0
			? "INSERT INTO projects(name,description,archived) VALUES(?,?,?)"
			: "UPDATE projects SET name=?,description=?,archived=?,version=version+1 WHERE id=? AND version=?" };
		statement.Text( 1, project.name );
		statement.Text( 2, project.description );
		statement.Integer( 3, project.archived );
		if( project.id ) { statement.Integer( 4, project.id ); statement.Integer( 5, project.version ); }
		statement.Step();
		CheckChanged( database );
		Record( std::string{ project.id ? "Updated project · " : "Created project · " } + project.name );
		Execute( "COMMIT" );
	}
	catch( ... ) { Execute( "ROLLBACK" ); throw; }
}

auto sample::Repository::DeleteTask( const Task& task ) -> void
{
	Execute( "BEGIN IMMEDIATE" );
	try
	{
		auto statement = Statement{ database, "DELETE FROM tasks WHERE id=? AND version=?" };
		statement.Integer( 1, task.id );
		statement.Integer( 2, task.version );
		statement.Step();
		CheckChanged( database );
		Record( "Deleted task · " + task.title );
		Execute( "COMMIT" );
	}
	catch( ... ) { Execute( "ROLLBACK" ); throw; }
}

auto sample::Repository::SavePreferences(
	bool compact,
	bool show_completed
) -> void
{
	auto statement = Statement{ database, "UPDATE preferences SET compact=?,show_completed=? WHERE id=1" };
	statement.Integer( 1, compact );
	statement.Integer( 2, show_completed );
	statement.Step();
}

auto sample::Repository::Seed() -> void
{
	Execute( "INSERT INTO projects(name,description) VALUES('Tatzel launch','Build a useful, dependable application with logical C++ controls.'),('Studio refresh','A calmer space for focused work.'),('Learning lab','Small experiments and ideas worth exploring.');" );
	auto first_id = sqlite3_last_insert_rowid( database ) - 2;
	const std::vector<Task> tasks{
		{ 0, first_id, "Shape the workspace", "Define the overview, tasks, projects, and settings views.", "done", "high" },
		{ 0, first_id, "Build the project editor", "Keep drafts intact when another window updates the workspace.", "doing", "high" },
		{ 0, first_id, "Test the complete task journey", "Create, edit, reload, archive, restore, and delete a task.", "todo", "high" },
		{ 0, first_id, "Write the getting-started guide", "Make the sample easy to build and run.", "todo", "normal" },
		{ 0, first_id + 1, "Clear the desk", "Keep only the tools used every day.", "done", "low" },
		{ 0, first_id + 1, "Plan the reading corner", "A comfortable chair and better task lighting.", "doing", "normal" },
		{ 0, first_id + 1, "Sort the reference shelf", "Group useful books and archive older notes.", "todo", "low" },
		{ 0, first_id + 2, "Explore a new idea", "Save a short note about what you want to learn next.", "todo", "normal" },
		{ 0, first_id + 2, "Share a working experiment", "Choose one small result to demonstrate.", "todo", "normal" }
	};
	for( const auto& task : tasks )
	{
		auto statement = Statement{ database, "INSERT INTO tasks(project_id,title,description,status,priority) VALUES(?,?,?,?,?)" };
		statement.Integer( 1, task.project_id );
		statement.Text( 2, task.title );
		statement.Text( 3, task.description );
		statement.Text( 4, task.status );
		statement.Text( 5, task.priority );
		statement.Step();
	}
	Record( "Welcome to your workspace · sample projects and tasks are ready" );
}

auto sample::Repository::Reset() -> void
{
	Execute( "BEGIN IMMEDIATE" );
	try
	{
		Execute( "DELETE FROM tasks; DELETE FROM projects; DELETE FROM activity; INSERT OR REPLACE INTO preferences VALUES(1,0,1);" );
		Seed();
		Execute( "COMMIT" );
	}
	catch( ... ) { Execute( "ROLLBACK" ); throw; }
}
