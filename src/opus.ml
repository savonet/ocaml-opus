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

(* Utility function to compute default arguments for multistreams. *)
let streams_mapping ?streams ~coupled_streams ?mapping () =
  let channels =
    match mapping with
    | Some mapping -> Array.length mapping
    | None -> 2 * coupled_streams
  in
  let streams =
    match streams with
    | Some streams -> streams
    | None -> channels - 2*coupled_streams
  in
  if streams < 0 then invalid_arg "mapping";
  let mapping =
    match mapping with
    | Some mapping -> mapping
    | None -> Array.init (streams + coupled_streams) (fun i -> i)
  in
  streams, mapping


module Decoder = struct
  type control = [
    | generic_control
    | `Set_gain of int
    | `Get_gain of int ref 
  ]

  external check_packet : Ogg.Stream.packet -> bool = "ocaml_opus_packet_check_header"

  external packet_channels : Ogg.Stream.packet -> int = "ocaml_opus_decoder_channels"

  external channel_mapping_family : Ogg.Stream.packet -> int = "ocaml_opus_decoder_channel_mapping_family"

  external channel_mapping : Ogg.Stream.packet -> (int * int * (int array)) = "ocaml_opus_decoder_channel_mapping"

  external comments : Ogg.Stream.packet -> string * string array = "ocaml_opus_comments"

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

  type decoder
  type t =
    { header   : Ogg.Stream.packet;
      comments : Ogg.Stream.packet;
      decoder  : decoder }

  external create : samplerate:int -> channels:int -> decoder = "ocaml_opus_decoder_create"

  let create ?(samplerate=48000) p1 p2 =
    if not (check_packet p1) then
      raise Invalid_packet;
    let decoder = create ~samplerate ~channels:(packet_channels p1) in
    { header   = p1;
      comments = p2;
      decoder  = decoder }

  external create_multistream : samplerate:int -> streams:int -> coupled_streams:int -> mapping:int array -> decoder = "ocaml_opus_decoder_create"
  let create_multistream ?(samplerate=48000) p1 p2 =
    if not (check_packet p1) then raise Invalid_packet;
    let channels = packet_channels p1 in
    let family = channel_mapping_family p1 in
    let streams, coupled_streams, mapping =
      if family = 0 then
        1, (if channels = 1 then 0 else 1), Array.init channels (fun i -> i)
      else
        channel_mapping p1
    in
    (* Printf.printf "streams: %d, coupled_streams: %d\n%!" streams coupled_streams; *)
    (* Printf.printf "mapping: %s\n%!" (String.concat ", " (List.map string_of_int (Array.to_list mapping))); *)
    let decoder = create_multistream ~samplerate ~streams ~coupled_streams ~mapping in
    { header   = p1;
      comments = p2;
      decoder  = decoder }

  external apply_control : control -> decoder -> unit = "ocaml_opus_decoder_ctl"

  let apply_control control t = apply_control control t.decoder

  external decode_float : decoder -> Ogg.Stream.t -> float array array ->
                          int -> int -> bool ->int = "ocaml_opus_decoder_decode_float_byte" "ocaml_opus_decoder_decode_float"

  let decode_float ?(decode_fec=false) t os buf ofs len =
    decode_float t.decoder os buf ofs len decode_fec

  let comments t = comments t.comments

  let channels t = packet_channels t.header

  let channel_mapping_family t = channel_mapping_family t.header

  external gain : Ogg.Stream.packet -> int = "ocaml_opus_decoder_gain"
  let gain t = gain t.header

  let gain_coef t =
    let g = gain t in
    let g = float_of_int g in
    10. ** (g /. (20. *. 256.))
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

  type encoder
  type t = {
    header     : Ogg.Stream.packet;
    comments   : Ogg.Stream.packet;
    os         : Ogg.Stream.t;
    samplerate : int;
    enc        : encoder
  }

  external create : pre_skip:int -> comments:(string array) ->
                    gain:int -> samplerate:int -> channels:int ->
                    application:application -> encoder*Ogg.Stream.packet*Ogg.Stream.packet = "ocaml_opus_encoder_create_byte" "ocaml_opus_encoder_create"

  let create ?(pre_skip=3840) ?(comments=[]) ?(gain=0) ~samplerate ~channels ~application os =
    let comments = List.map (fun (label, value) -> Printf.sprintf "%s=%s" label value) comments in
    let comments = Array.of_list comments in
    let enc,p1,p2 = create ~pre_skip ~comments ~gain ~samplerate ~channels ~application in
    { os         = os;
      header     = p1;
      comments   = p2;
      samplerate = samplerate;
      enc        = enc }

  external create_multistream : pre_skip:int -> comments:(string array) -> gain:int -> samplerate:int -> streams:int -> coupled_streams:int -> mapping:(int array) -> application:application -> encoder*Ogg.Stream.packet*Ogg.Stream.packet = "ocaml_opus_multistream_encoder_create_byte" "ocaml_opus_multistream_encoder_create"

  let create_multistream ?(pre_skip=3840) ?(comments=[]) ?(gain=0) ~samplerate ?streams ~coupled_streams ?mapping ~application os =
    let streams, mapping = streams_mapping ?streams ~coupled_streams ?mapping () in
    let comments = List.map (fun (label, value) -> Printf.sprintf "%s=%s" label value) comments in
    let comments = Array.of_list comments in
    let enc,p1,p2 = create_multistream ~pre_skip ~comments ~gain ~samplerate ~streams ~coupled_streams ~mapping ~application in
    { os         = os;
      header     = p1;
      comments   = p2;
      samplerate = samplerate;
      enc        = enc }

  let header enc = enc.header

  let comments enc = enc.comments

  external apply_control : control -> encoder -> unit = "ocaml_opus_encoder_ctl"

  let apply_control control enc = apply_control control enc.enc

  external encode_float : frame_size:int -> encoder ->
      float array array -> int -> int -> Ogg.Stream.t -> int = "ocaml_opus_encode_float_byte" "ocaml_opus_encode_float"

  let encode_float ?(frame_size=20.) t buf ofs len =
    let frame_size = frame_size *. (float t.samplerate) /. 1000. in
    encode_float (int_of_float frame_size) t.enc buf ofs len t.os

  external eos : Ogg.Stream.t -> encoder -> unit = "ocaml_opus_encode_eos"

  let eos t = eos t.os t.enc
end
