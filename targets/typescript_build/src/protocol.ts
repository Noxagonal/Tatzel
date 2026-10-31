
/** @brief ReceiveData Connected wire-format type; DOM identifiers are part IDs. */
export type ReceiveData_Connected = {
	message: string;
};

/** @brief ReceiveData Error wire-format type; DOM identifiers are part IDs. */
export type ReceiveData_Error = {
	code: string;
	message: string;
};

/** @brief ReceiveData CreateElementPart wire-format type; DOM identifiers are part IDs. */
export type ReceiveData_CreateElementPart = {
	part_id: string;
	parent_id: string;
	tag: string;
	classes: Array<string>;
	attributes: Record<string, string>;
	text_content: string;
};

/** @brief ReceiveData RemoveElementPart wire-format type; DOM identifiers are part IDs. */
export type ReceiveData_RemoveElementPart = {
	part_id: string;
}

/** @brief ReceiveData SetAttribute wire-format type; DOM identifiers are part IDs. */
export type ReceiveData_SetAttribute = {
	part_id: string;
	attribute: string;
	attribute_value: string;
}

/** @brief ReceiveData RemoveAttribute wire-format type; DOM identifiers are part IDs. */
export type ReceiveData_RemoveAttribute = {
	part_id: string;
	attribute: string;
}

/** @brief ReceiveData AddClass wire-format type; DOM identifiers are part IDs. */
export type ReceiveData_AddClass = {
	part_id: string;
	class_name: string;
}

/** @brief ReceiveData RemoveClass wire-format type; DOM identifiers are part IDs. */
export type ReceiveData_RemoveClass = {
	part_id: string;
	class_name: string;
}

/** @brief ReceiveData SetText wire-format type; DOM identifiers are part IDs. */
export type ReceiveData_SetText = {
	part_id: string;
	text: string;
}

/** @brief ReceiveData SetValue wire-format type; DOM identifiers are part IDs. */
export type ReceiveData_SetValue = {
	part_id: string;
	value: string;
}

/** @brief ReceiveData SetModalOpen wire-format type; DOM identifiers are part IDs. */
export type ReceiveData_SetModalOpen = {
	part_id: string;
	open: boolean;
}

/** @brief ReceiveData SetOnClick wire-format type; DOM identifiers are part IDs. */
export type ReceiveData_SetOnClick = {
	part_id: string;
}

/** @brief ReceiveData SetOnChange wire-format type; DOM identifiers are part IDs. */
export type ReceiveData_SetOnChange = {
	part_id: string;
}

/** @brief ReceiveData SetOnSubmit wire-format type; DOM identifiers are part IDs. */
export type ReceiveData_SetOnSubmit = {
	part_id: string;
}

/** @brief ReceiveServerMessage wire-format type; DOM identifiers are part IDs. */
export type ReceiveServerMessage =
	| { op: "connected"; data: ReceiveData_Connected; }
	| { op: "error"; data: ReceiveData_Error; }
	| { op: "create_element_part"; data: Array<ReceiveData_CreateElementPart>; }
	| { op: "remove_element_part"; data: Array<ReceiveData_RemoveElementPart>; }
	| { op: "set_attribute"; data: Array<ReceiveData_SetAttribute>; }
	| { op: "remove_attribute"; data: Array<ReceiveData_RemoveAttribute>; }
	| { op: "add_class"; data: Array<ReceiveData_AddClass>; }
	| { op: "remove_class"; data: Array<ReceiveData_RemoveClass>; }
	| { op: "set_text"; data: Array<ReceiveData_SetText>; }
	| { op: "set_value"; data: Array<ReceiveData_SetValue>; }
	| { op: "set_modal_open"; data: Array<ReceiveData_SetModalOpen>; }
	| { op: "set_on_click"; data: Array<ReceiveData_SetOnClick>; }
	| { op: "set_on_change"; data: Array<ReceiveData_SetOnChange>; }
	| { op: "set_on_submit"; data: Array<ReceiveData_SetOnSubmit>; };



/** @brief SendData Connect wire-format type; DOM identifiers are part IDs. */
export type SendData_Connect = {
	route_path: string;
	query: string;
	hash: string;
};

/** @brief SendData OnClick wire-format type; DOM identifiers are part IDs. */
export type SendData_OnClick = {
	part_id: string;
};

/** @brief SendData OnChange wire-format type; DOM identifiers are part IDs. */
export type SendData_OnChange = {
	part_id: string;
	value: string | boolean;
};

/** @brief SendData OnSubmit wire-format type; DOM identifiers are part IDs. */
export type SendData_OnSubmit = {
	part_id: string;
};

/** @brief SendServerMessage wire-format type; DOM identifiers are part IDs. */
export type SendServerMessage =
	| { op: "connect"; data: SendData_Connect; }
	| { op: "on_click"; data: SendData_OnClick; }
	| { op: "on_change"; data: SendData_OnChange; }
	| { op: "on_submit"; data: SendData_OnSubmit; };
