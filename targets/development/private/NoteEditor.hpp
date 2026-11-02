#pragma once

#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

import UI;

namespace example {

/// @brief Owned application record edited independently by each component instance.
struct NoteDraft
{
	std::string record; ///< Application identity of the loaded record.
	std::string title; ///< Required note title.
	std::string notes; ///< Multiline note body.
};

/// @brief Owned identity of one asynchronous editor save.
struct NoteSaveToken
{
	std::string component; ///< Logical component identity.
	std::uint64_t generation; ///< Load/save generation on that component.
};

/// @brief Demonstrates an application-owned reusable editor with guarded completion.
/// @details Each instance owns its draft and request generation. Workers receive
/// copied drafts/tokens and resolve checked handles only in dispatcher callbacks.
class NoteEditor : public tatzel::ui::Component
{
public:
	/// @brief Stores an independent initial record and accessible form title.
	/// @param id Unique logical identity.
	/// @param parent Owning logical parent.
	/// @param label Accessible editor title.
	/// @param draft Initial owned application record.
	/// @pre parent is nonnull.
	/// @post Configuration is owned and no save is pending.
	NoteEditor(
		std::string_view id,
		tatzel::ui::LogicalElement* parent,
		std::string label,
		NoteDraft draft
	) :
		Component{ id, parent },
		label{ std::move( label ) },
		draft{ std::move( draft ) }
	{
		SetAppearance( "tatzel-note-editor" );
	}

	/// @brief Builds a complete form using ordinary logical controls.
	/// @tparam BuilderT Factory-supplied scoped builder type.
	/// @param ui Scoped component builder.
	/// @pre Called once by PageBuilder::Component after attachment.
	/// @post Fields, feedback, and actions are owned by this subtree.
	template<typename BuilderT>
	auto Build( BuilderT ui ) -> void
	{
		auto self = GetHandle();
		ui.Heading( label, tatzel::ui::HeadingStyle::H2 );
		auto* form = ui.Form( label, [ this, self ]( auto fields ) {
			auto* title = fields.TextInput( "Title", draft.title );
			title->SetRequired( true );
			title_field = title->GetHandle();
			auto* notes = fields.TextArea( "Notes", draft.notes );
			notes->SetRows( 3 );
			notes_field = notes->GetHandle();
			auto* message = fields.Paragraph( "Ready" );
			message->SetLiveRegion( tatzel::ui::LiveRegion::Polite );
			message->SetAppearance( "tatzel-editor-feedback" );
			feedback = message->GetHandle();
			save_button = fields.SubmitButton( "Save note" )->GetHandle();
		} );
		form_handle = form->GetHandle();
		form->SetOnSubmit( [ self ] { if( auto* owner = self.Get() ) owner->Save(); } );
	}

	/// @brief Replaces the editor save-intent callback.
	/// @param on_save Callback receiving an owned record and completion token.
	/// @pre Called on the owning loop; captures outlive invocation.
	/// @post Future valid submissions use this callback.
	auto SetOnSave( std::function<void( NoteDraft, NoteSaveToken )> on_save ) -> void { this->on_save = std::move( on_save ); }

	/// @brief Loads another record and supersedes all earlier saves and drafts.
	/// @param draft New owned application record.
	/// @pre Called on the owning loop after Build.
	/// @post Pending completions become obsolete and fields receive authoritative replacement generations.
	auto Load( NoteDraft draft ) -> void
	{
		AdvanceGeneration();
		this->draft = std::move( draft );
		if( auto* title = title_field.Get() ) title->SetValue( this->draft.title, tatzel::ui::ValueUpdatePolicy::Replace );
		if( auto* notes = notes_field.Get() ) notes->SetValue( this->draft.notes, tatzel::ui::ValueUpdatePolicy::Replace );
		SetBusy( false );
		ShowFeedback( "Loaded " + this->draft.record, false );
	}

	/// @brief Applies an asynchronous save result only to its original active request.
	/// @param token Owned token captured by the save callback.
	/// @param error Empty for success, otherwise a user-visible failure description.
	/// @return Whether the completion still belongs to this component's current save.
	/// @pre Called on the owning loop through a checked component handle.
	/// @post Matching results clear busy state; failure preserves the draft; stale results change nothing.
	auto CompleteSave(
		const NoteSaveToken& token,
		std::string_view error = {}
	) -> bool
	{
		if( IsRemoved() || !busy || token.component != GetID() || token.generation != generation ) return false;
		SetBusy( false );
		ShowFeedback( error.empty() ? "Saved " + draft.record : std::string{ error }, !error.empty() );
		return true;
	}

	/// @brief Changes interaction state without rebuilding fields or clearing drafts.
	/// @param busy Whether a save is in progress.
	/// @pre Called on the owning loop after Build.
	/// @post The form and submit label reflect busy state.
	auto SetBusy( bool busy ) -> void
	{
		this->busy = busy;
		if( auto* form = form_handle.Get() ) form->SetDisabled( busy );
		if( auto* button = save_button.Get() ) button->SetText( busy ? "Saving…" : "Save note" );
	}

private:
	/// @brief Issues a fresh private request identity before a load or save.
	/// @pre Called on the owning loop.
	/// @post Previous request tokens no longer match.
	/// @throws tatzel::Exception If the private generation counter is exhausted.
	auto AdvanceGeneration() -> void
	{
		if( generation == std::numeric_limits<std::uint64_t>::max() ) throw tatzel::Exception{ "Editor request generation exhausted" };
		++generation;
	}

	/// @brief Publishes accessible feedback without changing field values.
	/// @param message Owned or transient plain text.
	/// @param error Whether this message describes a failed save.
	/// @pre Called on the owning loop.
	/// @post The live feedback region contains the message and semantic appearance.
	auto ShowFeedback(
		std::string_view message,
		bool error
	) -> void
	{
		if( auto* text = feedback.Get() )
		{
			text->SetText( message );
			text->SetAppearance( error ? "tatzel-editor-feedback error" : "tatzel-editor-feedback" );
		}
	}

	/// @brief Captures a valid draft and asks application code to persist it.
	/// @pre Called by the validated form on its owning loop.
	/// @post The callback owns its inputs; start failures restore interaction and propagate through the page boundary.
	auto Save() -> void
	{
		auto dispatch = BeginDispatch();
		if( IsRemoved() || busy || !title_field.Get() || !notes_field.Get() ) return;
		auto callback = on_save;
		if( !callback ) return;
		AdvanceGeneration();
		draft.title = title_field.Get()->GetValue();
		draft.notes = notes_field.Get()->GetValue();
		const auto request_generation = generation;
		auto token = NoteSaveToken{ std::string{ GetID() }, request_generation };
		SetBusy( true );
		ShowFeedback( "Saving " + draft.record + "…", false );
		try
		{
			auto boundary = tatzel::CallbackScope{ "Editor save" };
			callback( draft, std::move( token ) );
		}
		catch( ... )
		{
			if( !IsRemoved() && generation == request_generation && busy )
			{
				SetBusy( false );
				ShowFeedback( "The save could not be started. Try again.", true );
			}
			throw;
		}
	}

	std::string label;
	NoteDraft draft;
	std::uint64_t generation = 0;
	bool busy = false;
	tatzel::ui::ElementHandle<tatzel::ui::Form> form_handle;
	tatzel::ui::ElementHandle<tatzel::ui::TextInput> title_field;
	tatzel::ui::ElementHandle<tatzel::ui::TextArea> notes_field;
	tatzel::ui::ElementHandle<tatzel::ui::Paragraph> feedback;
	tatzel::ui::ElementHandle<tatzel::ui::Button> save_button;
	std::function<void( NoteDraft, NoteSaveToken )> on_save;
};

}
