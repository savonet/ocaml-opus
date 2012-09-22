(*
 * Copyright 2003 Savonet team
 *
 * This file is part of OCaml-Opus.
 *
 * OCaml-Opus is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * OCaml-Opus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OCaml-Opus; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *)

(**
  * An opus to wav converter using OCaml-Opus.
  *
  * @author Samuel Mimram
  *)

open Unix

let src = ref ""
let dst = ref ""

let output_int chan n =
  output_char chan (char_of_int ((n lsr 0) land 0xff));
  output_char chan (char_of_int ((n lsr 8) land 0xff));
  output_char chan (char_of_int ((n lsr 16) land 0xff));
  output_char chan (char_of_int ((n lsr 24) land 0xff))

let output_short chan n =
  output_char chan (char_of_int ((n lsr 0) land 0xff));
  output_char chan (char_of_int ((n lsr 8) land 0xff))

let usage = "usage: opus2wav [options] source destination"

let () =
  Opus.init ();
  Arg.parse
    []
    (
      let pnum = ref (-1) in
      (fun s -> incr pnum; match !pnum with
      | 0 -> src := s
      | 1 -> dst := s
      | _ -> Printf.eprintf "Error: too many arguments\n"; exit 1
      )
    ) usage;
  if !src = "" || !dst = "" then
    (
      Printf.printf "%s\n" usage;
      exit 1
    );

  let sync, fd = Ogg.Sync.create_from_file !src in
  Printf.printf "Checking file.\n%!";
  let os, chans =
    let page = Ogg.Sync.read sync in
    assert (Ogg.Page.bos page);
    let serial = Ogg.Page.serialno page in
    Printf.printf "Testing stream %nx.%!\n" serial;
    let os = Ogg.Stream.create ~serial () in
    Ogg.Stream.put_page os page;
    let packet = Ogg.Stream.get_packet os in
    assert (Opus.Packet.check_header packet);
    let chans = Opus.Packet.channels packet in
    Printf.printf "Found an opus stream with %d channels.\n%!" chans;
    os, chans
  in
  let vendor, comments =
    let page = Ogg.Sync.read sync in
    Ogg.Stream.put_page os page;
    let packet = Ogg.Stream.get_packet os in
    let vendor, comments = Opus.Packet.comments packet in
    Printf.printf "Vendor: %s\nComments:\n%!" vendor;
    List.iter (fun (l,v) -> Printf.printf "- %s = %s\n%!" l v) comments;
    vendor, comments
  in
  let samplerate = 48000 in
  Printf.printf "Creating decoder... %!";
  let dec = Opus.Decoder.create samplerate chans in
  Printf.printf "done.\n%!";

  Printf.printf "Decoding...%!";
  let samples = ref 0 in
  let max_frame_size = 960*6 in
  let buflen = max_frame_size in
  let buf = Array.init chans (fun _ -> Array.create buflen 0.) in
  let outbuf = Array.create chans ([||] : float array) in
  (
    try
      while true do
        let rec packet () =
          try
            Ogg.Stream.get_packet os
          with
          | Ogg.Not_enough_data ->
            let page = Ogg.Sync.read sync in
            if Ogg.Page.serialno page = Ogg.Stream.serialno os then Ogg.Stream.put_page os page;
            packet ()
        in
        let packet = packet () in
        let len =
          try
            let c = Opus.Packet.channels packet in
            if c <> chans then
              (
                Printf.printf "Ignoring packet with %d channels instead of %d.\n%!" c chans;
                0
              )
            else
              Opus.Decoder.decode_float dec packet buf 0 buflen
          with
          | Opus.Invalid_packet ->
            Printf.printf "Invalid packet!\n%!";
            0
          | Invalid_argument e ->
            Printf.printf "Invalid argument: %s\n%!" e;
            0
        in
        for c = 0 to chans - 1 do
          outbuf.(c) <- Array.append outbuf.(c) (Array.sub buf.(c) 0 len)
        done
      done
    with
    | Ogg.End_of_stream -> ()
  );
  Printf.printf "done.\n%!";
  Unix.close fd;

  let len = Array.length outbuf.(0) in
  let datalen = 2 * len in
  let oc = open_out_bin !dst in
  output_string oc "RIFF";
  output_int oc (4 + 24 + 8 + datalen);
  output_string oc "WAVE";
  output_string oc "fmt ";
  output_int oc 16;
  output_short oc 1; (* WAVE_FORMAT_PCM *)
  output_short oc 2; (* channels *)
  output_int oc samplerate; (* freq *)
  output_int oc (samplerate * 2 * 2); (* bytes / s *)
  output_short oc (2 * 2); (* block alignment *)
  output_short oc 16; (* bits per sample *)
  output_string oc "data";
  output_int oc datalen;

  for i = 0 to len - 1 do
    for c = 0 to chans - 1 do
      let x = outbuf.(c).(i) in
      let x = int_of_float (x *. 32767.) in
      output_short oc x;
    done
  done;
  close_out oc;

  Gc.full_major ()
