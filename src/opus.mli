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

  val check_header : t -> bool

  val channels : t -> int

  val comments : t -> string * (string * string) list
end

module Decoder : sig
  type t

  (** Create a decoder with given samplerate an number of channels. *)
  val create : int -> int -> t

  (** Reinitialize a decoder (this is not needed after [create]). *)
  val init : t -> int -> int -> unit

  val decode_float : t -> Packet.t -> float array array -> int -> int -> int
end

module Encoder : sig
type t

  type application = Application_voip | Application_audio | Application_restricted_lowdelay

  val create : int -> int -> application -> t

  val encode_float : t -> float array array -> int -> int -> string
end
