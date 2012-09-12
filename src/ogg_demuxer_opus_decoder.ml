(*
 * Copyright 2003-2011 Savonet team
 *
 * This file is part of Ocaml-vorbis.
 *
 * Ocaml-vorbis is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Ocaml-vorbis is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Ocaml-vorbis; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *)

(*

let check _ = true

let buflen = 1024

let decoder os =
  let decoder = ref None in
  let packet = ref None in
  let packet2 = ref None in
  let packet3 = ref None in
  let os = ref os in
  let init () =
    match !decoder with
    | None ->
      let packet =
        match !packet with
        | None ->
          let p = Ogg.Stream.get_packet !os in
          packet := Some p; p
        | Some p -> p
      in
      let samplerate = 44100 in
      let chans = Opus.Packet.channels packet in
      let dec = Opus.Decoder.create 44100 chans in


      (* This buffer is created once. The call to Array.sub
       * below makes a fresh array out of it to pass to
       * liquidsoap. *)
      let chan _ = Array.make buflen 0. in
      let buf = Array.init info.Opus.audio_channels chan in
      (* let meta = Opus.Decoder.comments d in *)
      let meta = [] in
      decoder := Some (d,samplerate,channels,buf,meta);
      d,info,buf,meta
    | Some d -> d
  in
  let info () =
    let (_,info,_,meta) = init () in
    { Ogg_demuxer.
      channels = info.Opus.audio_channels;
      sample_rate = info.Opus.audio_samplerate },meta
  in
  let restart new_os =
    os := new_os;
    let (d,_,_,_) = init () in
    Opus.Decoder.restart d
  in
  let decode feed =
    let decoder,_,buf,_ = init () in
    try
      let ret = Opus.Decoder.decode_pcm decoder !os buf 0 buflen in
      feed (Array.map (fun x -> Array.sub x 0 ret) buf)
    with
    (* Apparently, we should hide this one.. *)
    | Opus.False -> raise Ogg.Not_enough_data
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
*)

let register () = ()
