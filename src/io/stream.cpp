/*
 * MIT License
 *
 * Copyright (c) 2025 The Dyne Language Team
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 byte Unsigned byte
 halfword Two bytes
 long Signed long
 xlong 0 ≤ value ≤ 254: unsigned byte
 else: byte 0xFF followed by signed long
 object This definition (recursive). Must be a kPrecedent clause if the object has been assigned a precedent ID.

 immediate kImmediate=0 (byte)
 Immediate Ref (xlong)
 character See “Immediate Objects” on page 4-4
 kCharacter=1 (byte)
 Character code (byte)
 See “Special Case Types” on page 4-5
 uniChar kUnicodeCharacter=2 (byte)
 High byte of character code (byte)
 Low byte of character code (byte)
 See “Special Case Types” on page 4-5
 binary* kBinaryObject=3 (byte)
 Number of bytes of data (xlong)
 Class (object)
 Data
 See “Binary Object Data” on page 4-5
 array* kArray=4 (byte)
 Number of slots (xlong)
 Class (object)
 Slot values in ascending order (objects)
 plainArray* kPlainArray=5 (byte)
 Number of slots (xlong)
 Slot values in ascending order (objects)
 See “Special Case Types” on page 4-5
 frame* kFrame=6 (byte)
 Number of slots (xlong)
 Slot tags in ascending order (symbol objects)
 Slot values in ascending order (objects)
 symbol* kSymbol=7 (byte)
 Number of characters in name (xlong)
 Name (bytes)
 string* kString=8 (byte)
 Number of bytes in string (xlong)
 String (halfwords)
 See “Binary Object Data” and “Special Case Types” on
 page 4-5
 precedent kPrecedent=9 (byte)
 Precedent ID (xlong)
 nil kNIL=10 (byte)
 smallRect* See “Special Case Types” on page 4-5
 kSmallRect=11 (byte)
 Top value (byte)
 Left value (byte)
 Bottom value (byte)
 Right value (byte)
 See “Special Case Types” on page 4-5
 largeBinary* kLargeBinary=12 (byte)
 Class (object)
 Compressed? (non-zero means compressed) (byte)
 Number of bytes of data (long)
 Number of characters in compander name (long)
 Number of byte of compander parameters (long)
 Reserved (encode zero, ignore when decoding) (long)
 Compander name (bytes)
 Compander parameters (bytes)
 Data (bytes)

 Starts with version number (0x02)
 */

#include <dyn/io/stream.h>
#include <dyn/objects.h>
#include <dyn/io/package/package_bytes.h>

#include <iostream>
#include <fstream>
#include <iomanip>

using namespace dyn;

using namespace dyn::io;

StreamReader::StreamReader()
{
}

StreamReader::~StreamReader()
{
  close();
}

int StreamReader::open(const std::string &filename)
{
  std::ifstream source_file { filename, std::ios::binary };
  if (source_file) {
    bytes_ = std::make_shared<PackageBytes>();
    bytes_->assign(std::istreambuf_iterator<char>{source_file}, {});
    bytes_->rewind();
    return 0;
  } else {
    return -1;
  }
}

dyn::Ref StreamReader::read_next_()
{
  dyn::Ref ret = dyn::RefNIL;
  auto pos = bytes_->tell();
  auto prec_ix = precedent_.size();
  uint8_t tag = bytes_->get_ubyte();
  switch (tag) {
    case 0: { // immediate
      uint32_t imm = bytes_->get_xlong();
      if ((imm & 0x02)==0) imm ^= 0x01; // Newton to Dyne
      ret = dyn::Ref((dyn::Ref::Verbatim_)imm);
      break; }
    case 1: ret = dyn::Ref((UniChar)bytes_->get_ubyte()); break; // char
    case 2: ret = dyn::Ref((UniChar)bytes_->get_ushort()); break; // uniChar
    case 3: { // binary: xlong bytes, object class, raw bytes
      // TODO: recognize Real, String, maybe more
      precedent_.push_back(RefNIL);
      uint32_t size = bytes_->get_xlong(); // Number of slots (xlong)
      Ref klass = read_next_();
      auto bin = bytes_->get_data(size);
      void *data = ::calloc(1, size);
      ::memcpy(data, &bin[0], size);
      Ref binary = ret = new BinaryObject(klass, size, data);
      precedent_[prec_ix] = binary;
      break; }
    case 4: { // array
      precedent_.push_back(RefNIL);
      uint32_t length = bytes_->get_xlong(); // Number of slots (xlong)
      Ref klass = read_next_();
      Ref array = ret = AllocateArray(klass, length);
      precedent_[prec_ix] = array;
      for (uint32_t i=0; i<length; ++i) {
        dyn::SetArraySlot(array, i, read_next_());
      }
      break; }
    case 5: { // plainArray
      precedent_.push_back(RefNIL);
      uint32_t length = bytes_->get_xlong(); // Number of slots (xlong)
      Ref array = ret = AllocateArray(length);
      precedent_[prec_ix] = array;
      for (uint32_t i=0; i<length; ++i) {
        dyn::SetArraySlot(array, i, read_next_());
      }
      break; }
    case 6: { // frame
      precedent_.push_back(RefNIL);
      uint32_t length = bytes_->get_xlong(); // Number of slots (xlong)
      Ref frame = ret = AllocateFrame();
      SlottedObject *fobj = static_cast<SlottedObject*>(frame.GetObject());
      precedent_[prec_ix] = frame;
      for (uint32_t i=0; i<length; ++i) {
        dyn::SetFrameSlot(frame, read_next_(), dyn::RefNIL);
      }
      for (uint32_t i=0; i<length; ++i) {
        fobj->SetSlot(i, read_next_());
      }
      break; }
    case 7: { // symbol
      precedent_.push_back(RefNIL);
      uint32_t length = bytes_->get_xlong();
      dyn::Ref sym = ret = dyn::Sym(bytes_->get_cstring(length, false));
      precedent_[prec_ix] = sym;
      break; }
    case 8: { // string
      precedent_.push_back(RefNIL);
      uint32_t length = bytes_->get_xlong();
      dyn::Ref str = ret = dyn::MakeString(bytes_->get_ustring(length/2, false));
      precedent_[prec_ix] = str;
      break; }
    case 9: ret = precedent_[bytes_->get_xlong()]; break; // precedent
    case 10: ret = dyn::RefNIL; break; // nil
    case 11: // smallRect
    case 12: // largeBinary
    default: // unsupported
      std::cout << "ERROR: StreamReader: Unsupported tag " << (int)tag << " at " << pos << "." << std::endl;
      return ret;
  }
//  printf("%d: ", pos); dyn::Print(ret);
  return ret;
}

dyn::Ref StreamReader::read()
{
  uint16_t version = bytes_->get_ubyte();
  if (version==2) {
    dyn::Ref r = read_next_();
    return r;
  } else {
    std::cout << "ERROR: StreamReader: Unknown stream version " << (int)version << "." << std::endl;
    return dyn::RefNIL;
  }

}

void StreamReader::close()
{
  bytes_.reset();
}

dyn::Ref StreamReader::read(const std::string &filename)
{
  StreamReader in;
  if (in.open(filename) == 0) {
    return in.read();
  } else {
    return dyn::RefNIL;
  }
}
