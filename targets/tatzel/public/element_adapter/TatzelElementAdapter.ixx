module;

#include <vector>

export module UI.ElementAdapter.TatzelElementAdapter;

export import UI.ElementAdapter.ElementAdapter;


namespace tatzel {


export
class TatzelElementAdapter final : public ElementAdapter
{
public:

	TatzelElementAdapter() = default;
	TatzelElementAdapter( const TatzelElementAdapter& ) = default;
	TatzelElementAdapter( TatzelElementAdapter&& ) = default;
	~TatzelElementAdapter() override = default;

	auto operator=( const TatzelElementAdapter& ) -> TatzelElementAdapter& = default;
	auto operator=( TatzelElementAdapter&& ) -> TatzelElementAdapter& = default;

	virtual auto CreatePartsFor( ui::LogicalElement& element ) const -> void override;
	virtual auto CreatePartsFor( ui::Heading& element ) const -> void override;

	/*
	auto CreateElement( ui::LogicalElement* element ) -> ui::LogicalElement* override;
	//auto CreateLabel( ui::Label* element ) -> ui::LogicalElement override;
	//auto CreateContainer( ui::Container* element ) -> ui::LogicalElement override;
	//auto CreateButton( ui::Button* element ) -> ui::LogicalElement override;
	auto CreateHeading( ui::Heading* element ) -> ui::Heading* override;
	//auto CreateParagraph( ui::Paragraph* element ) -> ui::LogicalElement override;
	//auto CreateSpan( ui::Span* element ) -> ui::LogicalElement override;
	//auto CreateLink( ui::Link* element ) -> ui::LogicalElement override;
	//auto CreateImage( ui::Image* element ) -> ui::LogicalElement override;
	//auto CreateHorizontalRule( ui::HorizontalRule* element ) -> ui::LogicalElement override;
	//auto CreateModal( ui::Modal* element ) -> ui::LogicalElement override;
	//auto CreateCard( ui::Card* element ) -> ui::LogicalElement override;
	//auto CreateHeader( ui::Header* element ) -> ui::LogicalElement override;
	//auto CreateFooter( ui::Footer* element ) -> ui::LogicalElement override;
	*/
};


}
