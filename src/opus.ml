module Packet = struct
  type t = Ogg.Stream.packet

  external channels : t -> int = "ocaml_opus_decoder_channels"
end

module Decoder = struct
  type t

  external create : int -> int -> t = "ocaml_opus_decoder_create"

  external decode_float : t -> Packet.t -> float array array -> int -> int -> int = "ocaml_opus_decoder_decode_float"
end
