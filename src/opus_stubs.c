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
#include <opus/opus_multistream.h>

#ifdef BIGENDIAN
// Thank you, FFMPEG :-)
static inline opus_uint16 bswap_32(opus_uint16 x)
{
  unsigned tmp;
  __asm__("%1 = %0 >> 8 (V);      \n\t"
          "%0 = %0 << 8 (V);      \n\t"
          "%0 = %0 | %1;          \n\t"
          "%0 = PACK(%0.L, %0.H); \n\t"
          : "+d"(x), "=&d"(tmp));
  return x;
}
#define length_to_native(x) bswap_32(x)
#else
#define length_to_native(x) x
#endif


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

/***** Decoder ******/

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
  opus_int32 sr = Int_val(_sr);
  int chans = Int_val(_chans);
  int ret = 0;
  OpusDecoder *dec;

  caml_enter_blocking_section();
  dec = opus_decoder_create(sr, chans, &ret);
  caml_leave_blocking_section();

  check(ret);
  ans = caml_alloc_custom(&dec_ops, sizeof(OpusDecoder*), 0, 1);
  Dec_val(ans) = dec;
  CAMLreturn(ans);
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
  opus_int32 vendor_length = length_to_native((opus_int32)*(op->packet+off));
  off += 4;
  Store_field(ans, 0, caml_alloc_string(vendor_length));
  memcpy(String_val(Field(ans,0)), op->packet+off, vendor_length);
  off += vendor_length;

  /* Comments */
  opus_int32 comments_length = length_to_native((opus_int32)*(op->packet+off));
  off += 4;
  comments = caml_alloc_tuple(comments_length);
  Store_field(ans, 1, comments);
  opus_int32 i, len;
  for(i = 0; i < comments_length; i++)
    {
      len = length_to_native((opus_int32)*(op->packet+off));
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
  opus_int32 data_len = op->bytes;
  unsigned char *data = malloc(data_len);
  memcpy(data, op->packet, data_len);
  int ret;
  int chans = opus_packet_get_nb_channels(data);
  if (Wosize_val(buf) != chans)
    caml_invalid_argument("Wrong number of channels.");
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

/***** Encoder *****/

#define Enc_val(v) (*(OpusEncoder**)Data_custom_val(v))

static void finalize_enc(value v)
{
  OpusEncoder *enc = Enc_val(v);
  opus_encoder_destroy(enc);
}

static struct custom_operations enc_ops =
  {
    "ocaml_opus_enc",
    finalize_enc,
    custom_compare_default,
    custom_hash_default,
    custom_serialize_default,
    custom_deserialize_default
  };

static int int_of_application(value app)
{
  switch(Int_val(app))
    {
    case 0:
      return OPUS_APPLICATION_VOIP;

    case 1:
      return OPUS_APPLICATION_AUDIO;

    case 2:
      return OPUS_APPLICATION_RESTRICTED_LOWDELAY;

    default:
      assert(0);
    }
}

CAMLprim value ocaml_opus_encoder_create(value _sr, value _chans, value _application)
{
  CAMLparam0();
  CAMLlocal1(ans);
  opus_int32 sr = Int_val(_sr);
  int chans = Int_val(_chans);
  int ret = 0;
  int app = int_of_application(_application);
  OpusEncoder *enc;

  caml_enter_blocking_section();
  enc = opus_encoder_create(sr, chans, app, &ret);
  caml_leave_blocking_section();

  check(ret);
  ans = caml_alloc_custom(&enc_ops, sizeof(OpusEncoder*), 0, 1);
  Enc_val(ans) = enc;
  CAMLreturn(ans);
}

CAMLprim value ocaml_opus_encode_float(value _enc, value buf, value _off, value _len)
{
  CAMLparam2(_enc, buf);
  CAMLlocal1(ans);
  OpusEncoder *enc = Enc_val(_enc);
  int off = Int_val(_off);
  int len = Int_val(_len);
  int chans = Wosize_val(buf);
  /* This is the recommended value */
  int max_data_bytes = 4000;
  unsigned char *data = malloc(max_data_bytes);
  float *pcm = malloc(chans*len*sizeof(float));
  int i, c;
  int ret;
  for(i = 0; i < len; i ++)
    for(c = 0; c < chans; c++)
      pcm[chans*i+c] = Double_field(Field(buf, c), off+i);

  caml_enter_blocking_section();
  ret = opus_encode_float(enc, pcm, len, data, max_data_bytes);
  caml_leave_blocking_section();

  free(pcm);
  if (ret < 0) free(data);
  check(ret);

  ans = caml_alloc_string(ret);
  memcpy(String_val(ans), data, ret);
  free(data);

  CAMLreturn(ans);
}
