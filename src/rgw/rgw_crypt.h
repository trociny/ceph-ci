// -*- mode:C; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/**
 * Crypto filters for Put/Post/Get operations.
 */
#ifndef CEPH_RGW_CRYPT_H
#define CEPH_RGW_CRYPT_H

#include <rgw/rgw_op.h>
#include <rgw/rgw_rest_s3.h>
#include <boost/utility/string_ref.hpp>

class BlockCrypt {
public:
  BlockCrypt(){};
  virtual ~BlockCrypt(){};
  /**
    * Determines size of encryption block.
    * This usually is multiply of key size.
    * It determines size of chunks that should be passed to \ref encrypt and \ref decrypt.
    */
  virtual size_t get_block_size() = 0;
  /**
   * Encrypts packet of data.
   * This is basic encryption of wider stream of data.
   * Offset shows location of <src,src+size) in stream. Offset must be multiply of basic block size \ref get_block_size.
   * Usually size is also multiply of \ref get_block_size, unless encrypting last part of stream.
   * Src and dst may be equal (in place encryption), but otherwise <src,src+size) and <dst,dst+size) may not overlap.
   *
   * \params
   * src - source of data
   * size - size of data
   * dst - destination to encrypt to
   * offset - location of <src,src+size) chunk in data stream
   */
  virtual bool encrypt(bufferlist& input,
                       off_t in_ofs,
                       size_t size,
                       bufferlist& output,
                       off_t stream_offset) = 0;
  /**
   * Decrypts packet of data.
   * This is basic decryption of wider stream of data.
   * Offset shows location of <src,src+size) in stream. Offset must be multiply of basic block size \ref get_block_size.
   * Usually size is also multiply of \ref get_block_size, unless decrypting last part of stream.
   * Src and dst may be equal (in place encryption), but otherwise <src,src+size) and <dst,dst+size) may not overlap.
   *
   * \params
   * src - source of data
   * size - size of data
   * dst - destination to decrypt to
   * offset - location of <src,src+size) chunk in data stream
   */
  virtual bool decrypt(bufferlist& input,
                       off_t in_ofs,
                       size_t size,
                       bufferlist& output,
                       off_t stream_offset) = 0;
};



static const size_t AES_256_KEYSIZE = 256 / 8;
bool AES_256_ECB_encrypt(uint8_t* key, size_t key_size, uint8_t* data_in, uint8_t* data_out, size_t data_size);

class RGWGetObj_BlockDecrypt : public RGWGetObj_Filter {
  CephContext* cct;
  std::unique_ptr<BlockCrypt> crypt;
  off_t enc_begin_skip;
  off_t ofs;
  off_t end;
  bufferlist cache;
  size_t block_size;
  std::vector<size_t> parts_len;
public:
  RGWGetObj_BlockDecrypt(CephContext* cct, RGWGetDataCB* next,
                         std::unique_ptr<BlockCrypt> crypt);
  virtual ~RGWGetObj_BlockDecrypt();

  virtual int fixup_range(off_t& bl_ofs, off_t& bl_end) override;
  virtual int handle_data(bufferlist& bl, off_t bl_ofs, off_t bl_len) override;
  virtual int flush() override;

  int read_manifest(bufferlist& manifest_bl);
}; /* RGWGetObj_BlockDecrypt */

class RGWPutObj_BlockEncrypt : public RGWPutObj_Filter
{
  CephContext* cct;
  std::unique_ptr<BlockCrypt> crypt;
  off_t ofs;
  bufferlist cache;
  size_t block_size;
public:
  RGWPutObj_BlockEncrypt(CephContext* cct, RGWPutObjDataProcessor* next,
                         std::unique_ptr<BlockCrypt> crypt);
  virtual ~RGWPutObj_BlockEncrypt();
  virtual int handle_data(bufferlist& bl,
                          off_t ofs,
                          void **phandle,
                          rgw_obj *pobj,
                          bool *again) override;
  virtual int throttle_data(void *handle,
                            const rgw_obj& obj,
                            bool need_to_wait) override;
}; /* RGWPutObj_BlockEncrypt */

std::string create_random_key_selector();
int get_actual_key_from_kms(CephContext *cct,
                            boost::string_ref key_id,
                            boost::string_ref key_selector,
                            std::string& actual_key);

int s3_prepare_encrypt(struct req_state* s,
                       map<string, bufferlist>& attrs,
                       map<string, post_form_part, const ltstr_nocase>* parts,
                       std::unique_ptr<BlockCrypt>* block_crypt,
                       std::map<std::string, std::string>& crypt_http_responses);
int s3_prepare_decrypt(struct req_state* s,
                       map<string, bufferlist>& attrs,
                       std::unique_ptr<BlockCrypt>* block_crypt,
                       std::map<std::string, std::string>& crypt_http_responses);

#endif
