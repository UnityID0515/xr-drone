extends Node3D

const SPEED := 3.0
const ROTATION_SMOOTHING := 20.0
const SNAP_DISTANCE := 1.0
const SNAP_ANGLE := 15.0

@export var camera_3d: Camera3D

var target_position: Vector3
var velocity: Vector3
var target_rotation: Quaternion

func _ready() -> void:
	Event.data_received.connect(_on_event_data_received)

func _process(delta: float) -> void:
	if position.distance_to(target_position) > SNAP_DISTANCE:
		position = target_position
	else:
		position = position.move_toward(target_position, velocity.length() * delta)
	
	if quaternion.angle_to(target_rotation) > deg_to_rad(SNAP_ANGLE):
		quaternion = target_rotation
	else:
		quaternion = quaternion.slerp(target_rotation, 1.0 - exp(-ROTATION_SMOOTHING * delta))

	var camera_offset := basis.z * Vector3(1.0, 0, 1.0) * 0.4 + Vector3.UP * 0.3
	camera_3d.global_position = global_position + camera_offset
	camera_3d.look_at(global_position)

	var flat_input := Input.get_vector("move_left", "move_right", "move_front", "move_back")
	var vert_input := Input.get_axis("move_down", "move_up")
	if flat_input or vert_input:
		var data := {"type": "movement", "x": flat_input.x * SPEED, "y": vert_input * SPEED, "z": flat_input.y * SPEED}
		UDPSocket.peer.put_packet(JSON.stringify(data).to_utf8_buffer())

func _on_event_data_received(data: Dictionary) -> void:
	match data.get("type", ""):
		"position":
			target_position = Vector3(data.get("x", 0.0), data.get("y", 0.0), data.get("z", 0.0))
			velocity = Vector3(data.get("vx", 0.0), data.get("vy", 0.0), data.get("vz", 0.0))
		"attitude":
			target_rotation = Quaternion(data.get("x", 0.0), data.get("y", 0.0), data.get("z", 0.0), data.get("w", 1.0)).normalized()
