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

bool rejects_serialization(const AbiFactFile & fact_file,
                           const std::string & expected)
{
  try {
    (void)serialize_fact_file(fact_file);
  } catch(const std::exception & error) {
    return std::string(error.what()).find(expected) != std::string::npos;
  }
  return false;
}

AbiFactFile parse_template_boundary_case()
{
  return parse_fact_text(
    "case template-boundary\n"
    "let-type T template-param 0\n"
    "let-arg T_arg type T\n"
    "let-type AllocT template-param-template 1 T_arg\n"
    "function path ns::use\n"
    "param ref AllocT\n"
    "param ref AllocT\n");
}

AbiFactFile parse_substitution_boundary_case()
{
  return parse_fact_text(
    "case substitution-boundary\n"
    "let-context Host function path host int\n"
    "let-type LocalLambda local-type Host $_0 0\n"
    "let-arg Operation type LocalLambda\n"
    "function encoding\n"
    "name-source apply\n"
    "function-template-prefix apply\n"
    "function-template-arg Operation\n"
    "result int\n"
    "let-type OperationParam template-param-subst 0\n"
    "param OperationParam\n");
}

bool has_template_parameter_specialization(AbiFactCase & fact_case,
                                            AbiType ** result)
{
  for(std::vector<AbiFactRecord>::iterator it = fact_case.records.begin();
      it != fact_case.records.end(); ++it) {
    if(it->kind == ABI_FACT_RECORD_DEFINITION &&
       it->definition.kind == ABI_DEFINITION_TYPE &&
       it->definition.type.kind == ABI_TYPE_TEMPLATE_PARAMETER_SPECIALIZATION) {
      *result = &it->definition.type;
      return true;
    }
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
  const std::string template_mangle = "_ZN2ns3useERT0_IT_ES1_";
  const AbiFactFile template_file = parse_template_boundary_case();
  if(template_file.cases.size() != 1 ||
     mangle_abi_fact_case(template_file.cases[0]) != template_mangle) {
    std::cerr << "typed template-parameter specialization happy path changed\n";
    return 1;
  }
  const std::string template_serialized = serialize_fact_file(template_file);
  if(serialize_fact_file(parse_fact_text(template_serialized)) !=
       template_serialized ||
     mangle_abi_fact_case(parse_fact_text(template_serialized).cases[0]) !=
       template_mangle) {
    std::cerr << "template-param-template parse/serialize/parse is not stable\n";
    return 1;
  }

  AbiFactFile empty_template_file = template_file;
  AbiType * empty_template = NULL;
  if(!has_template_parameter_specialization(empty_template_file.cases[0],
                                            &empty_template)) {
    std::cerr << "typed template-parameter specialization was not found\n";
    return 1;
  }
  empty_template->argument_refs.clear();
  if(!rejects(empty_template_file.cases[0],
              "template-template specialization has no arguments") ||
     !rejects_serialization(empty_template_file,
                            "template-parameter specialization has no arguments")) {
    std::cerr << "empty template-parameter specialization was accepted\n";
    return 1;
  }

  AbiFactFile invalid_kind_file = template_file;
  invalid_kind_file.cases[0].records[0].definition.type
    .template_parameter_reference_kind =
      static_cast<AbiTemplateParameterReferenceKind>(99);
  if(!rejects(invalid_kind_file.cases[0],
              "invalid ABI template parameter reference kind") ||
     !rejects_serialization(invalid_kind_file,
                            "invalid ABI template parameter reference kind")) {
    std::cerr << "invalid template-parameter reference kind was accepted\n";
    return 1;
  }

  const std::string substitution_mangle = "_Z5applyIZ4hostiE3$_0EiT_";
  const AbiFactFile substitution_file = parse_substitution_boundary_case();
  if(substitution_file.cases.size() != 1 ||
     mangle_abi_fact_case(substitution_file.cases[0]) != substitution_mangle) {
    std::cerr << "typed template-param-subst happy path changed\n";
    return 1;
  }
  const std::string substitution_serialized =
    serialize_fact_file(substitution_file);
  if(serialize_fact_file(parse_fact_text(substitution_serialized)) !=
       substitution_serialized ||
     mangle_abi_fact_case(parse_fact_text(substitution_serialized).cases[0]) !=
       substitution_mangle) {
    std::cerr << "template-param-subst parse/serialize/parse is not stable\n";
    return 1;
  }

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
