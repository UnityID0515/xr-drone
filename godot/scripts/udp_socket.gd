extends Node

@export var ping_timer: Timer

var peer := PacketPeerUDP.new()

func _ready() -> void:
	peer.bind(4444)
	peer.set_dest_address("0.0.0.0", 4433)

func _process(_delta: float) -> void:
	while peer.get_available_packet_count() > 0:
		var array_bytes := peer.get_packet()
		var packet_string := array_bytes.get_string_from_utf8()

		var data = JSON.parse_string(packet_string)
		if not data is Dictionary: continue
		Event.data_received.emit(data)
