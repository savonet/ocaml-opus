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

let max_frame_size = 960*6

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

module Decoder = struct
  type t

  external create : int -> int -> t = "ocaml_opus_decoder_create"

  external init : t -> int -> int -> unit = "ocaml_opus_decoder_init"

  external decode_float : t -> Packet.t -> float array array -> int -> int -> int = "ocaml_opus_decoder_decode_float_byte" "ocaml_opus_decoder_decode_float"
end

module Encoder = struct
  type t

  type application = Application_voip | Application_audio | Application_restricted_lowdelay

  external create : int -> int -> application -> t = "ocaml_opus_encoder_create"

  external encode_float : t -> float array array -> int -> int -> string = "ocaml_opus_encode_float"
end
