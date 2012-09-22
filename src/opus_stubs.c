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
#include <stdint.h>
#include <string.h>
#include <endian.h>

#include <ogg/ogg.h>
#include <ocaml-ogg.h>
#include <opus/opus.h>
#include <opus/opus_multistream.h>


static void check(int ret)
{
  if (ret < 0)
    switch (ret)
      {
      case OPUS_BAD_ARG:
        caml_invalid_argument("opus");

      case OPUS_BUFFER_TOO_SMALL:
        caml_raise_constant(*caml_named_value("opus_exn_buffer_too_small"));

      case OPUS_INTERNAL_ERROR:
        caml_raise_constant(*caml_named_value("opus_exn_internal_error"));

      case OPUS_INVALID_PACKET:
        caml_raise_constant(*caml_named_value("opus_exn_invalid_packet"));

      case OPUS_UNIMPLEMENTED:
        caml_raise_constant(*caml_named_value("opus_exn_unimplemented"));

      case OPUS_INVALID_STATE:
        caml_raise_constant(*caml_named_value("opus_exn_invalid_state"));

      case OPUS_ALLOC_FAIL:
        caml_raise_constant(*caml_named_value("opus_exn_alloc_fail"));

      default:
        caml_failwith("Unknown opus error");
      }
}

CAMLprim value ocaml_opus_version_string(value unit)
{
  CAMLparam0();
  CAMLreturn(caml_copy_string(opus_get_version_string()));
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
  int32_t sr = Int_val(_sr);
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

CAMLprim value ocaml_opus_decoder_init(value _dec, value _samplerate, value _chans)
{
  CAMLparam1(_dec);
  OpusDecoder *dec = Dec_val(_dec);
  opus_int32 samplerate = Int_val(_samplerate);
  int chans = Int_val(_chans);
  int ret;

  caml_enter_blocking_section();
  ret = opus_decoder_init(dec, samplerate, chans);
  caml_leave_blocking_section();

  check(ret);
  CAMLreturn(Val_unit);
}

CAMLprim value ocaml_opus_packet_check_header(value packet)
{
  CAMLparam1(packet);
  ogg_packet *op = Packet_val(packet);
  int ans = 0;

  if (op->bytes >= 8 && !memcmp(op->packet, "OpusHead", 8))
    ans = 1;

  CAMLreturn(Val_bool(ans));
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

CAMLprim value ocaml_opus_comments(value packet)
{
  CAMLparam1(packet);
  CAMLlocal2(ans, comments);
  ogg_packet *op = Packet_val(packet);
  if (!(op->bytes >= 8 && !memcmp(op->packet, "OpusTags", 8)))
    check(OPUS_INVALID_PACKET);
  ans = caml_alloc_tuple(2);

  /* TODO: check for buffer overflows */

  int off = 8;
  /* Vendor */
  int32_t vendor_length = le32toh((int32_t)*(op->packet+off));
  off += 4;
  Store_field(ans, 0, caml_alloc_string(vendor_length));
  memcpy(String_val(Field(ans,0)), op->packet+off, vendor_length);
  off += vendor_length;

  /* Comments */
  int32_t comments_length = le32toh((int32_t)*(op->packet+off));
  off += 4;
  comments = caml_alloc_tuple(comments_length);
  Store_field(ans, 1, comments);
  int32_t i, len;
  for(i = 0; i < comments_length; i++)
    {
      len = le32toh((int32_t)*(op->packet+off));
      off += 4;
      Store_field(comments, i, caml_alloc_string(len));
      memcpy(String_val(Field(comments, i)), op->packet+off, len);
      off += len;
    }

  CAMLreturn(ans);
}

CAMLprim value ocaml_opus_decoder_decode_float(value _dec, value packet, value buf, value _ofs, value _len)
{
  CAMLparam3(_dec, packet, buf);
  CAMLlocal1(chan);
  ogg_packet *op = Packet_val(packet);
  OpusDecoder *dec = Dec_val(_dec);
  int32_t data_len = op->bytes;
  unsigned char *data = malloc(data_len);
  memcpy(data, op->packet, data_len);
  int ret;
  int chans = opus_packet_get_nb_channels(data);
  if (Wosize_val(buf) != chans)
    {
      printf("chans: %d\n", chans);
      caml_invalid_argument("Wrong number of channels.");
    }
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

CAMLprim value ocaml_opus_decoder_decode_float_byte(value args)
{
  /* TODO */
  assert(0);
}
