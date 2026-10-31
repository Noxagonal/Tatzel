/// @file
/// @brief Logical multiline text editor sharing text-field state and validation.
module;

#include <cstddef>
#include <functional>
#include <stdexcept>
#include <string_view>
#include <utility>

export module UI.Element.TextArea;

export import UI.Element.TextInput;

namespace tatzel::ui {

/// @brief Page-owned multiline editor with the TextInput value and validation API.
/// @details Enter inserts a newline; Ctrl/Cmd+Enter submits its form or invokes
/// the standalone submit callback. Mutations run on the owning event loop.
export class TextArea : public TextInput
{
public:
	/// @brief Creates a multiline logical editor.
	/// @param id Unique logical ID.
	/// @param parent Owning logical parent.
	/// @param label Accessible label, copied before returning.
	/// @param value Initial text, copied before returning.
	/// @param on_change Optional committed-change callback.
	/// @pre parent is nonnull; callback captures outlive their invocation.
	/// @post The editor owns its text and has four visible rows by default.
	TextArea(
		std::string_view id,
		LogicalElement* parent,
		std::string_view label,
		std::string_view value,
		std::function<void( std::string_view )> on_change
	) :
		TextInput{ id, parent, label, value, std::move( on_change ) }
	{}

	/// @brief Reads the desired number of visible text rows.
	/// @return The positive row count.
	/// @pre None.
	/// @post State is unchanged.
	auto GetRows() const noexcept -> size_t { return rows; }

	/// @brief Changes the editor's visible row count.
	/// @param rows Positive number of visible text rows.
	/// @pre Called on the owning event loop.
	/// @post The row count is stored and a bound adapter queues its presentation update.
	/// @throws std::invalid_argument If rows is zero.
	auto SetRows( size_t rows ) -> void
	{
		if( rows == 0 ) throw std::invalid_argument{ "TextArea rows must be positive" };
		if( this->rows == rows ) return;

		this->rows = rows;
		NotifyChanged();
	}

private:
	size_t rows = 4;
};

}
