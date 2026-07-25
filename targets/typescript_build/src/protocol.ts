
export type DomPart = {
	part_id: string;
	parent_id: string;
	tag: string;
	classes: Array<string>;
	attributes: Record<string, string>;
	text_content: string;
};

export type ReceiveServerMessage =
	| { op: "connected"; message: string }
	| { op: "create_element"; parts: Array<DomPart> }
	| { op: "delete_element"; id: string }
	| { op: "set_attribute"; id: string; attribute: string; attribute_value: string }
	| { op: "remove_attribute"; id: string; attribute: string; }
	| { op: "add_class"; id: string; class_name: string }
	| { op: "remove_class"; id: string; class_name: string }
	| { op: "set_text"; id: string; text: string }
	| { op: "set_value"; id: string; value: string }
	| { op: "set_modal_open"; id: string; open: boolean }
	| { op: "set_on_click"; id: string }
	| { op: "set_on_change"; id: string }
	| { op: "set_on_submit"; id: string };

export type SendServerMessage =
	| { op: "connect"; route_path: string; query: string; hash: string }
	| { op: "on_click"; id: string }
	| { op: "on_change"; id: string, value: string | boolean }
	| { op: "on_submit"; id: string };
