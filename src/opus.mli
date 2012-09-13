exception Buffer_too_small
exception Internal_error
exception Invalid_packet
exception Unimplemented
exception Invalid_state
exception Alloc_fail

val init : unit -> unit

(** Maximal size of a frame in sample. Buffers for decoding are typically of
    this size. *)
val max_frame_size : int

module Packet : sig
  type t = Ogg.Stream.packet

  val check : t -> bool

  val channels : t -> int
end

module Decoder : sig
  type t

  val create : int -> int -> t

  (** Reinitialize a decoder. *)
  val init : t -> int -> int -> unit

  val decode_float : t -> Packet.t -> float array array -> int -> int -> int
end
