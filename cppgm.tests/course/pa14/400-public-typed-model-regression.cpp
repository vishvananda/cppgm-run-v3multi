#include "abi_mangle.h"

#include <exception>
#include <iostream>
#include <string>
#include <vector>

namespace {

using namespace abi_mangle;

AbiDefinitionId id(std::size_t index)
{
  AbiDefinitionId result;
  result.index = index;
  return result;
}

AbiType int_type()
{
  AbiType result;
  result.kind = ABI_TYPE_BUILTIN;
  result.builtin = ABI_BUILTIN_INT;
  return result;
}

AbiDefinitionRecord literal_definition(std::size_t index)
{
  AbiDefinitionRecord result;
  result.kind = ABI_DEFINITION_EXPRESSION;
  result.id = id(index);
  result.expression.kind = ABI_EXPRESSION_LITERAL;
  result.expression.value_type = int_type();
  return result;
}

AbiFactCase expression_argument_case(const AbiDependentExpression & expression,
                                     const std::vector<AbiDefinitionId> & refs,
                                     const std::string & label,
                                     std::size_t literal_count)
{
  AbiFactCase result;
  result.label = label;

  for(std::size_t i = 0; i < literal_count; ++i) {
    AbiFactRecord record;
    record.kind = ABI_FACT_RECORD_DEFINITION;
    record.definition = literal_definition(i);
    result.records.push_back(record);
  }

  AbiFactRecord expression_record;
  expression_record.kind = ABI_FACT_RECORD_DEFINITION;
  expression_record.definition.kind = ABI_DEFINITION_EXPRESSION;
  expression_record.definition.id = id(literal_count);
  expression_record.definition.expression = expression;
  expression_record.definition.expression.expression_refs = refs;
  result.records.push_back(expression_record);

  AbiFactRecord argument_record;
  argument_record.kind = ABI_FACT_RECORD_DEFINITION;
  argument_record.definition.kind = ABI_DEFINITION_TEMPLATE_ARGUMENT;
  argument_record.definition.id = id(literal_count + 1);
  argument_record.definition.template_argument.kind =
    ABI_TEMPLATE_ARGUMENT_EXPRESSION;
  argument_record.definition.template_argument.entity_ref = id(literal_count);
  result.records.push_back(argument_record);

  AbiFactRecord target_record;
  target_record.kind = ABI_FACT_RECORD_TARGET;
  target_record.target.kind = ABI_TARGET_FACT_TYPE;
  target_record.target.type.kind = ABI_TYPE_TEMPLATE_SPECIALIZATION;
  target_record.target.type.name.components.push_back("Holder");
  target_record.target.type.argument_refs.push_back(id(literal_count + 1));
  result.records.push_back(target_record);
  return result;
}

bool rejects(const AbiFactCase & fact_case, const std::string & expected)
{
  try {
    (void)mangle_abi_fact_case(fact_case);
  } catch(const std::exception & error) {
    return std::string(error.what()).find(expected) != std::string::npos;
  }
  return false;
}

AbiDependentExpression unary_with_binary_operator()
{
  AbiDependentExpression result;
  result.kind = ABI_EXPRESSION_UNARY;
  result.operator_kind = ABI_EXPRESSION_OPERATOR_PLUS;
  return result;
}

AbiDependentExpression binary_with_unary_operator()
{
  AbiDependentExpression result;
  result.kind = ABI_EXPRESSION_BINARY;
  result.operator_kind = ABI_EXPRESSION_OPERATOR_UNARY_PLUS;
  return result;
}

AbiDependentExpression placeholder_external_entity()
{
  AbiDependentExpression result;
  result.kind = ABI_EXPRESSION_EXTERNAL_ENTITY;
  result.symbol = "-";
  return result;
}

}  // namespace

int main()
{
  const AbiDefinitionId literal = id(0);
  const std::vector<AbiDefinitionId> unary_refs(1, literal);
  if(!rejects(expression_argument_case(unary_with_binary_operator(), unary_refs,
                                       "unary-shape", 1),
              "unary ABI expression has invalid operator or arity")) {
    std::cerr << "typed unary/binary operator-shape case was accepted\n";
    return 1;
  }

  const std::vector<AbiDefinitionId> binary_refs(2, literal);
  if(!rejects(expression_argument_case(binary_with_unary_operator(), binary_refs,
                                       "binary-shape", 1),
              "binary ABI expression has invalid operator or arity")) {
    std::cerr << "typed binary/unary operator-shape case was accepted\n";
    return 1;
  }

  if(!rejects(expression_argument_case(placeholder_external_entity(),
                                       std::vector<AbiDefinitionId>(),
                                       "external-placeholder", 0),
              "external entity expression has no symbol")) {
    std::cerr << "typed placeholder external-symbol case was accepted\n";
    return 1;
  }

  return 0;
}
