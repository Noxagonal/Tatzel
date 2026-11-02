#pragma once

#include "Repository.hpp"

#include <charconv>
#include <functional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

import UI;

namespace sample {

/// @brief Application component owning one optimistic-version task draft and form.
/// @details Valid submission delivers an owned Task to the application service.
/// The page controls persistence/navigation; children remain private to the editor.
class TaskEditor : public tatzel::ui::Component
{
public:
	/// @brief Stores a task draft and available projects before component attachment.
	/// @param id Unique logical identity.
	/// @param parent Owning logical parent.
	/// @param task Owned task including optimistic version.
	/// @param projects Owned available project choices.
	/// @pre parent is nonnull and task.project_id names an available project.
	/// @post The editor owns independent record and configuration values.
	TaskEditor(
		std::string_view id,
		tatzel::ui::LogicalElement* parent,
		Task task,
		std::vector<tatzel::ui::SelectOption> projects
	) :
		Component{ id, parent, "task-editor" },
		task{ std::move( task ) },
		projects{ std::move( projects ) }
	{}

	/// @brief Builds the existing task editor from ordinary logical fields.
	/// @tparam BuilderT Factory-supplied scoped builder type.
	/// @param ui Scoped builder supplied by the component factory.
	/// @pre Called once after attachment.
	/// @post Fields and callbacks belong to this component's subtree.
	template<typename BuilderT>
	auto Build( BuilderT ui ) -> void
	{
		auto self = GetHandle();
		auto* form = ui.Form( "Task editor", [ this ]( auto fields ) {
			auto* title = fields.TextInput( "Task title", task.title );
			title->SetRequired( true );
			title_field = title->GetHandle();
			description = fields.TextArea( "Description", task.description )->GetHandle();
			auto* project = fields.Select( "Project", projects, std::to_string( task.project_id ) );
			project->SetRequired( true );
			project_field = project->GetHandle();
			status = fields.Select( "Status", { { "todo", "To do" }, { "doing", "In progress" }, { "done", "Done" } }, task.status )->GetHandle();
			priority = fields.Select( "Priority", { { "low", "Low" }, { "normal", "Normal" }, { "high", "High" } }, task.priority )->GetHandle();
			fields.SubmitButton( "Save task" );
		} );
		form->SetAppearance( "editor-form" );
		form_handle = form->GetHandle();
		form->SetOnSubmit( [ self ] { if( auto* owner = self.Get() ) owner->Submit(); } );
	}

	/// @brief Sets the application's validated save-intent callback.
	/// @param on_save Callback receiving a complete owned task snapshot.
	/// @pre Called on the owning loop; captures outlive invocation.
	/// @post Future valid submissions use this callback.
	auto SetOnSave( std::function<void( Task )> on_save ) -> void { this->on_save = std::move( on_save ); }

	/// @brief Obtains the editor's coordinated form for page-level busy management.
	/// @return Non-owning checked form handle retaining neither component nor page.
	/// @pre Called on the owning loop after Build.
	/// @post State is unchanged; removal invalidates the returned handle.
	auto GetFormHandle() const -> tatzel::ui::ElementHandle<tatzel::ui::Form> { return form_handle; }

private:
	/// @brief Collects a valid application draft without exposing field lifetimes.
	/// @pre Called by the validated form on the owning loop.
	/// @post The application callback receives owned values and may remove this component.
	auto Submit() -> void
	{
		auto dispatch = BeginDispatch();
		if( IsRemoved() || !title_field.Get() || !description.Get() || !project_field.Get() || !status.Get() || !priority.Get() ) return;
		auto draft = task;
		draft.title = title_field.Get()->GetValue();
		draft.description = description.Get()->GetValue();
		auto value = project_field.Get()->GetValue();
		auto parsed = std::from_chars( value.data(), value.data() + value.size(), draft.project_id );
		if( parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() ) throw tatzel::Exception{ "Task project identity is invalid" };
		draft.status = status.Get()->GetValue();
		draft.priority = priority.Get()->GetValue();
		auto callback = on_save;
		if( callback )
		{
			auto boundary = tatzel::CallbackScope{ "Task editor save" };
			callback( std::move( draft ) );
		}
	}

	Task task;
	std::vector<tatzel::ui::SelectOption> projects;
	tatzel::ui::ElementHandle<tatzel::ui::Form> form_handle;
	tatzel::ui::ElementHandle<tatzel::ui::TextInput> title_field;
	tatzel::ui::ElementHandle<tatzel::ui::TextArea> description;
	tatzel::ui::ElementHandle<tatzel::ui::Select> project_field;
	tatzel::ui::ElementHandle<tatzel::ui::Select> status;
	tatzel::ui::ElementHandle<tatzel::ui::Select> priority;
	std::function<void( Task )> on_save;
};

}
