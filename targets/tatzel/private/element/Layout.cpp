module UI.Element.Layout;

namespace tatzel::ui {

auto Layout::SetGap( Spacing gap ) -> void
{
	if( options.gap == gap ) return;

	options.gap = gap;
	NotifyChanged();
}

auto Layout::SetAlignment( Alignment alignment ) -> void
{
	if( options.alignment == alignment ) return;

	options.alignment = alignment;
	NotifyChanged();
}

auto Layout::SetJustification( Justification justification ) -> void
{
	if( options.justification == justification ) return;

	options.justification = justification;
	NotifyChanged();
}

auto Layout::SetWrap( bool wrap ) -> void
{
	if( options.wrap == wrap ) return;

	options.wrap = wrap;
	NotifyChanged();
}

}
