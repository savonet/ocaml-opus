let output_int chan n =
  output_char chan (char_of_int ((n lsr 0) land 0xff));
  output_char chan (char_of_int ((n lsr 8) land 0xff));
  output_char chan (char_of_int ((n lsr 16) land 0xff));
  output_char chan (char_of_int ((n lsr 24) land 0xff))

let output_short chan n =
  output_char chan (char_of_int ((n lsr 0) land 0xff));
  output_char chan (char_of_int ((n lsr 8) land 0xff))

let () =
  let dst = "gen.wav" in
  let chans = 2 in
  let samples = 4096 * 12 in
  let datalen = samples * chans * 2 in
  let oc = open_out_bin dst in
  output_string oc "RIFF";
  output_int oc (4 + 24 + 8 + datalen);
  output_string oc "WAVE";
  output_string oc "fmt ";
  output_int oc 16;
  output_short oc 1;
  (* WAVE_FORMAT_PCM *)
  output_short oc 2;
  (* channels *)
  output_int oc 48000;
  (* freq *)
  output_int oc (48000 * chans * 2);
  (* bytes / s *)
  output_short oc (chans * 2);
  (* block alignment *)
  output_short oc 16;
  (* bits per sample *)
  output_string oc "data";
  output_int oc datalen;
  for i = 0 to samples - 1 do
    for c = 0 to chans - 1 do
      output_short oc (i * c * 32767)
    done
  done;
  close_out oc
