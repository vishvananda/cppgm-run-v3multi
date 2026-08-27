#include "pa11_semantic.h"
#include "pa11_semantic_model.h"

#include <cstring>

namespace pa11_semantic_internal
{
using namespace pa11_semantic_storage;

namespace
{

bool address_addend_fits(__int128 value, long long* result)
{
	if (value < static_cast<__int128>(std::numeric_limits<long long>::min()) ||
		value > static_cast<__int128>(std::numeric_limits<long long>::max()))
		return false;
	*result = static_cast<long long>(value);
	return true;
}

const PA10AstNode* default_argument_expression(const PA10AstNode& parameter)
{
	for (std::size_t i = 0; i < parameter.children.size(); ++i)
	{
		const PA10AstNode& child = parameter.children[i];
		if (child.kind != PA10NodeKind::DefaultArgument)
			continue;
		if (child.children.size() != 1)
			return NULL;
		const PA10AstNode* initializer = &child.children.front();
		if (initializer->kind == PA10NodeKind::Initializer ||
			initializer->kind == PA10NodeKind::ParenInitializer)
		{
			if (initializer->children.size() != 1)
				return NULL;
			initializer = &initializer->children.front();
		}
		return initializer;
	}
	return NULL;
}

const PA10AstNode* function_declarator(const PA10AstNode& declaration,
	std::size_t declarator_index)
{
	if (declaration.kind == PA10NodeKind::FunctionDefinition)
	{
		if (declarator_index != 0 || declaration.children.size() < 2)
			return NULL;
		return &declaration.children[1];
	}
	if (declaration.kind == PA10NodeKind::SpecialMemberDeclaration ||
		declaration.kind == PA10NodeKind::SpecialMemberDefinition)
	{
		if (declarator_index != 0)
			return NULL;
		for (std::size_t i = 0; i < declaration.children.size(); ++i)
			if (declaration.children[i].kind == PA10NodeKind::Declarator)
				return &declaration.children[i];
		return NULL;
	}
	if (declaration.kind != PA10NodeKind::SimpleDeclaration ||
		declaration.children.size() != 2 ||
		declaration.children[1].kind != PA10NodeKind::InitDeclaratorList)
		return NULL;
	const PA10AstNode& list = declaration.children[1];
	if (declarator_index >= list.children.size())
		return NULL;
	const PA10AstNode& init = list.children[declarator_index];
	if (init.kind != PA10NodeKind::InitDeclarator || init.children.empty())
		return NULL;
	return &init.children.front();
}

bool parameter_has_default_argument(const PA10AstNode& parameter)
{
	for (std::size_t i = 0; i < parameter.children.size(); ++i)
		if (parameter.children[i].kind == PA10NodeKind::DefaultArgument)
			return true;
	return false;
}

template <typename T>
void zero_floating_literal(LiteralData* literal)
{
	const T value = T();
	literal->bytes.resize(sizeof(value));
	std::memcpy(literal->bytes.data(), &value, sizeof(value));
}

}

BindingId PA11SemanticModel::ensure_implicit_default_constructor(
	NamedRecordId record_id)
{
	if (!record_id.valid() || record_id.value >= named_.size() ||
		named_[record_id.value].kind != NamedKind::Class)
		throw std::runtime_error("invalid implicit constructor record");
	const BindingId declared = default_constructor_binding(record_id);
	if (declared.valid())
		return declared;
	if (has_constructor_declaration(record_id))
		throw std::runtime_error("PA12 implicit default constructor is unavailable");
	const NamedRecordSidecar* existing = named_record_sidecar(record_id);
	if (existing != NULL && existing->constructor_binding.valid())
		return existing->constructor_binding;
	const TypeId object = named_type(record_id);
	const bool union_constructor = named_[record_id.value].class_tag ==
		ClassTag::Union;
	if (!named_[record_id.value].scope.valid() ||
		named_[record_id.value].scope.value >= scopes_.size())
		throw std::runtime_error("implicit constructor has no owner scope");
	const ScopeId owner = named_[record_id.value].scope;
	const NameId name = named_[record_id.value].name;
	const TypeId constructor_type = make_function(std::vector<TypeId>(), false,
		fundamental(FundamentalType::Void));
	BindingId binding_id;
	if (name.valid() && !union_constructor)
		binding_id = add_value(owner, name, constructor_type, true, true, false,
			BindingId(), SourcePoint(), false, LanguageLinkage::Cxx);
	else
	{
		// Keep the pre-existing anonymous/unnamed representation isolated from
		// named-class constructor lookup; it still uses its legacy generated
		// binding until its own PA16 boundary is reached.
		Binding constructor(BindingKind::Function, NameId(),
			make_function(std::vector<TypeId>(1, make_pointer(object)), false,
				fundamental(FundamentalType::Void)));
		binding_id = store_binding(owner, constructor);
	}
	NamedRecordSidecar record_sidecar;
	if (existing != NULL)
		record_sidecar = *existing;
	record_sidecar.constructor_binding = binding_id;
	if (name.valid() && !union_constructor)
		record_sidecar.default_constructor_binding = binding_id;
	set_named_record_sidecar(record_id, record_sidecar);
	BindingSidecar binding_sidecar;
	binding_sidecar.constructor_record = record_id;
	set_binding_sidecar(binding_id, binding_sidecar);
	if (name.valid() && !union_constructor)
	{
		const ScopeId function_scope = create_scope(ScopeKind::Function, owner,
			name);
		FunctionFact function(NULL, owner, binding_id, function_scope,
			ScopeId());
		function.is_constructor = true;
		function.synthetic = true;
		function.constructor_record = record_id;
		prepare_pa12_member_parameter(function);
		const FunctionFactId function_id(function_facts_.size());
		function_facts_.push_back(function);
		function_binding_fact_index_.set(binding_id, function_id);
		build_constructor_actions(function_id);
		synthetic_function_facts_.push_back(
			SyntheticFunctionFact(record_id, binding_id));
	}
	else
		synthetic_function_facts_.push_back(
			SyntheticFunctionFact(record_id, binding_id));
	return binding_id;
}

BindingId PA11SemanticModel::ensure_implicit_destructor(NamedRecordId record_id)
{
	if (!record_id.valid() || record_id.value >= named_.size() ||
		named_[record_id.value].kind != NamedKind::Class ||
		named_[record_id.value].class_tag == ClassTag::Union)
		throw std::runtime_error("invalid implicit destructor record");
	const BindingId declared = destructor_binding(record_id);
	if (declared.valid())
		return declared;
	const NamedRecordSidecar* existing = named_record_sidecar(record_id);
	if (existing != NULL && existing->destructor_binding.valid())
		return existing->destructor_binding;
	const NamedRecord& record = named_[record_id.value];
	if (!record.scope.valid() || record.scope.value >= scopes_.size() ||
		!record.name.valid())
		throw std::runtime_error("implicit destructor has no owner");
	const TypeId destructor_type = make_function(std::vector<TypeId>(), false,
		fundamental(FundamentalType::Void));
	const BindingId binding_id = store_binding(record.scope,
		Binding(BindingKind::Function, record.name, destructor_type));
	NamedRecordSidecar record_sidecar;
	if (existing != NULL)
		record_sidecar = *existing;
	record_sidecar.destructor_binding = binding_id;
	set_named_record_sidecar(record_id, record_sidecar);
	BindingSidecar binding_sidecar;
	binding_sidecar.destructor_record = record_id;
	set_binding_sidecar(binding_id, binding_sidecar);
	const ScopeId function_scope = create_scope(ScopeKind::Function,
		record.scope, record.name);
	FunctionFact function(NULL, record.scope, binding_id, function_scope,
		ScopeId());
	function.is_destructor = true;
	function.synthetic = true;
	function.destructor_record = record_id;
	prepare_pa12_member_parameter(function);
	const FunctionFactId function_id(function_facts_.size());
	function_facts_.push_back(function);
	function_binding_fact_index_.set(binding_id, function_id);
	build_destructor_actions(function_id);
	return binding_id;
}

void PA11SemanticModel::build_destructor_actions(FunctionFactId function_id)
{
	if (!function_id.valid() || function_id.value >= function_facts_.size())
		throw std::runtime_error("PA16 destructor fact identity is invalid");
	const FunctionFact initial_function = function_facts_[function_id.value];
	if (!initial_function.is_destructor ||
		!initial_function.destructor_record.valid() ||
		initial_function.destructor_record.value >= named_.size() ||
	!initial_function.function_scope.valid() ||
	initial_function.function_scope.value >= scopes_.size() ||
	scopes_[initial_function.function_scope.value].kind != ScopeKind::Function ||
	scopes_[initial_function.function_scope.value].parent != initial_function.owner)
		throw std::runtime_error("PA16 destructor function identity is invalid");
	if (initial_function.destructor_action_begin != InvalidIdentityValue)
		return;
	const NamedRecordId record_id = initial_function.destructor_record;
	const NamedRecord& record = named_[record_id.value];
	if (record.kind != NamedKind::Class || !record.scope.valid() ||
		record.scope.value >= scopes_.size() ||
		scopes_[record.scope.value].kind != ScopeKind::Class ||
		scopes_[record.scope.value].record != record_id ||
		initial_function.owner != record.scope)
		throw std::runtime_error("PA16 destructor action owner is invalid");
	if (record.direct_base_virtual || (!record.has_base && record.direct_base.valid()))
		throw std::runtime_error("PA16 virtual base destruction is outside checkpoint");
	if (record.has_base && (!record.direct_base.valid() ||
		record.direct_base.value >= named_.size() ||
		named_[record.direct_base.value].kind != NamedKind::Class ||
		named_[record.direct_base.value].class_tag == ClassTag::Union))
		throw std::runtime_error("PA16 destructor base metadata is invalid");

	// The class binding list is copied before any child destructor demand can
	// publish a new function binding into another class arena.
	const std::vector<BindingId> class_members =
		scopes_[record.scope.value].bindings;
	std::vector<DestructorActionFact> actions;
	for (std::size_t position = class_members.size(); position != 0; --position)
	{
		const BindingId member_id = class_members[position - 1];
		if (!member_id.valid() || member_id.value >= bindings_.size() ||
			member_id.value >= binding_owners_.size() ||
			binding_owners_[member_id.value] != record.scope)
			throw std::runtime_error("PA16 destructor member identity is invalid");
		const Binding& member = binding(member_id);
		if (member.kind != BindingKind::Variable || is_static_member(member_id))
			continue;
		const NamedRecordId member_record = class_record_for_object_type(member.type);
		if (!member_record.valid() || !destructor_requires_runtime(member_record))
			continue;
		const BindingId member_destructor = ensure_implicit_destructor(member_record);
		if (!member_destructor.valid())
			throw std::runtime_error("PA16 member destructor is missing");
		actions.push_back(DestructorActionFact(
			ConstructorActionTarget::Member, NamedRecordId(), member_id,
			member_destructor, member.type));
	}
	if (record.has_base && destructor_requires_runtime(record.direct_base))
	{
		const BindingId base_destructor = ensure_implicit_destructor(record.direct_base);
		if (!base_destructor.valid())
			throw std::runtime_error("PA16 base destructor is missing");
		actions.push_back(DestructorActionFact(
			ConstructorActionTarget::Base, record.direct_base, BindingId(),
			base_destructor, named_type(record.direct_base)));
	}
	const std::size_t action_begin = destructor_actions_.size();
	destructor_actions_.insert(destructor_actions_.end(), actions.begin(), actions.end());
	FunctionFact& function = function_facts_[function_id.value];
	function.destructor_action_begin = action_begin;
	function.destructor_action_count = actions.size();
}

void PA11SemanticModel::record_automatic_lifetime(BindingId object,
	TypeId object_type, ScopeId scope)
{
	if (!object.valid() || object.value >= bindings_.size() ||
		object.value >= binding_owners_.size() || binding_owners_[object.value] != scope ||
		binding(object).kind != BindingKind::Variable)
		throw std::runtime_error("PA16 automatic lifetime object is invalid");
	const NamedRecordId record = class_record_for_object_type(object_type);
	if (!record.valid() || !destructor_requires_runtime(record))
		return;
	const BindingId destructor = ensure_implicit_destructor(record);
	if (!destructor.valid())
		throw std::runtime_error("PA16 automatic lifetime destructor is missing");
	lifetime_facts_.push_back(LifetimeFact(object, object_type, destructor, scope));
}

TypeId PA11SemanticModel::constructor_callable_type(BindingId constructor)
{
	if (!constructor.valid() || constructor.value >= bindings_.size())
		throw std::runtime_error("PA12 constructor binding is invalid");
	const Binding& value = binding(constructor);
	if (value.kind != BindingKind::Function ||
		type_kind(value.type) != TypeKind::Function)
		throw std::runtime_error("PA12 constructor binding is not a function");
	const BindingSidecar* sidecar = binding_sidecar(constructor);
	if (sidecar == NULL || !sidecar->constructor_record.valid() ||
		sidecar->constructor_record.value >= named_.size())
		throw std::runtime_error("PA12 constructor owner is missing");
	const NamedRecord& record = named_[sidecar->constructor_record.value];
	if (record.kind != NamedKind::Class || !record.scope.valid() ||
		record.scope.value >= scopes_.size() ||
		scopes_[record.scope.value].kind != ScopeKind::Class ||
		scopes_[record.scope.value].record != sidecar->constructor_record)
		throw std::runtime_error("PA12 constructor owner is invalid");
	const TypeKey function = types_[value.type.value];
	// Named-class constructor bindings contain only their explicit parameters.
	// The one legacy exception is the generated anonymous-union binding, whose
	// raw type already carries its hidden destination.  Do not infer the
	// representation from the first explicit parameter: a perfectly ordinary
	// constructor may itself take a pointer to its class.
	if (record.class_tag == ClassTag::Union && function.parameters.size() == 1)
	{
		const TypeId first = strip_cv_type(expression_object_type(
			function.parameters.front()));
		const TypeId object = named_type(sidecar->constructor_record);
		if (first.valid() && first.value < types_.size() &&
			type_kind(first) == TypeKind::Pointer &&
			types_[first.value].child == object)
			return value.type;
	}
	std::vector<TypeId> parameters;
	parameters.push_back(make_pointer(named_type(sidecar->constructor_record)));
	parameters.insert(parameters.end(), function.parameters.begin(),
		function.parameters.end());
	return make_function(parameters, function.variadic, function.result);
}
BindingId PA11SemanticModel::ensure_anonymous_union_constructor(
	NamedRecordId record_id)
{
	if (!record_id.valid() || record_id.value >= named_.size() ||
		named_[record_id.value].kind != NamedKind::Class ||
		named_[record_id.value].class_tag != ClassTag::Union)
		throw std::runtime_error("invalid anonymous union constructor record");
	return ensure_implicit_default_constructor(record_id);
}
const AnonymousUnionFact* PA11SemanticModel::anonymous_union_fact(
	const PA10AstNode& node) const
{
	return anonymous_union_fact_index_.find(&node);
}

ExprInfo PA11SemanticModel::semantic_empty_braced_init_list(
	const PA10AstNode& node, TypeId target)
{
	const TypeId object = strip_top_cv_type(target);
	if ((!integral_id(object) && !pointer_id(object) &&
		!bool_id(object) && !floating_id(object)) || void_id(object))
		throw std::runtime_error("PA12 empty braced initializer needs scalar target");
	SemanticFact fact(SemanticFactKind::Literal, object,
		SemanticValueCategory::Prvalue, &node);
	const bool integer_zero = !floating_id(object);
	fact.has_literal_value = integer_zero;
	fact.literal_value = 0;
	fact.literal_value_unsigned = unsigned_integral_type(object);
	if (!integer_zero)
	{
		FundamentalType fundamental_type;
		if (!fundamental_of(object, &fundamental_type))
			throw std::runtime_error("PA12 empty floating initializer has no type");
		LiteralData zero;
		zero.type = fundamental_type;
		if (fundamental_type == FundamentalType::Float)
			zero_floating_literal<float>(&zero);
		else if (fundamental_type == FundamentalType::Double)
			zero_floating_literal<double>(&zero);
		else
			zero_floating_literal<long double>(&zero);
		fact.literal_float = add_floating_literal(zero);
	}
	return ExprInfo(make_semantic_fact(fact), object,
		SemanticValueCategory::Prvalue, integer_zero);
}

bool PA11SemanticModel::unsigned_integral_type(TypeId type) const
{
	type = strip_cv_type(expression_object_type(type));
	const NamedRecordId record = named_record_for_type(type);
	if (record.valid() && record.value < named_.size() &&
		named_[record.value].kind == NamedKind::Enum)
	{
		if (!named_[record.value].has_underlying)
			return false;
		return unsigned_integral_type(named_[record.value].underlying);
	}
	FundamentalType fundamental_type;
	return fundamental_of(type, &fundamental_type) &&
		integral_type(fundamental_type) && unsigned_type(fundamental_type);
}

bool PA11SemanticModel::enumeration_id(TypeId type) const
{
	const NamedRecordId record = named_record_for_type(type);
	return record.valid() && record.value < named_.size() &&
		named_[record.value].kind == NamedKind::Enum;
}

TypeId PA11SemanticModel::promote_integral_type(TypeId type) const
{
	type = strip_cv_type(expression_object_type(type));
	const NamedRecordId record = named_record_for_type(type);
	if (record.valid() && record.value < named_.size() &&
		named_[record.value].kind == NamedKind::Enum &&
		!named_[record.value].scoped_enum)
	{
		return promote_integral_type(named_[record.value].has_underlying ?
			strip_cv_type(expression_object_type(named_[record.value].underlying)) :
			fundamental(FundamentalType::Int));
	}
	FundamentalType fundamental_type;
	if (!fundamental_of(type, &fundamental_type))
		return type;
	switch (fundamental_type)
	{
	case FundamentalType::Bool:
	case FundamentalType::SignedChar:
	case FundamentalType::UnsignedChar:
	case FundamentalType::ShortInt:
	case FundamentalType::UnsignedShortInt:
	case FundamentalType::Char:
		return fundamental(FundamentalType::Int);
	case FundamentalType::Char16T:
	case FundamentalType::WcharT:
		return fundamental(FundamentalType::Int);
	case FundamentalType::Char32T:
		return fundamental(type_size(type) <
			type_size(fundamental(FundamentalType::Int)) ?
			FundamentalType::Int : FundamentalType::UnsignedInt);
	default:
		return type;
	}
}

const FunctionFact* PA11SemanticModel::function_fact_for_binding(
	BindingId binding_id) const
{
	const FunctionFactId* found = function_binding_fact_index_.find(binding_id);
	if (found == NULL || !found->valid() || found->value >= function_facts_.size())
		return NULL;
	return &function_facts_[found->value];
}
FunctionFact* PA11SemanticModel::function_fact_for_binding(
	BindingId binding_id)
{
	return const_cast<FunctionFact*>(
		static_cast<const PA11SemanticModel*>(this)->function_fact_for_binding(
			binding_id));
}
SemanticFactId PA11SemanticModel::function_default_argument(
	BindingId binding_id, std::size_t parameter) const
{
	const FunctionFact* function = function_fact_for_binding(binding_id);
	if (function == NULL || parameter >= function->default_argument_count ||
		function->default_argument_begin == InvalidIdentityValue)
		return SemanticFactId();
	if (parameter > std::numeric_limits<std::size_t>::max() -
		function->default_argument_begin)
		return SemanticFactId();
	const std::size_t index = function->default_argument_begin + parameter;
	if (index >= function_default_arguments_.size())
		return SemanticFactId();
	return function_default_arguments_[index];
}

bool PA11SemanticModel::has_function_default_argument(
	const PA10AstNode& declaration, std::size_t declarator_index) const
{
	const PA10AstNode* declarator = function_declarator(
		declaration, declarator_index);
	if (declarator == NULL)
		return false;
	const PA10AstNode* clause = top_parameter_clause(*declarator);
	if (clause == NULL)
		return false;
	for (std::size_t i = 0; i < clause->children.size(); ++i)
		if (clause->children[i].kind == PA10NodeKind::ParameterDeclaration &&
			parameter_has_default_argument(clause->children[i]))
			return true;
	return false;
}

void PA11SemanticModel::record_function_default_arguments(
	FunctionFactId function_id, const PA10AstNode& declaration,
	std::size_t declarator_index)
{
	if (!function_id.valid() || function_id.value >= function_facts_.size())
		throw std::runtime_error("PA12 default argument function fact is missing");
	if (!has_function_default_argument(declaration, declarator_index))
		return;
	const FunctionFact function = function_facts_[function_id.value];
	if (!function.binding.valid() || function.binding.value >= bindings_.size())
		throw std::runtime_error("PA12 default argument function is missing");
	const TypeId function_type = binding(function.binding).type;
	if (type_kind(function_type) != TypeKind::Function)
		throw std::runtime_error("PA12 default argument type is not a function");
	// Default-expression analysis can publish more typed facts; retain a
	// signature value rather than a reference into the growing type arena.
	const TypeKey signature = types_[function_type.value];
	std::size_t default_argument_begin = function.default_argument_begin;
	if (function.default_argument_begin == InvalidIdentityValue)
	{
		default_argument_begin = function_default_arguments_.size();
		function_facts_[function_id.value].default_argument_begin =
			default_argument_begin;
		function_facts_[function_id.value].default_argument_count =
			signature.parameters.size();
		for (std::size_t i = 0; i < signature.parameters.size(); ++i)
			function_default_arguments_.push_back(SemanticFactId());
	}
	else if (function.default_argument_count != signature.parameters.size() ||
		function.default_argument_begin > function_default_arguments_.size() ||
		function.default_argument_count > function_default_arguments_.size() -
			default_argument_begin)
		throw std::runtime_error("PA12 default argument range mismatch");
	const PA10AstNode* declarator = function_declarator(
		declaration, declarator_index);
	const PA10AstNode* clause = declarator == NULL ? NULL :
		top_parameter_clause(*declarator);
	if (clause == NULL)
		throw std::runtime_error("PA12 default parameter clause is missing");
	std::size_t parameter_index = 0;
	for (std::size_t i = 0; i < clause->children.size(); ++i)
	{
		const PA10AstNode& parameter = clause->children[i];
		if (parameter.kind == PA10NodeKind::ParameterPack)
			continue;
		if (parameter.kind != PA10NodeKind::ParameterDeclaration)
			throw std::runtime_error("PA12 invalid default parameter");
		if (parameter_index >= signature.parameters.size())
			throw std::runtime_error("PA12 default parameter count mismatch");
		const PA10AstNode* expression =
			default_argument_expression(parameter);
		if (parameter_has_default_argument(parameter) && expression == NULL)
			throw std::runtime_error("PA12 invalid default argument");
		if (expression != NULL)
		{
				const std::size_t range_index = default_argument_begin +
					parameter_index;
			if (function_default_arguments_[range_index].valid())
				throw std::runtime_error("PA12 duplicate default argument");
			const ExprInfo value = semantic_expression_for_target(
				*expression, function.owner, signature.parameters[parameter_index]);
			const ExprInfo converted = apply_context_conversion(value,
				signature.parameters[parameter_index],
				semantic_facts_[value.fact.value].source);
			function_default_arguments_[range_index] = converted.fact;
		}
		++parameter_index;
	}
	if (parameter_index != signature.parameters.size())
		throw std::runtime_error("PA12 default parameter count mismatch");
}

bool PA11SemanticModel::constant_integer_value(SemanticFactId fact_id,
	__int128* value, bool* is_unsigned) const
{
	if (!fact_id.valid() || fact_id.value >= semantic_facts_.size() ||
		value == NULL || is_unsigned == NULL)
		return false;
	const SemanticFact& fact = semantic_facts_[fact_id.value];
	if (fact.has_constant_value)
	{
		*value = fact.constant_value;
		*is_unsigned = fact.constant_value_unsigned;
		return true;
	}
	if (fact.has_literal_value)
	{
		__int128 literal = static_cast<__int128>(fact.literal_value);
		if (fact.literal_value_negative) literal = -literal;
		*value = literal;
		*is_unsigned = fact.literal_value_unsigned;
		return true;
	}
	return false;
}

void PA11SemanticModel::record_constant_expression_value(
	SemanticFactId fact_id, ScopeId scope)
{
	if (!fact_id.valid() || fact_id.value >= semantic_facts_.size())
		return;
	const SemanticFact& fact = semantic_facts_[fact_id.value];
	const TypeId object_type = expression_object_type(fact.type);
	SemanticFact& owner = semantic_facts_[fact_id.value];
	if (!owner.constant_value_evaluated && owner.source != NULL &&
		(integral_id(object_type) || enumeration_id(object_type)))
	{
		// PA12 is the sole owner of this bounded constant-expression attempt.
		// The evaluated bit is set even when the typed value is invalid so a
		// later lowering phase cannot retry semantic evaluation.
		owner.constant_value_evaluated = true;
		try
		{
			const ConstValue value = eval_constexpr(*owner.source, scope);
			if (value.valid)
			{
				owner.has_constant_value = true;
				owner.constant_value = value.value;
				owner.constant_value_unsigned = value.is_unsigned;
			}
		}
		catch (const NonConstantExpression&)
		{
			// The typed invalid result is retained by constant_value_evaluated.
		}
	}
}

ConstantAddressFactId PA11SemanticModel::make_constant_address_fact(
	const ConstantAddressFact& fact)
{
	const ConstantAddressFactId result(constant_address_facts_.size());
	constant_address_facts_.push_back(fact);
	return result;
}

bool PA11SemanticModel::constant_address_fact_well_formed(
	const ConstantAddressFact& fact) const
{
	if (!fact.evaluated || !fact.valid)
		return false;
	const bool no_literal_payload = fact.literal_element_count == 0 &&
		fact.literal_byte_begin == InvalidIdentityValue &&
		fact.literal_byte_count == 0;
	switch (fact.kind)
	{
	case ConstantAddressKind::SymbolAddend:
		if (!fact.target.valid() || fact.target.value >= bindings_.size() ||
			!no_literal_payload || fact.element_type.valid() ||
			fact.index_type.valid() || fact.index_fact.valid())
			return false;
		return binding(fact.target).kind == BindingKind::Function ||
			binding(fact.target).kind == BindingKind::Variable;
	case ConstantAddressKind::ArrayElement:
		if (!fact.target.valid() || fact.target.value >= bindings_.size() ||
			!no_literal_payload || !fact.element_type.valid() ||
			fact.element_type.value >= types_.size() ||
			!fact.index_type.valid() || fact.index_type.value >= types_.size() ||
			!fact.index_fact.valid() ||
			fact.index_fact.value >= semantic_facts_.size())
			return false;
		return binding(fact.target).kind == BindingKind::Variable;
	case ConstantAddressKind::Literal:
		if (fact.target.valid() || fact.byte_addend != 0 ||
			fact.index_type.valid() || fact.index_fact.valid() ||
			!fact.element_type.valid() || fact.element_type.value >= types_.size() ||
			fact.literal_element_count == 0 ||
			fact.literal_byte_begin == InvalidIdentityValue ||
			fact.literal_byte_begin > constant_address_literal_bytes_.size() ||
			fact.literal_byte_count == 0 ||
			fact.literal_byte_count > constant_address_literal_bytes_.size() -
			fact.literal_byte_begin)
			return false;
		{
			const std::size_t element_size = type_size(fact.element_type);
			return element_size != 0 &&
				fact.literal_element_count <=
				std::numeric_limits<std::size_t>::max() / element_size &&
				fact.literal_byte_count == fact.literal_element_count * element_size;
		}
	case ConstantAddressKind::None:
		break;
	}
	return false;
}

bool PA11SemanticModel::resolve_constant_address(SemanticFactId fact_id,
	ScopeId scope, ConstantAddressContext context, ConstantAddressFact* result)
{
	if (result == NULL)
		return false;
	*result = ConstantAddressFact();
	result->evaluated = true;
	if (!fact_id.valid() || fact_id.value >= semantic_facts_.size())
		return false;
	const std::size_t byte_begin = constant_address_literal_bytes_.size();
	ConstantAddressFact candidate;
	candidate.evaluated = true;
	try
	{
		if (!resolve_constant_address_impl(fact_id, scope, context, &candidate) ||
			!constant_address_fact_well_formed(candidate))
		{
			constant_address_literal_bytes_.resize(byte_begin);
			return false;
		}
	}
	catch (...)
	{
		constant_address_literal_bytes_.resize(byte_begin);
		throw;
	}
	*result = candidate;
	return true;
}

bool PA11SemanticModel::resolve_constant_address_literal(
	const SemanticFact& fact, ConstantAddressContext context,
	ConstantAddressFact* result)
{
	if (fact.conversion_begin == InvalidIdentityValue)
	{
		// A transparent cast wrapper owns the ArrayToPointer conversion;
		// its child literal has no conversion range of its own.
		if (fact.conversion_count != 0 ||
			context != ConstantAddressContext::ArrayDecay)
			return false;
	}
	else if (fact.conversion_count == 0)
	{
		if (context != ConstantAddressContext::ArrayDecay)
			return false;
	}
	else
	{
		const ConversionFact& terminal = conversion_facts_[
			fact.conversion_begin + fact.conversion_count - 1];
		if (terminal.kind != ConversionKind::ArrayToPointer ||
			!pointer_id(terminal.target))
			return false;
	}
	const TypeId array_type = strip_cv_type(expression_object_type(fact.type));
	if (!array_type.valid() || type_kind(array_type) != TypeKind::Array)
		return false;
	const TypeId element_type = types_[array_type.value].child;
	const std::size_t element_size = type_size(element_type);
	const LiteralData& literal = fact.source->literal;
	if (element_size == 0 || literal.element_count != fact.literal_element_count ||
		fact.literal_element_count > std::numeric_limits<std::size_t>::max() /
			element_size || literal.bytes.size() !=
			fact.literal_element_count * element_size)
		return false;
	result->kind = ConstantAddressKind::Literal;
	result->valid = true;
	result->element_type = element_type;
	result->literal_element_count = fact.literal_element_count;
	result->literal_byte_begin = constant_address_literal_bytes_.size();
	result->literal_byte_count = literal.bytes.size();
	constant_address_literal_bytes_.insert(constant_address_literal_bytes_.end(),
		literal.bytes.begin(), literal.bytes.end());
	return true;
}

bool PA11SemanticModel::resolve_constant_address_impl(SemanticFactId fact_id,
	ScopeId scope, ConstantAddressContext context, ConstantAddressFact* result)
{
	if (result == NULL || !fact_id.valid() ||
		fact_id.value >= semantic_facts_.size())
		return false;
	result->evaluated = true;
	const SemanticFact& fact = semantic_facts_[fact_id.value];
	if (fact.conversion_begin != InvalidIdentityValue &&
		(fact.conversion_begin > conversion_facts_.size() ||
		fact.conversion_count > conversion_facts_.size() -
		fact.conversion_begin))
		return false;
	if (fact.kind == SemanticFactKind::Literal &&
		fact.literal_element_count != 0 && fact.source != NULL &&
		fact.source->kind == PA10NodeKind::Literal)
		return resolve_constant_address_literal(fact, context, result);
	if (fact.kind == SemanticFactKind::IdExpression)
	{
		if (!fact.binding.valid() || fact.binding.value >= bindings_.size())
			return false;
		const Binding& target = binding(fact.binding);
		if (target.kind == BindingKind::Function)
		{
			// A function identity is already an address-producing semantic fact;
			// both direct function-to-pointer use and explicit &function preserve
			// the canonical function binding.
			result->kind = ConstantAddressKind::SymbolAddend;
			result->target = fact.binding;
			result->valid = true;
			return true;
		}
		if (target.kind != BindingKind::Variable)
			return false;
		bool array_decay = false;
		if (fact.conversion_begin != InvalidIdentityValue)
			for (std::size_t i = 0; i < fact.conversion_count; ++i)
				if (conversion_facts_[fact.conversion_begin + i].kind ==
					ConversionKind::ArrayToPointer)
				{
					array_decay = true;
					break;
				}
		const bool object_address =
			context == ConstantAddressContext::ObjectAddress &&
			fact.category == SemanticValueCategory::Lvalue;
		const TypeId object_type = strip_cv_type(
			expression_object_type(fact.type));
		const bool decay_context =
			context == ConstantAddressContext::ArrayDecay && object_type.valid() &&
			type_kind(object_type) == TypeKind::Array;
		if (!object_address && !array_decay &&
			!decay_context)
			return false;
		result->kind = ConstantAddressKind::SymbolAddend;
		result->target = fact.binding;
		result->valid = true;
		return true;
	}

	std::vector<SemanticFactId> children;
	if (fact.child_begin != InvalidIdentityValue)
		for (std::size_t i = 0; i < fact.child_count; ++i)
			children.push_back(semantic_children_[fact.child_begin + i]);
	if (fact.kind == SemanticFactKind::UnaryExpression && children.size() == 1 &&
		(fact.token == SimpleTokenType::OP_AMP ||
		 fact.token == SimpleTokenType::OP_PLUS))
		return resolve_constant_address_impl(children.front(), scope,
			fact.token == SimpleTokenType::OP_AMP ?
			ConstantAddressContext::ObjectAddress : context, result);
	if (fact.kind == SemanticFactKind::CastExpression && children.size() == 1)
	{
		ConstantAddressContext child_context = context;
		if (fact.conversion_begin != InvalidIdentityValue)
			for (std::size_t i = 0; i < fact.conversion_count; ++i)
				if (conversion_facts_[fact.conversion_begin + i].kind ==
					ConversionKind::ArrayToPointer)
				{
					child_context = ConstantAddressContext::ArrayDecay;
					break;
				}
		return resolve_constant_address_impl(children.front(), scope,
			child_context, result);
	}

	if (fact.kind == SemanticFactKind::BinaryExpression && children.size() == 2 &&
		(fact.token == SimpleTokenType::OP_PLUS ||
		 fact.token == SimpleTokenType::OP_MINUS))
	{
		const TypeId left = strip_cv_type(expression_object_type(
			semantic_facts_[children[0].value].type));
		const TypeId right = strip_cv_type(expression_object_type(
			semantic_facts_[children[1].value].type));
		const bool left_pointer = left.valid() &&
			(type_kind(left) == TypeKind::Pointer ||
			 type_kind(left) == TypeKind::Array);
		const bool right_pointer = right.valid() &&
			(type_kind(right) == TypeKind::Pointer ||
			 type_kind(right) == TypeKind::Array);
		SemanticFactId pointer_fact;
		SemanticFactId integer_fact;
		bool negate = false;
		if (left_pointer && !right_pointer &&
			fact.token == SimpleTokenType::OP_PLUS)
		{
			pointer_fact = children[0];
			integer_fact = children[1];
		}
		else if (left_pointer && !right_pointer &&
			fact.token == SimpleTokenType::OP_MINUS)
		{
			pointer_fact = children[0];
			integer_fact = children[1];
			negate = true;
		}
		else if (!left_pointer && right_pointer &&
			fact.token == SimpleTokenType::OP_PLUS)
		{
			pointer_fact = children[1];
			integer_fact = children[0];
		}
		else
			return false;

		ConstantAddressFact base;
		if (!resolve_constant_address_impl(pointer_fact, scope,
			ConstantAddressContext::Value, &base) ||
			base.kind != ConstantAddressKind::SymbolAddend)
			return false;
		__int128 index = 0;
		bool index_unsigned = false;
		if (!constant_integer_value(integer_fact, &index, &index_unsigned))
		{
			record_constant_expression_value(integer_fact, scope);
		}
		if (!constant_integer_value(integer_fact, &index, &index_unsigned))
			return false;
		TypeId pointer_type = strip_cv_type(expression_object_type(
			semantic_facts_[pointer_fact.value].type));
		if (!pointer_type.valid() ||
			(type_kind(pointer_type) != TypeKind::Pointer &&
			 type_kind(pointer_type) != TypeKind::Array))
			return false;
		const TypeId element = types_[pointer_type.value].child;
		const __int128 scale = static_cast<__int128>(type_size(element));
		const __int128 offset = index * scale * (negate ? -1 : 1);
		long long addend = 0;
		if (!address_addend_fits(
			static_cast<__int128>(base.byte_addend) + offset, &addend))
			return false;
		*result = base;
		result->evaluated = true;
		result->valid = true;
		result->kind = ConstantAddressKind::SymbolAddend;
		result->byte_addend = addend;
		result->element_type = TypeId();
		result->index_type = TypeId();
		result->index_fact = SemanticFactId();
		result->index_value = 0;
		result->index_unsigned = false;
		return true;
	}

	if (fact.kind == SemanticFactKind::SubscriptExpression &&
		children.size() == 2)
	{
		TypeId sequence = strip_cv_type(expression_object_type(
			semantic_facts_[children.front().value].type));
		// A global pointer's stored value is not a static address expression;
		// only an array object can retain the direct array-element projection.
		if (context != ConstantAddressContext::ObjectAddress ||
			!sequence.valid() || type_kind(sequence) != TypeKind::Array)
			return false;
		ConstantAddressFact base;
		if (!resolve_constant_address_impl(children.front(), scope,
			ConstantAddressContext::Value, &base) ||
			base.kind != ConstantAddressKind::SymbolAddend)
			return false;
		__int128 index = 0;
		bool index_unsigned = false;
		if (!constant_integer_value(children.back(), &index, &index_unsigned))
		{
			record_constant_expression_value(children.back(), scope);
		}
		if (!constant_integer_value(children.back(), &index, &index_unsigned))
			return false;
		const TypeId element = types_[sequence.value].child;
		const __int128 offset = index * static_cast<__int128>(type_size(element));
		long long addend = 0;
		if (!address_addend_fits(
			static_cast<__int128>(base.byte_addend) + offset, &addend))
			return false;
		if (base.byte_addend != 0)
		{
			*result = base;
			result->evaluated = true;
			result->valid = true;
			result->byte_addend = addend;
			return true;
		}
		*result = base;
		result->evaluated = true;
		result->valid = true;
		result->kind = ConstantAddressKind::ArrayElement;
		result->byte_addend = addend;
		result->element_type = fact.type;
		result->index_type = expression_object_type(
			semantic_facts_[children.back().value].type);
		result->index_fact = children.back();
		result->index_value = index;
		result->index_unsigned = index_unsigned;
		return true;
	}
	return false;
}

void PA11SemanticModel::record_constant_address(SemanticFactId fact_id,
	ScopeId scope)
{
	if (!fact_id.valid() || fact_id.value >= semantic_facts_.size())
		return;
	SemanticFact& fact = semantic_facts_[fact_id.value];
	if (fact.constant_address.valid()) return;
	ConstantAddressFact result;
	result.evaluated = true;
	result.valid = resolve_constant_address(fact_id, scope,
		ConstantAddressContext::Value, &result);
	fact.constant_address = make_constant_address_fact(result);
}

void PA11SemanticModel::record_constant_initializer(SemanticFactId fact_id,
	ScopeId scope)
{
	if (!fact_id.valid() || fact_id.value >= semantic_facts_.size() ||
		!scope.valid() || scope.value >= scopes_.size() ||
		scopes_[scope.value].kind != ScopeKind::Namespace)
		return;
	SemanticFact& fact = semantic_facts_[fact_id.value];
	if (fact.kind == SemanticFactKind::Variable ||
		fact.kind == SemanticFactKind::BracedInitList)
	{
		if (fact.child_count == 0 || fact.child_begin == InvalidIdentityValue)
			return;
		for (std::size_t i = 0; i < fact.child_count; ++i)
			record_constant_initializer(
				semantic_children_[fact.child_begin + i], scope);
		return;
	}
	record_constant_expression_value(fact_id, scope);
	// The complete expression tree has now been folded once where its typed
	// facts permit it.  Resolve the address/relocation relation exactly once at
	// the initializer owner; descendants are never retried independently.
	record_constant_address(fact_id, scope);
}

std::size_t PA11SemanticModel::add_floating_literal(const LiteralData& literal)
{
	std::size_t expected = 0;
	switch (literal.type)
	{
	case FundamentalType::Float: expected = sizeof(float); break;
	case FundamentalType::Double: expected = sizeof(double); break;
	case FundamentalType::LongDouble: expected = sizeof(long double); break;
	default: throw std::runtime_error("PA12 invalid floating literal type");
	}
	if (literal.bytes.size() != expected)
		throw std::runtime_error("PA12 invalid floating literal bytes");
	const std::size_t byte_begin = floating_literal_bytes_.size();
	floating_literal_bytes_.insert(floating_literal_bytes_.end(),
		literal.bytes.begin(), literal.bytes.end());
	const std::size_t result = floating_literal_facts_.size();
	floating_literal_facts_.push_back(FloatingLiteralFact(literal.type,
		byte_begin, literal.bytes.size()));
	return result;
}

SemanticFactId PA11SemanticModel::semantic_literal(const PA10AstNode& node)
{
	TypeId type;
	SemanticValueCategory category = SemanticValueCategory::Prvalue;
	std::size_t element_count = 0;
	if (node.kind == PA10NodeKind::Literal)
	{
		type = fundamental(node.literal.type);
		element_count = node.literal.element_count;
		if (element_count != 0)
		{
			type = make_array(make_cv(type, 1u), false,
				ArrayBound(element_count));
			category = SemanticValueCategory::Lvalue;
		}
	}
	else if (node.kind == PA10NodeKind::KeywordLiteral)
	{
		if (node.token == SimpleTokenType::KW_TRUE ||
			node.token == SimpleTokenType::KW_FALSE)
			type = fundamental(FundamentalType::Bool);
		else if (node.token == SimpleTokenType::KW_NULLPTR)
			type = fundamental(FundamentalType::NullptrT);
		else
			throw std::runtime_error("PA12 unsupported keyword literal");
	}
	else
		throw std::runtime_error("PA12 expected literal");
	SemanticFact fact(SemanticFactKind::Literal, type, category, &node);
	fact.token = node.token;
	fact.literal_element_count = element_count;
	if (element_count == 0 && node.kind == PA10NodeKind::Literal &&
		integral_type(node.literal.type))
	{
		const ConstValue value = literal_constant(node);
		fact.has_constant_value = value.valid;
		fact.constant_value = value.value;
		fact.constant_value_unsigned = value.is_unsigned;
		fact.constant_value_evaluated = true;
	}
	if (element_count == 0 && node.kind == PA10NodeKind::Literal &&
		(node.literal.type == FundamentalType::Float ||
			node.literal.type == FundamentalType::Double ||
			node.literal.type == FundamentalType::LongDouble))
	{
		// PA2 already decoded the literal into typed bytes.  Publish an index
		// into the sparse payload so PA15 never consults or reparses text.
		fact.literal_float = add_floating_literal(node.literal);
	}
	return make_semantic_fact(fact);
}

ExprInfo PA11SemanticModel::semantic_unary_expression(const PA10AstNode& node, ScopeId scope)
{
	if (node.children.size() != 1 || !node.has_token)
		throw std::runtime_error("PA12 invalid unary expression");
	const ExprInfo operand = semantic_expression(node.children.front(), scope);
	std::vector<TypeId> associated_objects;
	associated_objects.push_back(operand.type);
	std::vector<const PA10AstNode*> no_member_nodes;
	std::vector<ExprInfo> no_member_arguments;
	std::vector<const PA10AstNode*> nonmember_nodes;
	nonmember_nodes.push_back(&node.children.front());
	std::vector<ExprInfo> nonmember_arguments;
	nonmember_arguments.push_back(operand);
	const ExprInfo overloaded = semantic_operator_call(node, scope,
		PA10OperatorFunctionKind::Token, node.token, operand,
		associated_objects, no_member_nodes, no_member_arguments,
		nonmember_nodes, nonmember_arguments, true);
	if (overloaded.fact.valid())
		return overloaded;
	TypeId type = expression_object_type(operand.type);
	SemanticValueCategory category = SemanticValueCategory::Prvalue;
	switch (node.token)
	{
	case SimpleTokenType::OP_AMP:
		if (operand.category != SemanticValueCategory::Lvalue)
			throw std::runtime_error("PA12 address-of requires lvalue");
		if (node.children.front().kind == PA10NodeKind::IdExpression)
		{
			const std::vector<ValueRef> values = lookup_value_path(
				name_path(node.children.front()), scope);
			if (values.size() == 1 && operand.fact.valid() &&
				values.front().binding ==
				semantic_facts_[operand.fact.value].binding &&
				!is_static_member(values.front().binding) &&
				values.front().scope.valid() &&
				values.front().scope.value < scopes_.size() &&
				scopes_[values.front().scope.value].kind == ScopeKind::Class)
			{
				const NamedRecordId owner =
					scopes_[values.front().scope.value].record;
				type = make_member_pointer(owner,
					binding(values.front().binding).type);
				break;
			}
		}
		type = make_pointer(operand.type);
		break;
	case SimpleTokenType::OP_STAR:
	{
		TypeId pointer = strip_cv_type(expression_object_type(operand.type));
		if (type_kind(pointer) == TypeKind::Array)
		{
			pointer = make_pointer(types_[pointer.value].child);
			record_builtin_conversion(operand, pointer);
		}
		if (type_kind(pointer) != TypeKind::Pointer)
			throw std::runtime_error("PA12 dereference requires pointer");
		if (type_kind(strip_cv_type(expression_object_type(operand.type))) ==
			TypeKind::Pointer)
			record_builtin_conversion(operand, pointer);
		type = types_[pointer.value].child;
		category = SemanticValueCategory::Lvalue;
		break;
	}
	case SimpleTokenType::OP_INC:
	case SimpleTokenType::OP_DEC:
		if (operand.category != SemanticValueCategory::Lvalue ||
			!modifiable_lvalue(operand.type) ||
			(!integral_id(operand.type) && !floating_id(operand.type) && !pointer_id(operand.type)))
			throw std::runtime_error("PA12 increment requires modifiable lvalue");
		type = strip_top_cv_type(operand.type);
		record_builtin_conversion(operand, integral_id(operand.type) ?
			promote_integral_type(operand.type) : type);
		category = SemanticValueCategory::Lvalue;
		break;
	case SimpleTokenType::OP_PLUS:
		if (type_kind(strip_cv_type(type)) == TypeKind::Array)
		{
			const TypeId array = strip_cv_type(type);
			type = make_pointer(types_[array.value].child);
			record_builtin_conversion(operand, type);
			break;
		}
	case SimpleTokenType::OP_MINUS:
	case SimpleTokenType::OP_COMPL:
		if (!integral_id(operand.type) &&
			(node.token == SimpleTokenType::OP_COMPL || !floating_id(operand.type)))
			throw std::runtime_error("PA12 unary arithmetic requires integral");
		type = node.token == SimpleTokenType::OP_COMPL ?
			common_integral_type(operand.type, operand.type) :
			common_arithmetic_type(operand.type, operand.type);
		record_builtin_conversion(operand, type);
		break;
	case SimpleTokenType::OP_LNOT:
		if (!scalar_id(operand.type))
			throw std::runtime_error("PA12 logical negation requires scalar");
		record_builtin_conversion(operand, fundamental(FundamentalType::Bool));
		type = fundamental(FundamentalType::Bool);
		break;
	default:
		throw std::runtime_error("PA12 unsupported unary operator");
	}
	return ExprInfo(make_expression_fact(SemanticFactKind::UnaryExpression,
		type, category, node, std::vector<SemanticFactId>(1, operand.fact)),
		type, category, false);
}

} // namespace pa11_semantic_internal
