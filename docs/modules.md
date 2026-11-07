# Module and source layout

| Module or family | Location | Purpose |
| --- | --- | --- |
| UI | targets/tatzel/public/UI.ixx | Application umbrella exporting building and diagnostic APIs. |
| UI.App, UI.Page | targets/tatzel/public | Application and registered page factories. |
| UI.Logging, UI.Diagnostics | targets/tatzel/public | Shared backend logging, traced exceptions, and callback boundaries. |
| UI.Dispatcher | targets/tatzel/public | Weak cross-thread submission handles and framework-owned bounded UI queues. |
| UI.PageBuilder, UI.PageBuilderCore | targets/tatzel/public | Logical building scopes and their borrowed services. |
| UI.Element.* | targets/tatzel/public/element | Application-facing logical control types. |
| UI.Element.ElementHandle, UI.Element.ElementLifecycle | targets/tatzel/public/element | Checked references and synchronous execution guards. |
| UI.Element.Interface.* | targets/tatzel/public/element/interface | Logical event interfaces. |
| UI.DOM.ElementPart, UI.DOM.SelectOption | targets/tatzel/public/dom | Concrete browser parts and single-choice payloads. |
| UI.ElementAdapter.* | targets/tatzel/public/element_adapter | Mapping and synchronization between the two models. |
| UI.ClientElementTree | targets/tatzel/public | Logical ownership and logical/part identity lookup. |
| UI.ResourceURL | targets/tatzel/public | Shared scheme checks for logical links and images. |
| UI.ClientUpdater | targets/tatzel/public | Serialization of concrete DOM operations. |
| UI.ClientConnection, UI.WSController, UI.Internal.JSON | targets/tatzel/private | Session ownership and transport routing. |
| Browser TypeScript | targets/typescript_build/src | Live DOM operations, event delegation, and message validation. |
| Framework CSS | www/framework | Adapter-owned layout rules layered over the default stylesheet. |

C++ modules exposed for framework integration are not automatically stable
application APIs. Application examples import UI and work with tatzel::ui
logical controls.

Some UI.Element.* modules are reserved placeholders and export no class yet.
Only supported controls are re-exported by PageBuilder. This keeps an unfinished
module from implying an available builder method.

Module interfaces (`.ixx`) hold declarations and API contracts, plus definitions
for small functions and templates when appropriate. Other function bodies live
in matching module implementation files (`.cpp`) under the target's private
directory. See the [coding conventions](../CODING_CONVENTIONS.md#c-module-interfaces-and-implementations).
CMake helpers collect ordinary sources, headers, and module interfaces from
public and private. Browser builds use the source path above, and assets are
emitted into the ignored www/app directory.

UI.Protocol defines shared batch limits, version 3, ClientEdit, EditRevision,
ValueUpdate, and ValueUpdatePolicy. UI.ClientUpdater owns nested update scopes
and ordered control-frame barriers.

UI.Element.Component provides the logical composition base. UI.Component.SearchFilterBar
and UI.Component.ConfirmationDialog provide reusable compositions and are exported
by UI. UI.Element.Style defines ControlSize and LiveRegion.

UI.Element.Table exports Table, TableRow, TableCell, TableColumn, TableSort,
TableSortDirection, and TableSelection. The default adapter maps these logical
objects to native caption/table/thead/tbody/tr/th/td parts. See [tables](tables.md).

AppOptions defines owned resource locations. The installed CMake configuration
imports the compiled shared library as Tatzel::Tatzel, provides its public
module sources for consumer BMI generation, and exposes
tatzel_deploy_runtime. See [package installation and usage](package.md).

UI.TaskQueue provides the owned background queue without exposing a transport
backend. See [background tasks](task-queue.md). JsonCpp stays in private protocol
implementation; applications need no JSON wrapper or backend headers.
