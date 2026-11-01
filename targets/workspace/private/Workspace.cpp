#include "Repository.hpp"

#include <algorithm>
#include <charconv>
#include <condition_variable>
#include <cstddef>
#include <cstdlib>
#include <deque>
#include <exception>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

import UI;

namespace sample {

using Builder = tatzel::PageBuilder<>;
using ContentBuilder = tatzel::PageBuilder<tatzel::ui::Container>;

/// @brief Formats a task status for display without exposing storage identifiers.
/// @param status Persisted status value.
/// @return Human-readable label.
/// @pre status is a repository status.
/// @post State is unchanged.
auto StatusLabel( std::string_view status ) -> std::string
{
	if( status == "done" ) return "Done";
	if( status == "doing" ) return "In progress";
	return "To do";
}

/// @brief Parses a complete positive numeric record identifier.
/// @param value Decimal route suffix.
/// @return Positive ID, or zero for malformed input.
/// @pre None.
/// @post State is unchanged.
auto ParseID( std::string_view value ) -> std::int64_t
{
	std::int64_t result = 0;
	auto parsed = std::from_chars( value.data(), value.data() + value.size(), result );
	return parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size() && result > 0 ? result : 0;
}

/// @brief Case-folds ASCII search text while retaining other UTF-8 bytes unchanged.
/// @param value Text to copy.
/// @return Owned search key.
/// @pre None.
/// @post Input text is unchanged.
auto SearchKey( std::string_view value ) -> std::string
{
	auto result = std::string{ value };
	for( auto& character : result ) if( character >= 'A' && character <= 'Z' ) character += 'a' - 'A';
	return result;
}

/// @brief Serializes database work and publishes committed snapshots through weak dispatchers.
/// @details Pages own their views. Subscribers capture weak page references. The worker
/// drains accepted operations before shutdown and never touches logical controls.
class Workspace
{
public:
	/// @brief Opens the store and starts one database worker.
	/// @param path Persistent SQLite filename.
	/// @pre Its parent directory exists.
	/// @post Accepted work runs sequentially outside UI event loops.
	explicit Workspace( const std::string& path ) :
		repository{ path },
		worker{ [ this ] { Run(); } }
	{}

	/// @brief Stops accepting work, drains the queue, and joins the worker.
	/// @pre Called outside the database worker after page submission has stopped.
	/// @post No database work remains and captured jobs have been released.
	~Workspace()
	{
		{
			auto lock = std::lock_guard{ mutex };
			stopping = true;
		}
		condition.notify_one();
		worker.join();
	}

	/// @brief Enqueues bounded application work without blocking the UI loop.
	/// @param work Operation using only the worker-confined repository.
	/// @return Whether ownership of work was accepted.
	/// @pre Captures own their data and never access logical controls from the worker.
	/// @post Accepted work runs once before shutdown; rejected captures are released here.
	auto Submit( std::function<void( Repository& )> work ) -> bool
	{
		{
			auto lock = std::lock_guard{ mutex };
			if( stopping || queue.size() >= 256 ) return false;
			queue.push_back( std::move( work ) );
		}
		condition.notify_one();
		return true;
	}

	/// @brief Registers a page to receive committed snapshots, including its first load.
	/// @param dispatcher Weak handle for the page's owning loop.
	/// @param lifetime Weak page lifetime used to prune disconnected observers.
	/// @param callback Callback with only weak page captures; invoked on that loop.
	/// @return Whether registration entered the worker queue.
	/// @pre callback never resolves another page's controls.
	/// @post Accepted registration queues an initial snapshot or reports the load failure.
	auto Subscribe(
		tatzel::Dispatcher dispatcher,
		std::weak_ptr<void> lifetime,
		std::function<void( std::shared_ptr<const Snapshot>, std::string )> callback
	) -> bool
	{
		return Submit( [ this, dispatcher, lifetime, callback = std::move( callback ) ]( Repository& repository ) {
			std::erase_if( subscribers, []( const auto& subscriber ) { return subscriber.lifetime.expired(); } );
			try
			{
				auto snapshot = std::make_shared<const Snapshot>( repository.Load() );
				if( lifetime.expired() ) return;
				subscribers.push_back( { dispatcher, lifetime, callback } );
				dispatcher.Post( [ callback, snapshot ] { callback( snapshot, {} ); } );
			}
			catch( const std::exception& error )
			{
				auto message = std::string{ error.what() };
				dispatcher.Post( [ callback, message ] { callback( {}, message ); } );
			}
		} );
	}

	/// @brief Offers the latest committed snapshot to every connected subscriber.
	/// @param snapshot Complete owned data shared as immutable across UI loops.
	/// @pre Called on the database worker after a successful commit.
	/// @post Closed subscribers are removed; active callbacks are queued in commit order.
	auto Publish( std::shared_ptr<const Snapshot> snapshot ) -> void
	{
		std::erase_if( subscribers, [ &snapshot ]( auto& subscriber ) {
			if( subscriber.lifetime.expired() ) return true;
			auto callback = subscriber.callback;
			return subscriber.dispatcher.Post( [ callback, snapshot ] { callback( snapshot, {} ); } ) == tatzel::DispatchResult::Closed;
		} );
	}

private:
	/// @brief Owns a weak UI submission handle and callback with weak page captures.
	struct Subscriber
	{
		tatzel::Dispatcher dispatcher;
		std::weak_ptr<void> lifetime;
		std::function<void( std::shared_ptr<const Snapshot>, std::string )> callback;
	};

	/// @brief Runs queued operations sequentially until a drained shutdown.
	/// @pre Invoked only by worker.
	/// @post Returns once stopping is set and the queue is empty.
	auto Run() -> void
	{
		for( ;; )
		{
			auto work = std::function<void( Repository& )>{};
			{
				auto lock = std::unique_lock{ mutex };
				condition.wait( lock, [ this ] { return stopping || !queue.empty(); } );
				if( queue.empty() ) return;
				work = std::move( queue.front() );
				queue.pop_front();
			}
			try { work( repository ); }
			catch( ... ) { tatzel::LogException( std::current_exception(), { "/", "workspace_worker" } ); }
		}
	}

	Repository repository;
	std::mutex mutex;
	std::condition_variable condition;
	std::deque<std::function<void( Repository& )>> queue;
	std::vector<Subscriber> subscribers;
	bool stopping = false;
	std::thread worker;
};

/// @brief Owns one browser's navigation and filters; logical controls own its callback lifetime.
/// @details All methods run on its UI loop. Repository jobs receive value copies,
/// and their completions resolve a weak page through its dispatcher.
class WorkspacePage : public std::enable_shared_from_this<WorkspacePage>
{
public:
	/// @brief Binds this browser to its borrowed builder and shared service.
	/// @param root Builder borrowed from the live logical root.
	/// @param workspace Shared background service retained by the application.
	/// @pre root belongs to the current owning loop.
	/// @post No controls or subscriptions have been created yet.
	WorkspacePage(
		Builder root,
		std::shared_ptr<Workspace> workspace
	) :
		root{ root },
		workspace{ std::move( workspace ) },
		dispatcher{ root.GetDispatcher() }
	{}

	/// @brief Creates the persistent shell, navigation, and initial data subscription.
	/// @pre This page has shared ownership and runs on its owning loop.
	/// @post The shell is visible and initial location/load callbacks can render its content.
	auto Build() -> void
	{
		auto self = shared_from_this();
		auto* shell = root.Container( [ self ]( auto shell ) {
			shell.Container( [ self ]( auto sidebar ) {
				sidebar.Text( "T" )->SetAppearance( "brand-mark" );
				sidebar.Heading( "Tatzel", tatzel::ui::HeadingStyle::H2 );
				sidebar.Paragraph( "A little room to make progress." )->SetAppearance( "sidebar-caption" );
				const std::vector<std::pair<std::string, std::string>> links{
					{ "Overview", "#/overview" }, { "Tasks", "#/tasks" }, { "Projects", "#/projects" }, { "Settings", "#/settings" }
				};
				for( const auto& [ label, route ] : links )
				{
					auto* link = sidebar.Link( label, route );
					link->SetAppearance( "nav-link" );
					self->navigation.push_back( { route, link->GetHandle() } );
				}
				sidebar.Paragraph( "WORKSPACE / SAMPLE 01" )->SetAppearance( "sidebar-footer" );
			} )->SetAppearance( "sidebar" );
			shell.Container( [ self ]( auto main ) {
				main.Container( []( auto top ) {
					top.Text( "YOUR WORK, AT A GLANCE" )->SetAppearance( "eyebrow" );
					top.Text( "Shared workspace" )->SetAppearance( "workspace-badge" );
				} )->SetAppearance( "topbar" );
				self->notice = main.Paragraph( "Loading your workspace…" )->GetHandle();
				self->notice.Get()->SetAppearance( "notice" );
				self->content = main.Container( []( auto ) {} )->GetHandle();
				self->content.Get()->SetAppearance( "page-content" );
			} )->SetAppearance( "main-panel" );
		} );
		shell->SetAppearance( "workspace-shell" );
		root.SetNavigationHandler( [ self ]( std::string_view hash ) { self->Navigate( hash ); } );
		Subscribe();
	}

private:
	/// @brief Starts or retries the initial background subscription.
	/// @pre Called on the owning loop with a built shell.
	/// @post A load is queued or a busy message is displayed.
	auto Subscribe() -> void
	{
		if( load_pending ) return;
		load_pending = true;
		auto weak = weak_from_this();
		if( !workspace->Subscribe( dispatcher, weak, [ weak ](
			std::shared_ptr<const Snapshot> snapshot,
			std::string error
		) {
			if( auto self = weak.lock() )
			{
				self->load_pending = false;
				if( !error.empty() ) { self->Notify( "Could not load the workspace: " + error, true ); return; }
				const auto first = !self->snapshot;
				self->snapshot = std::move( snapshot );
				if( first ) self->Notify( "Everything is up to date." );
				if( !self->editing ) self->Render();
				else self->Notify( "Workspace updated. Your open draft has been preserved." );
			}
		} ) )
		{
			load_pending = false;
			Notify( "The workspace is busy. Use Reload workspace to retry.", true );
		}
	}

	/// @brief Shows persistent operation feedback in the shell.
	/// @param message Plain-text feedback.
	/// @param error Whether the message describes a failure.
	/// @pre Called on the owning loop.
	/// @post The live notification text and appearance are synchronized.
	auto Notify(
		std::string_view message,
		bool error = false
	) -> void
	{
		if( auto* text = notice.Get() )
		{
			text->SetText( message );
			text->SetAppearance( error ? "notice error" : "notice" );
		}
	}

	/// @brief Enqueues a mutation and handles success or failure without discarding drafts.
	/// @param operation Repository operation with owned input captures.
	/// @param success User-visible completion message.
	/// @param destination Optional hash to open after success.
	/// @pre Called on the owning loop; no other mutation is pending on this page.
	/// @post Work is queued; successful commits publish snapshots to all pages.
	auto Mutate(
		std::function<void( Repository& )> operation,
		std::string success,
		std::string destination = {}
	) -> void
	{
		if( busy ) return;
		busy = true;
		if( auto* form = active_form.Get() ) form->SetDisabled( true );
		Notify( "Saving changes…" );
		const auto generation = view_generation;
		auto weak = weak_from_this();
		auto* service = workspace.get();
		auto accepted = workspace->Submit( [ operation = std::move( operation ), success = std::move( success ), destination = std::move( destination ), weak, service, generation, dispatcher = dispatcher ]( Repository& repository ) {
			try
			{
				auto boundary = tatzel::CallbackScope{ "Workspace operation" };
				operation( repository );
				auto snapshot = std::make_shared<const Snapshot>( repository.Load() );
				service->Publish( snapshot );
				dispatcher.Post( [ weak, snapshot, success, destination, generation ] {
					if( auto self = weak.lock() )
					{
						self->snapshot = snapshot;
						self->busy = false;
						if( auto* form = self->active_form.Get() ) form->SetDisabled( false );
						if( auto* modal = self->confirmation.Get() ) modal->Remove();
						self->Notify( success );
						if( !destination.empty() && self->view_generation == generation ) self->root.Navigate( destination );
						else if( !self->editing ) self->Render();
					}
				} );
			}
			catch( const std::exception& error )
			{
				tatzel::LogException( std::current_exception(), { "/", "workspace_save" } );
				auto message = std::string{ error.what() };
				dispatcher.Post( [ weak, message ] {
					if( auto self = weak.lock() )
					{
						self->busy = false;
						if( auto* form = self->active_form.Get() ) form->SetDisabled( false );
						if( auto* modal = self->confirmation.Get() ) modal->Close();
						self->Notify( message, true );
					}
				} );
			}
		} );
		if( !accepted )
		{
			busy = false;
			if( auto* form = active_form.Get() ) form->SetDisabled( false );
			Notify( "The workspace is busy. Your changes are still here; try saving again.", true );
		}
	}

	/// @brief Stores a hash route and renders its view without rebuilding the shell.
	/// @param hash Browser-provided same-page location.
	/// @pre Called on the owning loop.
	/// @post A recognized view or a useful not-found state is rendered.
	auto Navigate( std::string_view hash ) -> void
	{
		location = hash.empty() ? "#/overview" : std::string{ hash };
		++view_generation;
		if( location.starts_with( "#/tasks/project/" ) )
		{
			project_filter = location.substr( 16 );
			status_filter = "all";
			show_archived = true;
		}
		if( location.starts_with( "#/tasks/status/" ) )
		{
			status_filter = location.substr( 15 );
			if( status_filter != "todo" && status_filter != "doing" && status_filter != "done" ) status_filter = "all";
			project_filter = "all";
		}
		editing = false;
		if( auto* modal = confirmation.Get() ) modal->Remove();
		Render();
	}

	/// @brief Replaces only the central view and updates navigation appearance.
	/// @pre Called on the owning loop.
	/// @post Previous view handles are invalid; a view matching location is built.
	auto Render() -> void
	{
		auto* parent = content.Get();
		if( !parent ) return;
		parent->ClearChildren();
		active_form = {};
		auto ui = root.Scope( *parent );
		for( auto& [ route, handle ] : navigation )
			if( auto* link = handle.Get() ) link->SetAppearance( location.starts_with( route ) ? "nav-link active" : "nav-link" );
		if( !snapshot )
		{
			ui.Heading( "Your workspace is on its way." );
			ui.Button( "Reload workspace", [ self = shared_from_this() ] { self->Subscribe(); } );
			return;
		}
		parent->SetAppearance( snapshot->compact ? "page-content compact" : "page-content" );
		if( location == "#/overview" ) RenderOverview( ui );
		else if( location == "#/tasks" || location.starts_with( "#/tasks/project/" ) || location.starts_with( "#/tasks/status/" ) ) RenderTasks( ui );
		else if( location.starts_with( "#/task/" ) ) RenderTaskEditor( ui );
		else if( location == "#/projects" ) RenderProjects( ui );
		else if( location.starts_with( "#/project/" ) ) RenderProjectEditor( ui );
		else if( location == "#/settings" ) RenderSettings( ui );
		else
		{
			ui.Heading( "This view could not be found." );
			ui.Paragraph( "The link may be incomplete, or the item may have been removed." );
			ui.Link( "Back to overview", "#/overview" );
		}
	}

	/// @brief Builds a consistent page heading and supporting text.
	/// @param ui Current central-content builder.
	/// @param title Page heading.
	/// @param subtitle Supporting text.
	/// @pre Called during view construction.
	/// @post Logical heading and paragraph controls are created.
	auto Header(
		ContentBuilder ui,
		std::string_view title,
		std::string_view subtitle
	) -> void
	{
		ui.Heading( title );
		ui.Paragraph( subtitle )->SetAppearance( "subtitle" );
	}

	/// @brief Resolves the name of a task's owning project.
	/// @param id Persistent project ID.
	/// @return Owned name, with a fallback for stale data.
	/// @pre snapshot exists.
	/// @post State is unchanged.
	auto ProjectName( std::int64_t id ) const -> std::string
	{
		for( const auto& project : snapshot->projects ) if( project.id == id ) return project.name;
		return "Unknown project";
	}

	/// @brief Reports whether a task belongs to an active project.
	/// @param task Task record from the current snapshot.
	/// @return Whether the project exists and is active.
	/// @pre snapshot exists.
	/// @post State is unchanged.
	auto ActiveProject( const Task& task ) const -> bool
	{
		return std::ranges::any_of( snapshot->projects, [ &task ]( const auto& project ) { return project.id == task.project_id && !project.archived; } );
	}

	/// @brief Renders counts, project summaries, and committed activity.
	/// @param ui Current central-content builder.
	/// @pre snapshot exists and this page is on its owning loop.
	/// @post Overview controls and navigation links are built.
	auto RenderOverview( ContentBuilder ui ) -> void
	{
		Header( ui, "Make room for good work.", "A clear view of what matters, and a little momentum for what comes next." );
		ui.Link( "+ Create a task", "#/task/new" )->SetAppearance( "primary-link" );
		ui.Container( [ this ]( auto metrics ) {
			for( const auto& status : { "todo", "doing", "done" } )
			{
				auto count = std::ranges::count_if( snapshot->tasks, [ this, status ]( const auto& task ) { return !task.archived && ActiveProject( task ) && task.status == status; } );
				metrics.Container( [ count, status ]( auto metric ) {
					metric.Text( std::to_string( count ) )->SetAppearance( "metric-number" );
					metric.Link( StatusLabel( status ), std::string{ "#/tasks/status/" } + status );
				} )->SetAppearance( std::string{ "metric " } + status );
			}
		} )->SetAppearance( "metrics" );
		ui.Heading( "Your projects", tatzel::ui::HeadingStyle::H2 );
		ui.Container( [ this ]( auto projects ) {
			for( const auto& project : snapshot->projects )
			{
				if( project.archived ) continue;
				auto total = std::ranges::count_if( snapshot->tasks, [ &project ]( const auto& task ) { return task.project_id == project.id && !task.archived; } );
				auto done = std::ranges::count_if( snapshot->tasks, [ &project ]( const auto& task ) { return task.project_id == project.id && !task.archived && task.status == "done"; } );
				projects.Card( project.name, [ &project, total, done ]( auto card ) {
					card.Paragraph( project.description );
					card.Text( std::to_string( done ) + " of " + std::to_string( total ) + " tasks complete" )->SetAppearance( "muted" );
					card.Link( "View tasks →", "#/tasks/project/" + std::to_string( project.id ) );
				} )->SetAppearance( "project-card" );
			}
		} )->SetAppearance( "project-grid" );
		ui.Heading( "Recent activity", tatzel::ui::HeadingStyle::H2 );
		ui.Container( [ this ]( auto activity ) {
			for( std::size_t index = 0; index < std::min<std::size_t>( 6, snapshot->activity.size() ); ++index )
				activity.Paragraph( snapshot->activity[ index ] )->SetAppearance( "activity-item" );
		} )->SetAppearance( "activity-list" );
	}

	/// @brief Renders filters once and a separately replaceable task-results subtree.
	/// @param ui Current central-content builder.
	/// @pre snapshot exists.
	/// @post Search typing retains its editor and caret while result rows update.
	auto RenderTasks( ContentBuilder ui ) -> void
	{
		Header( ui, "Tasks", "Find your focus. Move one useful thing forward." );
		ui.Link( "+ Create a task", "#/task/new" )->SetAppearance( "primary-link" );
		auto self = shared_from_this();
		ui.Container( [ this, self ]( auto filters ) {
			auto* search = filters.TextInput( "Search tasks", query );
			search->SetPlaceholder( "Search titles and descriptions…" );
			search->SetOnInput( [ self ]( std::string_view value ) { self->query = value; self->RenderTaskRows(); } );
			filters.Select( "Status", { { "all", "All statuses" }, { "todo", "To do" }, { "doing", "In progress" }, { "done", "Done" } }, status_filter,
				[ self ]( std::string_view value ) { self->status_filter = value; self->RenderTaskRows(); } );
			auto projects = std::vector<tatzel::ui::SelectOption>{ { "all", "All projects" } };
			for( const auto& project : snapshot->projects ) projects.push_back( { std::to_string( project.id ), project.name + ( project.archived ? " (archived)" : "" ) } );
			if( std::ranges::none_of( projects, [ this ]( const auto& option ) { return option.id == project_filter; } ) ) project_filter = "all";
			filters.Select( "Project", std::move( projects ), project_filter,
				[ self ]( std::string_view value ) { self->project_filter = value; self->RenderTaskRows(); } );
			filters.Select( "Sort", { { "newest", "Newest first" }, { "title", "Title A–Z" }, { "priority", "Priority" } }, sort,
				[ self ]( std::string_view value ) { self->sort = value; self->RenderTaskRows(); } );
		} )->SetAppearance( "filters" );
		ui.Container( [ this, self ]( auto controls ) {
			controls.Checkbox( "Show archived tasks and projects", show_archived,
				[ self ]( bool value ) { self->show_archived = value; self->RenderTaskRows(); } );
			controls.Button( "Clear filters", [ self ] {
				self->query.clear();
				self->status_filter = "all";
				self->project_filter = "all";
				self->show_archived = false;
				self->root.Navigate( "#/tasks" );
			} )->SetAppearance( "quiet" );
		} )->SetAppearance( "list-controls" );
		results = ui.Container( []( auto ) {} )->GetHandle();
		RenderTaskRows();
	}

	/// @brief Updates only task results using local filters and the current snapshot.
	/// @pre Called on the owning loop while the task-results container is live.
	/// @post Result rows reflect search, archive policy, project, status, and sorting.
	auto RenderTaskRows() -> void
	{
		auto* parent = results.Get();
		if( !parent ) return;
		parent->ClearChildren();
		auto ui = root.Scope( *parent );
		auto tasks = std::vector<Task>{};
		auto search = SearchKey( query );
		for( const auto& task : snapshot->tasks )
		{
			if( !show_archived && ( task.archived || !ActiveProject( task ) ) ) continue;
			if( status_filter != "all" && task.status != status_filter ) continue;
			if( !snapshot->show_completed && status_filter == "all" && task.status == "done" ) continue;
			if( project_filter != "all" && std::to_string( task.project_id ) != project_filter ) continue;
			if( !search.empty() && SearchKey( task.title + " " + task.description ).find( search ) == std::string::npos ) continue;
			tasks.push_back( task );
		}
		if( sort == "title" ) std::ranges::stable_sort( tasks, {}, []( const auto& task ) { return SearchKey( task.title ); } );
		if( sort == "priority" ) std::ranges::stable_sort( tasks, {}, []( const auto& task ) { return task.priority == "high" ? 0 : task.priority == "normal" ? 1 : 2; } );
		ui.Paragraph( std::to_string( tasks.size() ) + ( tasks.size() == 1 ? " task" : " tasks" ) )->SetAppearance( "result-count" );
		if( tasks.empty() )
		{
			ui.Card( "A little breathing room.", []( auto card ) { card.Paragraph( "No tasks match this view. Adjust the filters or create something new." ); } );
			return;
		}
		for( const auto& task : tasks )
		{
			ui.Container( [ this, &task ]( auto row ) {
				row.Text( StatusLabel( task.status ) )->SetAppearance( "status " + task.status );
				row.Container( [ this, &task ]( auto details ) {
					details.Link( task.title, "#/task/" + std::to_string( task.id ) )->SetAppearance( "task-title" );
					details.Text( ProjectName( task.project_id ) + ( task.archived ? " · Archived" : "" ) )->SetAppearance( "muted" );
				} )->SetAppearance( "task-summary" );
				row.Text( task.priority == "high" ? "High priority" : task.priority == "low" ? "Low priority" : "Normal priority" )->SetAppearance( "priority " + task.priority );
				row.Link( "Open →", "#/task/" + std::to_string( task.id ) )->SetAppearance( "row-action" );
			} )->SetAppearance( "task-row" );
		}
	}

	/// @brief Builds an editable task form with owned optimistic-version captures.
	/// @param ui Current central-content builder.
	/// @pre snapshot exists.
	/// @post The form's draft survives unrelated snapshot updates and save failures.
	auto RenderTaskEditor( ContentBuilder ui ) -> void
	{
		const auto suffix = location.substr( 7 );
		auto task = Task{};
		if( suffix != "new" )
		{
			auto found = std::ranges::find( snapshot->tasks, ParseID( suffix ), &Task::id );
			if( found == snapshot->tasks.end() ) { Header( ui, "Task not found", "It may have been removed in another window." ); ui.Link( "Back to tasks", "#/tasks" ); return; }
			task = *found;
		}
		auto options = std::vector<tatzel::ui::SelectOption>{};
		for( const auto& project : snapshot->projects ) if( !project.archived ) options.push_back( { std::to_string( project.id ), project.name } );
		if( options.empty() || ( task.id && !ActiveProject( task ) ) )
		{
			Header( ui, "An active project is needed", "Create or restore a project before editing its tasks." );
			ui.Link( "Go to projects", "#/projects" );
			return;
		}
		if( task.project_id == 0 ) task.project_id = ParseID( options.front().id );
		Header( ui, task.id ? "Task details" : "Create a task", "A clear next step starts with a little context." );
		ui.Link( "← All tasks", "#/tasks" );
		auto self = shared_from_this();
		auto* form = ui.Form( "Task editor", [ self, task, options = std::move( options ) ](
			auto fields,
			tatzel::ui::Form* form
		) {
			auto* title = fields.TextInput( "Task title", task.title );
			title->SetRequired( true );
			auto* description = fields.TextArea( "Description", task.description );
			auto* project = fields.Select( "Project", options, std::to_string( task.project_id ) );
			project->SetRequired( true );
			auto* status = fields.Select( "Status", { { "todo", "To do" }, { "doing", "In progress" }, { "done", "Done" } }, task.status );
			auto* priority = fields.Select( "Priority", { { "low", "Low" }, { "normal", "Normal" }, { "high", "High" } }, task.priority );
			fields.SubmitButton( "Save task" );
			form->SetOnSubmit( [ self, task, title = title->GetHandle(), description = description->GetHandle(), project = project->GetHandle(), status = status->GetHandle(), priority = priority->GetHandle() ]() mutable {
				if( !title.Get() || !description.Get() || !project.Get() || !status.Get() || !priority.Get() ) return;
				task.title = title.Get()->GetValue();
				task.description = description.Get()->GetValue();
				task.project_id = ParseID( project.Get()->GetValue() );
				task.status = status.Get()->GetValue();
				task.priority = priority.Get()->GetValue();
				self->Mutate( [ task ]( Repository& repository ) { repository.SaveTask( task ); }, "Task saved. Everything is up to date.", "#/tasks" );
			} );
		} );
		form->SetAppearance( "editor-form" );
		active_form = form->GetHandle();
		editing = true;
		if( task.id )
		{
			ui.Container( [ self, task ]( auto actions ) {
				actions.Button( task.archived ? "Restore task" : "Archive task", [ self, task ]() mutable {
					auto updated = Task{ task };
					updated.archived = !updated.archived;
					self->Mutate( [ updated ]( Repository& repository ) { repository.SaveTask( updated ); }, "Task archive state updated.", "#/tasks" );
				} )->SetAppearance( "quiet" );
				actions.Button( "Delete task", [ self, task ] {
					self->Confirm( "Delete this task?", "“" + task.title + "” will be permanently removed.", [ self, task ] {
						self->Mutate( [ task ]( Repository& repository ) { repository.DeleteTask( task ); }, "Task deleted.", "#/tasks" );
					} );
				} )->SetAppearance( "danger" );
			} )->SetAppearance( "editor-actions" );
		}
	}

	/// @brief Builds project cards with task links and archive/restore actions.
	/// @param ui Current central-content builder.
	/// @pre snapshot exists.
	/// @post All projects are discoverable, including archived projects.
	auto RenderProjects( ContentBuilder ui ) -> void
	{
		Header( ui, "Projects", "Give related work a place to belong." );
		ui.Link( "+ Create a project", "#/project/new" )->SetAppearance( "primary-link" );
		auto self = shared_from_this();
		ui.Container( [ this, self ]( auto projects ) {
			for( const auto& project : snapshot->projects )
			{
				projects.Card( project.name + ( project.archived ? " · Archived" : "" ), [ &project ]( auto card ) {
					card.Paragraph( project.description );
					card.Link( "View tasks →", "#/tasks/project/" + std::to_string( project.id ) );
					card.Link( "Edit project", "#/project/" + std::to_string( project.id ) );
				}, [ self, project ]( auto actions ) {
					actions.Button( project.archived ? "Restore project" : "Archive project", [ self, project ]() mutable {
						auto updated = Project{ project };
						updated.archived = !updated.archived;
						self->Mutate( [ updated ]( Repository& repository ) { repository.SaveProject( updated ); }, "Project archive state updated." );
					} )->SetAppearance( "quiet" );
				} )->SetAppearance( "project-card" );
			}
		} )->SetAppearance( "project-grid" );
	}

	/// @brief Builds a project creation or rename/description form.
	/// @param ui Current central-content builder.
	/// @pre snapshot exists.
	/// @post The form saves with optimistic concurrency and preserves failed drafts.
	auto RenderProjectEditor( ContentBuilder ui ) -> void
	{
		auto project = Project{};
		auto suffix = location.substr( 10 );
		if( suffix != "new" )
		{
			auto found = std::ranges::find( snapshot->projects, ParseID( suffix ), &Project::id );
			if( found == snapshot->projects.end() ) { Header( ui, "Project not found", "It may have been removed by a workspace reset." ); ui.Link( "Back to projects", "#/projects" ); return; }
			project = *found;
		}
		Header( ui, project.id ? "Edit project" : "Create a project", "A name, a purpose, and room to make progress." );
		ui.Link( "← All projects", "#/projects" );
		auto self = shared_from_this();
		auto* form = ui.Form( "Project editor", [ self, project ](
			auto fields,
			tatzel::ui::Form* form
		) {
			auto* name = fields.TextInput( "Project name", project.name );
			name->SetRequired( true );
			auto* description = fields.TextArea( "Description", project.description );
			fields.SubmitButton( "Save project" );
			form->SetOnSubmit( [ self, project, name = name->GetHandle(), description = description->GetHandle() ]() mutable {
				if( !name.Get() || !description.Get() ) return;
				project.name = name.Get()->GetValue();
				project.description = description.Get()->GetValue();
				self->Mutate( [ project ]( Repository& repository ) { repository.SaveProject( project ); }, "Project saved.", "#/projects" );
			} );
		} );
		form->SetAppearance( "editor-form" );
		active_form = form->GetHandle();
		editing = true;
	}

	/// @brief Builds persistent display preferences and confirmed dataset reset.
	/// @param ui Current central-content builder.
	/// @pre snapshot exists.
	/// @post Settings changes run through the worker and synchronize connected pages.
	auto RenderSettings( ContentBuilder ui ) -> void
	{
		Header( ui, "Settings", "Make this workspace feel right for the way you work." );
		auto self = shared_from_this();
		auto* form = ui.Form( "Workspace preferences", [ self ](
			auto fields,
			tatzel::ui::Form* form
		) {
			fields.Heading( "Display preferences", tatzel::ui::HeadingStyle::H2 );
			auto* compact = fields.Checkbox( "Use compact task rows", self->snapshot->compact );
			auto* completed = fields.Checkbox( "Show completed tasks by default", self->snapshot->show_completed );
			fields.SubmitButton( "Save preferences" );
			form->SetOnSubmit( [ self, compact = compact->GetHandle(), completed = completed->GetHandle() ] {
				if( !compact.Get() || !completed.Get() ) return;
				self->Mutate( [ compact = compact.Get()->IsChecked(), completed = completed.Get()->IsChecked() ]( Repository& repository ) {
					repository.SavePreferences( compact, completed );
				}, "Preferences saved.", "#/settings" );
			} );
		} );
		form->SetAppearance( "editor-form" );
		active_form = form->GetHandle();
		editing = true;
		ui.Card( "A fresh start", [ self ]( auto card ) {
			card.Paragraph( "Restore the original sample projects, tasks, and preferences. This replaces the current workspace for every connected window." );
			card.Button( "Restore sample data", [ self ] {
				self->Confirm( "Restore sample data?", "All current projects and tasks will be replaced. This cannot be undone.", [ self ] {
					self->Mutate( []( Repository& repository ) { repository.Reset(); }, "Sample workspace restored.", "#/overview" );
				} );
			} )->SetAppearance( "danger" );
		} )->SetAppearance( "reset-card" );
	}

	/// @brief Opens a native modal and runs the supplied action only after confirmation.
	/// @param title Accessible dialog title.
	/// @param message Explanation of the pending operation.
	/// @param action Owned application callback.
	/// @pre Called on the owning loop; captures outlive their invocation.
	/// @post A modal with cancel and confirmation controls is open.
	auto Confirm(
		std::string_view title,
		std::string_view message,
		std::function<void()> action
	) -> void
	{
		if( busy ) return;
		if( auto* modal = confirmation.Get() ) modal->Remove();
		auto self = shared_from_this();
		auto* modal = root.Modal( title, [ self, message = std::string{ message }, action = std::move( action ) ]( auto content ) {
			content.Paragraph( message );
			content.Button( "Cancel", [ self ] { if( auto* dialog = self->confirmation.Get() ) dialog->Close(); } )->SetAppearance( "quiet" );
			content.Button( "Confirm", [ self, action ] { if( !self->busy ) action(); } )->SetAppearance( "danger" );
		} );
		confirmation = modal->GetHandle();
		modal->Open();
	}

	Builder root;
	std::shared_ptr<Workspace> workspace;
	tatzel::Dispatcher dispatcher;
	std::shared_ptr<const Snapshot> snapshot;
	tatzel::ui::ElementHandle<tatzel::ui::Container> content;
	tatzel::ui::ElementHandle<tatzel::ui::Container> results;
	tatzel::ui::ElementHandle<tatzel::ui::Paragraph> notice;
	tatzel::ui::ElementHandle<tatzel::ui::Form> active_form;
	tatzel::ui::ElementHandle<tatzel::ui::Modal> confirmation;
	std::vector<std::pair<std::string, tatzel::ui::ElementHandle<tatzel::ui::Link>>> navigation;
	std::string location = "#/overview";
	std::string query;
	std::string project_filter = "all";
	std::string status_filter = "all";
	std::string sort = "newest";
	std::size_t view_generation = 0;
	bool show_archived = false;
	bool editing = false;
	bool busy = false;
	bool load_pending = false;
};

}

/// @brief Runs the persistent Tatzel sample application.
/// @return Zero after normal shutdown, one after a reported startup failure.
/// @pre Run from the repository root with compiled browser assets and writable data storage.
/// @post The server has stopped and the database worker has drained its accepted queue.
auto main() -> int
try
{
	auto path = std::getenv( "TATZEL_WORKSPACE_DB" );
	auto workspace = std::make_shared<sample::Workspace>( path ? path : "data/workspace.sqlite3" );
	auto app = tatzel::App{ "./data/drogon_config.json", "./data/workspace.html" };
	app.RegisterPage( "/", [ workspace ]( tatzel::PageBuilder<> ui ) {
		auto page = std::make_shared<sample::WorkspacePage>( ui, workspace );
		page->Build();
	} );
	app.Run();
	return 0;
}
catch( ... )
{
	tatzel::LogException( std::current_exception(), { "/", "workspace_startup" } );
	return 1;
}
