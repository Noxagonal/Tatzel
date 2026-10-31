# Module and source layout

| Module or family | Location | Purpose |
| --- | --- | --- |
| UI | targets/tatzel/public/UI.ixx | Application umbrella exporting App and PageBuilder. |
| UI.App, UI.Page | targets/tatzel/public | Application and registered page factories. |
| UI.PageBuilder, UI.PageBuilderCore | targets/tatzel/public | Logical building scopes and their borrowed services. |
| UI.Element.* | targets/tatzel/public/element | Application-facing logical control types. |
| UI.Element.Interface.* | targets/tatzel/public/element/interface | Logical event interfaces. |
| UI.DOM.ElementPart, UI.DOM.SelectOption | targets/tatzel/public/dom | Concrete browser parts and native-select payloads. |
| UI.ElementAdapter.* | targets/tatzel/public/element_adapter | Mapping and synchronization between the two models. |
| UI.ClientElementTree | targets/tatzel/public | Logical ownership and logical/part identity lookup. |
| UI.ClientUpdater | targets/tatzel/public | Serialization of concrete DOM operations. |
| UI.ClientConnection, UI.WSController | targets/tatzel/public | Session ownership and transport routing. |
| Browser TypeScript | targets/typescript_build/src | Live DOM operations, event delegation, and message validation. |
| Framework CSS | www/framework | Adapter-owned layout rules layered over the default stylesheet. |

C++ modules exposed for framework integration are not automatically stable
application APIs. Application examples import UI and work with tatzel::ui
logical controls.

Some UI.Element.* modules are reserved placeholders and export no class yet.
Only supported controls are re-exported by PageBuilder. This keeps an unfinished
module from implying an available builder method.

Implementations live under the corresponding target's private directory.
CMake helpers collect ordinary sources, headers, and module interfaces from
public and private. Browser builds use the source path above, and assets are
emitted into the ignored www/app directory.
