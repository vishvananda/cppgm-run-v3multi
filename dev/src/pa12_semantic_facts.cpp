#include "pa11_semantic.h"
#include "pa11_semantic_model.h"

#include <cstring>
#include <limits>

namespace pa11_semantic_internal
{
using namespace pa11_semantic_storage;

ConversionFactId PA11SemanticModel::add_conversion(TypeId source, TypeId target,
	ConversionKind kind, unsigned int rank)
{
	if (kind == ConversionKind::DerivedToBase)
		throw std::runtime_error(
			"PA12 derived-base fact requires a selected conversion choice");
	const ConversionFactId result(conversion_facts_.size());
	conversion_facts_.push_back(ConversionFact(source, target, kind, rank));
	return result;
}
ConversionFactId PA11SemanticModel::add_conversion(TypeId source, TypeId target,
	const ConversionChoice& choice)
{
	if (!choice.valid || !source.valid() || !target.valid())
		throw std::runtime_error("PA12 invalid selected conversion fact");
	std::vector<NamedRecordId> base_path;
	unsigned int base_distance = 0;
	if (choice.kind == ConversionKind::DerivedToBase)
	{
		if (!choice.base_access_checked || !choice.base_source.valid() ||
			!choice.base_target.valid() || !choice.base_access_scope.valid() ||
			choice.base_distance == 0 ||
			!derived_base_relation(choice.base_source, choice.base_target,
				&base_distance, &base_path, choice.base_access_scope) ||
			base_path.empty() || base_distance != choice.base_distance)
			throw std::runtime_error("PA12 derived-base conversion path is missing");
	}
	if (choice.kind != ConversionKind::DerivedToBase &&
		(choice.base_source.valid() || choice.base_target.valid() ||
			choice.base_access_scope.valid() || choice.base_distance != 0 ||
			choice.added_cv != 0 || choice.base_access_checked))
		throw std::runtime_error("PA12 non-derived conversion has base metadata");
	for (std::size_t i = 0; i < base_path.size(); ++i)
		if (!base_path[i].valid() || base_path[i].value >= named_.size())
			throw std::runtime_error("PA12 derived-base path identity is invalid");
	const ConversionFactId result(conversion_facts_.size());
	ConversionFact fact(source, target, choice.kind, choice.rank);
	fact.rank_category = choice.rank_category;
	fact.base_distance = base_distance;
	fact.added_cv = choice.added_cv;
	fact.base_access_checked = choice.base_access_checked;
	fact.base_access_scope = choice.base_access_scope;
	if (!base_path.empty())
	{
		fact.base_path_begin = conversion_base_paths_.size();
		fact.base_path_count = base_path.size();
		conversion_base_paths_.insert(conversion_base_paths_.end(),
			base_path.begin(), base_path.end());
	}
	conversion_facts_.push_back(fact);
	return result;
}
void PA11SemanticModel::set_fact_conversion(SemanticFactId fact, ConversionFactId conversion)
{
	SemanticFact& owner = semantic_facts_[fact.value];
	ConversionFact& owned_conversion = conversion_facts_[conversion.value];
	if (owner.canonical_truth && owner.direct_bool_boundary &&
		bool_id(owned_conversion.source))
		owned_conversion.canonical_truth_policy =
			CanonicalTruthPolicy::Preserve;
	if (owner.conversion_begin == InvalidIdentityValue)
		owner.conversion_begin = conversion.value;
	else if (owner.conversion_begin + owner.conversion_count != conversion.value)
		throw std::runtime_error("PA12 non-contiguous conversion range");
	++owner.conversion_count;
}

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

void PA11SemanticModel::process_bit_field_declaration(
	const PA10AstNode& node, ScopeId scope)
{
	if (!scope.valid() || scope.value >= scopes_.size() ||
		scopes_[scope.value].kind != ScopeKind::Class ||
		scopes_[scope.value].record.value >= named_.size() ||
		named_[scopes_[scope.value].record.value].kind != NamedKind::Class ||
		node.children.size() < 2)
		throw std::runtime_error("invalid PA16 bit-field owner");
	const SpecFact spec = spec_fact(node.children.front(), scope);
	if (!spec.has_base || spec.is_typedef || spec.is_constexpr ||
		spec.is_static || spec.is_extern || spec.is_thread_local)
		throw std::runtime_error("invalid PA16 bit-field specifiers");
	const TypeId declared_type = spec.base;
	const TypeId unqualified_type = strip_cv_type(
		expression_object_type(declared_type));
	const NamedRecordId declared_record = named_record_for_type(
		unqualified_type);
	const bool enum_type = declared_record.valid() &&
		declared_record.value < named_.size() &&
		named_[declared_record.value].kind == NamedKind::Enum;
	if (!integral_id(unqualified_type) && !enum_type)
		throw std::runtime_error("PA16 bit-field type is not integral or enum");
	TypeId storage_type = unqualified_type;
	if (enum_type)
		storage_type = named_[declared_record.value].has_underlying ?
			strip_cv_type(named_[declared_record.value].underlying) :
			fundamental(FundamentalType::Int);
	FundamentalType storage_fundamental;
	if (!fundamental_of(storage_type, &storage_fundamental) ||
		!integral_type(storage_fundamental))
		throw std::runtime_error("PA16 bit-field storage is not integral");
	const std::size_t declared_storage_unit_size = type_size(storage_type);
	if (declared_storage_unit_size == 0 || declared_storage_unit_size > 8)
		throw std::runtime_error("PA16 bit-field storage is too wide");
	const std::size_t declared_storage_width = declared_storage_unit_size * 8;
	DeclarationFact declaration(&node, scope);
	declaration.binding_begin = declaration_bindings_.size();
	for (std::size_t i = 1; i < node.children.size(); ++i)
	{
		const PA10AstNode& field = node.children[i];
		if (field.kind != PA10NodeKind::BitFieldDeclarator ||
			field.children.empty() || field.children.size() > 2)
			throw std::runtime_error("invalid PA16 bit-field declarator");
		const PA10AstNode& width_node = field.children.back();
		const ConstValue width_value = eval_constexpr(width_node, scope);
		if (!width_value.valid || width_value.value < 0 ||
			width_value.value > static_cast<__int128>(
				std::numeric_limits<std::size_t>::max()))
			throw std::runtime_error("invalid PA16 bit-field width");
		const std::size_t width = static_cast<std::size_t>(width_value.value);
		// Keep the declared scalar as the physical unit.  A width beyond its
		// value representation allocates additional units; those extra bits are
		// padding and the value projection still reads only the first unit.
		const TypeId effective_storage_type = storage_type;
		const std::size_t storage_unit_size = declared_storage_unit_size;
		const std::size_t storage_width = declared_storage_width;
		const bool named = field.children.size() == 2;
		DeclaratorName name;
		if (named)
		{
			const PA10AstNode& declarator = field.children.front();
			// [class.bit] names an entity directly; pointer, array, function,
			// and parenthesized declarator shapes must not be silently reduced
			// to the declaration's base type.
			if (declarator.kind != PA10NodeKind::Declarator ||
				declarator.children.size() != 1 ||
				declarator.children.front().kind != PA10NodeKind::Identifier)
				throw std::runtime_error("invalid PA16 named bit-field");
			name = declarator_name(declarator);
			if (!name.found || name.path.global ||
				name.path.components.size() != 1 || width == 0)
				throw std::runtime_error("invalid PA16 named bit-field name");
		}
		BitFieldFact fact;
		fact.owner_scope = scope;
		fact.owner_record = scopes_[scope.value].record;
		fact.declared_type = declared_type;
		fact.storage_type = effective_storage_type;
		fact.width = width;
		fact.value_width = width == 0 ? 0 : storage_fundamental ==
			FundamentalType::Bool ? 1 :
			(width < declared_storage_width ? width : declared_storage_width);
		fact.storage_unit_size = storage_unit_size;
		fact.storage_width = storage_width;
		if (width != 0)
		{
			const std::size_t unit_count = width / storage_width +
				(width % storage_width == 0 ? 0 : 1);
			if (unit_count == 0 || unit_count >
				std::numeric_limits<std::size_t>::max() / storage_unit_size)
				throw std::runtime_error("PA16 bit-field allocation is too large");
			fact.allocation_size = unit_count * storage_unit_size;
		}
		fact.named = named;
		fact.zero_width = width == 0;
		fact.is_signed = storage_fundamental != FundamentalType::Bool &&
			!unsigned_integral_type(storage_type);
		const bool scoped_enum = enum_type &&
			named_[declared_record.value].scoped_enum;
		fact.operation_type = scoped_enum ? declared_type : storage_type;
		if (!scoped_enum && integral_rank(storage_type) <= integral_rank(
			fundamental(FundamentalType::Int)))
		{
			const TypeId int_type = fundamental(FundamentalType::Int);
			const TypeId unsigned_int_type = fundamental(
				FundamentalType::UnsignedInt);
			const std::size_t int_width = type_size(int_type) * 8;
			const bool int_represents = storage_fundamental ==
				FundamentalType::Bool || !unsigned_type(storage_fundamental) ||
				fact.value_width < int_width;
			if (int_represents)
				fact.operation_type = int_type;
			else if (fact.value_width <= type_size(unsigned_int_type) * 8)
				fact.operation_type = unsigned_int_type;
		}
		fact.value_mask = fact.value_width == 0 ? 0 : fact.value_width == 64 ?
			std::numeric_limits<std::uint64_t>::max() :
			((static_cast<std::uint64_t>(1) << fact.value_width) - 1);
		if (named)
		{
			const BindingId binding_id = add_value(scope, name.path.last(),
				declared_type, false, true, true, BindingId(),
				SourcePoint(node.source_begin), false,
				current_language_linkage_, FunctionDeclarationKind::Normal,
				false, PA10OperatorFunctionKind::None,
				SimpleTokenType::OP_SEMICOLON);
			fact.binding = binding_id;
			set_bit_field_fact(binding_id, fact);
			declaration_bindings_.push_back(binding_id);
			declaration_definition_flags_.push_back(
				binding(binding_id).has_definition ? 1 : 0);
		}
		append_record_bit_field(fact.owner_record, fact);
	}
	declaration.binding_count = declaration_bindings_.size() -
		declaration.binding_begin;
	const DeclarationFactId declaration_id(declaration_facts_.size());
	declaration_facts_.push_back(declaration);
	declaration_fact_index_.set(&node, declaration_id);
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

BindingId PA11SemanticModel::ensure_aggregate_constructor(
	NamedRecordId record_id)
{
	if (!record_id.valid() || record_id.value >= named_.size() ||
		named_[record_id.value].kind != NamedKind::Class ||
		named_[record_id.value].class_tag == ClassTag::Union ||
		!aggregate_class_initialization_supported(record_id))
		throw std::runtime_error("PA12 aggregate constructor record is invalid");
	const NamedRecord& record = named_[record_id.value];
	if (!record.name.valid() || !record.scope.valid() ||
		record.scope.value >= scopes_.size() ||
		scopes_[record.scope.value].kind != ScopeKind::Class ||
		scopes_[record.scope.value].record != record_id)
		throw std::runtime_error("PA12 aggregate constructor owner is invalid");
	const NamedRecordSidecar* existing = named_record_sidecar(record_id);
	if (existing != NULL && existing->aggregate_constructor_binding.valid())
		return existing->aggregate_constructor_binding;

	// Capture the direct member sequence once.  The synthetic function below
	// forwards exactly this typed sequence; it does not rediscover member
	// appertainment while lowering an array element.
	std::vector<BindingId> members;
	const RecordLayout& layout = record_layout(record_id);
	if (layout.state != RecordLayoutState::Complete)
		throw std::runtime_error("PA12 aggregate constructor layout is incomplete");
	members.reserve(layout.members.size());
	for (std::size_t i = 0; i < layout.members.size(); ++i)
	{
		const BindingId member_id = layout.members[i].binding;
		if (!member_id.valid() || member_id.value >= bindings_.size() ||
			member_id.value >= binding_owners_.size() ||
			binding_owners_[member_id.value] != record.scope ||
			binding(member_id).kind != BindingKind::Variable ||
			is_static_member(member_id))
			throw std::runtime_error(
				"PA12 aggregate constructor member identity is invalid");
		members.push_back(member_id);
	}
	std::vector<TypeId> parameters;
	parameters.reserve(members.size());
	for (std::size_t i = 0; i < members.size(); ++i)
		parameters.push_back(normalize_parameter_type(
			binding(members[i]).type));
	const TypeId constructor_type = make_function(parameters, false,
		fundamental(FundamentalType::Void));
	Binding helper(BindingKind::Function, record.name, constructor_type);
	helper.has_definition = true;
	helper.language_linkage = LanguageLinkage::Cxx;
	const BindingId binding_id = store_binding(record.scope, helper);
	BindingSidecar constructor_sidecar;
	const BindingSidecar* existing_binding = binding_sidecar(binding_id);
	if (existing_binding != NULL)
		constructor_sidecar = *existing_binding;
	constructor_sidecar.constructor_record = record_id;
	set_binding_sidecar(binding_id, constructor_sidecar);
	NamedRecordSidecar record_sidecar;
	if (existing != NULL)
		record_sidecar = *existing;
	record_sidecar.aggregate_constructor_binding = binding_id;
	set_named_record_sidecar(record_id, record_sidecar);

	const ScopeId function_scope = create_scope(ScopeKind::Function,
		record.scope, record.name);
	function_bindings_.set(function_scope, binding_id);
	FunctionFact function(NULL, record.scope, binding_id, function_scope,
		ScopeId());
	function.is_constructor = true;
	function.synthetic = true;
	function.constructor_record = record_id;
	prepare_pa12_member_parameter(function);
	std::vector<BindingId> parameter_bindings;
	parameter_bindings.reserve(parameters.size());
	for (std::size_t i = 0; i < members.size(); ++i)
	{
		// store_binding may reallocate bindings_; do not retain a reference to
		// that arena across the publish.
		const Binding member = binding(members[i]);
		const BindingId parameter = store_binding(function_scope,
			Binding(BindingKind::Parameter, member.name, parameters[i]));
		if (member.name.valid())
			append_value_index(function_scope, member.name, parameter,
				ScopeId(), SourcePoint());
		parameter_bindings.push_back(parameter);
	}
	const FunctionFactId function_id(function_facts_.size());
	function_facts_.push_back(function);
	function_binding_fact_index_.set(binding_id, function_id);

	const std::size_t action_begin = constructor_actions_.size();
	for (std::size_t i = 0; i < members.size(); ++i)
	{
		const TypeId member_type = binding(members[i]).type;
		TypeId expression_type = member_type;
		const TypeKind member_kind = type_kind(expression_type);
		if (member_kind == TypeKind::LvalueReference ||
			member_kind == TypeKind::RvalueReference)
			expression_type = types_[expression_type.value].child;
		SemanticFact parameter_fact(SemanticFactKind::IdExpression,
			expression_type, SemanticValueCategory::Lvalue, NULL);
		parameter_fact.binding = parameter_bindings[i];
		const SemanticFactId initializer = make_semantic_fact(parameter_fact);
		ConstructorActionFact action(ConstructorActionTarget::Member,
			NamedRecordId(), members[i], BindingId(), initializer);
		action.object_type = member_type;
		constructor_actions_.push_back(action);
	}
	FunctionFact& stored = function_facts_[function_id.value];
	stored.constructor_action_begin = action_begin;
	stored.constructor_action_count = members.size();
	synthetic_function_facts_.push_back(
		SyntheticFunctionFact(record_id, binding_id));
	return binding_id;
}

BindingId PA11SemanticModel::ensure_special_member_base_entry(
	BindingId special_member, bool constructor)
{
	if (!special_member.valid() || special_member.value >= bindings_.size() ||
		special_member.value >= binding_owners_.size())
		throw std::runtime_error("PA12 special-member base entry binding is invalid");
	FlatIndex<BindingId, BindingId, IdentityHash<BindingId> >& entry_bindings =
		constructor ? constructor_base_entry_bindings_ :
		destructor_base_entry_bindings_;
	const BindingId* existing = entry_bindings.find(special_member);
	if (existing != NULL)
		return *existing;
	const FunctionFactId* source_id =
		function_binding_fact_index_.find(special_member);
	if (source_id == NULL || !source_id->valid() ||
		source_id->value >= function_facts_.size())
		throw std::runtime_error("PA12 special-member base entry fact is missing");
	FunctionFact source = function_facts_[source_id->value];
	const NamedRecordId record_id = constructor ? source.constructor_record :
		source.destructor_record;
	if ((constructor ? !source.is_constructor : !source.is_destructor) ||
		source.binding != special_member || !record_id.valid() ||
		record_id.value >= named_.size() ||
		!source.owner.valid() || source.owner.value >= scopes_.size() ||
		scopes_[source.owner.value].kind != ScopeKind::Class ||
		source.owner != named_[record_id.value].scope ||
		binding_owners_[special_member.value] != source.owner ||
		!source.function_scope.valid() ||
		source.function_scope.value >= scopes_.size() ||
		scopes_[source.function_scope.value].kind != ScopeKind::Function ||
		scopes_[source.function_scope.value].parent != source.owner)
		throw std::runtime_error("PA12 special-member base entry owner is invalid");
	if ((constructor && source.constructor_base_entry) ||
		(!constructor && source.destructor_base_entry))
	{
		entry_bindings.set(special_member, special_member);
		return special_member;
	}
	if (constructor && source.constructor_action_begin == InvalidIdentityValue &&
		(source.body_fact.valid() || source.synthetic))
	{
		build_constructor_actions(*source_id);
		source = function_facts_[source_id->value];
	}
	if (constructor && source.constructor_action_begin != InvalidIdentityValue &&
		(source.constructor_action_begin > constructor_actions_.size() ||
			source.constructor_action_count > constructor_actions_.size() -
				source.constructor_action_begin))
		throw std::runtime_error("PA12 constructor base entry actions are invalid");
	if (!constructor && source.destructor_action_count != 0)
		throw std::runtime_error(
			"PA16 destructor base entries with member cleanup are outside checkpoint");

	const Binding source_binding = binding(special_member);
	Binding entry_binding = source_binding;
	const BindingId entry_id = store_binding(source.owner, entry_binding);
	const BindingSidecar* source_sidecar = binding_sidecar(special_member);
	if (source_sidecar == NULL ||
		(constructor ? source_sidecar->constructor_record != record_id :
			source_sidecar->destructor_record != record_id))
		throw std::runtime_error("PA12 special-member base entry sidecar is invalid");
	BindingSidecar entry_sidecar = *source_sidecar;
	set_binding_sidecar(entry_id, entry_sidecar);

	FunctionFact entry = source;
	entry.node = NULL;
	entry.binding = entry_id;
	if (constructor)
	{
		entry.constructor_base_entry = true;
		entry.constructor_entry_source = special_member;
	}
	else
	{
		entry.destructor_base_entry = true;
		entry.destructor_entry_source = special_member;
	}
	// Preserve the source fact's synthetic bit.  A generated entry for a
	// user-defined empty constructor still has a real body and must not be
	// folded as an unused synthetic no-op by PA15's demand walk.
	entry.synthetic = source.synthetic;
	if (constructor)
	{
		entry.constructor_action_begin = InvalidIdentityValue;
		entry.constructor_action_count = 0;
	}
	else
	{
		// A base-entry destructor has no member/base cleanup of its own.  Keep
		// an empty, valid range so the existing PA15 destructor fact checks remain
		// truthful for both the complete and base ABI entries.
		entry.destructor_action_begin = destructor_actions_.size();
		entry.destructor_action_count = 0;
	}
	const FunctionFactId entry_id_fact(function_facts_.size());
	function_facts_.push_back(entry);
	function_binding_fact_index_.set(entry_id, entry_id_fact);

	if (constructor && source.constructor_action_begin != InvalidIdentityValue)
	{
		const std::size_t action_begin = constructor_actions_.size();
		for (std::size_t i = 0; i < source.constructor_action_count; ++i)
		{
			ConstructorActionFact action = constructor_actions_[
				source.constructor_action_begin + i];
			if (action.constructor.valid() &&
				action.target == ConstructorActionTarget::Base)
			{
					action.constructor = ensure_constructor_base_entry(
					action.constructor);
				if (!action.callable_type.valid())
					action.callable_type = constructor_callable_type(
					action.constructor);
			}
			constructor_actions_.push_back(action);
		}
		function_facts_[entry_id_fact.value].constructor_action_begin = action_begin;
		function_facts_[entry_id_fact.value].constructor_action_count =
			source.constructor_action_count;
	}
	entry_bindings.set(special_member, entry_id);
	return entry_id;
}

BindingId PA11SemanticModel::ensure_constructor_base_entry(BindingId constructor)
{
	return ensure_special_member_base_entry(constructor, true);
}

BindingId PA11SemanticModel::ensure_destructor_base_entry(BindingId destructor)
{
	return ensure_special_member_base_entry(destructor, false);
}

BindingId PA11SemanticModel::ensure_inheriting_constructor(
	NamedRecordId derived_id, NamedRecordId base_id, BindingId base_constructor,
	std::size_t parameter_count)
{
	if (!derived_id.valid() || derived_id.value >= named_.size() ||
		!base_id.valid() || base_id.value >= named_.size() ||
		named_[derived_id.value].kind != NamedKind::Class ||
		named_[base_id.value].kind != NamedKind::Class)
		throw std::runtime_error("PA12 inheriting constructor record is invalid");
	const NamedRecord& derived = named_[derived_id.value];
	const NamedRecord& base = named_[base_id.value];
	if (!derived.name.valid() || !derived.scope.valid() ||
		derived.scope.value >= scopes_.size() ||
		scopes_[derived.scope.value].kind != ScopeKind::Class ||
		scopes_[derived.scope.value].record != derived_id ||
		!derived.has_base || derived.direct_base != base_id ||
		!base.name.valid() || !base.scope.valid() ||
		base.scope.value >= scopes_.size() ||
		scopes_[base.scope.value].kind != ScopeKind::Class ||
		scopes_[base.scope.value].record != base_id)
		throw std::runtime_error("PA12 inheriting constructor owner is invalid");
	if (!base_constructor.valid() || base_constructor.value >= bindings_.size() ||
		base_constructor.value >= binding_owners_.size() ||
		binding_owners_[base_constructor.value] != base.scope)
		throw std::runtime_error("PA12 inherited constructor binding owner is invalid");
	const Binding base_binding = binding(base_constructor);
	const BindingSidecar* base_sidecar = binding_sidecar(base_constructor);
	if (base_binding.kind != BindingKind::Function ||
		type_kind(base_binding.type) != TypeKind::Function ||
		base_sidecar == NULL || base_sidecar->constructor_record != base_id)
		throw std::runtime_error("PA12 inherited constructor binding is invalid");
	FunctionFact* base_function = function_fact_for_binding(base_constructor);
	if (base_function == NULL || !base_function->is_constructor ||
		base_function->binding != base_constructor ||
		base_function->owner != base.scope ||
		!base_function->function_scope.valid() ||
		base_function->function_scope.value >= scopes_.size() ||
		scopes_[base_function->function_scope.value].kind != ScopeKind::Function ||
		scopes_[base_function->function_scope.value].parent != base.scope)
		throw std::runtime_error("PA12 inherited constructor fact is missing");
	prepare_pa12_member_parameter(*base_function);
	const FunctionFact base_function_copy = *base_function;
	const TypeKey base_signature = types_[base_binding.type.value];
	if (base_signature.variadic)
		throw std::runtime_error(
			"PA16 variadic inheriting constructors are outside checkpoint");
	const Scope& base_function_scope =
		scopes_[base_function_copy.function_scope.value];
	const BindingId base_this = base_function_scope.implicit_object_binding;
	if (!base_this.valid() || base_this.value >= bindings_.size() ||
		base_this.value >= binding_owners_.size() ||
		binding_owners_[base_this.value] != base_function_copy.function_scope ||
		binding(base_this).kind != BindingKind::Parameter ||
		binding(base_this).type != member_object_pointer_type(
			base_binding.type, base.scope))
		throw std::runtime_error("PA12 inherited constructor object is missing");
	std::vector<Binding> base_parameters;
	for (std::size_t i = 0; i < base_function_scope.bindings.size(); ++i)
	{
		const BindingId parameter_id = base_function_scope.bindings[i];
		if (!parameter_id.valid() || parameter_id.value >= bindings_.size() ||
			parameter_id.value >= binding_owners_.size() ||
			binding_owners_[parameter_id.value] !=
				base_function_copy.function_scope)
			throw std::runtime_error(
				"PA12 inherited constructor parameter identity is invalid");
		const Binding& parameter = binding(parameter_id);
		if (parameter.kind == BindingKind::Parameter && parameter_id != base_this)
			base_parameters.push_back(parameter);
	}
	if (base_parameters.size() != base_signature.parameters.size())
		throw std::runtime_error("PA12 inherited constructor parameter count mismatch");
	for (std::size_t i = 0; i < base_parameters.size(); ++i)
		if (base_parameters[i].type != base_signature.parameters[i])
			throw std::runtime_error(
				"PA12 inherited constructor parameter type mismatch");
	const std::size_t minimum_arity =
		inherited_constructor_minimum_arity(base_constructor, base_signature);
	if (parameter_count == 0 || parameter_count > base_parameters.size() ||
		parameter_count < minimum_arity)
		throw std::runtime_error(
			"PA12 inherited constructor notional arity is invalid");
	std::vector<TypeId> wrapper_signature_parameters(
		base_signature.parameters.begin(),
		base_signature.parameters.begin() + parameter_count);
	const TypeId wrapper_type = make_function(wrapper_signature_parameters, false,
		base_signature.result, base_signature.cv);

	SourcePoint declaration_point;
	const NamedRecordSidecar* derived_sidecar =
		named_record_sidecar(derived_id);
	if (derived_sidecar == NULL)
		throw std::runtime_error("PA12 inheriting constructor relation is missing");
	for (std::size_t i = 0; i < derived_sidecar->inheriting_constructors.size(); ++i)
		if (derived_sidecar->inheriting_constructors[i].base_record == base_id)
		{
			declaration_point =
				derived_sidecar->inheriting_constructors[i].declaration_point;
			break;
		}
	if (!declaration_point.valid())
		throw std::runtime_error("PA12 inheriting constructor relation is invalid");

	// The relation key is (derived record, base constructor binding).  Reuse an
	// already published wrapper instead of appending a second value entry when
	// overload probing reaches the same constructor again.
	const ValueList* existing = scopes_[derived.scope.value].values.find(
		derived.name);
	if (existing != NULL)
		for (std::size_t i = 0; i < existing->entries.size(); ++i)
		{
			const ValueEntry& entry = existing->entries[i];
			if (entry.origin != derived.scope || !entry.binding.valid() ||
				entry.binding.value >= bindings_.size())
				continue;
			const FunctionFact* candidate = function_fact_for_binding(
				entry.binding);
			if (candidate == NULL || !candidate->inheriting_constructor ||
				candidate->inherited_base_record != base_id ||
				candidate->inherited_base_constructor != base_constructor)
				continue;
			if (binding(entry.binding).type != wrapper_type)
				continue;
			return entry.binding;
		}

	Binding wrapper(BindingKind::Function, derived.name, wrapper_type);
	wrapper.has_definition = true;
	wrapper.language_linkage = base_binding.language_linkage;
	const BindingId wrapper_id = store_binding(derived.scope, wrapper);
	BindingSidecar wrapper_sidecar;
	wrapper_sidecar.constructor_record = derived_id;
	// Explicitness, declaration kind, and exception behavior belong to the
	// selected base constructor.  The wrapper access remains the default public
	// value; select_constructor checks the inherited chain separately.
	wrapper_sidecar.explicit_constructor = base_sidecar->explicit_constructor;
	wrapper_sidecar.declaration_kind = base_sidecar->declaration_kind;
	wrapper_sidecar.nonthrowing = base_sidecar->nonthrowing;
	set_binding_sidecar(wrapper_id, wrapper_sidecar);
	append_value_index(derived.scope, derived.name, wrapper_id, derived.scope,
		declaration_point);

	const ScopeId wrapper_scope = create_scope(ScopeKind::Function,
		derived.scope, derived.name);
	function_bindings_.set(wrapper_scope, wrapper_id);
	FunctionFact wrapper_function(NULL, derived.scope, wrapper_id,
		wrapper_scope, ScopeId());
	wrapper_function.is_constructor = true;
	wrapper_function.synthetic = true;
	wrapper_function.constructor_record = derived_id;
	wrapper_function.inheriting_constructor = true;
	wrapper_function.inherited_base_record = base_id;
	wrapper_function.inherited_base_constructor = base_constructor;
	// Default arguments are forwarded by the wrapper's base action below.
	prepare_pa12_member_parameter(wrapper_function);
	std::vector<BindingId> wrapper_parameters;
	for (std::size_t i = 0; i < parameter_count; ++i)
	{
		const BindingId parameter = store_binding(wrapper_scope,
			Binding(BindingKind::Parameter, base_parameters[i].name,
				base_parameters[i].type));
		if (base_parameters[i].name.valid())
			append_value_index(wrapper_scope, base_parameters[i].name, parameter,
				ScopeId(), declaration_point);
		wrapper_parameters.push_back(parameter);
	}
	const FunctionFactId wrapper_function_id(function_facts_.size());
	function_facts_.push_back(wrapper_function);
	function_binding_fact_index_.set(wrapper_id, wrapper_function_id);
	std::vector<SemanticFactId> base_arguments;
	base_arguments.reserve(base_signature.parameters.size());
	for (std::size_t i = 0; i < wrapper_parameters.size(); ++i)
	{
		TypeId expression_type = base_parameters[i].type;
		const TypeKind parameter_kind = type_kind(expression_type);
		if (parameter_kind == TypeKind::LvalueReference ||
			parameter_kind == TypeKind::RvalueReference)
			expression_type = types_[expression_type.value].child;
		SemanticFact parameter_fact(SemanticFactKind::IdExpression,
			expression_type, SemanticValueCategory::Lvalue, NULL);
		parameter_fact.binding = wrapper_parameters[i];
		base_arguments.push_back(make_semantic_fact(parameter_fact));
	}
	for (std::size_t i = parameter_count; i < base_signature.parameters.size(); ++i)
	{
		const SemanticFactId default_fact =
			function_default_argument(base_constructor, i);
		if (!default_fact.valid() || default_fact.value >= semantic_facts_.size())
			throw std::runtime_error(
				"PA12 inherited constructor default fact is missing");
		base_arguments.push_back(default_fact);
	}
	if (constructor_arguments_.size() > std::numeric_limits<std::size_t>::max() -
		base_arguments.size())
		throw std::runtime_error(
			"PA12 inherited constructor argument arena overflow");
	const std::size_t argument_begin = constructor_arguments_.size();
	constructor_arguments_.insert(constructor_arguments_.end(),
		base_arguments.begin(), base_arguments.end());
	const BindingId base_entry = ensure_constructor_base_entry(base_constructor);
	ConstructorActionFact base_action(ConstructorActionTarget::Base, base_id,
		BindingId(), base_entry);
	base_action.object_type = named_type(base_id);
	base_action.callable_type = constructor_callable_type(base_entry);
	if (!base_arguments.empty())
	{
		base_action.argument_begin = argument_begin;
		base_action.argument_count = base_arguments.size();
	}
	std::vector<ConstructorActionFact> member_actions;
	std::vector<SemanticFactId> member_arguments;
	const ConstructorMemberInitializerIndex no_member_initializers;
	// Collect direct members in declaration order through local action arenas.
	append_constructor_member_actions(derived_id, wrapper_scope,
		no_member_initializers, member_actions, member_arguments);
	if (member_arguments.size() > std::numeric_limits<std::size_t>::max() -
		constructor_arguments_.size())
		throw std::runtime_error(
			"PA12 inherited constructor member argument arena overflow");
	const std::size_t member_argument_begin = constructor_arguments_.size();
	constructor_arguments_.insert(constructor_arguments_.end(),
		member_arguments.begin(), member_arguments.end());
	for (std::size_t i = 0; i < member_actions.size(); ++i)
		if (member_actions[i].argument_begin != InvalidIdentityValue)
		{
			if (member_actions[i].argument_begin >
				std::numeric_limits<std::size_t>::max() - member_argument_begin)
				throw std::runtime_error(
					"PA12 inherited constructor member argument range overflow");
			member_actions[i].argument_begin += member_argument_begin;
	}
	const std::size_t action_begin = constructor_actions_.size();
	constructor_actions_.push_back(base_action);
	constructor_actions_.insert(constructor_actions_.end(),
		member_actions.begin(), member_actions.end());
	function_facts_[wrapper_function_id.value].constructor_action_begin =
		action_begin;
	function_facts_[wrapper_function_id.value].constructor_action_count =
		1 + member_actions.size();
	synthetic_function_facts_.push_back(
		SyntheticFunctionFact(derived_id, wrapper_id));
	return wrapper_id;
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

void PA11SemanticModel::record_namespace_lifetime(BindingId object,
	TypeId object_type, ScopeId scope)
{
	if (!object.valid() || object.value >= bindings_.size() ||
		object.value >= binding_owners_.size() || !scope.valid() ||
		scope.value >= scopes_.size() || binding_owners_[object.value] != scope ||
		binding(object).kind != BindingKind::Variable)
		throw std::runtime_error("PA16 namespace lifetime object is invalid");
	const Scope& owner = scopes_[scope.value];
	if (owner.kind != ScopeKind::Namespace && owner.kind != ScopeKind::Class)
		throw std::runtime_error("PA16 namespace lifetime scope is invalid");
	if (owner.kind == ScopeKind::Class && !is_static_member(object))
		throw std::runtime_error("PA16 namespace lifetime class object is invalid");
	if (owner.kind == ScopeKind::Namespace && is_static_member(object))
		throw std::runtime_error("PA16 namespace lifetime member owner is invalid");
	const NamedRecordId record = class_record_for_object_type(object_type);
	if (!record.valid() || !destructor_requires_runtime(record))
		return;
	const BindingId destructor = ensure_implicit_destructor(record);
	if (!destructor.valid())
		throw std::runtime_error("PA16 namespace lifetime destructor is missing");
	lifetime_facts_.push_back(LifetimeFact(object, object_type, destructor, scope,
		LifetimeStorageKind::Namespace));
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

std::size_t PA11SemanticModel::inherited_constructor_minimum_arity(
	BindingId binding_id, const TypeKey& signature) const
{
	const FunctionFact* function = function_fact_for_binding(binding_id);
	if (function == NULL || !function->is_constructor ||
		function->binding != binding_id ||
		function->default_argument_begin == InvalidIdentityValue)
	{
		if (function != NULL && function->default_argument_count != 0)
			throw std::runtime_error(
				"PA12 inherited constructor default range is invalid");
		return signature.parameters.size();
	}
	if (function->default_argument_count != signature.parameters.size() ||
		function->default_argument_begin > function_default_arguments_.size() ||
		function->default_argument_count > function_default_arguments_.size() -
			function->default_argument_begin)
		throw std::runtime_error(
			"PA12 inherited constructor default range is invalid");
	for (std::size_t i = 0; i < signature.parameters.size(); ++i)
	{
		const SemanticFactId default_fact = function_default_argument(binding_id, i);
		if (default_fact.valid() && default_fact.value >= semantic_facts_.size())
			throw std::runtime_error(
				"PA12 inherited constructor default fact is invalid");
	}
	std::size_t minimum = signature.parameters.size();
	while (minimum != 0 &&
		function_default_argument(binding_id, minimum - 1).valid())
		--minimum;
	for (std::size_t i = 0; i < minimum; ++i)
		if (function_default_argument(binding_id, i).valid())
			throw std::runtime_error(
				"PA12 inherited constructor default is not trailing");
	return minimum;
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
				semantic_facts_[value.fact.value].source, function.owner);
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
	if (fact.kind == SemanticFactKind::BracedInitList)
	{
		const AggregateFactRange* range = aggregate_ranges_.find(fact_id);
		if (range != NULL)
		{
			if (range->count != 0 &&
				(range->begin == InvalidIdentityValue ||
					range->begin > aggregate_elements_.size() ||
					range->count > aggregate_elements_.size() - range->begin))
				throw std::runtime_error(
					"PA12 aggregate constant range is invalid");
			for (std::size_t i = 0; i < range->count; ++i)
				record_constant_initializer(
					aggregate_elements_[range->begin + i].initializer, scope);
			return;
		}
	}
	if (fact.kind == SemanticFactKind::Variable)
	{
		if (fact.child_count == 0 || fact.child_begin == InvalidIdentityValue)
			return;
		for (std::size_t i = 0; i < fact.child_count; ++i)
			record_constant_initializer(
				semantic_children_[fact.child_begin + i], scope);
		return;
	}
	if (fact.kind == SemanticFactKind::ConstructorAction)
		return;
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
		nonmember_nodes, nonmember_arguments);
	if (overloaded.fact.valid())
		return overloaded;
	if (node.token == SimpleTokenType::OP_AMP &&
		bit_field_fact_for_expression(operand) != NULL)
		throw std::runtime_error("PA12 address-of bit-field is not allowed");
	if (node.token == SimpleTokenType::OP_DEC)
	{
		const BitFieldFact* bit_field = bit_field_fact_for_expression(operand);
		FundamentalType storage_fundamental;
		if (bit_field != NULL && fundamental_of(bit_field->storage_type,
			&storage_fundamental) && storage_fundamental == FundamentalType::Bool)
			throw std::runtime_error(
				"PA12 decrement of a bool bit-field is not allowed");
	}
	TypeId type = expression_object_type(operand.type);
	SemanticValueCategory category = SemanticValueCategory::Prvalue;
	switch (node.token)
	{
	case SimpleTokenType::OP_AMP:
		if (operand.category != SemanticValueCategory::Lvalue)
			throw std::runtime_error("PA12 address-of requires lvalue");
		if (node.children.front().kind == PA10NodeKind::IdExpression)
		{
			const NamePath address_path = name_path(node.children.front());
			const std::vector<ValueRef> values = lookup_value_path(
				address_path, scope);
			if ((address_path.global || address_path.components.size() > 1) &&
				values.size() == 1 && operand.fact.valid() &&
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
		type = make_pointer(expression_object_type(operand.type));
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
			integral_operation_type(operand) : type);
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
		{
			const TypeId operation_operand = integral_id(operand.type) ?
				integral_operation_type(operand) : operand.type;
			type = node.token == SimpleTokenType::OP_COMPL ?
				common_integral_type(operation_operand, operation_operand) :
				common_arithmetic_type(operation_operand, operation_operand);
		}
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
	const SemanticFactId result = make_expression_fact(
		SemanticFactKind::UnaryExpression, type, category, node,
		std::vector<SemanticFactId>(1, operand.fact));
	SemanticFact& result_fact = semantic_facts_[result.value];
	result_fact.canonical_truth = bool_id(type) &&
		node.token == SimpleTokenType::OP_LNOT;
	result_fact.direct_bool_boundary = result_fact.canonical_truth &&
		(result_fact.contains_member_value ||
			semantic_facts_[operand.fact.value].operator_result);
	return ExprInfo(result, type, category, false);
}

} // namespace pa11_semantic_internal
