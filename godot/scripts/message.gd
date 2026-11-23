extends RefCounted
## Binary codec shared with the C++ server (see SPEC.md).
## Packet: [u8 header_size][type name][little-endian float32 payload]
##   ping      no payload
##   position  x, y, z, vx, vy, vz   (server -> Godot, already in Godot EUS)
##   attitude  w, x, y, z            (server -> Godot, unit quaternion)
##   movement  x, y, z               (Godot -> server, body velocity in m/s)

const FLOAT_SIZE := 4

const PAYLOAD_FIELDS := {
	"position": ["x", "y", "z", "vx", "vy", "vz"],
	"attitude": ["w", "x", "y", "z"],
}


static func encode(type: String, values: Array = []) -> PackedByteArray:
	var type_bytes := type.to_utf8_buffer()
	var offset := 1 + type_bytes.size()
	var bytes := PackedByteArray()
	bytes.resize(offset + values.size() * FLOAT_SIZE)
	bytes[0] = type_bytes.size()
	for i in type_bytes.size():
		bytes[1 + i] = type_bytes[i]
	for i in values.size():
		bytes.encode_float(offset + i * FLOAT_SIZE, values[i])
	return bytes


static func encode_ping() -> PackedByteArray:
	return encode("ping")


static func encode_movement(velocity: Vector3) -> PackedByteArray:
	return encode("movement", [velocity.x, velocity.y, velocity.z])


## Returns {"type": ..., <fields>} or an empty Dictionary if the packet is
## malformed or of an unknown type. Same shape the old JSON messages had.
static func decode(bytes: PackedByteArray) -> Dictionary:
	if bytes.is_empty():
		return {}
	var header_size := bytes[0]
	if bytes.size() < 1 + header_size:
		return {}

	var type := bytes.slice(1, 1 + header_size).get_string_from_utf8()
	var offset := 1 + header_size
	var payload_size := bytes.size() - offset

	if type == "ping":
		return {"type": type}
	if not PAYLOAD_FIELDS.has(type):
		return {}

	var fields: Array = PAYLOAD_FIELDS[type]
	if payload_size != fields.size() * FLOAT_SIZE:
		return {}

	var data := {"type": type}
	for i in fields.size():
		data[fields[i]] = bytes.decode_float(offset + i * FLOAT_SIZE)
	return data
