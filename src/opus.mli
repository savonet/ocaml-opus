exception Buffer_too_small
exception Internal_error
exception Invalid_packet
exception Unimplemented
exception Invalid_state
exception Alloc_fail

(** Recommended size of a frame in sample. Buffers for decoding are typically of
    this size. *)
val recommended_frame_size : int

val version_string : string

type max_bandwidth = [
  | `Narrow_band
  | `Medium_band
  | `Wide_band
  | `Super_wide_band
  | `Full_band   
]

type bandwidth = [
  | `Auto
  | max_bandwidth
]

type generic_control = [
  | `Reset_state
  | `Get_final_range of int ref
  | `Get_pitch       of int ref
  | `Get_bandwidth   of bandwidth ref
  | `Set_lsb_depth   of int
  | `Get_lsb_depth   of int ref
]

module Decoder : sig
  type control = [
    | generic_control
    | `Set_gain of int
    | `Get_gain of int ref
  ]

  type t

  val check_packet : Ogg.Stream.packet -> bool

  (** Create a decoder with given samplerate an number of channels. *)
  val create : ?samplerate:int -> Ogg.Stream.packet -> Ogg.Stream.packet -> t

  val create_multistream : ?samplerate:int -> Ogg.Stream.packet -> Ogg.Stream.packet -> t

  val comments : t -> string * ((string * string) list)

  val channels : t -> int

  val gain : t -> int

  val gain_coef : t -> float

  val channel_mapping_family : t -> int

  val apply_control : control -> t -> unit

  val decode_float : ?decode_fec:bool -> t -> Ogg.Stream.t -> float array array -> int -> int -> int
end

module Encoder : sig
  type application = [
    | `Voip
    | `Audio
    | `Restricted_lowdelay
  ]

  type signal = [
    | `Auto
    | `Voice
    | `Music
  ]

  type bitrate = [
    | `Auto
    | `Bitrate_max
    | `Bitrate of int
  ]

  type control = [
    | generic_control 
    | `Set_complexity        of int
    | `Get_complexity        of int ref
    | `Set_bitrate           of bitrate
    | `Get_bitrate           of bitrate ref
    | `Set_vbr               of bool
    | `Get_vbr               of bool ref
    | `Set_vbr_constraint    of bool
    | `Get_vbr_constraint    of bool ref
    | `Set_force_channels    of bool
    | `Get_force_channels    of bool ref
    | `Set_max_bandwidth     of max_bandwidth 
    | `Get_max_bandwidth     of max_bandwidth
    | `Set_bandwidth         of bandwidth
    | `Set_signal            of signal
    | `Get_signal            of signal ref
    | `Set_application       of application
    | `Get_application       of application
    | `Get_samplerate        of int
    | `Get_lookhead          of int
    | `Set_inband_fec        of bool
    | `Get_inband_fec        of bool ref
    | `Set_packet_loss_perc  of int
    | `Get_packet_loss_perc  of int ref
    | `Set_dtx               of bool
    | `Get_dtx               of bool ref
  ]

  type t

  val create : ?pre_skip:int -> ?comments:((string*string) list) -> ?gain:int ->
    samplerate:int -> channels:int -> application:application ->
    Ogg.Stream.t -> t

  (** Create a multistream encoder with given number of [streams] amongst which
      there are [coupled_streams] stereo streams. The [mapping] argument
      describes how channels (which should be >= [streams] + [coupled_streams])
      are mapped; stereo streams are always encoded first and grouped by two. *)
  val create_multistream : ?pre_skip:int -> ?comments:(string * string) list ->
    ?gain:int -> samplerate:int -> ?streams:int -> coupled_streams:int ->
    ?mapping:(int array) -> application:application -> Ogg.Stream.t -> t

  val header : t -> Ogg.Stream.packet

  val comments : t -> Ogg.Stream.packet

  val apply_control : control -> t -> unit

  val encode_float : ?frame_size:float -> t -> float array array -> int -> int -> int

  val eos : t -> unit
end
