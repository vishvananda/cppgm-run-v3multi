#include "pa15_lowering.h"

namespace pa11_semantic_internal
{

std::vector<std::string> Pa15Lowerer::function_abi_components(
	BindingId binding_id, ScopeId owner) const
{
	std::vector<std::string> result = value_components(owner,
		model_.binding(binding_id).name);
	const BindingSidecar* sidecar = model_.binding_sidecar(binding_id);
	if (sidecar != NULL && sidecar->operator_function_kind !=
		PA10OperatorFunctionKind::None)
	{
		// The PA14 encoder consumes the typed terminal and expects the
		// ordinary path to end at its operator owner component.
		if (result.empty())
			throw std::runtime_error("PA15 operator ABI path is empty");
		result.back() = "operator";
	}
	return result;
}

abi_mangle::AbiOperatorTerminalKind Pa15Lowerer::operator_terminal(
	BindingId binding_id, std::size_t parameter_count) const
{
	const BindingSidecar* sidecar = model_.binding_sidecar(binding_id);
	if (sidecar == NULL)
		return abi_mangle::ABI_OPERATOR_TERMINAL_NONE;
	switch (sidecar->operator_function_kind)
	{
	case PA10OperatorFunctionKind::New:
		return abi_mangle::ABI_OPERATOR_TERMINAL_NEW;
	case PA10OperatorFunctionKind::NewArray:
		return abi_mangle::ABI_OPERATOR_TERMINAL_NEW_ARRAY;
	case PA10OperatorFunctionKind::Delete:
		return abi_mangle::ABI_OPERATOR_TERMINAL_DELETE;
	case PA10OperatorFunctionKind::DeleteArray:
		return abi_mangle::ABI_OPERATOR_TERMINAL_DELETE_ARRAY;
	case PA10OperatorFunctionKind::Subscript:
		return abi_mangle::ABI_OPERATOR_TERMINAL_INDEX;
	case PA10OperatorFunctionKind::Call:
		return abi_mangle::ABI_OPERATOR_TERMINAL_CALL;
	case PA10OperatorFunctionKind::Token:
		switch (sidecar->operator_token)
		{
		case SimpleTokenType::OP_PLUS:
			return abi_mangle::ABI_OPERATOR_TERMINAL_PLUS;
		case SimpleTokenType::OP_MINUS:
			return abi_mangle::ABI_OPERATOR_TERMINAL_MINUS;
		case SimpleTokenType::OP_STAR:
			return parameter_count == 1 ?
				abi_mangle::ABI_OPERATOR_TERMINAL_DEREF :
				abi_mangle::ABI_OPERATOR_TERMINAL_MULTIPLY;
		case SimpleTokenType::OP_DIV:
			return abi_mangle::ABI_OPERATOR_TERMINAL_DIVIDE;
		case SimpleTokenType::OP_MOD:
			return abi_mangle::ABI_OPERATOR_TERMINAL_REMAINDER;
		case SimpleTokenType::OP_XOR:
			return abi_mangle::ABI_OPERATOR_TERMINAL_BIT_XOR;
		case SimpleTokenType::OP_AMP:
			return parameter_count == 1 ?
				abi_mangle::ABI_OPERATOR_TERMINAL_ADDRESS_OF :
				abi_mangle::ABI_OPERATOR_TERMINAL_BIT_AND;
		case SimpleTokenType::OP_BOR:
			return abi_mangle::ABI_OPERATOR_TERMINAL_BIT_OR;
		case SimpleTokenType::OP_COMPL:
			return abi_mangle::ABI_OPERATOR_TERMINAL_COMPLEMENT;
		case SimpleTokenType::OP_LNOT:
			return abi_mangle::ABI_OPERATOR_TERMINAL_LOGICAL_NOT;
		case SimpleTokenType::OP_ASS:
			return abi_mangle::ABI_OPERATOR_TERMINAL_ASSIGN;
		case SimpleTokenType::OP_PLUSASS:
			return abi_mangle::ABI_OPERATOR_TERMINAL_PLUS_ASSIGN;
		case SimpleTokenType::OP_MINUSASS:
			return abi_mangle::ABI_OPERATOR_TERMINAL_MINUS_ASSIGN;
		case SimpleTokenType::OP_STARASS:
			return abi_mangle::ABI_OPERATOR_TERMINAL_MULTIPLY_ASSIGN;
		case SimpleTokenType::OP_DIVASS:
			return abi_mangle::ABI_OPERATOR_TERMINAL_DIVIDE_ASSIGN;
		case SimpleTokenType::OP_MODASS:
			return abi_mangle::ABI_OPERATOR_TERMINAL_REMAINDER_ASSIGN;
		case SimpleTokenType::OP_XORASS:
			return abi_mangle::ABI_OPERATOR_TERMINAL_BIT_XOR_ASSIGN;
		case SimpleTokenType::OP_BANDASS:
			return abi_mangle::ABI_OPERATOR_TERMINAL_BIT_AND_ASSIGN;
		case SimpleTokenType::OP_BORASS:
			return abi_mangle::ABI_OPERATOR_TERMINAL_BIT_OR_ASSIGN;
		case SimpleTokenType::OP_LSHIFT:
			return abi_mangle::ABI_OPERATOR_TERMINAL_LEFT_SHIFT;
		case SimpleTokenType::OP_RSHIFT:
			return abi_mangle::ABI_OPERATOR_TERMINAL_RIGHT_SHIFT;
		case SimpleTokenType::OP_LSHIFTASS:
			return abi_mangle::ABI_OPERATOR_TERMINAL_LEFT_SHIFT_ASSIGN;
		case SimpleTokenType::OP_RSHIFTASS:
			return abi_mangle::ABI_OPERATOR_TERMINAL_RIGHT_SHIFT_ASSIGN;
		case SimpleTokenType::OP_EQ:
			return abi_mangle::ABI_OPERATOR_TERMINAL_EQUAL;
		case SimpleTokenType::OP_NE:
			return abi_mangle::ABI_OPERATOR_TERMINAL_NOT_EQUAL;
		case SimpleTokenType::OP_LT:
			return abi_mangle::ABI_OPERATOR_TERMINAL_LESS;
		case SimpleTokenType::OP_GT:
			return abi_mangle::ABI_OPERATOR_TERMINAL_GREATER;
		case SimpleTokenType::OP_LE:
			return abi_mangle::ABI_OPERATOR_TERMINAL_LESS_EQUAL;
		case SimpleTokenType::OP_GE:
			return abi_mangle::ABI_OPERATOR_TERMINAL_GREATER_EQUAL;
		case SimpleTokenType::OP_LAND:
			return abi_mangle::ABI_OPERATOR_TERMINAL_LOGICAL_AND;
		case SimpleTokenType::OP_LOR:
			return abi_mangle::ABI_OPERATOR_TERMINAL_LOGICAL_OR;
		case SimpleTokenType::OP_INC:
			return abi_mangle::ABI_OPERATOR_TERMINAL_INCREMENT;
		case SimpleTokenType::OP_DEC:
			return abi_mangle::ABI_OPERATOR_TERMINAL_DECREMENT;
		case SimpleTokenType::OP_COMMA:
			return abi_mangle::ABI_OPERATOR_TERMINAL_COMMA;
		case SimpleTokenType::OP_DOTSTAR:
		case SimpleTokenType::OP_ARROWSTAR:
			return abi_mangle::ABI_OPERATOR_TERMINAL_MEMBER_POINTER;
		case SimpleTokenType::OP_ARROW:
			return abi_mangle::ABI_OPERATOR_TERMINAL_ARROW;
		default:
			break;
		}
		throw std::runtime_error("PA15 unsupported operator ABI token");
	case PA10OperatorFunctionKind::Conversion:
	case PA10OperatorFunctionKind::Literal:
		throw std::runtime_error("PA15 unsupported typed operator ABI terminal");
	case PA10OperatorFunctionKind::None:
		return abi_mangle::ABI_OPERATOR_TERMINAL_NONE;
	}
	return abi_mangle::ABI_OPERATOR_TERMINAL_NONE;
}

} // namespace pa11_semantic_internal
