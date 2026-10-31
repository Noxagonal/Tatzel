/**
 * @brief Converts a wire-format checkbox string into a boolean.
 * @param value String value supplied by the server.
 * @returns True for case-insensitive true or 1; false otherwise.
 * @pre None.
 * @post The input string is unchanged.
 */
export function stringToBoolean(value: string): boolean
{
	if (value.toLowerCase() === "true") return true;
	if (value.toLowerCase() === "1") return true;
	return false;
}
