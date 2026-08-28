#include "pa11_semantic_model.h"

namespace pa11_semantic_internal
{

void PA11SemanticModel::initialize_builtin_names()
{
	builtin_constant_p_name_ = intern_name("__builtin_constant_p");
	builtin_abort_name_ = intern_name("__builtin_abort");
	builtin_strlen_name_ = intern_name("__builtin_strlen");
	builtin_unreachable_name_ = intern_name("__builtin_unreachable");
	builtin_memcpy_name_ = intern_name("__builtin_memcpy");
	builtin_memmove_name_ = intern_name("__builtin_memmove");
}

BuiltinKind PA11SemanticModel::builtin_kind(const PA10AstNode& node)
{
	if (node.kind != PA10NodeKind::IdExpression || node.has_token ||
		node.global_name || node.name_prefix_count != 0)
		return BuiltinKind::None;
	const NamePath path = name_path(node);
	if (path.components.size() != 1)
		return BuiltinKind::None;
	if (path.last() == builtin_constant_p_name_)
		return BuiltinKind::ConstantP;
	if (path.last() == builtin_abort_name_)
		return BuiltinKind::Abort;
	if (path.last() == builtin_strlen_name_)
		return BuiltinKind::Strlen;
	if (path.last() == builtin_unreachable_name_)
		return BuiltinKind::Unreachable;
	if (path.last() == builtin_memcpy_name_)
		return BuiltinKind::Memcpy;
	if (path.last() == builtin_memmove_name_)
		return BuiltinKind::Memmove;
	return BuiltinKind::None;
}

const BuiltinFunctionFact* PA11SemanticModel::builtin_function_fact(
	BuiltinKind kind) const
{
	for (std::size_t i = 0; i < builtin_function_facts_.size(); ++i)
		if (builtin_function_facts_[i].kind == kind)
			return &builtin_function_facts_[i];
	return NULL;
}

const BuiltinFunctionFact* PA11SemanticModel::builtin_function_fact(
	BindingId binding_id) const
{
	if (!binding_id.valid())
		return NULL;
	for (std::size_t i = 0; i < builtin_function_facts_.size(); ++i)
		if (builtin_function_facts_[i].binding == binding_id)
			return &builtin_function_facts_[i];
	return NULL;
}

BindingId PA11SemanticModel::builtin_binding(BuiltinKind kind)
{
	if (kind == BuiltinKind::Abort)
	{
		if (builtin_abort_binding_.valid())
			return builtin_abort_binding_;
		const TypeId function_type = make_function(std::vector<TypeId>(), false,
			fundamental(FundamentalType::Void));
		if (!global_.valid() || global_.value >= scopes_.size())
			throw std::runtime_error("builtin binding has no owner scope");
		const BindingId result(bindings_.size());
		bindings_.push_back(Binding(BindingKind::Function, builtin_abort_name_,
			function_type));
		binding_owners_.push_back(global_);
		builtin_abort_binding_ = result;
		return result;
	}
	if (kind != BuiltinKind::Strlen && kind != BuiltinKind::Unreachable &&
		kind != BuiltinKind::Memcpy && kind != BuiltinKind::Memmove)
		return BindingId();
	const BuiltinFunctionFact* existing = builtin_function_fact(kind);
	if (existing != NULL)
		return existing->binding;
	if (!global_.valid() || global_.value >= scopes_.size())
		throw std::runtime_error("builtin binding has no owner scope");

	NameId name;
	NameId object_symbol;
	std::vector<TypeId> parameters;
	TypeId result_type;
	BuiltinFunctionFact facts(kind, BindingId(), NameId());
	const TypeId void_type = fundamental(FundamentalType::Void);
	const TypeId const_void_type = make_cv(void_type, 1u);
	const TypeId void_pointer = make_pointer(void_type);
	const TypeId const_void_pointer = make_pointer(const_void_type);
	const TypeId size_type = fundamental(FundamentalType::UnsignedLongInt);
	switch (kind)
	{
	case BuiltinKind::Strlen:
		name = builtin_strlen_name_;
		object_symbol = intern_name("cppgm_builtin_strlen");
		parameters.push_back(make_pointer(make_cv(
			fundamental(FundamentalType::Char), 1u)));
		result_type = size_type;
		facts.effects = BuiltinCallEffects::ReadOnly;
		facts.unwind = BuiltinCallUnwind::No;
		facts.parameters.push_back(BuiltinParameterFact(
			BuiltinParameterCapture::NoCapture,
			BuiltinParameterAccess::Read));
		break;
	case BuiltinKind::Unreachable:
		name = builtin_unreachable_name_;
		object_symbol = intern_name("cppgm_builtin_unreachable");
		result_type = void_type;
		facts.effects = BuiltinCallEffects::ReadNone;
		facts.unwind = BuiltinCallUnwind::No;
		facts.returns = BuiltinCallReturn::NoReturn;
		break;
	case BuiltinKind::Memcpy:
		name = builtin_memcpy_name_;
		object_symbol = intern_name("cppgm_builtin_memcpy");
		parameters.push_back(void_pointer);
		parameters.push_back(const_void_pointer);
		parameters.push_back(size_type);
		result_type = void_pointer;
		facts.effects = BuiltinCallEffects::ReadWrite;
		facts.unwind = BuiltinCallUnwind::No;
		facts.parameters.push_back(BuiltinParameterFact(
			BuiltinParameterCapture::NoCapture,
			BuiltinParameterAccess::Write,
			BuiltinParameterAlias::NoAlias));
		facts.parameters.push_back(BuiltinParameterFact(
			BuiltinParameterCapture::NoCapture,
			BuiltinParameterAccess::Read,
			BuiltinParameterAlias::NoAlias));
		facts.parameters.push_back(BuiltinParameterFact());
		break;
	case BuiltinKind::Memmove:
		name = builtin_memmove_name_;
		object_symbol = intern_name("cppgm_builtin_memmove");
		parameters.push_back(void_pointer);
		parameters.push_back(const_void_pointer);
		parameters.push_back(size_type);
		result_type = void_pointer;
		facts.effects = BuiltinCallEffects::ReadWrite;
		facts.unwind = BuiltinCallUnwind::No;
		facts.parameters.push_back(BuiltinParameterFact(
			BuiltinParameterCapture::NoCapture,
			BuiltinParameterAccess::ReadWrite));
		facts.parameters.push_back(BuiltinParameterFact(
			BuiltinParameterCapture::NoCapture,
			BuiltinParameterAccess::Read));
		facts.parameters.push_back(BuiltinParameterFact());
		break;
	default:
		throw std::runtime_error("invalid typed builtin kind");
	}
	facts.object_symbol = object_symbol;
	const TypeId function_type = make_function(parameters, false, result_type);
	const BindingId result(bindings_.size());
	bindings_.push_back(Binding(BindingKind::Function, name, function_type));
	binding_owners_.push_back(global_);
	facts.binding = result;
	builtin_function_facts_.push_back(facts);
	return result;
}

TypeId PA11SemanticModel::builtin_expression_type(BuiltinKind builtin,
	std::size_t argument_count)
{
	if (builtin == BuiltinKind::ConstantP)
	{
		if (argument_count != 1)
			throw std::runtime_error("invalid __builtin_constant_p arity");
		return fundamental(FundamentalType::Int);
	}
	if (builtin == BuiltinKind::Abort)
	{
		if (argument_count != 0)
			throw std::runtime_error("invalid __builtin_abort arity");
		return fundamental(FundamentalType::Void);
	}
	if (builtin == BuiltinKind::None)
		throw std::runtime_error("invalid builtin expression type");
	const BindingId binding_id = builtin_binding(builtin);
	if (!binding_id.valid() || binding_id.value >= bindings_.size() ||
		type_kind(binding(binding_id).type) != TypeKind::Function)
		throw std::runtime_error("invalid typed builtin binding");
	const TypeKey& type = types_[binding(binding_id).type.value];
	if (argument_count != type.parameters.size())
		throw std::runtime_error("invalid typed builtin arity");
	return type.result;
}

ExprInfo PA11SemanticModel::semantic_builtin_call(const PA10AstNode& node,
	ScopeId scope, BuiltinKind builtin, const PA10AstNode& argument_node)
{
	if (builtin == BuiltinKind::ConstantP)
	{
		if (argument_node.children.size() != 1)
			throw std::runtime_error("PA12 invalid __builtin_constant_p arity");
		const PA10AstNode& operand_node = argument_node.children.front();
		SemanticTailGuard operand_tail(*this);
		const ExprInfo operand = semantic_expression(operand_node, scope);
		const TypeId operand_type = operand.type;
		operand_tail.discard();
		const bool integral_operand = integral_id(operand_type);
		bool constant = false;
		if (integral_operand)
		{
			// Semantic validation is complete.  Only a typed fold failure is a
			// nonconstant result; malformed or invalid model state must escape.
			try
			{
				constant = eval_constexpr(operand_node, scope).valid;
			}
			catch (const NonConstantExpression&)
			{
				constant = false;
			}
		}
		SemanticFact fact(SemanticFactKind::Literal,
			fundamental(FundamentalType::Int), SemanticValueCategory::Prvalue,
			&node);
		fact.has_literal_value = true;
		fact.literal_value = constant ? 1 : 0;
		const SemanticFactId result = make_semantic_fact(fact);
		return ExprInfo(result, fact.type, SemanticValueCategory::Prvalue, !constant);
	}
	if (builtin == BuiltinKind::Abort)
	{
		if (!argument_node.children.empty())
			throw std::runtime_error("PA12 invalid __builtin_abort arity");
		SemanticFact fact(SemanticFactKind::CallExpression,
			fundamental(FundamentalType::Void), SemanticValueCategory::Prvalue,
			&node);
		fact.has_callee = true;
		fact.selected_binding = builtin_binding(builtin);
		fact.selected_scope = global_;
		const SemanticFactId result = make_semantic_fact(fact);
		set_semantic_children(result, std::vector<SemanticFactId>());
		return ExprInfo(result, fact.type, SemanticValueCategory::Prvalue, false);
	}
	if (builtin != BuiltinKind::Strlen &&
		builtin != BuiltinKind::Unreachable && builtin != BuiltinKind::Memcpy &&
		builtin != BuiltinKind::Memmove)
		throw std::runtime_error("PA12 invalid typed builtin kind");
	const BindingId builtin_id = builtin_binding(builtin);
	if (!builtin_id.valid())
		throw std::runtime_error("PA12 typed builtin binding is missing");
	std::vector<const PA10AstNode*> argument_nodes;
	std::vector<ExprInfo> arguments;
	argument_nodes.reserve(argument_node.children.size());
	arguments.reserve(argument_node.children.size());
	for (std::size_t i = 0; i < argument_node.children.size(); ++i)
	{
		argument_nodes.push_back(&argument_node.children[i]);
		if (target_function_id(argument_node.children[i], scope) != NULL)
			arguments.push_back(ExprInfo());
		else
			arguments.push_back(semantic_expression(argument_node.children[i],
				scope));
	}
	const std::vector<ValueRef> candidates(1, ValueRef(global_, builtin_id));
	const TypedFunctionSelection selection = select_typed_function(
		candidates, argument_nodes, arguments, scope);
	if (!selection.valid() || selection.selected.binding != builtin_id)
		throw std::runtime_error("PA12 typed builtin selection is invalid");
	const TypeId result_type = function_result_type(selection.type);
	SemanticValueCategory result_category = SemanticValueCategory::Prvalue;
	if (type_kind(result_type) == TypeKind::LvalueReference)
		result_category = SemanticValueCategory::Lvalue;
	else if (type_kind(result_type) == TypeKind::RvalueReference)
		result_category = SemanticValueCategory::Xvalue;
	SemanticFact call(SemanticFactKind::CallExpression, result_type,
		result_category, &node);
	call.has_callee = true;
	call.bool_context_operand = bool_id(result_type);
	call.direct_bool_boundary = bool_id(result_type);
	call.selected_binding = builtin_id;
	call.selected_scope = global_;
	call.callable_type = selection.type;
	const SemanticFactId result = make_semantic_fact(call);
	std::vector<SemanticFactId> children;
	for (std::size_t i = 0; i < selection.arguments.size(); ++i)
		children.push_back(selection.arguments[i].fact);
	set_semantic_children(result, children);
	return ExprInfo(result, result_type, result_category, false);
}

}
