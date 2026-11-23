extends Node

const Message := preload("res://scripts/message.gd")

const SERVER_HOST := "127.0.0.1"
const SERVER_PORT := 4444

@export var ping_timer: Timer

var peer := PacketPeerUDP.new()


func _ready() -> void:
	# The server learns our address from the first packet it receives, so we
	# ping on a timer. It also works as a keepalive: the server drops clients
	# that stay silent for a few seconds.
	peer.connect_to_host(SERVER_HOST, SERVER_PORT)
	ping_timer.timeout.connect(_send_ping)
	_send_ping()


func _process(_delta: float) -> void:
	while peer.get_available_packet_count() > 0:
		var data := Message.decode(peer.get_packet())
		if data.is_empty() or data["type"] == "ping":
			continue
		Event.data_received.emit(data)


func send_movement(velocity: Vector3) -> void:
	peer.put_packet(Message.encode_movement(velocity))


func _send_ping() -> void:
	peer.put_packet(Message.encode_ping())
