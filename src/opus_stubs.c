#include <caml/alloc.h>
#include <caml/callback.h>
#include <caml/custom.h>
#include <caml/fail.h>
#include <caml/memory.h>
#include <caml/misc.h>
#include <caml/mlvalues.h>
#include <caml/signals.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <ogg/ogg.h>
#include <ocaml-ogg.h>
#include <opus/opus.h>


static inline void check(int ret)
{
  if (ret < 0)
    printf("ret: %d\n", ret);
  assert(ret >= 0);
}

#define Dec_val(v) (*(OpusDecoder**)Data_custom_val(v))

static void finalize_dec(value v)
{
  OpusDecoder *dec = Dec_val(v);
  opus_decoder_destroy(dec);
}

static struct custom_operations dec_ops =
  {
    "ocaml_opus_dec",
    finalize_dec,
    custom_compare_default,
    custom_hash_default,
    custom_serialize_default,
    custom_deserialize_default
  };

CAMLprim value ocaml_opus_decoder_create(value _sr, value _chans)
{
  CAMLparam0();
  CAMLlocal1(ans);
  int32 sr = Int_val(_sr);
  int chans = Int_val(_chans);
  int ret = 0;
  OpusDecoder *dec;

  caml_enter_blocking_section();
  /* printf("sr: %d   chans: %d\n", sr, chans); */
  dec = opus_decoder_create(sr, chans, &ret);
  caml_leave_blocking_section();

  check(ret);
  ans = caml_alloc_custom(&dec_ops, sizeof(OpusDecoder*), 0, 1);
  Dec_val(ans) = dec;
  CAMLreturn(ans);
}

CAMLprim value ocaml_opus_decoder_channels(value packet)
{
  CAMLparam1(packet);
  ogg_packet *op = Packet_val(packet);
  int ret;

  ret = opus_packet_get_nb_channels(op->packet);
  check(ret);

  CAMLreturn(Val_int(ret));
}

CAMLprim value ocaml_opus_decoder_decode_float(value _dec, value packet, value buf, value _ofs, value _len)
{
  CAMLparam3(_dec, packet, buf);
  CAMLlocal1(chan);
  ogg_packet *op = Packet_val(packet);
  OpusDecoder *dec = Dec_val(_dec);
  int32 data_len = op->bytes;
  unsigned char *data = malloc(data_len);
  memcpy(data, op->packet, data_len);
  int ret;
  int chans = opus_packet_get_nb_channels(data);
  assert(Wosize_val(buf) == chans);
  int ofs = Int_val(_ofs);
  int len = Int_val(_len);
  float *pcm = malloc(chans * len * sizeof(float));
  int i, c;

  caml_enter_blocking_section();
  /* TODO: what is the last argument exactly? */
  ret = opus_decode_float(dec, data, data_len, pcm, len, 0);
  caml_leave_blocking_section();
  free(data);

  if (ret < 0)
    {
      free(pcm);
      check(ret);
    }
  len = ret;

  for (c = 0; c < chans; c++)
    {
      chan = Field(buf, c);
      for (i = 0; i < len; i++)
        Store_double_field(chan, ofs+i, pcm[i*chans+c]);
      }

  CAMLreturn(Val_int(ret));
}
