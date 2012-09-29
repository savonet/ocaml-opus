(*
 * Copyright 2003-2011 Savonet team
 *
 * This file is part of ocaml-opus.
 *
 * ocaml-opus is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * ocaml-opus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ocaml-opus; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *)

let check = Opus.Packet.check_header

let buflen = Opus.recommended_frame_size

let decoder_samplerate = ref 48000

let decoder os =
  let decoder = ref None in
  let packet1 = ref None in
  let packet2 = ref None in
  let os = ref os in
  let init () =
    match !decoder with
    | None ->
      let packet1 =
        match !packet1 with
        | None ->
          let p = Ogg.Stream.get_packet !os in
          packet1 := Some p; p
        | Some p -> p
      in
      let packet2 =
        match !packet2 with
        | None ->
          let p = Ogg.Stream.get_packet !os in
          packet2 := Some p; p
        | Some p -> p
      in
      let chans = Opus.Packet.channels packet1 in
      let meta = Opus.Packet.comments packet2 in
      let dec = Opus.Decoder.create ~samplerate:!decoder_samplerate ~channels:chans in
      (* This buffer is created once. The call to Array.sub
       * below makes a fresh array out of it to pass to
       * liquidsoap. *)
      let chan _ = Array.make buflen 0. in
      let buf = Array.init chans chan in
      decoder := Some (dec,chans,buf,meta);
      dec,chans,buf,meta
    | Some dec -> dec
  in
  let info () =
    let (_,chans,_,meta) = init () in
    { Ogg_demuxer.
      channels = chans;
      sample_rate = !decoder_samplerate },
    meta
  in
  let restart new_os =
    os := new_os;
    let (dec,chans,buf,meta) = init () in
    let dec = Opus.Decoder.create ~samplerate:!decoder_samplerate ~channels:chans in
    decoder := Some (dec,chans,buf,meta)
  in
  let decode feed =
    let dec,chans,buf,_ = init () in
    let ret = Opus.Decoder.decode_float dec !os buf 0 buflen in
    feed (Array.map (fun x -> Array.sub x 0 ret) buf)
  in
  Ogg_demuxer.Audio
    { Ogg_demuxer.
      name = "opus";
      info = info;
      decode = decode;
      restart = restart;
      samples_of_granulepos = (fun x -> x) }

let register () =
  Hashtbl.add Ogg_demuxer.ogg_decoders "opus" (check,decoder)
