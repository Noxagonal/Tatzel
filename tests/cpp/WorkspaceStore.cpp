#include "Repository.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <exception>
#include <filesystem>
#include <string>

/// @brief Verifies persistence, bound text, transactional validation, and stale-write protection.
/// @return Zero when every repository boundary assertion passes.
/// @pre A writable temporary directory and SQLite runtime are available.
/// @post All temporary database files are removed.
auto main() -> int
{
	auto path = std::filesystem::temp_directory_path() / ( "tatzel-workspace-" + std::to_string( std::chrono::steady_clock::now().time_since_epoch().count() ) + ".sqlite3" );
	{
		auto repository = sample::Repository{ path.string() };
		auto initial = repository.Load();
		assert( initial.projects.size() == 3 && initial.tasks.size() == 9 );
		auto project = sample::Project{ 0, "Ääkköset '); DROP TABLE tasks; --", "Unicode and literal SQL text" };
		repository.SaveProject( project );
		auto snapshot = repository.Load();
		project = snapshot.projects.back();
		assert( project.name == "Ääkköset '); DROP TABLE tasks; --" );

		auto task = sample::Task{ 0, project.id, "A persistent task", "A draft", "doing", "high" };
		repository.SaveTask( task );
		task = repository.Load().tasks.front();
		auto stale = task;
		task.title = "Saved from another window";
		repository.SaveTask( task );
		auto rejected = false;
		try { repository.SaveTask( stale ); } catch( const std::exception& ) { rejected = true; }
		assert( rejected );
		assert( repository.Load().tasks.front().title == "Saved from another window" );
		rejected = false;
		try { repository.DeleteTask( stale ); } catch( const std::exception& ) { rejected = true; }
		assert( rejected );

		auto invalid = task;
		invalid.id = 0;
		invalid.project_id = 999999;
		auto before = repository.Load();
		rejected = false;
		try { repository.SaveTask( invalid ); } catch( const std::exception& ) { rejected = true; }
		assert( rejected );
		auto after = repository.Load();
		assert( after.tasks.size() == before.tasks.size() && after.activity.size() == before.activity.size() );

		project.archived = true;
		repository.SaveProject( project );
		rejected = false;
		try { auto copy = after.tasks.front(); repository.SaveTask( copy ); } catch( const std::exception& ) { rejected = true; }
		assert( rejected );
		project = repository.Load().projects.back();
		project.archived = false;
		repository.SaveProject( project );
		task = repository.Load().tasks.front();
		task.archived = true;
		repository.SaveTask( task );
		assert( repository.Load().tasks.front().archived );
		repository.SavePreferences( true, false );
	}
	{
		auto repository = sample::Repository{ path.string() };
		auto persisted = repository.Load();
		assert( persisted.projects.size() == 4 && persisted.tasks.size() == 10 );
		assert( persisted.compact && !persisted.show_completed );
		auto last_id = persisted.tasks.front().id;
		repository.DeleteTask( persisted.tasks.front() );
		assert( repository.Load().tasks.size() == 9 );
		repository.Reset();
		auto reset = repository.Load();
		assert( reset.projects.size() == 3 && reset.tasks.size() == 9 );
		assert( !reset.compact && reset.show_completed );
		assert( std::ranges::all_of( reset.tasks, [ last_id ]( const auto& task ) { return task.id > last_id; } ) );
	}
	std::filesystem::remove( path );
	std::filesystem::remove( path.string() + "-wal" );
	std::filesystem::remove( path.string() + "-shm" );
}
