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
// code from bits/byteswap.h (C) 1997, 1998 Free Software Foundation, Inc.
#define length_to_native(x) \
     ((((x) & 0xff000000) >> 24) | (((x) & 0x00ff0000) >>  8) | \
      (((x) & 0x0000ff00) <<  8) | (((x) & 0x000000ff) << 24))
#else
#define length_to_native(x) x
#endif

/* polymorphic variant utility macro */
#define get_var(x) caml_hash_variant(#x)

/* Macros to convert variants to controls. */
#define set_ctl(tag, variant, handle, fn, name, v) \
  if (tag == get_var(variant)) { \
    check(fn(handle,name(Int_val(v)))); \
    CAMLreturn(Val_unit); \
  }

#define set_value_ctl(tag, variant, handle, fn, name, v, convert) \
  if (tag == get_var(variant)) { \
    check(fn(handle,name(convert(v)))); \
    CAMLreturn(Val_unit); \
  }

#define get_ctl(tag, variant, handle, fn, name, v, type) \
  if (tag == get_var(variant)) { \
    type name = (type)Int_val(Field(v,0)); \
    check(fn(handle,name(&name))); \
    Store_field(v,0,Val_int(name)); \
    CAMLreturn(Val_unit); \
  }

#define get_value_ctl(tag, variant, handle, fn, name, v, type, convert) \
  if (tag == get_var(variant)) { \
    type name = (type)Int_val(Field(v,0)); \
    check(fn(handle,name(&name))); \
    Store_field(v,0,convert(name)); \
    CAMLreturn(Val_unit); \
  }

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

CAMLprim value ocaml_opus_decoder_ctl(value ctl, value _dec)
{
  CAMLparam2(_dec, ctl);
  CAMLlocal2(tag,v);
  OpusDecoder *dec = Dec_val(_dec);
  if (Is_long(ctl)) {
    // Only ctl without argument here is reset state..
    opus_decoder_ctl(dec, OPUS_RESET_STATE);
    CAMLreturn(Val_unit);
  } else {
    v   = Field(ctl,1);
    tag = Field(ctl,0);

    /* Generic controls. */
    get_ctl(tag, Get_final_range, dec, opus_decoder_ctl, OPUS_GET_FINAL_RANGE, v, opus_uint32);
    get_ctl(tag, Get_pitch, dec, opus_decoder_ctl, OPUS_GET_PITCH, v, opus_int32);
    //get_ctl(tag, Get_bandwidth, dec, opus_decoder_ctl, OPUS_GET_BANDWIDTH, v, opus_int32);
    set_ctl(tag, Set_lsb_depth, dec, opus_decoder_ctl, OPUS_SET_LSB_DEPTH, v);
    get_ctl(tag, Get_lsb_depth, dec, opus_decoder_ctl, OPUS_GET_LSB_DEPTH, v, opus_int32);

    /* Decoder controls. */
    get_ctl(tag, Get_gain, dec, opus_decoder_ctl, OPUS_GET_GAIN, v, opus_int32);
    set_ctl(tag, Set_gain, dec, opus_decoder_ctl, OPUS_SET_GAIN, v);
  }

  caml_failwith("Unknown opus error"); 
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

static opus_int32 application_of_value(value v) {
  if (v == get_var(Voip))
    return OPUS_APPLICATION_VOIP;
  if (v == get_var(Audio))
    return OPUS_APPLICATION_AUDIO;
  if (v == get_var(Restricted_lowdelay))
    return OPUS_APPLICATION_RESTRICTED_LOWDELAY;

  caml_failwith("Unknown opus error");
}

static value value_of_application(opus_int32 a) {
  switch (a) {
    case OPUS_APPLICATION_VOIP:
      return get_var(Voip);
    case OPUS_APPLICATION_AUDIO:
      return get_var(Audio);
    case OPUS_APPLICATION_RESTRICTED_LOWDELAY:
      return get_var(Restricted_lowdelay);
    default:
      caml_failwith("Unknown opus error");
  }
}

CAMLprim value ocaml_opus_encoder_create(value _sr, value _chans, value _application)
{
  CAMLparam0();
  CAMLlocal1(ans);
  opus_int32 sr = Int_val(_sr);
  int chans = Int_val(_chans);
  int ret = 0;
  int app = application_of_value(_application);
  OpusEncoder *enc;

  caml_enter_blocking_section();
  enc = opus_encoder_create(sr, chans, app, &ret);
  caml_leave_blocking_section();

  check(ret);
  ans = caml_alloc_custom(&enc_ops, sizeof(OpusEncoder*), 0, 1);
  Enc_val(ans) = enc;
  CAMLreturn(ans);
}

static opus_int32 signal_of_value(value v) {
  if (v == get_var(Auto))
    return OPUS_AUTO;
  if (v == get_var(Voice))
    return OPUS_SIGNAL_VOICE;
  if (v == get_var(Music))
    return OPUS_SIGNAL_MUSIC;

  caml_failwith("Unknown opus error");
}

static value value_of_signal(opus_int32 a) {
  switch (a) {
    case OPUS_AUTO:
      return get_var(Auto);
    case OPUS_SIGNAL_VOICE:
      return get_var(Voice);
    case OPUS_SIGNAL_MUSIC:
      return get_var(Music);
    default:
      caml_failwith("Unknown opus error");
  }
}

static opus_int32 bandwidth_of_value(value v) {
  if (v == get_var(Auto))
    return OPUS_AUTO;
  if (v == get_var(Narrow_band))
    return OPUS_BANDWIDTH_NARROWBAND;
  if (v == get_var(Medium_band))
    return OPUS_BANDWIDTH_MEDIUMBAND;
  if (v == get_var(Wide_band))
    return OPUS_BANDWIDTH_WIDEBAND;
  if (v == get_var(Super_wide_band))
    return OPUS_BANDWIDTH_SUPERWIDEBAND;
  if (v == get_var(Full_band))
    return OPUS_BANDWIDTH_FULLBAND;

  caml_failwith("Unknown opus error");
}

static value value_of_bandwidth(opus_int32 a) {
  switch (a) {
    case OPUS_AUTO:
      return get_var(Auto);
    case OPUS_BANDWIDTH_NARROWBAND:
      return get_var(Narrow_band);
    case OPUS_BANDWIDTH_MEDIUMBAND:
      return get_var(Medium_band);
    case OPUS_BANDWIDTH_WIDEBAND:
      return get_var(Wide_band);
    case OPUS_BANDWIDTH_SUPERWIDEBAND:
      return get_var(Super_wide_band);
    case OPUS_BANDWIDTH_FULLBAND:
      return get_var(Full_band);
    default:
      caml_failwith("Unknown opus error");
  }
}

CAMLprim value ocaml_opus_encoder_ctl(value ctl, value _enc)
{
  CAMLparam2(_enc, ctl);
  CAMLlocal2(tag,v);
  OpusEncoder *enc = Enc_val(_enc);
  if (Is_long(ctl)) {
    // Only ctl without argument here is reset state..
    opus_encoder_ctl(enc, OPUS_RESET_STATE);
    CAMLreturn(Val_unit);
  } else {
    v   = Field(ctl,1);
    tag = Field(ctl,0);

    /* Generic controls. */
    get_ctl(tag, Get_final_range, enc, opus_encoder_ctl, OPUS_GET_FINAL_RANGE, v, opus_uint32);
    get_ctl(tag, Get_pitch, enc, opus_encoder_ctl, OPUS_GET_PITCH, v, opus_int32);
    get_value_ctl(tag, Get_bandwidth, enc, opus_encoder_ctl, OPUS_GET_BANDWIDTH, v, opus_int32, value_of_bandwidth);
    set_ctl(tag, Set_lsb_depth, enc, opus_encoder_ctl, OPUS_SET_LSB_DEPTH, v);
    get_ctl(tag, Get_lsb_depth, enc, opus_encoder_ctl, OPUS_GET_LSB_DEPTH, v, opus_int32);
    
    /* Encoder controls. */
    set_ctl(tag, Set_complexity, enc, opus_encoder_ctl, OPUS_SET_COMPLEXITY, v);
    get_ctl(tag, Get_complexity, enc, opus_encoder_ctl, OPUS_GET_COMPLEXITY, v, opus_int32);
    set_ctl(tag, Set_bitrate, enc, opus_encoder_ctl, OPUS_SET_BITRATE, v);
    get_ctl(tag, Get_bitrate, enc, opus_encoder_ctl, OPUS_GET_BITRATE, v, opus_int32);
    set_ctl(tag, Set_vbr, enc, opus_encoder_ctl, OPUS_SET_VBR, v);
    get_ctl(tag, Get_vbr, enc, opus_encoder_ctl, OPUS_GET_VBR, v, opus_int32);
    set_ctl(tag, Set_vbr_constraint, enc, opus_encoder_ctl, OPUS_SET_VBR_CONSTRAINT, v);
    get_ctl(tag, Get_vbr_constraint, enc, opus_encoder_ctl, OPUS_GET_VBR_CONSTRAINT, v, opus_int32);
    set_ctl(tag, Set_force_channels, enc, opus_encoder_ctl, OPUS_SET_FORCE_CHANNELS, v);
    get_ctl(tag, Get_force_channels, enc, opus_encoder_ctl, OPUS_GET_FORCE_CHANNELS, v, opus_int32);
    get_ctl(tag, Get_samplerate, enc, opus_encoder_ctl, OPUS_GET_SAMPLE_RATE, v, opus_int32);
    get_ctl(tag, Get_lookhead, enc, opus_encoder_ctl, OPUS_GET_LOOKAHEAD, v, opus_int32);
    set_ctl(tag, Set_inband_fec, enc, opus_encoder_ctl, OPUS_SET_INBAND_FEC, v);
    get_ctl(tag, Get_inband_fec, enc, opus_encoder_ctl, OPUS_GET_INBAND_FEC, v, opus_int32);
    set_ctl(tag, Set_packet_loss_perc, enc, opus_encoder_ctl, OPUS_SET_PACKET_LOSS_PERC, v);
    get_ctl(tag, Get_packet_loss_perc, enc, opus_encoder_ctl, OPUS_GET_PACKET_LOSS_PERC, v, opus_int32);
    set_ctl(tag, Set_dtx, enc, opus_encoder_ctl, OPUS_SET_DTX, v);
    get_ctl(tag, Get_dtx, enc, opus_encoder_ctl, OPUS_GET_DTX, v, opus_int32);

    /* These guys have polynmorphic variant as argument.. */
    set_value_ctl(tag, Set_max_bandwidth, enc, opus_encoder_ctl, OPUS_SET_MAX_BANDWIDTH, v, bandwidth_of_value);
    get_value_ctl(tag, Get_max_bandwidth, enc, opus_encoder_ctl, OPUS_GET_MAX_BANDWIDTH, v, opus_int32, value_of_bandwidth);
    set_value_ctl(tag, Set_bandwidth, enc, opus_encoder_ctl, OPUS_SET_BANDWIDTH, v, bandwidth_of_value);
    set_value_ctl(tag, Set_signal, enc, opus_encoder_ctl, OPUS_SET_SIGNAL, v, signal_of_value);
    get_value_ctl(tag, Get_signal, enc, opus_encoder_ctl, OPUS_GET_SIGNAL, v, opus_int32, value_of_signal);
    set_value_ctl(tag, Set_application, enc, opus_encoder_ctl, OPUS_SET_APPLICATION, v, application_of_value);
    get_value_ctl(tag, Get_application, enc, opus_encoder_ctl, OPUS_GET_APPLICATION, v, opus_int32, value_of_application);
  }

  caml_failwith("Unknown opus error");
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
