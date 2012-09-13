exception Buffer_too_small
exception Internal_error
exception Invalid_packet
exception Unimplemented
exception Invalid_state
exception Alloc_fail

val init : unit -> unit

module Packet : sig
  type t = Ogg.Stream.packet

  val check : t -> bool

  val channels : t -> int
end

module Decoder : sig
  type t

  val create : int -> int -> t

  val decode_float : t -> Packet.t -> float array array -> int -> int -> int
end
