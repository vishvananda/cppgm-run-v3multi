#include "lowir_model.h"

namespace lowir_model {

int LowType::integer_width() const
{
  switch (integer_kind) {
  case INTEGER_I1: return 1;
  case INTEGER_I8:
  case INTEGER_U8: return 8;
  case INTEGER_I16:
  case INTEGER_U16: return 16;
  case INTEGER_I32:
  case INTEGER_U32: return 32;
  case INTEGER_I64:
  case INTEGER_U64: return 64;
  default: return 0;
  }
}

std::size_t LowType::storage_size() const
{
  if (kind == TYPE_OBJECT) return object_bytes;
  if (kind == TYPE_FLOAT) {
    if (float_kind == FLOAT_F32) return 4;
    if (float_kind == FLOAT_F64) return 8;
    if (float_kind == FLOAT_F80) return 16;
  }
  if (kind == TYPE_POINTER) return 8;
  if (kind == TYPE_INTEGER) {
    const int width = integer_width();
    return width <= 8 ? 1 : width <= 16 ? 2 : width <= 32 ? 4 : width == 64 ? 8 : 0;
  }
  return 0;
}

std::size_t LowType::storage_alignment() const
{
  if (kind == TYPE_OBJECT) return object_alignment;
  if (kind == TYPE_FLOAT && float_kind == FLOAT_F80) return 8;
  const std::size_t size = storage_size();
  return size >= 8 ? 8 : size;
}

bool operator==(const LowType &left, const LowType &right)
{
  if (left.kind != right.kind) return false;
  if (left.kind == LowType::TYPE_INTEGER) return left.integer_kind == right.integer_kind;
  if (left.kind == LowType::TYPE_FLOAT) return left.float_kind == right.float_kind;
  if (left.kind == LowType::TYPE_OBJECT)
    return left.object_bytes == right.object_bytes && left.object_alignment == right.object_alignment;
  return true;
}

bool operator!=(const LowType &left, const LowType &right) { return !(left == right); }

}  // namespace lowir_model
