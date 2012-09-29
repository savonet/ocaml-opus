exception Buffer_too_small
exception Internal_error
exception Invalid_packet
exception Unimplemented
exception Invalid_state
exception Alloc_fail

let () =
  Callback.register_exception "opus_exn_buffer_too_small" Buffer_too_small;
  Callback.register_exception "opus_exn_internal_error" Internal_error;
  Callback.register_exception "opus_exn_invalid_packet" Invalid_packet;
  Callback.register_exception "opus_exn_unimplemented" Unimplemented;
  Callback.register_exception "opus_exn_invalid_state" Invalid_state;
  Callback.register_exception "opus_exn_alloc_fail" Alloc_fail

let recommended_frame_size = 960*6

external version_string : unit -> string = "ocaml_opus_version_string"

let version_string = version_string ()

module Packet = struct
  type t = Ogg.Stream.packet

  external check_header : t -> bool = "ocaml_opus_packet_check_header"

  external channels : t -> int = "ocaml_opus_decoder_channels"

  external comments : t -> string * string array = "ocaml_opus_comments"

  let comments p =
    let vendor, comments = comments p in
    let comments =
      Array.map
        (fun s ->
          let n = String.index s '=' in
          String.sub s 0 n,
          String.sub s (n+1) (String.length s - n - 1)
        ) comments
    in
    let comments = Array.to_list comments in
    vendor, comments
end

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

module Decoder = struct
  type control = [
    | generic_control
    | `Set_gain of int
    | `Get_gain of int ref 
  ]

  type t

  external create : samplerate:int -> channels:int -> t = "ocaml_opus_decoder_create"

  external apply_control : control -> t -> unit = "ocaml_opus_decoder_ctl"

  external decode_float : t -> Ogg.Stream.t -> float array array -> 
                          int -> int -> bool ->int = "ocaml_opus_decoder_decode_float_byte" "ocaml_opus_decoder_decode_float"

  let decode_float ?(decode_fec=false) decoder os buf ofs len =
    decode_float decoder os buf ofs len decode_fec
end

module Encoder = struct
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

  external create : samplerate:int -> channels:int -> application:application -> t = "ocaml_opus_encoder_create"

  external apply_control : control -> t -> unit = "ocaml_opus_encoder_ctl"

  external encode_float : t -> float array array -> int -> int -> string = "ocaml_opus_encode_float"
end
