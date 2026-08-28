#include "pa11_semantic_model.h"

#include <limits>

namespace pa11_semantic_internal
{
using namespace pa11_semantic_storage;

namespace
{

LanguageLinkage language_linkage(const PA10AstNode& node)
{
	if (!node.has_literal)
		throw std::runtime_error("linkage specification has no language literal");
	std::string value;
	for (std::size_t i = 0; i < node.literal.bytes.size(); ++i)
	{
		if (node.literal.bytes[i] == 0)
			break;
		value.push_back(static_cast<char>(node.literal.bytes[i]));
	}
	if (value == "C")
		return LanguageLinkage::C;
	if (value == "C++")
		return LanguageLinkage::Cxx;
	throw std::runtime_error("unsupported language linkage");
}

// PA11's existing value table is keyed by NameId.  This one-way adapter maps
// PA10's typed operator alternative to a fixed vocabulary for that table's
// lookup/presentation only.  It is not a canonical semantic key: operator
// identity remains in the typed PA10 kind/token sidecar, and no consumer
// parses these labels back into an operator.
const char* operator_name_key(PA10OperatorFunctionKind kind,
	SimpleTokenType token)
{
	switch (kind)
	{
	case PA10OperatorFunctionKind::Subscript: return "operatorsubscript";
	case PA10OperatorFunctionKind::Call: return "operatorcall";
	case PA10OperatorFunctionKind::New: return "operatornew";
	case PA10OperatorFunctionKind::Delete: return "operatordelete";
	case PA10OperatorFunctionKind::NewArray: return "operatornewarray";
	case PA10OperatorFunctionKind::DeleteArray: return "operatordeletearray";
	case PA10OperatorFunctionKind::Token:
		switch (token)
		{
		case SimpleTokenType::OP_PLUS: return "operatorplus";
		case SimpleTokenType::OP_MINUS: return "operatorminus";
		case SimpleTokenType::OP_STAR: return "operatorstar";
		case SimpleTokenType::OP_DIV: return "operatordiv";
		case SimpleTokenType::OP_MOD: return "operatormod";
		case SimpleTokenType::OP_XOR: return "operatorxor";
		case SimpleTokenType::OP_AMP: return "operatoramp";
		case SimpleTokenType::OP_BOR: return "operatorbor";
		case SimpleTokenType::OP_COMPL: return "operatorcompl";
		case SimpleTokenType::OP_LNOT: return "operatorlnot";
		case SimpleTokenType::OP_ASS: return "operatorassign";
		case SimpleTokenType::OP_LT: return "operatorlt";
		case SimpleTokenType::OP_GT: return "operatorgt";
		case SimpleTokenType::OP_PLUSASS: return "operatorplusassign";
		case SimpleTokenType::OP_MINUSASS: return "operatorminusassign";
		case SimpleTokenType::OP_STARASS: return "operatorstarassign";
		case SimpleTokenType::OP_DIVASS: return "operatordivassign";
		case SimpleTokenType::OP_MODASS: return "operatormodassign";
		case SimpleTokenType::OP_XORASS: return "operatorxorassign";
		case SimpleTokenType::OP_BANDASS: return "operatorampassign";
		case SimpleTokenType::OP_BORASS: return "operatorborassign";
		case SimpleTokenType::OP_LSHIFT: return "operatorshiftleft";
		case SimpleTokenType::OP_RSHIFT: return "operatorshiftright";
		case SimpleTokenType::OP_LSHIFTASS: return "operatorshiftleftassign";
		case SimpleTokenType::OP_RSHIFTASS: return "operatorshiftrightassign";
		case SimpleTokenType::OP_EQ: return "operatorequal";
		case SimpleTokenType::OP_NE: return "operatornotequal";
		case SimpleTokenType::OP_LE: return "operatorlessequal";
		case SimpleTokenType::OP_GE: return "operatorgreaterequal";
		case SimpleTokenType::OP_LAND: return "operatorlogicaland";
		case SimpleTokenType::OP_LOR: return "operatorlogicalor";
		case SimpleTokenType::OP_INC: return "operatorincrement";
		case SimpleTokenType::OP_DEC: return "operatordecrement";
		case SimpleTokenType::OP_COMMA: return "operatorcomma";
		case SimpleTokenType::OP_ARROWSTAR: return "operatorarrowstar";
		case SimpleTokenType::OP_ARROW: return "operatorarrow";
		case SimpleTokenType::OP_DOTSTAR: return "operatordotstar";
		default: return NULL;
		}
	case PA10OperatorFunctionKind::Conversion:
	case PA10OperatorFunctionKind::Literal:
	case PA10OperatorFunctionKind::None:
		return NULL;
	}
	return NULL;
}

FunctionDeclarationKind special_initializer_kind(const PA10AstNode& init)
{
	if (init.children.size() != 2 ||
		(init.children[1].kind != PA10NodeKind::Initializer &&
		 init.children[1].kind != PA10NodeKind::ParenInitializer) ||
		init.children[1].children.size() != 1)
		return FunctionDeclarationKind::Normal;
	const PA10AstNode& special = init.children[1].children.front();
	if (special.kind != PA10NodeKind::SpecialInitializer ||
		!special.has_token)
		return FunctionDeclarationKind::Normal;
	if (special.token == SimpleTokenType::KW_DELETE)
		return FunctionDeclarationKind::Deleted;
	if (special.token == SimpleTokenType::KW_DEFAULT)
		return FunctionDeclarationKind::Defaulted;
	return FunctionDeclarationKind::Normal;
}

bool has_bare_noexcept(const PA10AstNode& declarator)
{
	for (std::size_t i = 0; i < declarator.children.size(); ++i)
	{
		const PA10AstNode& child = declarator.children[i];
		if (child.kind == PA10NodeKind::FunctionQualifier && child.has_token &&
			child.token == SimpleTokenType::KW_NOEXCEPT &&
			child.children.empty())
			return true;
	}
	return false;
}

}

bool PA11SemanticModel::layout_depends_on_template_parameter(TypeId type) const
{
	if (!type.valid() || type.value >= types_.size())
		throw std::runtime_error("invalid PA11 layout dependency type");
	const TypeKey& key = types_[type.value];
	if (key.kind == TypeKind::Cv)
		return layout_depends_on_template_parameter(key.child);
	if (key.kind == TypeKind::Array)
		return !key.unknown_bound &&
			layout_depends_on_template_parameter(key.child);
	if (key.kind != TypeKind::Named || !key.named.valid() ||
		key.named.value >= named_.size())
		return false;
	return named_[key.named.value].kind == NamedKind::TemplateParameter;
}

bool PA11SemanticModel::type_checkpoint_zero_storage_eligible(TypeId type) const
{
	if (!type.valid() || type.value >= types_.size())
		return false;
	const TypeKey& key = types_[type.value];
	switch (key.kind)
	{
	case TypeKind::Cv:
		return type_checkpoint_zero_storage_eligible(key.child);
	case TypeKind::Fundamental:
		return key.fundamental != FundamentalType::Void;
	case TypeKind::Pointer:
		return true;
	case TypeKind::MemberPointer:
	case TypeKind::LvalueReference:
	case TypeKind::RvalueReference:
	case TypeKind::Function:
		return false;
	case TypeKind::Array:
		return !key.unknown_bound &&
			type_checkpoint_zero_storage_eligible(key.child);
	case TypeKind::Named:
	{
		if (!key.named.valid() || key.named.value >= named_.size())
			return false;
		const NamedRecord& record = named_[key.named.value];
		if (record.kind == NamedKind::Enum)
			return true;
		if (record.kind != NamedKind::Class ||
			record.class_tag == ClassTag::Union || record.has_base ||
			record.has_virtual_member)
			return false;
		const RecordLayout& layout = record_layout(key.named);
		return layout.state == RecordLayoutState::Complete &&
			layout.checkpoint_zero_storage_eligible;
	}
	}
	return false;
}

template<typename Identity>
bool append_lookup_candidate(std::vector<Identity>* candidates,
	Identity candidate)
{
	for (std::size_t i = 0; i < candidates->size(); ++i)
		if ((*candidates)[i] == candidate)
			return false;
	candidates->push_back(candidate);
	return true;
}
PA11SemanticModel::PA11SemanticModel(const PA10Ast& ast)
	: ast_(ast), names_(), name_ids_(), types_(), type_ids_(), named_(),
	  record_layouts_(), record_member_declarations_(),
	  record_member_event_owners_(),
	  bit_field_facts_(), named_record_sidecars_(),
	  named_record_alignment_facts_(), template_function_facts_(),
	  template_function_index_(), template_specialization_facts_(),
	  template_specialization_index_(), scopes_(),
	  unnamed_namespace_index_(),
	  namespace_alias_declaration_points_(),
	  type_declaration_points_(), inline_namespace_declaration_points_(),
	  scope_declaration_points_(), function_definition_points_(),
	  function_bindings_(), friend_lexical_scopes_(), bindings_(), binding_owners_(),
	  binding_sidecars_(), hidden_friend_bindings_(),
	  global_(), deferred_scopes_(),
	  dump_binding_views_(), dump_scope_views_(),
	  anonymous_union_count_(0), anonymous_enum_count_(0), creation_order_(0),
	  lookup_marks_(),
	lookup_generation_(0), lexical_marks_(), lexical_generation_(0),
	lookup_frames_(), declaration_facts_(),
	declaration_fact_index_(), declaration_bindings_(), function_facts_(),
	function_fact_index_(), function_binding_fact_index_(),
	function_default_arguments_(), label_facts_(), label_tables_(),
	class_function_facts_(), constructor_actions_(), constructor_arguments_(),
	destructor_actions_(), lifetime_facts_(),
	constructor_runtime_states_(), constructor_runtime_results_(), constructor_runtime_invalid_(),
	destructor_runtime_states_(), destructor_runtime_results_(),
	destructor_runtime_invalid_(),
	synthetic_function_facts_(), namespace_facts_(), namespace_fact_index_(),
	compound_facts_(), compound_scope_index_(), statement_facts_(),
	statement_fact_index_(), substatement_scope_index_(), semantic_facts_(),
	semantic_children_(), floating_literal_facts_(), floating_literal_bytes_(),
	conversion_facts_(), conversion_base_paths_(), declaration_semantic_ids_(),
	semantic_name_components_(), anonymous_union_fact_index_(),
	builtin_constant_p_name_(), builtin_abort_name_(),
	builtin_abort_binding_(), pa12_render_mode_(false),
	current_language_linkage_(LanguageLinkage::Cxx)
{
	global_ = create_scope(ScopeKind::Namespace, ScopeId(), NameId());
	for (int i = static_cast<int>(FundamentalType::SignedChar);
		i <= static_cast<int>(FundamentalType::NullptrT); ++i)
	{
		TypeKey key;
		key.kind = TypeKind::Fundamental;
		key.fundamental = static_cast<FundamentalType>(i);
		intern_type(key);
	}
	builtin_constant_p_name_ = intern_name("__builtin_constant_p");
	builtin_abort_name_ = intern_name("__builtin_abort");
}
void PA11SemanticModel::analyze()
{
	if (ast_.root.kind != PA10NodeKind::TranslationUnit)
		throw std::runtime_error("PA11 root is not a translation unit");
	for (std::size_t i = 0; i < ast_.root.children.size(); ++i)
		process_declaration(ast_.root.children[i], global_);
	for (std::size_t i = 0; i < deferred_scopes_.size(); ++i)
	{
		const ScopeId scope = deferred_scopes_[i];
		if (scopes_[scope.value].parent.valid())
			scopes_[scopes_[scope.value].parent.value].children.push_back(scope);
	}
}
void PA11SemanticModel::unsupported(const char* feature)
{
	throw std::runtime_error(std::string("PA11 semantic feature not implemented: ") +
		feature);
}
NameId PA11SemanticModel::intern_name(const std::string& name)
{
	const NameId* found = name_ids_.find(name);
	if (found != NULL)
		return *found;
	const NameId result(names_.size());
	names_.push_back(name);
	name_ids_.set(name, result);
	return result;
}
const std::string& PA11SemanticModel::name_text(NameId name) const
{
	if (!name.valid() || name.value >= names_.size())
		throw std::runtime_error("invalid PA11 name identity");
	return names_[name.value];
}
NameId PA11SemanticModel::name_from_spelling(PPSpellingId spelling)
{
	if (spelling == 0 || spelling >= ast_.producer_spellings.size())
		throw std::runtime_error("invalid PA11 producer spelling");
	return intern_name(ast_.producer_spelling(spelling));
}
TypeId PA11SemanticModel::intern_type(const TypeKey& key)
{
	const TypeId* found = type_ids_.find(key);
	if (found != NULL)
		return *found;
	const TypeId result(types_.size());
	types_.push_back(key);
	type_ids_.set(key, result);
	return result;
}
TypeId PA11SemanticModel::fundamental(FundamentalType type) const
{
	TypeKey key;
	key.kind = TypeKind::Fundamental;
	key.fundamental = type;
	const TypeId* found = type_ids_.find(key);
	if (found == NULL)
		throw std::runtime_error("missing PA11 fundamental type");
	return *found;
}
TypeKind PA11SemanticModel::type_kind(TypeId type) const
{
	if (!type.valid() || type.value >= types_.size())
		throw std::runtime_error("invalid PA11 type identity");
	return types_[type.value].kind;
}
NamedRecordId PA11SemanticModel::append_named_record(
	const NamedRecord& record)
{
	const NamedRecordId result(named_.size());
	named_.push_back(record);
	record_layouts_.push_back(RecordLayout());
	record_member_declarations_.push_back(
		std::vector<RecordMemberDeclaration>());
	return result;
}
ScopeId PA11SemanticModel::create_scope(ScopeKind kind, ScopeId parent, NameId name,
	NamedRecordId record ,
	bool inline_namespace , bool attach )
{
	const ScopeId result(scopes_.size());
	const std::size_t depth = parent.valid() ?
		scopes_[parent.value].depth + 1 : 0;
	scopes_.push_back(Scope(kind, parent, name, record,
		inline_namespace, creation_order_++, depth));
	if (parent.valid() && attach)
		scopes_[parent.value].children.push_back(result);
	if (parent.valid() && !attach)
		deferred_scopes_.push_back(result);
	return result;
}
const PA10AstNode* PA11SemanticModel::child_of_kind(const PA10AstNode& node,
	PA10NodeKind kind) const
{
	for (std::size_t i = 0; i < node.children.size(); ++i)
		if (node.children[i].kind == kind)
			return &node.children[i];
	return NULL;
}
bool PA11SemanticModel::is_cv_node(const PA10AstNode& node) const
{
	return node.kind == PA10NodeKind::CvQualifier ||
		(node.kind == PA10NodeKind::DeclSpecifier && node.has_token &&
			(node.token == SimpleTokenType::KW_CONST ||
			 node.token == SimpleTokenType::KW_VOLATILE));
}
unsigned int PA11SemanticModel::cv_bit(const PA10AstNode& node) const
{
	if (!node.has_token)
		return 0;
	if (node.token == SimpleTokenType::KW_CONST)
		return 1u;
	if (node.token == SimpleTokenType::KW_VOLATILE)
		return 2u;
	return 0;
}
NameId PA11SemanticModel::operator_name(PA10OperatorFunctionKind kind,
	SimpleTokenType token)
{
	const char* key = operator_name_key(kind, token);
	if (key == NULL)
		unsupported("operator function identity");
	return intern_name(key);
}
NamePath PA11SemanticModel::name_path(const PA10AstNode& node)
{
	if (node.name_prefix_count != 0)
		unsupported("decltype-qualified names");
	NamePath result;
	result.global = node.global_name;
	for (std::size_t i = 0; i < node.name_parts.size(); ++i)
	{
		const PA10NameComponent& part = node.name_parts[i];
		if (part.has_template_id)
			unsupported("template-ids");
		result.components.push_back(name_from_spelling(part.spelling));
	}
	if (result.components.empty() && node.producer_spelling != 0)
		result.components.push_back(name_from_spelling(node.producer_spelling));
	if (node.unqualified_id_kind == PA10UnqualifiedIdKind::OperatorFunction)
		result.components.push_back(operator_name(node.operator_function_kind,
			node.operator_token));
	if (result.components.empty())
		throw std::runtime_error("PA11 name has no semantic component");
	return result;
}
TypeId PA11SemanticModel::strip_cv_type(TypeId type) const
{
	return type_kind(type) == TypeKind::Cv ? types_[type.value].child : type;
}
TypeId PA11SemanticModel::strip_reference_type(TypeId type) const
{
	const TypeKind kind = type_kind(type);
	if (kind == TypeKind::LvalueReference ||
		kind == TypeKind::RvalueReference)
		return types_[type.value].child;
	return type;
}
bool PA11SemanticModel::find_declarator_name(const PA10AstNode& node,
	DeclaratorName* result)
{
	if (node.kind == PA10NodeKind::Identifier &&
		(node.producer_spelling != 0 || !node.name_parts.empty() ||
			node.global_name || node.name_prefix_count != 0 ||
			node.unqualified_id_kind == PA10UnqualifiedIdKind::OperatorFunction))
	{
		result->path = name_path(node);
		if (node.unqualified_id_kind == PA10UnqualifiedIdKind::OperatorFunction)
		{
			result->operator_function = true;
			result->operator_function_kind = node.operator_function_kind;
			result->operator_token = node.operator_token;
		}
		return true;
	}
	for (std::size_t i = 0; i < node.children.size(); ++i)
	{
		if (node.children[i].kind == PA10NodeKind::PtrOperator)
			continue;
		if (find_declarator_name(node.children[i], result))
			return true;
	}
	return false;
}
DeclaratorName PA11SemanticModel::declarator_name(const PA10AstNode& node)
{
	DeclaratorName result;
	result.found = find_declarator_name(node, &result);
	return result;
}
ClassTag PA11SemanticModel::class_tag(const PA10AstNode& node) const
{
	const PA10AstNode* key = child_of_kind(node, PA10NodeKind::ClassKey);
	if (key == NULL || !key->has_token)
		throw std::runtime_error("class declaration has no class-key");
	if (key->token == SimpleTokenType::KW_CLASS)
		return ClassTag::Class;
	if (key->token == SimpleTokenType::KW_UNION)
		return ClassTag::Union;
	return ClassTag::Struct;
}
TypeId PA11SemanticModel::named_type(NamedRecordId named)
{
	TypeKey key;
	key.kind = TypeKind::Named;
	key.named = named;
	return intern_type(key);
}
NamedRecordId PA11SemanticModel::named_record_for_type(TypeId type) const
{
	TypeId cursor = type;
	if (type_kind(cursor) == TypeKind::Cv)
		cursor = types_[cursor.value].child;
	if (type_kind(cursor) != TypeKind::Named)
		return NamedRecordId();
	return types_[cursor.value].named;
}
ScopeId PA11SemanticModel::class_scope_for_type(TypeId type) const
{
	const NamedRecordId record = named_record_for_type(type);
	if (!record.valid() || record.value >= named_.size() ||
		named_[record.value].kind != NamedKind::Class)
		return ScopeId();
	return named_[record.value].scope;
}
ScopeId PA11SemanticModel::scope_for_type(TypeId type) const
{
	const NamedRecordId record = named_record_for_type(type);
	if (!record.valid() || record.value >= named_.size())
		return ScopeId();
	if (named_[record.value].kind == NamedKind::Class ||
		named_[record.value].kind == NamedKind::Enum)
		return named_[record.value].scope;
	return ScopeId();
}
bool PA11SemanticModel::object_type(TypeId type) const
{
	type = strip_cv_type(type);
	if (!type.valid())
		return false;
	switch (type_kind(type))
	{
	case TypeKind::Fundamental:
		return types_[type.value].fundamental != FundamentalType::Void;
	case TypeKind::Named:
	case TypeKind::Pointer:
	case TypeKind::MemberPointer:
	case TypeKind::Array:
		return true;
	case TypeKind::Cv:
		return object_type(types_[type.value].child);
	case TypeKind::LvalueReference:
	case TypeKind::RvalueReference:
	case TypeKind::Function:
		return false;
	}
	return false;
}
bool PA11SemanticModel::complete_object_type(TypeId type) const
{
	type = strip_cv_type(type);
	if (!object_type(type))
		return false;
	switch (type_kind(type))
	{
	case TypeKind::Array:
		return !types_[type.value].unknown_bound &&
			complete_object_type(types_[type.value].child);
	case TypeKind::Named:
	{
		const NamedRecordId record = named_record_for_type(type);
		if (!record.valid() || record.value >= named_.size())
			return false;
		return named_[record.value].kind == NamedKind::Enum ||
			(named_[record.value].kind == NamedKind::Class &&
				record.value < record_layouts_.size() &&
				record_layouts_[record.value].state ==
				RecordLayoutState::Complete);
	}
	case TypeKind::Fundamental:
	case TypeKind::Pointer:
	case TypeKind::MemberPointer:
		return true;
	case TypeKind::Cv:
		return complete_object_type(types_[type.value].child);
	case TypeKind::LvalueReference:
	case TypeKind::RvalueReference:
	case TypeKind::Function:
		return false;
	}
	return false;
}
bool PA11SemanticModel::pointer_convertible(TypeId source, TypeId target) const
{
	source = strip_cv_type(source);
	target = strip_cv_type(target);
	if (type_kind(source) != TypeKind::Pointer ||
		type_kind(target) != TypeKind::Pointer)
		return false;
	const TypeKey& source_key = types_[source.value];
	const TypeKey& target_key = types_[target.value];
	// Value conversion discards pointer-object cv; pointee cv remains typed.
	// Reference binding uses qualification_convertible and preserves object cv.
	TypeId source_pointee = source_key.child;
	TypeId target_pointee = target_key.child;
	FundamentalType target_fundamental;
	if (fundamental_of(target_pointee, &target_fundamental) &&
		target_fundamental == FundamentalType::Void)
	{
		if (object_type(source_pointee))
			return (cv_qualifiers(source_pointee) &
				~cv_qualifiers(target_pointee)) == 0;
	}
	return qualification_convertible_impl(source_pointee, target_pointee, true);
}
TypeId PA11SemanticModel::pointer_subtraction_common_type(TypeId left,
	TypeId right)
{
	left = strip_cv_type(left);
	right = strip_cv_type(right);
	if (type_kind(left) != TypeKind::Pointer ||
		type_kind(right) != TypeKind::Pointer)
		return TypeId();
	const TypeId left_pointee = types_[left.value].child;
	const TypeId right_pointee = types_[right.value].child;
	if (!complete_object_type(left_pointee) ||
		!complete_object_type(right_pointee))
		return TypeId();
	const TypeId left_unqualified = strip_cv_type(left_pointee);
	const TypeId right_unqualified = strip_cv_type(right_pointee);
	if (left_unqualified != right_unqualified)
		return TypeId();
	const unsigned int qualifiers = cv_qualifiers(left_pointee) |
		cv_qualifiers(right_pointee);
	return make_pointer(make_cv(left_unqualified, qualifiers));
}
TypeId PA11SemanticModel::conditional_pointer_common_type(TypeId left,
	TypeId right)
{
	if (pointer_common_type_convertible(left, right))
		return strip_top_cv_type(right);
	if (pointer_common_type_convertible(right, left))
		return strip_top_cv_type(left);
	left = strip_top_cv_type(left);
	right = strip_top_cv_type(right);
	if (type_kind(left) != TypeKind::Pointer ||
		type_kind(right) != TypeKind::Pointer)
		return TypeId();
	const TypeId left_pointee = types_[left.value].child;
	const TypeId right_pointee = types_[right.value].child;
	const TypeId left_unqualified = strip_cv_type(left_pointee);
	const TypeId right_unqualified = strip_cv_type(right_pointee);
	if (left_unqualified != right_unqualified ||
		!object_type(left_unqualified))
		return TypeId();
	const unsigned int qualifiers = cv_qualifiers(left_pointee) |
		cv_qualifiers(right_pointee);
	return make_pointer(make_cv(left_unqualified, qualifiers));
}
bool PA11SemanticModel::direct_value_exists(ScopeId scope, NameId name) const
{
	const ValueList* found = scopes_[scope.value].values.find(name);
	return found != NULL && !found->entries.empty();
}
bool PA11SemanticModel::direct_namespace_exists(ScopeId scope, NameId name) const
{
	const Scope& current = scopes_[scope.value];
	return current.namespaces.find(name) != NULL ||
		current.namespace_aliases.find(name) != NULL;
}
ScopeId PA11SemanticModel::named_namespace(ScopeId parent, NameId name)
{
	Scope& current = scopes_[parent.value];
	const ScopeId* found = current.namespaces.find(name);
	if (found != NULL)
		return *found;
	if (current.namespace_aliases.find(name) != NULL ||
		current.types.find(name) != NULL ||
		direct_value_exists(parent, name))
		throw std::runtime_error("namespace conflicts with binding");
	return create_named_namespace(parent, name);
}
ScopeId PA11SemanticModel::create_named_namespace(ScopeId parent, NameId name)
{
	const ScopeId result = create_scope(ScopeKind::Namespace, parent, name);
	scopes_[parent.value].namespaces.set(name, result);
	return result;
}
ScopeId PA11SemanticModel::lookup_namespace_here(ScopeId scope, NameId name,
	SourcePoint point) const
{
	const Scope& current = scopes_[scope.value];
	const ScopeId* found = current.namespaces.find(name);
	if (found != NULL)
		return *found;
	found = current.namespace_aliases.find(name);
	return found == NULL || !namespace_alias_visible_at(scope, name, point) ?
		ScopeId() : *found;
}
SourcePoint PA11SemanticModel::lookup_source_point(ScopeId start) const
{
	for (ScopeId scope = start; scope.valid();
		scope = scopes_[scope.value].parent)
	{
		const SourcePoint* found = function_definition_points_.find(scope);
		if (found != NULL)
			return *found;
	}
	return SourcePoint();
}
bool PA11SemanticModel::scope_visible_at(ScopeId scope, SourcePoint point) const
{
	if (!scope.valid() || !point.valid() || scope == global_ ||
		scopes_[scope.value].kind != ScopeKind::Namespace)
		return true;
	const SourcePoint* declaration = scope_declaration_points_.find(scope);
	return declaration == NULL || !declaration->valid() ||
		declaration->value <= point.value;
}
bool PA11SemanticModel::relation_visible_at(ScopeId owner,
	SourcePoint declaration_point, SourcePoint point) const
{
	// Local block using directives retain their existing PA12 formation
	// behavior.  Namespace relations, including the implicit unnamed-
	// namespace relation, obey the function declaration point.
	if (!point.valid() || !declaration_point.valid() ||
		scopes_[owner.value].kind != ScopeKind::Namespace)
		return true;
	return declaration_point.value <= point.value;
}
void PA11SemanticModel::begin_lookup() const
{
	if (lookup_marks_.size() < scopes_.size())
		lookup_marks_.resize(scopes_.size(), 0);
	++lookup_generation_;
	if (lookup_generation_ == 0)
	{
		std::fill(lookup_marks_.begin(), lookup_marks_.end(), 0);
		lookup_generation_ = 1;
	}
}
bool PA11SemanticModel::mark_lookup_scope(ScopeId scope) const
{
	if (!scope.valid() || scope.value >= scopes_.size())
		return false;
	if (lookup_marks_[scope.value] == lookup_generation_)
		return false;
	lookup_marks_[scope.value] = lookup_generation_;
	return true;
}
void PA11SemanticModel::prepare_unqualified_lookup(ScopeId start) const
{
	if (lexical_marks_.size() < scopes_.size())
		lexical_marks_.resize(scopes_.size(), 0);
	++lexical_generation_;
	if (lexical_generation_ == 0)
	{
		std::fill(lexical_marks_.begin(), lexical_marks_.end(), 0);
		lexical_generation_ = 1;
	}
	for (ScopeId scope = start; scope.valid();
		scope = scopes_[scope.value].parent)
		lexical_marks_[scope.value] = lexical_generation_;
}
bool PA11SemanticModel::lexical_scope_is_applicable(ScopeId scope) const
{
	return scope.valid() && scope.value < lexical_marks_.size() &&
		lexical_marks_[scope.value] == lexical_generation_;
}
ScopeId PA11SemanticModel::common_ancestor(ScopeId left, ScopeId right) const
{
	if (!left.valid() || !right.valid())
		return ScopeId();
	while (scopes_[left.value].depth > scopes_[right.value].depth)
		left = scopes_[left.value].parent;
	while (scopes_[right.value].depth > scopes_[left.value].depth)
		right = scopes_[right.value].parent;
	while (left != right)
	{
		left = scopes_[left.value].parent;
		right = scopes_[right.value].parent;
	}
	return left;
}
void PA11SemanticModel::append_effective_using_targets(ScopeId level,
	std::vector<ScopeId>* targets, SourcePoint point) const
{
	const Scope& current = scopes_[level.value];
	for (std::size_t i = current.effective_using_directives.size(); i != 0; --i)
	{
		const EffectiveUsingDirective& directive =
			current.effective_using_directives[i - 1];
		if (!lexical_scope_is_applicable(directive.lexical_scope) ||
			!relation_visible_at(level, directive.declaration_point, point))
			continue;
		// Repeated target edges are harmless: the graph generation mark
		// suppresses a second traversal without a per-level deduplication scan.
		targets->push_back(directive.target);
	}
}
void PA11SemanticModel::reset_lookup_frames(LookupGraphKind kind, ScopeId start) const
{
	lookup_frames_.clear();
	lookup_frames_.push_back(LookupFrame(kind, start));
}
ScopeId PA11SemanticModel::lookup_namespace_graph(ScopeId start, NameId name,
	bool include_using, SourcePoint point) const
{
	std::vector<ScopeId> candidates;
	reset_lookup_frames(LookupGraphKind::Namespace, start);
	while (!lookup_frames_.empty())
	{
		LookupFrame& frame = lookup_frames_.back();
		if (!frame.entered)
		{
			frame.entered = true;
			if (!mark_lookup_scope(frame.scope))
			{
				lookup_frames_.pop_back();
				continue;
			}
			if (!scope_visible_at(frame.scope, point))
			{
				lookup_frames_.pop_back();
				continue;
			}
			const ScopeId direct = lookup_namespace_here(frame.scope, name, point);
			if (direct.valid() && scope_visible_at(direct, point))
			{
				append_lookup_candidate(&candidates, direct);
				if (lookup_frames_.size() == 1)
					return direct;
				frame.next_using = 0;
				frame.next_inline_child = 0;
			}
			else
			{
				frame.next_using = include_using ?
					scopes_[frame.scope.value].using_directives.size() : 0;
				frame.next_inline_child = scopes_[frame.scope.value].children.size();
			}
		}
		if (frame.next_using != 0)
		{
			const UsingDirectiveRelation& relation =
				scopes_[frame.scope.value].using_directives[--frame.next_using];
			if (!relation_visible_at(frame.scope, relation.declaration_point,
				point))
				continue;
			lookup_frames_.push_back(
				LookupFrame(LookupGraphKind::Namespace, relation.target));
			continue;
		}
		bool pushed = false;
		while (frame.next_inline_child != 0)
		{
			const ScopeId child = scopes_[frame.scope.value].children[
				--frame.next_inline_child];
			if (!scopes_[child.value].inline_namespace ||
				!inline_namespace_visible_at(child, point) ||
				!scope_visible_at(child, point))
				continue;
			lookup_frames_.push_back(
				LookupFrame(LookupGraphKind::Namespace, child));
			pushed = true;
			break;
		}
		if (pushed)
			continue;
		lookup_frames_.pop_back();
	}
	if (candidates.size() > 1)
		throw std::runtime_error("ambiguous namespace lookup");
	return candidates.empty() ? ScopeId() : candidates.front();
}
ScopeId PA11SemanticModel::lookup_namespace_unqualified(ScopeId start,
	NameId name, SourcePoint point) const
{
	prepare_unqualified_lookup(start);
	ScopeId scope = start;
	while (scope.valid())
	{
		begin_lookup();
		const ScopeId direct = lookup_namespace_graph(scope, name, false, point);
		if (direct.valid())
			return direct;
		std::vector<ScopeId> targets;
		append_effective_using_targets(scope, &targets, point);
		std::vector<ScopeId> found;
		for (std::size_t i = 0; i < targets.size(); ++i)
		{
			const ScopeId candidate = lookup_namespace_graph(targets[i], name,
				true, point);
			if (!candidate.valid())
				continue;
			append_lookup_candidate(&found, candidate);
			if (found.size() > 1)
				throw std::runtime_error("ambiguous namespace lookup");
		}
		if (!found.empty())
			return found.front();
		if (scope == global_)
			break;
		scope = scopes_[scope.value].parent;
	}
	return ScopeId();
}
TypeId PA11SemanticModel::lookup_type_graph(ScopeId start, NameId name,
	bool include_using, SourcePoint point, BindingId* declaration) const
{
	if (declaration != NULL)
		*declaration = BindingId();
	std::vector<TypeLookupCandidate> candidates;
	reset_lookup_frames(LookupGraphKind::Type, start);
	while (!lookup_frames_.empty())
	{
		LookupFrame& frame = lookup_frames_.back();
		if (!frame.entered)
		{
			frame.entered = true;
			if (!mark_lookup_scope(frame.scope))
			{
				lookup_frames_.pop_back();
				continue;
			}
			if (!scope_visible_at(frame.scope, point))
			{
				lookup_frames_.pop_back();
				continue;
			}
			const Scope& current = scopes_[frame.scope.value];
			TypeLookupCandidate direct;
			const TypeId* found = current.types.find(name);
			if (found != NULL && type_visible_at(frame.scope, name, point))
				direct = TypeLookupCandidate(*found,
					type_declaration_identity(frame.scope, name));
			found = current.using_types.find(name);
			if (!direct.type.valid() && found != NULL &&
				type_visible_at(frame.scope, name, point))
				direct = TypeLookupCandidate(*found,
					type_declaration_identity(frame.scope, name));
			if (direct.type.valid())
			{
				append_lookup_candidate(&candidates, direct);
				if (lookup_frames_.size() == 1)
				{
					if (declaration != NULL)
						*declaration = direct.declaration;
					return direct.type;
				}
				frame.next_using = 0;
				frame.next_inline_child = 0;
			}
			else
			{
				frame.next_using = include_using ? current.using_directives.size() : 0;
				frame.next_inline_child = current.children.size();
			}
		}
		if (frame.next_using != 0)
		{
			const UsingDirectiveRelation& relation =
				scopes_[frame.scope.value].using_directives[--frame.next_using];
			if (!relation_visible_at(frame.scope, relation.declaration_point,
				point))
				continue;
			lookup_frames_.push_back(
				LookupFrame(LookupGraphKind::Type, relation.target));
			continue;
		}
		bool pushed = false;
		while (frame.next_inline_child != 0)
		{
			const ScopeId child = scopes_[frame.scope.value].children[
				--frame.next_inline_child];
			if (!scopes_[child.value].inline_namespace ||
				!inline_namespace_visible_at(child, point) ||
				!scope_visible_at(child, point))
				continue;
			lookup_frames_.push_back(
				LookupFrame(LookupGraphKind::Type, child));
			pushed = true;
			break;
		}
		if (pushed)
			continue;
		lookup_frames_.pop_back();
	}
	if (candidates.size() > 1)
		throw std::runtime_error("ambiguous type lookup");
	if (candidates.empty())
		return TypeId();
	if (declaration != NULL)
		*declaration = candidates.front().declaration;
	return candidates.front().type;
}
TypeId PA11SemanticModel::lookup_type_unqualified(ScopeId start, NameId name,
	SourcePoint point, BindingId* declaration) const
{
	if (declaration != NULL) *declaration = BindingId();
	prepare_unqualified_lookup(start);
	ScopeId scope = start;
	while (scope.valid())
	{
		begin_lookup();
		BindingId direct_declaration;
		const TypeId direct = lookup_type_graph(scope, name, false, point, &direct_declaration);
		if (direct.valid())
		{
			if (declaration != NULL) *declaration = direct_declaration;
			return direct;
		}
		// An unqualified type used in a derived member declaration also sees
		// inherited type members.  Search only this class's typed direct-base
		// chain before continuing to enclosing lexical scopes; this preserves
		// hiding while avoiding an unrelated namespace/program scan.
		if (scopes_[scope.value].kind == ScopeKind::Class &&
			scopes_[scope.value].record.valid())
		{
			std::vector<NamedRecordId> bases;
			if (!direct_base_chain(named_type(scopes_[scope.value].record), &bases))
				throw std::runtime_error("type lookup base relation is invalid");
			for (std::size_t i = 0; i < bases.size(); ++i)
			{
				if (!bases[i].valid() || bases[i].value >= named_.size() ||
					!named_[bases[i].value].scope.valid())
					throw std::runtime_error("type lookup base owner is invalid");
				begin_lookup();
				BindingId base_declaration;
				const TypeId inherited = lookup_type_graph(
					named_[bases[i].value].scope, name, true, point,
					&base_declaration);
				if (!inherited.valid())
					continue;
				if (declaration != NULL) *declaration = base_declaration;
				return inherited;
			}
		}
		// Namespace-owned friend bodies retain one exact class lexical type
		// scope.  Access friendship is a separate relation and is not a
		// substitute for the definition's lexical owner.
		if (scopes_[scope.value].kind == ScopeKind::Function)
		{
			const FriendLexicalScopeRelation* lexical =
				friend_lexical_scopes_.find(scope);
			if (lexical != NULL)
			{
				if (!lexical->class_scope.valid() ||
					lexical->class_scope.value >= scopes_.size() ||
					scopes_[lexical->class_scope.value].kind != ScopeKind::Class ||
					!lexical->class_record.valid() ||
					lexical->class_record.value >= named_.size() ||
					named_[lexical->class_record.value].kind != NamedKind::Class ||
					named_[lexical->class_record.value].scope != lexical->class_scope ||
					scopes_[lexical->class_scope.value].record !=
						lexical->class_record)
					throw std::runtime_error(
						"PA11 friend lexical type relation is invalid");
				BindingId friend_declaration;
				begin_lookup();
				const TypeId friend_type = lookup_type_graph(
					lexical->class_scope, name, true, point,
					&friend_declaration);
				if (friend_type.valid())
				{
					if (declaration != NULL) *declaration = friend_declaration;
					return friend_type;
				}
			}
		}
		std::vector<ScopeId> targets;
		append_effective_using_targets(scope, &targets, point);
		std::vector<TypeLookupCandidate> found;
		for (std::size_t i = 0; i < targets.size(); ++i)
		{
			BindingId candidate_declaration;
			const TypeId candidate = lookup_type_graph(targets[i], name, true, point, &candidate_declaration);
			if (!candidate.valid()) continue;
			append_lookup_candidate(&found, TypeLookupCandidate(candidate, candidate_declaration));
			if (found.size() > 1) throw std::runtime_error("ambiguous type lookup");
		}
		if (!found.empty())
		{
			if (declaration != NULL) *declaration = found.front().declaration;
			return found.front().type;
		}
		if (scope == global_)
			break;
		scope = scopes_[scope.value].parent;
	}
	return TypeId();
}
TypeId PA11SemanticModel::lookup_type_qualified(ScopeId scope, NameId name,
	SourcePoint point, BindingId* declaration) const
{
	begin_lookup();
	return lookup_type_graph(scope, name, true, point, declaration);
}
bool PA11SemanticModel::lookup_value_graph(ScopeId start, NameId name,
	std::vector<ValueRef>* result, bool include_using, SourcePoint point) const
{
	reset_lookup_frames(LookupGraphKind::Value, start);
	while (!lookup_frames_.empty())
	{
		LookupFrame& frame = lookup_frames_.back();
		if (!frame.entered)
		{
			frame.entered = true;
			if (!mark_lookup_scope(frame.scope))
			{
				lookup_frames_.pop_back();
				continue;
			}
			if (!scope_visible_at(frame.scope, point))
			{
				lookup_frames_.pop_back();
				continue;
			}
			const Scope& current = scopes_[frame.scope.value];
			const ValueList* found = current.values.find(name);
			if (found != NULL)
			{
				bool have_visible = false;
				for (std::size_t i = 0; i < found->entries.size(); ++i)
				{
					const ValueEntry& entry = found->entries[i];
					if (!relation_visible_at(frame.scope, entry.declaration_point,
						point))
						continue;
					result->push_back(ValueRef(entry.origin, entry.binding));
					have_visible = true;
				}
				if (have_visible)
					return true;
			}
			frame.next_using = include_using ? current.using_directives.size() : 0;
			frame.next_inline_child = current.children.size();
		}
		if (frame.next_using != 0)
		{
			const UsingDirectiveRelation& relation =
				scopes_[frame.scope.value].using_directives[--frame.next_using];
			if (!relation_visible_at(frame.scope, relation.declaration_point,
				point))
				continue;
			lookup_frames_.push_back(
				LookupFrame(LookupGraphKind::Value, relation.target));
			continue;
		}
		bool pushed = false;
		while (frame.next_inline_child != 0)
		{
			const ScopeId child = scopes_[frame.scope.value].children[
				--frame.next_inline_child];
			if (!scopes_[child.value].inline_namespace ||
				!inline_namespace_visible_at(child, point) ||
				!scope_visible_at(child, point))
				continue;
			lookup_frames_.push_back(
				LookupFrame(LookupGraphKind::Value, child));
			pushed = true;
			break;
		}
		if (pushed)
			continue;
		lookup_frames_.pop_back();
	}
	return false;
}
std::vector<ValueRef> PA11SemanticModel::lookup_value_unqualified(
	ScopeId start, NameId name, SourcePoint point) const
{
	prepare_unqualified_lookup(start);
	ScopeId scope = start;
	while (scope.valid())
	{
		begin_lookup();
		std::vector<ValueRef> found;
		const bool have_direct = lookup_value_graph(scope, name, &found, false,
			point);
		if (have_direct)
			return found;
		// Namespace-owned friend bodies retain one exact class lexical value
		// scope; access friendship is deliberately not used for lookup.
		if (scopes_[scope.value].kind == ScopeKind::Function)
		{
			const FriendLexicalScopeRelation* lexical =
				friend_lexical_scopes_.find(scope);
			if (lexical != NULL)
			{
				if (!lexical->class_scope.valid() ||
					lexical->class_scope.value >= scopes_.size() ||
					scopes_[lexical->class_scope.value].kind != ScopeKind::Class ||
					!lexical->class_record.valid() ||
					lexical->class_record.value >= named_.size() ||
					named_[lexical->class_record.value].kind != NamedKind::Class ||
					named_[lexical->class_record.value].scope != lexical->class_scope ||
					scopes_[lexical->class_scope.value].record !=
						lexical->class_record)
					throw std::runtime_error(
						"PA11 friend lexical value relation is invalid");
				begin_lookup();
				std::vector<ValueRef> friend_values;
				if (lookup_value_graph(lexical->class_scope, name,
					&friend_values, true, point))
				{
					return friend_values;
				}
			}
		}
		std::vector<ScopeId> targets;
		append_effective_using_targets(scope, &targets, point);
		for (std::size_t i = 0; i < targets.size(); ++i)
		{
			std::vector<ValueRef> nominated;
			if (!lookup_value_graph(targets[i], name, &nominated, true, point))
				continue;
			found.insert(found.end(), nominated.begin(), nominated.end());
		}
		if (!found.empty())
			return found;
		if (scope == global_)
			break;
		scope = scopes_[scope.value].parent;
	}
	return std::vector<ValueRef>();
}
std::vector<ValueRef> PA11SemanticModel::lookup_value_path(
	const NamePath& path, ScopeId start, SourcePoint point) const
{
	if (path.components.empty())
		return std::vector<ValueRef>();
	if (!point.valid())
		point = lookup_source_point(start);
	if (path.components.size() == 1)
	{
		if (!path.global)
			return lookup_value_unqualified(start, path.last(), point);
		begin_lookup();
		std::vector<ValueRef> result;
		lookup_value_graph(global_, path.last(), &result, true, point);
		return result;
	}
	std::vector<NameId> prefix(path.components.begin(), path.components.end() - 1);
	const ScopeId scope = path.global ? resolve_global_qualifier_scope(prefix,
		point) :
		resolve_qualifier_scope(prefix, start, point);
	if (!scope.valid())
		return std::vector<ValueRef>();
	begin_lookup();
	std::vector<ValueRef> result;
	lookup_value_graph(scope, path.last(), &result, true, point);
	return result;
}
ScopeId PA11SemanticModel::resolve_qualifier_scope(const std::vector<NameId>& components,
	ScopeId start, SourcePoint point) const
{
	if (components.empty())
		return ScopeId();
	if (!point.valid())
		point = lookup_source_point(start);
	ScopeId scope = lookup_namespace_unqualified(start, components[0], point);
	std::size_t at = 1;
	if (!scope.valid())
	{
		const TypeId type = lookup_type_unqualified(start, components[0], point);
		scope = scope_for_type(type);
		if (!scope.valid())
			return ScopeId();
	}
	for (; at < components.size(); ++at)
	{
		begin_lookup();
		const ScopeId next_namespace = lookup_namespace_graph(scope, components[at],
			true, point);
		if (next_namespace.valid())
		{
			scope = next_namespace;
			continue;
		}
		const TypeId type = lookup_type_qualified(scope, components[at], point);
		scope = scope_for_type(type);
		if (!scope.valid())
			return ScopeId();
	}
	return scope;
}
TypeId PA11SemanticModel::lookup_type_path(const NamePath& path, ScopeId start,
	SourcePoint point, BindingId* declaration) const
{
	if (declaration != NULL) *declaration = BindingId();
	if (path.components.empty())
		return TypeId();
	if (!point.valid())
		point = lookup_source_point(start);
	if (path.components.size() == 1)
	{
		const TypeId found = path.global ?
			lookup_type_qualified(global_, path.last(), point, declaration) :
			lookup_type_unqualified(start, path.last(), point, declaration);
		if (found.valid())
			return found;
		if (name_text(path.last()) == "nullptr_t")
			return fundamental(FundamentalType::NullptrT);
		return found;
	}
	std::vector<NameId> prefix(path.components.begin(), path.components.end() - 1);
	const ScopeId scope = path.global ?
		resolve_global_qualifier_scope(prefix, point) :
		resolve_qualifier_scope(prefix, start, point);
	return !scope.valid() ? TypeId() :
		lookup_type_qualified(scope, path.last(), point, declaration);
}
ScopeId PA11SemanticModel::resolve_global_qualifier_scope(
	const std::vector<NameId>& components, SourcePoint point) const
{
	if (components.empty())
		return global_;
	begin_lookup();
	ScopeId scope = lookup_namespace_graph(global_, components[0], true, point);
	if (!scope.valid())
	{
		const TypeId type = lookup_type_qualified(global_, components[0], point);
		scope = scope_for_type(type);
	}
	for (std::size_t i = 1; i < components.size() && scope.valid(); ++i)
	{
		begin_lookup();
		const ScopeId next_namespace = lookup_namespace_graph(scope,
			components[i], true, point);
		if (next_namespace.valid())
			scope = next_namespace;
		else
			scope = scope_for_type(lookup_type_qualified(scope,
				components[i], point));
	}
	return scope;
}
ScopeId PA11SemanticModel::resolve_namespace_path(const NamePath& path,
	ScopeId start, SourcePoint point) const
{
	if (path.components.empty())
		return ScopeId();
	if (!point.valid())
		point = lookup_source_point(start);
	if (path.components.size() == 1)
		return path.global ? lookup_namespace_graph(global_, path.last(),
			false, point) : lookup_namespace_unqualified(start, path.last(), point);
	std::vector<NameId> prefix(path.components.begin(), path.components.end() - 1);
	ScopeId scope = path.global ? resolve_global_qualifier_scope(prefix, point) :
		resolve_qualifier_scope(prefix, start, point);
	if (!scope.valid())
		return ScopeId();
	begin_lookup();
	return lookup_namespace_graph(scope, path.last(), true, point);
}
BindingId PA11SemanticModel::store_binding(ScopeId scope, const Binding& binding,
	std::size_t position )
{
	if (!scope.valid() || scope.value >= scopes_.size()) { throw std::runtime_error("invalid PA11 binding owner scope"); } if (bindings_.size() != binding_owners_.size()) { throw std::runtime_error("PA11 binding owner index is out of sync"); }
	const BindingId result(bindings_.size());
	bindings_.push_back(binding); binding_owners_.push_back(scope);
	Scope& current = scopes_[scope.value];
	if (position == InvalidIdentityValue || position > current.bindings.size())
		position = current.bindings.size();
	current.bindings.insert(current.bindings.begin() + position, result);
	return result;
}
const BindingSidecar* PA11SemanticModel::binding_sidecar(BindingId id) const
{
	return id.valid() ? binding_sidecars_.find(id) : NULL;
}
void PA11SemanticModel::set_binding_sidecar(BindingId id,
	const BindingSidecar& sidecar)
{
	if (!id.valid() || id.value >= bindings_.size())
		throw std::runtime_error("invalid PA11 binding sidecar identity");
	binding_sidecars_.set(id, sidecar);
}
void PA11SemanticModel::record_function_declarator(BindingId binding_id,
	const DeclaratorName& name, const PA10AstNode& declarator,
	FunctionDeclarationKind declaration_kind)
{
	const bool nonthrowing = has_bare_noexcept(declarator);
	const BindingSidecar* existing = binding_sidecar(binding_id);
	if (!name.operator_function && !nonthrowing &&
		declaration_kind == FunctionDeclarationKind::Normal)
		return;
	BindingSidecar sidecar;
	if (existing != NULL)
		sidecar = *existing;
	if (name.operator_function)
	{
		if (sidecar.operator_function_kind != PA10OperatorFunctionKind::None &&
			(sidecar.operator_function_kind != name.operator_function_kind ||
			 sidecar.operator_token != name.operator_token))
			throw std::runtime_error("conflicting operator declaration identity");
		sidecar.operator_function_kind = name.operator_function_kind;
		sidecar.operator_token = name.operator_token;
	}
	if (nonthrowing)
		sidecar.nonthrowing = true;
	if (declaration_kind != FunctionDeclarationKind::Normal)
	{
		if (sidecar.declaration_kind != FunctionDeclarationKind::Normal &&
			sidecar.declaration_kind != declaration_kind)
			throw std::runtime_error("conflicting function declaration kind");
		sidecar.declaration_kind = declaration_kind;
	}
	set_binding_sidecar(binding_id, sidecar);
}
FunctionDeclarationKind PA11SemanticModel::function_declaration_kind(
	BindingId binding_id) const
{
	const BindingSidecar* sidecar = binding_sidecar(binding_id);
	return sidecar == NULL ? FunctionDeclarationKind::Normal :
		sidecar->declaration_kind;
}
bool PA11SemanticModel::is_static_member(BindingId id) const
{
	const BindingSidecar* sidecar = binding_sidecar(id);
	return sidecar != NULL && sidecar->static_member;
}
void PA11SemanticModel::mark_static_member(BindingId id)
{
	if (!id.valid() || id.value >= bindings_.size())
		throw std::runtime_error("invalid static member binding identity");
	BindingSidecar sidecar;
	const BindingSidecar* existing = binding_sidecar(id);
	if (existing != NULL)
		sidecar = *existing;
	sidecar.static_member = true;
	set_binding_sidecar(id, sidecar);
}
MemberAccess PA11SemanticModel::member_access(BindingId id) const
{
	const BindingSidecar* sidecar = binding_sidecar(id);
	return sidecar == NULL ? MemberAccess::Public : sidecar->member_access;
}
void PA11SemanticModel::set_member_access(BindingId id, MemberAccess access)
{
	if (!id.valid() || id.value >= bindings_.size())
		throw std::runtime_error("invalid member access binding identity");
	BindingSidecar sidecar;
	const BindingSidecar* existing = binding_sidecar(id);
	if (existing != NULL)
		sidecar = *existing;
	sidecar.member_access = access;
	set_binding_sidecar(id, sidecar);
}
const NamedRecordSidecar* PA11SemanticModel::named_record_sidecar(
	NamedRecordId id) const
{
	return id.valid() ? named_record_sidecars_.find(id) : NULL;
}
void PA11SemanticModel::remember_type_display_path(TypeId type,
	const NamePath& path)
{
	if (!type.valid() || path.components.empty())
		return;
	if (!canonical_type_display_path(type, path))
		return;
	const NamedRecordId record = named_record_for_type(type);
	if (!record.valid())
		return;
	NamedRecordSidecar sidecar;
	const NamedRecordSidecar* existing = named_record_sidecar(record);
	if (existing != NULL)
		sidecar = *existing;
	if (sidecar.has_display_path)
		return;
	sidecar.has_display_path = true;
	sidecar.display_path = path;
	set_named_record_sidecar(record, sidecar);
}
bool PA11SemanticModel::canonical_type_display_path(TypeId type,
	const NamePath& path) const
{
	const NamedRecordId record_id = named_record_for_type(type);
	if (!record_id.valid() || record_id.value >= named_.size())
		return false;
	const NamedRecord& record = named_[record_id.value];
	if ((record.kind != NamedKind::Class && record.kind != NamedKind::Enum) ||
		!record.name.valid() || !record.owner.valid())
		return false;
	std::vector<NameId> reverse;
	ScopeId cursor = record.owner;
	while (cursor.valid() && cursor != global_)
	{
		if (cursor.value >= scopes_.size())
			return false;
		const Scope& scope = scopes_[cursor.value];
		if ((scope.kind != ScopeKind::Namespace &&
			scope.kind != ScopeKind::Class) || !scope.name.valid())
			return false;
		reverse.push_back(scope.name);
		cursor = scope.parent;
	}
	if (cursor != global_ || path.components.size() != reverse.size() + 1)
		return false;
	for (std::size_t i = 0; i < reverse.size(); ++i)
		if (path.components[i] != reverse[reverse.size() - i - 1])
			return false;
	return path.components.back() == record.name;
}
const NamePath* PA11SemanticModel::type_display_path(TypeId type) const
{
	if (!type.valid())
		return NULL;
	const NamedRecordId record = named_record_for_type(type);
	const NamedRecordSidecar* sidecar = named_record_sidecar(record);
	return sidecar != NULL && sidecar->has_display_path ?
		&sidecar->display_path : NULL;
}
void PA11SemanticModel::set_named_record_sidecar(NamedRecordId id,
	const NamedRecordSidecar& sidecar)
{
	if (!id.valid() || id.value >= named_.size())
		throw std::runtime_error("invalid PA11 named-record sidecar identity");
	named_record_sidecars_.set(id, sidecar);
}
const NamedRecordAlignmentFact* PA11SemanticModel::named_record_alignment_fact(
	NamedRecordId id) const
{
	return id.valid() ? named_record_alignment_facts_.find(id) : NULL;
}
void PA11SemanticModel::set_named_record_alignment_fact(NamedRecordId id,
	const NamedRecordAlignmentFact& fact)
{
	if (!id.valid() || id.value >= named_.size())
		throw std::runtime_error(
			"invalid PA11 named-record alignment fact identity");
	named_record_alignment_facts_.set(id, fact);
}
void PA11SemanticModel::add_dump_binding_view(ScopeId scope, BindingId binding_id)
{
	DumpBindingView view;
	view.parent = scope;
	view.position = scopes_[scope.value].bindings.size();
	view.record = NamedRecordId();
	view.qualified_name = NamePath();
	view.binding = binding_id;
	const DumpBindingViewId view_id(dump_binding_views_.size());
	dump_binding_views_.push_back(view);
	scopes_[scope.value].binding_views.push_back(view_id);
}
const Binding& PA11SemanticModel::binding(BindingId id) const
{
	if (!id.valid() || id.value >= bindings_.size())
		throw std::runtime_error("invalid PA11 binding identity");
	return bindings_[id.value];
}
Binding& PA11SemanticModel::binding(BindingId id)
{
	return const_cast<Binding&>(
		static_cast<const PA11SemanticModel*>(this)->binding(id));
}
void PA11SemanticModel::append_value_index(ScopeId scope, NameId name,
	BindingId id, ScopeId origin, SourcePoint declaration_point)
{
	FlatIndex<NameId, ValueList, IdentityHash<NameId> >& index =
		scopes_[scope.value].values;
	ValueList* list = index.find(name);
	if (list == NULL)
	{
		index.set(name, ValueList());
		list = index.find(name);
	}
	list->entries.push_back(ValueEntry(id, origin.valid() ? origin : scope,
		declaration_point));
}
TypeId PA11SemanticModel::ensure_named_class(ScopeId owner, NameId name, ClassTag tag,
	bool definition)
{
	Scope& current = scopes_[owner.value];
	if (direct_namespace_exists(owner, name))
		throw std::runtime_error("class name conflicts with namespace");
	const TypeId* found = current.types.find(name);
	NamedRecordId record_id;
	if (found != NULL)
	{
		const NamedRecordId old_record = named_record_for_type(*found);
		if (!old_record.valid() || old_record.value >= named_.size() ||
			named_[old_record.value].kind != NamedKind::Class)
			throw std::runtime_error("class name conflicts with type alias");
		if ((tag == ClassTag::Union) !=
			(named_[old_record.value].class_tag == ClassTag::Union))
			throw std::runtime_error("incompatible union redeclaration");
		record_id = old_record;
	}
	else
	{
		for (std::size_t i = 0; i < current.bindings.size(); ++i)
			if (binding(current.bindings[i]).name == name &&
				binding(current.bindings[i]).kind == BindingKind::TypeAlias)
				throw std::runtime_error("class name conflicts with type alias");
		if (current.namespace_aliases.find(name) != NULL)
			throw std::runtime_error("class name conflicts with namespace");
		NamedRecord record(NamedKind::Class, name, owner);
		record.class_tag = tag;
		record_id = append_named_record(record);
		const TypeId type = named_type(record_id);
		current.types.set(name, type);
	}
	if (definition)
	{
		if (named_[record_id.value].defined)
			throw std::runtime_error("class redefinition");
		named_[record_id.value].defined = true;
		if (!named_[record_id.value].scope.valid())
			named_[record_id.value].scope = create_scope(ScopeKind::Class,
				owner, name, record_id);
	}
	return named_type(record_id);
}
TypeId PA11SemanticModel::create_anonymous_class(ScopeId owner, ClassTag tag,
	const PA10AstNode& origin)
{
	NamedRecord record(NamedKind::Class, NameId(), owner);
	record.class_tag = tag;
	record.defined = true;
	if (tag == ClassTag::Union)
	{
		const std::size_t ordinal = anonymous_union_count_++;
		std::size_t begin = origin.source_begin;
		std::size_t end = origin.source_end;
		if (end <= begin)
		{
			begin = ordinal;
			end = begin + 1;
		}
		record.has_generated_identity = true;
		record.generated_identity = GeneratedIdentity(
			GeneratedEntityKind::AnonymousUnion, owner,
			SourceInterval(begin, end), GeneratedOrdinal(ordinal));
	}
	const NamedRecordId record_id = append_named_record(record);
	named_[record_id.value].scope = create_scope(ScopeKind::Class, owner,
		NameId(), record_id, false, tag != ClassTag::Union);
	return named_type(record_id);
}
TypeId PA11SemanticModel::ensure_named_enum(ScopeId owner, NameId name, bool scoped,
	bool has_underlying, TypeId underlying, bool definition)
{
	Scope& current = scopes_[owner.value];
	if (direct_namespace_exists(owner, name) || direct_value_exists(owner, name))
		throw std::runtime_error("enum name conflicts with binding");
	const TypeId* found = current.types.find(name);
	NamedRecordId record_id;
	if (found != NULL)
	{
		record_id = named_record_for_type(*found);
		if (!record_id.valid() || record_id.value >= named_.size() ||
			named_[record_id.value].kind != NamedKind::Enum)
			throw std::runtime_error("enum name conflicts with type");
		if (named_[record_id.value].scoped_enum != scoped)
			throw std::runtime_error("incompatible enum redeclaration");
		if (has_underlying && named_[record_id.value].has_underlying &&
			named_[record_id.value].underlying != underlying)
			throw std::runtime_error("incompatible enum underlying type");
		if (has_underlying && !named_[record_id.value].has_underlying)
		{
			named_[record_id.value].has_underlying = true;
			named_[record_id.value].underlying = underlying;
		}
	}
	else
	{
		for (std::size_t i = 0; i < current.bindings.size(); ++i)
			if (binding(current.bindings[i]).name == name &&
				binding(current.bindings[i]).kind == BindingKind::TypeAlias)
				throw std::runtime_error("enum name conflicts with type alias");
		NamedRecord record(NamedKind::Enum, name, owner);
		record.scoped_enum = scoped;
		record.has_underlying = has_underlying;
		record.underlying = underlying;
		record_id = append_named_record(record);
		current.types.set(name, named_type(record_id));
	}
	if (definition)
	{
		if (named_[record_id.value].defined)
			throw std::runtime_error("enum redefinition");
		named_[record_id.value].defined = true;
	}
	if (scoped && !named_[record_id.value].scope.valid())
		named_[record_id.value].scope = create_scope(ScopeKind::Enum,
			owner, name, record_id);
	return named_type(record_id);
}
TypeId PA11SemanticModel::create_anonymous_enum(ScopeId owner, bool scoped, bool has_underlying,
	TypeId underlying, bool definition)
{
	NamedRecord record(NamedKind::Enum, NameId(), owner);
	record.scoped_enum = scoped;
	record.has_underlying = has_underlying;
	record.underlying = underlying;
	record.defined = definition;
	record.has_generated_identity = true;
	record.generated_identity = GeneratedIdentity(
		GeneratedEntityKind::AnonymousEnum, owner, SourceInterval(),
		GeneratedOrdinal(anonymous_enum_count_++));
	const NamedRecordId record_id = append_named_record(record);
	if (scoped)
		named_[record_id.value].scope = create_scope(ScopeKind::Enum, owner,
		NameId(), record_id);
	return named_type(record_id);
}
void PA11SemanticModel::finalize_anonymous_record(TypeId type, NameId name,
	ScopeId owner, SourcePoint declaration_point)
{
	const NamedRecordId record_id = named_record_for_type(type);
	if (!record_id.valid() || record_id.value >= named_.size())
		throw std::runtime_error("invalid anonymous type");
	NamedRecord& record = named_[record_id.value];
	if (record.name.valid())
		return;
	Scope& current = scopes_[owner.value];
	if (direct_namespace_exists(owner, name) || direct_value_exists(owner, name) ||
		current.types.find(name) != NULL)
		throw std::runtime_error("anonymous type conflicts with binding");
	record.name = name;
	current.types.set(name, type);
	if (record.scope.valid())
		scopes_[record.scope.value].name = name;
	if (record.kind == NamedKind::Class)
		add_type_binding(owner, name, type, record.class_tag, true,
			declaration_point);
	else
	{
		Binding type_binding(BindingKind::Type, name, type);
		std::size_t position = current.bindings.size();
		for (std::size_t i = 0; i < current.bindings.size(); ++i)
			if (this->binding(current.bindings[i]).kind == BindingKind::Enumerator)
			{
				position = i;
				break;
			}
		const BindingId declaration = store_binding(owner, type_binding, position);
		record_type_declaration(owner, name, declaration_point, declaration);
	}
}
void PA11SemanticModel::inject_anonymous_union(TypeId type, ScopeId owner, bool create_storage, const PA10AstNode* origin)
{
	const NamedRecordId record_id = named_record_for_type(type);
	if (!record_id.valid() || record_id.value >= named_.size() || !named_[record_id.value].scope.valid())
		throw std::runtime_error("anonymous union has no scope");
	if (!create_storage && owner.valid() && owner.value < scopes_.size() &&
		scopes_[owner.value].kind == ScopeKind::Class)
		throw std::runtime_error("class anonymous member injection is outside PA16 checkpoint");
	BindingId storage;
	if (create_storage)
	{
		const NamedRecordSidecar* existing = named_record_sidecar(record_id);
		if (existing != NULL && existing->backing_storage.valid())
			storage = existing->backing_storage;
		else
		{
			Binding value(BindingKind::Variable, NameId(), type);
			storage = store_binding(owner, value);
			NamedRecordSidecar record_sidecar;
			if (existing != NULL) record_sidecar = *existing;
			record_sidecar.backing_storage = storage;
			set_named_record_sidecar(record_id, record_sidecar);
			BindingSidecar binding_sidecar;
			binding_sidecar.generated_name_record = record_id;
			set_binding_sidecar(storage, binding_sidecar);
		}
	}
	if (origin != NULL && create_storage)
		anonymous_union_fact_index_.set(origin, AnonymousUnionFact(record_id, owner, storage));
	const Scope& source = scopes_[named_[record_id.value].scope.value];
	for (std::size_t i = 0; i < source.bindings.size(); ++i)
	{
		const Binding& source_binding = binding(source.bindings[i]);
		if (source_binding.kind == BindingKind::Variable || source_binding.kind == BindingKind::Function)
			add_value(owner, source_binding.name, source_binding.type,
				source_binding.kind == BindingKind::Function, false, false,
				storage);
	}
}
bool PA11SemanticModel::enum_is_scoped(const PA10AstNode& node) const
{
	const PA10AstNode* key = child_of_kind(node, PA10NodeKind::EnumKey);
	return key != NULL && key->has_token &&
		(key->token == SimpleTokenType::KW_CLASS ||
		 key->token == SimpleTokenType::KW_STRUCT);
}
NamePath PA11SemanticModel::enum_name(const PA10AstNode& node)
{
	if (!node.name_parts.empty() || node.global_name)
		return name_path(node);
	if (node.producer_spelling != 0)
	{
		NamePath result;
		result.components.push_back(name_from_spelling(node.producer_spelling));
		return result;
	}
	return NamePath();
}
void PA11SemanticModel::add_qualified_enum_view(ScopeId parent, NamedRecordId record,
	const NamePath& qualified_name)
{
	DumpBindingView binding_view;
	binding_view.parent = parent;
	binding_view.position = scopes_[parent.value].bindings.size();
	binding_view.record = record;
	binding_view.qualified_name = qualified_name;
	binding_view.binding = BindingId();
	const DumpBindingViewId binding_view_id(dump_binding_views_.size());
	dump_binding_views_.push_back(binding_view);
	scopes_[parent.value].binding_views.push_back(binding_view_id);
	DumpScopeView scope_view;
	scope_view.parent = parent;
	scope_view.order = creation_order_++;
	scope_view.record = record;
	scope_view.qualified_name = qualified_name;
	const DumpScopeViewId scope_view_id(dump_scope_views_.size());
	dump_scope_views_.push_back(scope_view);
	scopes_[parent.value].scope_views.push_back(scope_view_id);
	named_[record.value].dump_scope_view = scope_view_id;
}
bool PA11SemanticModel::integral_type(FundamentalType type) const
{
	switch (type)
	{
	case FundamentalType::SignedChar:
	case FundamentalType::ShortInt:
	case FundamentalType::Int:
	case FundamentalType::LongInt:
	case FundamentalType::LongLongInt:
	case FundamentalType::UnsignedChar:
	case FundamentalType::UnsignedShortInt:
	case FundamentalType::UnsignedInt:
	case FundamentalType::UnsignedLongInt:
	case FundamentalType::UnsignedLongLongInt:
	case FundamentalType::WcharT:
	case FundamentalType::Char:
	case FundamentalType::Char16T:
	case FundamentalType::Char32T:
	case FundamentalType::Bool:
		return true;
	default:
		return false;
	}
}
bool PA11SemanticModel::unsigned_type(FundamentalType type) const
{
	return type == FundamentalType::UnsignedChar ||
		type == FundamentalType::UnsignedShortInt ||
		type == FundamentalType::UnsignedInt ||
		type == FundamentalType::UnsignedLongInt ||
		type == FundamentalType::UnsignedLongLongInt ||
		type == FundamentalType::Char16T ||
		type == FundamentalType::Char32T;
}
ConstValue PA11SemanticModel::literal_constant(const PA10AstNode& node) const
{
	if (!node.has_literal || node.literal.element_count != 0 ||
		!integral_type(node.literal.type) || node.literal.bytes.empty() ||
		node.literal.bytes.size() > sizeof(std::uint64_t))
		throw std::runtime_error("non-integral constant expression");
	std::uint64_t bits = 0;
	for (std::size_t i = 0; i < node.literal.bytes.size(); ++i)
		bits |= static_cast<std::uint64_t>(node.literal.bytes[i]) << (i * 8);
	__int128 value = static_cast<__int128>(bits);
	if (!unsigned_type(node.literal.type) &&
		(node.literal.bytes.back() & 0x80) != 0 &&
		node.literal.bytes.size() < sizeof(std::uint64_t))
		value -= static_cast<__int128>(1) <<
			(node.literal.bytes.size() * 8);
	return ConstValue(true, value, unsigned_type(node.literal.type));
}
bool PA11SemanticModel::integer_zero(const PA10AstNode& node) const
{
	if (node.kind == PA10NodeKind::ParenthesizedExpression)
		return node.children.size() == 1 && integer_zero(node.children.front());
	if (node.kind != PA10NodeKind::Literal || !node.has_literal ||
		node.literal.element_count != 0 || !integral_type(node.literal.type))
		return false;
	return literal_constant(node).valid && literal_constant(node).value == 0;
}
void PA11SemanticModel::check_constant_range(const ConstValue& value) const
{
	if (value.is_unsigned)
	{
		if (value.value < 0 || value.value >
			static_cast<__int128>(std::numeric_limits<std::uint64_t>::max()))
			throw NonConstantExpression("constant expression overflow");
	}
	else if (value.value < static_cast<__int128>(std::numeric_limits<std::int64_t>::min()) ||
		value.value > static_cast<__int128>(std::numeric_limits<std::int64_t>::max()))
		throw NonConstantExpression("constant expression overflow");
}
TypeId PA11SemanticModel::expression_type(const PA10AstNode& node, ScopeId scope)
{
	if (node.kind == PA10NodeKind::Literal)
	{
		const TypeId type = fundamental(node.literal.type);
		if (node.literal.element_count != 0)
			return make_array(make_cv(type, 1u), false,
				ArrayBound(node.literal.element_count));
		return type;
	}
	if (node.kind == PA10NodeKind::KeywordLiteral)
	{
		if (node.token == SimpleTokenType::KW_NULLPTR)
			return fundamental(FundamentalType::NullptrT);
		return fundamental(FundamentalType::Bool);
	}
	if (node.kind == PA10NodeKind::ParenthesizedExpression)
	{
		if (node.children.empty())
			throw std::runtime_error("empty parenthesized expression");
		return expression_type(node.children.front(), scope);
	}
	if (node.kind == PA10NodeKind::CallExpression)
	{
		if (node.children.size() != 2)
			throw std::runtime_error("invalid call expression type");
		const PA10AstNode& callee = node.children.front();
		const PA10AstNode& arguments = node.children.back();
		const std::size_t argument_count = arguments.children.size();
		if (callee.kind == PA10NodeKind::IdExpression)
		{
			const std::vector<ValueRef> candidates = lookup_value_path(
				name_path(callee), scope);
			bool has_direct_function = false;
			bool has_matching_function = false;
			TypeId matching_result;
			for (std::size_t i = 0; i < candidates.size(); ++i)
			{
				const Binding& candidate = binding(candidates[i].binding);
				if (candidate.kind != BindingKind::Function ||
					type_kind(candidate.type) != TypeKind::Function)
					continue;
				const TypeKey& function = types_[candidate.type.value];
				has_direct_function = true;
				if ((!function.variadic &&
					argument_count != function.parameters.size()) ||
					(function.variadic && argument_count <
					function.parameters.size()))
					continue;
				if (has_matching_function)
					throw std::runtime_error("ambiguous call expression type");
				has_matching_function = true;
				matching_result = function.result;
			}
			if (has_matching_function)
				return matching_result;
			if (has_direct_function)
				throw std::runtime_error("no matching function for call expression type");
		}
		TypeId callee_type = expression_type(callee, scope);
		if (type_kind(callee_type) == TypeKind::LvalueReference ||
			type_kind(callee_type) == TypeKind::RvalueReference)
			callee_type = types_[callee_type.value].child;
		callee_type = strip_cv_type(callee_type);
		if (type_kind(callee_type) == TypeKind::Pointer)
			callee_type = strip_cv_type(types_[callee_type.value].child);
		if (type_kind(callee_type) == TypeKind::Function)
		{
			const TypeKey& function = types_[callee_type.value];
			if ((!function.variadic &&
				argument_count != function.parameters.size()) ||
				(function.variadic && argument_count <
				function.parameters.size()))
				throw std::runtime_error("no matching function for call expression type");
			return function.result;
		}
		throw std::runtime_error("call expression has no function type");
	}
	if (node.kind == PA10NodeKind::IdExpression)
	{
		const NamePath name = name_path(node);
		const std::vector<ValueRef> values = lookup_value_path(name, scope);
		if (!values.empty()) return binding(values.front().binding).type;
		const TypeId type = lookup_type_path(name, scope);
		if (type.valid())
			return type;
		throw std::runtime_error("unknown expression name");
	}
	if (node.kind == PA10NodeKind::UnaryExpression ||
		node.kind == PA10NodeKind::BinaryExpression ||
		node.kind == PA10NodeKind::AssignmentExpression ||
		node.kind == PA10NodeKind::ConditionalExpression)
	{
		if (node.children.empty())
			throw std::runtime_error("empty expression");
		return expression_type(node.children.front(), scope);
	}
	if (node.kind == PA10NodeKind::CastExpression)
	{
		if (node.children.size() < 2)
			throw std::runtime_error("invalid cast expression");
		return type_from_type_id(node.children.front(), scope);
	}
	if (node.kind == PA10NodeKind::SizeofExpression)
		return fundamental(FundamentalType::UnsignedLongInt);
	if (node.kind == PA10NodeKind::TypeTraitExpression)
		return fundamental(FundamentalType::LongLongInt);
	throw std::runtime_error("unsupported expression type");
}
TypeId PA11SemanticModel::sizeof_operand_type(const PA10AstNode& node, ScopeId scope)
{
	if (node.children.empty())
		throw std::runtime_error("sizeof has no operand");
	const PA10AstNode& operand = node.children.front();
	if (operand.kind == PA10NodeKind::TypeId)
		return type_from_type_id(operand, scope);
	if (operand.kind == PA10NodeKind::IdExpression)
	{
		const std::vector<ValueRef> values = lookup_value_path(
			name_path(operand), scope);
		if (!values.empty())
		{
			for (std::size_t i = 0; i < values.size(); ++i)
				if (bit_field_fact(values[i].binding) != NULL)
					throw std::runtime_error("sizeof cannot apply to a bit-field");
			if (values.size() != 1)
				throw std::runtime_error("sizeof operand is overloaded");
			return binding(values.front().binding).type;
		}
		const TypeId type = lookup_type_path(name_path(operand), scope);
		if (type.valid())
			return type;
	}
	const ExprInfo expression = semantic_expression(operand, scope);
	if (bit_field_fact_for_expression(expression) != NULL)
		throw std::runtime_error("sizeof cannot apply to a bit-field");
	return expression.type;
}
TypeId PA11SemanticModel::decltype_type(const PA10AstNode& node, ScopeId scope)
{
	if (node.children.empty())
		throw std::runtime_error("decltype has no expression");
	const PA10AstNode& expression = node.children.front();
	bool parenthesized = expression.kind == PA10NodeKind::ParenthesizedExpression;
	const PA10AstNode* subject = &expression;
	if (parenthesized)
	{
		if (expression.children.empty())
			throw std::runtime_error("empty decltype expression");
		subject = &expression.children.front();
	}
	if (subject->kind == PA10NodeKind::IdExpression)
	{
		const std::vector<ValueRef> values = lookup_value_path(
			name_path(*subject), scope);
		if (!values.empty())
		{
			const Binding& value_binding = binding(values.front().binding);
			if (parenthesized && value_binding.kind != BindingKind::Enumerator)
				return make_reference(value_binding.type, false);
			return value_binding.type;
		}
	}
	const TypeId type = expression_type(*subject, scope);
	return parenthesized ? make_reference(type, false) : type;
}
void PA11SemanticModel::add_type_binding(ScopeId scope, NameId name,
	TypeId type, ClassTag tag, bool has_tag, SourcePoint declaration_point)
{
	Scope& current = scopes_[scope.value];
	const TypeId* type_found = current.types.find(name);
	if (type_found == NULL)
		current.types.set(name, type);
	else if (*type_found != type)
		throw std::runtime_error("incompatible type binding");
	for (std::size_t i = 0; i < current.bindings.size(); ++i)
	{
		const BindingId existing_id = current.bindings[i];
		Binding& existing = binding(existing_id);
		if (existing.kind != BindingKind::Type || existing.name != name)
			continue;
		if (existing.type != type)
			throw std::runtime_error("incompatible type binding");
		if (has_tag)
		{
			existing.has_tag = true;
			bool present = false;
			for (std::size_t j = 0; j < existing.declaration_tags.size(); ++j)
				present = present || existing.declaration_tags[j] == tag;
			if (!present)
				existing.declaration_tags.push_back(tag);
		}
		record_type_declaration(scope, name, declaration_point, existing_id);
		return;
	}
	Binding binding(BindingKind::Type, name, type);
	binding.has_tag = has_tag;
	binding.class_tag = tag;
	if (has_tag)
		binding.declaration_tags.push_back(tag);
	const BindingId declaration = store_binding(scope, binding);
	record_type_declaration(scope, name, declaration_point, declaration);
}
BindingId PA11SemanticModel::add_type_alias(ScopeId scope, NameId name,
	TypeId type, SourcePoint declaration_point)
{
	Scope& current = scopes_[scope.value];
	if (current.namespaces.find(name) != NULL ||
		current.namespace_aliases.find(name) != NULL ||
		direct_value_exists(scope, name))
		throw std::runtime_error("type alias conflicts with binding");
	const TypeId* found = current.types.find(name);
	if (found != NULL && *found != type)
		throw std::runtime_error("type alias redefinition");
	current.types.set(name, type);
	const BindingId declaration = store_binding(scope,
		Binding(BindingKind::TypeAlias, name, type));
	record_type_declaration(scope, name, declaration_point, declaration);
	return declaration;
}
TypeId PA11SemanticModel::normalize_function_type(TypeId type)
{
	if (type_kind(type) != TypeKind::Function)
		return type;
	const TypeKey& source = types_[type.value];
	TypeKey normalized = source;
	bool changed = false;
	for (std::size_t i = 0; i < normalized.parameters.size(); ++i)
	{
		const TypeId embedded = normalize_embedded_function_types(
			normalized.parameters[i]);
		const TypeId parameter = normalize_parameter_type(embedded);
		if (parameter != normalized.parameters[i])
		{
			normalized.parameters[i] = parameter;
			changed = true;
		}
	}
	return changed ? intern_type(normalized) : type;
}
ScopeId PA11SemanticModel::declaration_scope(const NamePath& path, ScopeId current) const
{
	if (path.components.size() <= 1 && !path.global)
		return current;
	if (path.components.empty())
		return ScopeId();
	std::vector<NameId> prefix(path.components.begin(), path.components.end() - 1);
	return path.global ? resolve_global_qualifier_scope(prefix) :
		resolve_qualifier_scope(prefix, current);
}
SpecFact PA11SemanticModel::spec_fact(const PA10AstNode& node, ScopeId scope)
{
	if (node.kind != PA10NodeKind::DeclSpecifierSeq &&
		node.kind != PA10NodeKind::TypeSpecifierSeq)
		throw std::runtime_error("PA11 expected declaration specifier sequence");
	SpecFact result;
	bool has_signed = false, has_unsigned = false, has_short = false;
	unsigned int long_count = 0;
	bool has_int = false, has_char = false, has_char16 = false, has_char32 = false;
	bool has_wchar = false, has_bool = false, has_float = false,
		has_double = false, has_void = false;
	for (std::size_t i = 0; i < node.children.size(); ++i)
	{
		const PA10AstNode& child = node.children[i];
		if (child.kind == PA10NodeKind::ClassSpecifier)
		{
			const NamePath name = class_name(child);
			const ClassTag tag = class_tag(child);
			ScopeId owner = scope;
			TypeId type;
			if (name.empty())
			{
				type = create_anonymous_class(scope, tag, child);
				result.anonymous_record = named_record_for_type(type);
			}
			else
			{
				owner = declaration_scope(name, scope);
				if (!owner.valid())
					throw std::runtime_error("unresolved class declaration scope");
				type = ensure_named_class(owner, name.last(), tag, true);
				add_type_binding(owner, name.last(), type, tag, true,
					SourcePoint(child.source_begin));
			}
			apply_record_alignment(node, named_record_for_type(type), owner,
				true, &child);
			process_class_body(child, type, owner, true);
			result.base = type;
			result.has_base = true;
			continue;
		}
		if (child.kind == PA10NodeKind::ClassForwardDeclaration)
		{
			const NamePath name = class_name(child);
			if (name.empty())
				unsupported("anonymous class forward declaration");
			const ClassTag tag = class_tag(child);
			const TypeId visible = lookup_type_path(name, scope);
			TypeId type;
			if (visible.valid())
			{
				const NamedRecordId record = named_record_for_type(visible);
				if (!record.valid() || record.value >= named_.size() ||
					named_[record.value].kind != NamedKind::Class ||
					named_[record.value].class_tag != tag)
					throw std::runtime_error("elaborated class tag mismatch");
				type = visible;
			}
			else
			{
				const ScopeId owner = declaration_scope(name, scope);
				if (!owner.valid())
					throw std::runtime_error("unresolved class declaration scope");
				type = ensure_named_class(owner, name.last(), tag, false);
				add_type_binding(owner, name.last(), type, tag, true,
					SourcePoint(child.source_begin));
			}
			apply_record_alignment(node, named_record_for_type(type), scope,
				false, &child);
			result.base = type;
			result.has_base = true;
			continue;
		}
		if (child.kind == PA10NodeKind::EnumSpecifier)
		{
			NamedRecordId anonymous_record;
			result.base = process_enum_specifier(child, scope,
				&anonymous_record);
			result.anonymous_record = anonymous_record;
			result.has_base = true;
			continue;
		}
		if (child.kind == PA10NodeKind::DecltypeSpecifier ||
			(child.kind == PA10NodeKind::DeclSpecifier && child.has_token &&
			 child.token == SimpleTokenType::KW_DECLTYPE))
		{
			result.base = decltype_type(child, scope);
			result.has_base = true;
			continue;
		}
		if (child.kind == PA10NodeKind::TypeName ||
			(child.kind == PA10NodeKind::DeclSpecifier &&
				(!child.name_parts.empty() || child.producer_spelling != 0)))
		{
			const NamePath name = name_path(child);
			const TypeId type = lookup_type_path(name, scope);
			if (!type.valid())
				throw std::runtime_error("unknown PA11 type name");
			if (name.global)
			{
				NamePath display = name;
				display.global = false;
				remember_type_display_path(type, display);
			}
			result.base = type;
			result.has_base = true;
			continue;
		}
		if (!child.has_token)
			continue;
		switch (child.token)
		{
		case SimpleTokenType::KW_TYPEDEF:
			result.is_typedef = true;
			break;
		case SimpleTokenType::KW_CONSTEXPR:
			result.is_constexpr = true;
			break;
		case SimpleTokenType::KW_STATIC:
			result.is_static = true;
			break;
		case SimpleTokenType::KW_EXTERN:
			result.is_extern = true;
			break;
		case SimpleTokenType::KW_THREAD_LOCAL:
			result.is_thread_local = true;
			break;
		case SimpleTokenType::KW_AUTO:
			if (result.is_auto) throw std::runtime_error(
				"duplicate PA11 auto type specifier");
			result.is_auto = true; break;
		case SimpleTokenType::KW_CONST:
		case SimpleTokenType::KW_VOLATILE:
			result.cv |= cv_bit(child);
			break;
		case SimpleTokenType::KW_SIGNED:
			has_signed = true;
			break;
		case SimpleTokenType::KW_UNSIGNED:
			has_unsigned = true;
			break;
		case SimpleTokenType::KW_SHORT:
			has_short = true;
			break;
		case SimpleTokenType::KW_LONG:
			++long_count;
			break;
		case SimpleTokenType::KW_INT:
			has_int = true;
			break;
		case SimpleTokenType::KW_CHAR:
			has_char = true;
			break;
		case SimpleTokenType::KW_CHAR16_T:
			has_char16 = true;
			break;
		case SimpleTokenType::KW_CHAR32_T:
			has_char32 = true;
			break;
		case SimpleTokenType::KW_WCHAR_T:
			has_wchar = true;
			break;
		case SimpleTokenType::KW_BOOL:
			has_bool = true;
			break;
		case SimpleTokenType::KW_FLOAT:
			has_float = true;
			break;
		case SimpleTokenType::KW_DOUBLE:
			has_double = true;
			break;
		case SimpleTokenType::KW_VOID:
			has_void = true;
			break;
		default:
			break;
		}
	}
	const bool explicit_type = result.has_base || has_signed || has_unsigned || has_short || long_count != 0 || has_int || has_char || has_char16 || has_char32 || has_wchar || has_bool || has_float || has_double || has_void;
	if (result.is_auto && (explicit_type || result.cv != 0 || result.is_typedef))
		throw std::runtime_error("PA11 auto requires a single type-specifier and no cv qualifier");
	if (result.is_auto) return result;
	if (!result.has_base) {
		FundamentalType type = FundamentalType::Int;
		if (has_void)
			type = FundamentalType::Void;
		else if (has_char)
			type = has_signed ? FundamentalType::SignedChar :
				has_unsigned ? FundamentalType::UnsignedChar : FundamentalType::Char;
		else if (has_char16)
			type = FundamentalType::Char16T;
		else if (has_char32)
			type = FundamentalType::Char32T;
		else if (has_wchar)
			type = FundamentalType::WcharT;
		else if (has_bool)
			type = FundamentalType::Bool;
		else if (has_float)
			type = FundamentalType::Float;
		else if (has_double)
			type = long_count == 0 ? FundamentalType::Double :
				FundamentalType::LongDouble;
		else if (long_count >= 2)
			type = has_unsigned ? FundamentalType::UnsignedLongLongInt :
				FundamentalType::LongLongInt;
		else if (long_count == 1)
			type = has_unsigned ? FundamentalType::UnsignedLongInt :
				FundamentalType::LongInt;
		else if (has_short)
			type = has_unsigned ? FundamentalType::UnsignedShortInt :
				has_signed ? FundamentalType::ShortInt : FundamentalType::ShortInt;
		else if (has_unsigned)
			type = FundamentalType::UnsignedInt;
		else if (has_signed || has_int)
			type = FundamentalType::Int;
		else
			throw std::runtime_error("declaration has no PA11 type");
		result.base = fundamental(type);
		result.has_base = true;
	}
	result.base = make_cv(result.base, result.cv);
	return result;
}
NamePath PA11SemanticModel::class_name(const PA10AstNode& node)
{
	if (!node.name_parts.empty() || node.global_name)
		return name_path(node);
	if (node.producer_spelling != 0)
	{
		NamePath result;
		result.components.push_back(name_from_spelling(node.producer_spelling));
		return result;
	}
	return NamePath();
}
ArrayBound PA11SemanticModel::literal_bound(const PA10AstNode& node) const
{
	if (node.kind != PA10NodeKind::Literal || !node.has_literal ||
		node.literal.bytes.size() > sizeof(std::uint64_t))
		throw std::runtime_error("unsupported PA11 array bound");
	std::uint64_t value = 0;
	for (std::size_t i = 0; i < node.literal.bytes.size(); ++i)
		value |= static_cast<std::uint64_t>(node.literal.bytes[i]) << (i * 8);
	if (value == 0 || value > static_cast<std::uint64_t>(InvalidIdentityValue))
		throw std::runtime_error("invalid PA11 array bound");
	return ArrayBound(static_cast<std::size_t>(value));
}
bool PA11SemanticModel::ambiguous_call_statement(const PA10AstNode& node, ScopeId scope,
	NamePath* callee, const PA10AstNode** argument)
{
	if (node.kind != PA10NodeKind::SimpleDeclaration || node.children.size() != 2 ||
		node.children[0].kind != PA10NodeKind::DeclSpecifierSeq ||
		node.children[0].children.size() != 1 ||
		node.children[1].kind != PA10NodeKind::InitDeclaratorList ||
		node.children[1].children.size() != 1)
		return false;
	const PA10AstNode& spec = node.children[0].children.front();
	if (spec.kind != PA10NodeKind::DeclSpecifier || spec.has_token ||
		(spec.name_parts.empty() && spec.producer_spelling == 0))
		return false;
	const PA10AstNode& init = node.children[1].children.front();
	if (init.kind != PA10NodeKind::InitDeclarator || init.children.size() != 1)
		return false;
	const PA10AstNode& outer = init.children.front();
	if (outer.kind != PA10NodeKind::Declarator || outer.children.size() != 1 ||
		outer.children.front().kind != PA10NodeKind::NestedDeclarator)
		return false;
	const PA10AstNode& nested = outer.children.front();
	if (nested.children.size() != 1 ||
		nested.children.front().kind != PA10NodeKind::Declarator)
		return false;
	const PA10AstNode& inner = nested.children.front();
	if (inner.children.size() != 1 ||
		inner.children.front().kind != PA10NodeKind::Identifier)
		return false;
	const NamePath function_name = name_path(spec);
	if (lookup_type_path(function_name, scope).valid())
		return false;
	const std::vector<ValueRef> values = lookup_value_path(function_name, scope);
	if (values.empty())
		return false;
	if (callee != NULL)
		*callee = function_name;
	if (argument != NULL)
		*argument = &inner.children.front();
	return true;
}
const PA10AstNode* PA11SemanticModel::top_parameter_clause(const PA10AstNode& node) const
{
	std::size_t direct = node.children.size();
	for (std::size_t i = 0; i < node.children.size(); ++i)
		if (node.children[i].kind == PA10NodeKind::Identifier ||
			node.children[i].kind == PA10NodeKind::NestedDeclarator)
		{
			direct = i;
			break;
		}
	if (direct == node.children.size() ||
		node.children[direct].kind != PA10NodeKind::Identifier)
		return NULL;
	for (std::size_t i = direct + 1; i < node.children.size(); ++i)
		if (node.children[i].kind == PA10NodeKind::ParameterClause)
			return &node.children[i];
	return NULL;
}
void PA11SemanticModel::process_simple_declaration(const PA10AstNode& node, ScopeId scope)
{
	if (node.children.empty())
		throw std::runtime_error("invalid PA11 simple declaration");
	if (ambiguous_call_statement(node, scope, NULL, NULL) ||
		ambiguous_assignment_statement(node, scope, NULL, NULL, NULL))
		return;
	const SpecFact spec = spec_fact(node.children.front(), scope);
	const bool friend_declaration = has_friend_specifier(node.children.front());
	const NamedRecordId friend_record = friend_declaration ?
		friend_record_for_scope(scope) : NamedRecordId();
	if (node.children.size() == 1)
	{
		if (spec.is_auto) throw std::runtime_error("PA11 auto declaration has no declarator");
		if (spec.anonymous_record.valid())
		{
			const TypeId type = named_type(spec.anonymous_record);
			if (named_[spec.anonymous_record.value].class_tag == ClassTag::Union)
				inject_anonymous_union(type, scope);
		}
		return;
	}
	const PA10AstNode& list = node.children[1];
	if (list.kind != PA10NodeKind::InitDeclaratorList)
		throw std::runtime_error("invalid PA11 declarator list");
	DeclarationFact declaration(&node, scope);
	declaration.is_constexpr = spec.is_constexpr;
	declaration.is_extern = spec.is_extern;
	declaration.is_static = spec.is_static;
	declaration.is_thread_local = spec.is_thread_local;
	declaration.automatic_storage = scope.valid() &&
		scope.value < scopes_.size() &&
		scopes_[scope.value].kind == ScopeKind::Block &&
		!spec.is_static && !spec.is_extern && !spec.is_thread_local;
	declaration.binding_begin = declaration_bindings_.size();
	for (std::size_t i = 0; i < list.children.size(); ++i)
	{
		const PA10AstNode& init = list.children[i];
		if (init.kind != PA10NodeKind::InitDeclarator || init.children.empty())
			throw std::runtime_error("invalid PA11 init-declarator");
		const PA10AstNode& declarator = init.children.front();
		const DeclaratorName name = declarator_name(declarator);
		if (!name.found)
			throw std::runtime_error("unnamed PA11 declaration");
		const bool hidden_friend = friend_record.valid() &&
			!name.path.global && name.path.components.size() == 1;
		const ScopeId target = hidden_friend ? friend_namespace_scope(scope) :
			declaration_scope(name.path, scope);
		if (!target.valid())
			throw std::runtime_error("unresolved PA11 declaration scope");
		if (spec.anonymous_record.valid())
		{
			const TypeId record_name_type = spec.is_typedef ? spec.base : named_type(spec.anonymous_record);
			finalize_anonymous_record(record_name_type, name.path.last(), target,
				SourcePoint(node.source_begin));
			if (!spec.is_typedef &&
				named_[spec.anonymous_record.value].class_tag == ClassTag::Union)
			{
				NamedRecordSidecar sidecar;
				const NamedRecordSidecar* existing =
					named_record_sidecar(spec.anonymous_record);
				if (existing != NULL)
					sidecar = *existing;
				sidecar.local_object_name = true;
				set_named_record_sidecar(spec.anonymous_record, sidecar);
			}
		}
		const bool direct_initializer = direct_initializer_operand(init, target, NULL);
		DeclaratorBaseKind base_kind = spec.is_auto ? DeclaratorBaseKind::AutoPlaceholder : DeclaratorBaseKind::Typed;
		TypeId type = direct_initializer ? spec.base :
			apply_declarator(declarator, spec.base, target, base_kind);
		if (!type.valid()) throw std::runtime_error("PA11 declaration has no typed type");
		if (!direct_initializer && type_kind(type) == TypeKind::Array &&
			types_[type.value].unknown_bound && init.children.size() > 1 &&
			init.children[1].kind == PA10NodeKind::Initializer &&
			init.children[1].children.size() == 1 &&
			init.children[1].children.front().kind ==
				PA10NodeKind::BracedInitList)
		{
			const PA10AstNode& braces = init.children[1].children.front();
			type = make_array(types_[type.value].child, false,
				ArrayBound(braces.children.size()));
		}
		if (spec.is_constexpr && type_kind(type) != TypeKind::Function)
			type = make_cv(type, 1u);
		BindingId binding_id;
		if (spec.is_typedef)
			binding_id = add_type_alias(target, name.path.last(), type,
				SourcePoint(node.source_begin));
		else
		{
			const bool function = type_kind(type) == TypeKind::Function;
			const FunctionDeclarationKind declaration_kind = function ? special_initializer_kind(init) : FunctionDeclarationKind::Normal;
			const bool has_initializer = init.children.size() > 1;
			// Qualified class targets keep in-class static data declarations distinct from definitions.
			const bool class_target = target.value < scopes_.size() && scopes_[target.value].kind == ScopeKind::Class; if (!function && class_target && (name.path.global || name.path.components.size() > 1)) validate_qualified_class_static_definition(target, name.path.last());
			const bool definition = function ? declaration_kind != FunctionDeclarationKind::Normal :
				(!spec.is_extern || has_initializer) && (!class_target || !spec.is_static);
			const bool internal_linkage = spec.is_static && target.value < scopes_.size() &&
				scopes_[target.value].kind == ScopeKind::Namespace;
			binding_id = add_value(target, name.path.last(), type,
				function, definition, true, BindingId(),
				SourcePoint(node.source_begin), internal_linkage,
				current_language_linkage_, declaration_kind, hidden_friend,
				name.operator_function_kind, name.operator_token);
			if (function)
			{
				record_function_declarator(binding_id, name, declarator,
					declaration_kind);
				if (friend_record.valid())
					record_friend_function(binding_id, friend_record,
						hidden_friend, SourcePoint(node.source_begin));
				validate_nonmember_operator(binding_id);
			}
			const NamedRecordId constant_record = named_record_for_type(type);
			const bool ordinary_const_record = !spec.is_constexpr &&
				((spec.cv & 1u) != 0) && constant_record.valid() &&
				constant_record.value < named_.size() &&
				named_[constant_record.value].kind == NamedKind::Class;
			const bool integral_constant_type = integral_id(type) ||
				enumeration_id(type);
			if (!ordinary_const_record && integral_constant_type && (spec.is_constexpr ||
				((spec.cv & 1u) != 0 && type_kind(type) == TypeKind::Cv)))
			{
				if (init.children.size() > 1)
				{
					const PA10AstNode& initializer = init.children[1];
					if (initializer.children.empty())
						throw std::runtime_error("empty constant initializer");
					const ConstValue value = eval_constexpr(
						initializer.children.front(), target);
					if (value.is_unsigned)
					{
						if (value.value < 0 || value.value >
							static_cast<__int128>(std::numeric_limits<std::uint64_t>::max()))
							throw std::runtime_error("constant initializer overflow");
					}
					else if (value.value < static_cast<__int128>(std::numeric_limits<std::int64_t>::min()) ||
						value.value > static_cast<__int128>(std::numeric_limits<std::int64_t>::max()))
						throw std::runtime_error("constant initializer overflow");
					Binding& constant = binding(binding_id);
					constant.has_value = true;
					constant.value = static_cast<std::int64_t>(value.value);
					constant.value_bits = static_cast<std::uint64_t>(value.value);
					constant.value_unsigned = value.is_unsigned;
				}
			}
		}
		if (spec.is_static && target.value < scopes_.size() &&
			scopes_[target.value].kind == ScopeKind::Class)
			mark_static_member(binding_id);
		const bool record_member = binding(binding_id).kind ==
			BindingKind::Variable && !is_static_member(binding_id) &&
			type_kind(type) != TypeKind::Function &&
			target.value < scopes_.size() &&
			scopes_[target.value].kind == ScopeKind::Class;
		if (record_member)
			append_record_member(scopes_[target.value].record, binding_id);
		if (record_member)
			apply_member_alignment(node.children.front(), binding_id, target);
		if (!spec.is_static && type_kind(type) != TypeKind::Function &&
			target.value < scopes_.size() &&
			scopes_[target.value].kind == ScopeKind::Class &&
			init.children.size() > 1)
		{
			BindingSidecar sidecar;
			const BindingSidecar* existing = binding_sidecar(binding_id);
			if (existing != NULL)
				sidecar = *existing;
			sidecar.has_default_member_initializer = true;
			set_binding_sidecar(binding_id, sidecar);
		}
		declaration_bindings_.push_back(binding_id);
	}
	declaration.binding_count = declaration_bindings_.size() -
		declaration.binding_begin;
	const DeclarationFactId declaration_id(declaration_facts_.size());
	declaration_facts_.push_back(declaration);
	declaration_fact_index_.set(&node, declaration_id);
}

NameId PA11SemanticModel::template_parameter_name(const PA10AstNode& node)
{
	for (std::size_t i = node.children.size(); i != 0; --i)
		if (node.children[i - 1].kind == PA10NodeKind::Identifier)
			return name_path(node.children[i - 1]).last();
	throw std::runtime_error("unnamed template parameter");
}
void PA11SemanticModel::process_template_parameter(const PA10AstNode& node, ScopeId scope)
{
	if (node.kind != PA10NodeKind::TypeParameter)
		throw std::runtime_error("unsupported template parameter");
	bool template_template = false;
	for (std::size_t i = 0; i < node.children.size(); ++i)
		if (node.children[i].kind == PA10NodeKind::TemplateTemplateParameter)
			template_template = true;
	const NameId name = template_parameter_name(node);
	Scope& current = scopes_[scope.value];
	if (current.types.find(name) != NULL ||
		direct_namespace_exists(scope, name) || direct_value_exists(scope, name))
		throw std::runtime_error("template parameter conflicts with binding");
	NamedRecord record(NamedKind::TemplateParameter, name, scope);
	record.template_template = template_template;
	const NamedRecordId record_id = append_named_record(record);
	const TypeId type = named_type(record_id);
	current.types.set(name, type);
	add_type_binding(scope, name, type, ClassTag::Struct, false,
		SourcePoint(node.source_begin));
	// The parameter list nested inside a template-template parameter is a
	// separate scope owned by the template argument grammar.  Its names are
	// deliberately not visible in the surrounding declaration.
}
void PA11SemanticModel::record_template_function(const PA10AstNode& node,
	ScopeId visible_scope, ScopeId parameter_scope)
{
	const DeclarationFact* declaration = declaration_fact(node);
	if (declaration == NULL || declaration->binding_count != 1 ||
		declaration->binding_begin == InvalidIdentityValue)
		return;
	const BindingId binding_id = declaration_bindings_[
		declaration->binding_begin];
	if (!binding_id.valid() || binding_id.value >= bindings_.size() ||
		binding(binding_id).kind != BindingKind::Function)
		return;
	const Binding& value = binding(binding_id);
	TemplateFunctionFact function(visible_scope, binding_id, value.name);
	if (!parameter_scope.valid() || parameter_scope.value >= scopes_.size())
		return;
	const Scope& parameters = scopes_[parameter_scope.value];
	for (std::size_t i = 0; i < parameters.bindings.size(); ++i)
	{
		const Binding& parameter = binding(parameters.bindings[i]);
		if (parameter.kind != BindingKind::Type)
			continue;
		const NamedRecordId record = named_record_for_type(parameter.type);
		if (!record.valid() || record.value >= named_.size() ||
			named_[record.value].kind != NamedKind::TemplateParameter)
			return;
		function.parameters.push_back(record);
	}
	if (function.parameters.empty())
		return;
	const TemplateFunctionId id(template_function_facts_.size());
	template_function_facts_.push_back(function);
	TemplateFunctionList* list = template_function_index_.find(function.name);
	if (list == NULL)
	{
		template_function_index_.set(function.name, TemplateFunctionList());
		list = template_function_index_.find(function.name);
	}
	list->entries.push_back(id);
}
void PA11SemanticModel::process_template_declaration(const PA10AstNode& node, ScopeId parent)
{
	if (node.children.size() != 2 ||
		node.children[0].kind != PA10NodeKind::TemplateParameterClause)
		throw std::runtime_error("invalid template declaration");
	const ScopeId parameters = create_scope(ScopeKind::TemplateParameters,
		parent, NameId());
	const PA10AstNode& clause = node.children[0];
	for (std::size_t i = 0; i < clause.children.size(); ++i)
	{
		if (clause.children[i].kind != PA10NodeKind::TemplateParameterList)
			continue;
		for (std::size_t j = 0; j < clause.children[i].children.size(); ++j)
			process_template_parameter(clause.children[i].children[j], parameters);
	}
	process_declaration(node.children[1], parameters);
	record_template_function(node.children[1], parent, parameters);
}
bool PA11SemanticModel::ambiguous_assignment_statement(const PA10AstNode& node,
	ScopeId scope, NamePath* callee, const PA10AstNode** argument,
	const PA10AstNode** right)
{
	if (node.kind != PA10NodeKind::SimpleDeclaration || node.children.size() != 2 ||
		node.children[0].kind != PA10NodeKind::DeclSpecifierSeq ||
		node.children[0].children.size() != 1 ||
		node.children[1].kind != PA10NodeKind::InitDeclaratorList ||
		node.children[1].children.size() != 1)
		return false;
	const PA10AstNode& spec = node.children[0].children.front();
	if (spec.kind != PA10NodeKind::DeclSpecifier || spec.has_token ||
		(spec.name_parts.empty() && spec.producer_spelling == 0))
		return false;
	const PA10AstNode& init = node.children[1].children.front();
	if (init.kind != PA10NodeKind::InitDeclarator || init.children.size() != 2 ||
		init.children[0].kind != PA10NodeKind::Declarator ||
		init.children[1].kind != PA10NodeKind::Initializer ||
		init.children[1].children.size() != 1)
		return false;
	const PA10AstNode& outer = init.children.front();
	if (outer.children.size() != 1 ||
		outer.children.front().kind != PA10NodeKind::NestedDeclarator)
		return false;
	const PA10AstNode& nested = outer.children.front();
	if (nested.children.size() != 1 ||
		nested.children.front().kind != PA10NodeKind::Declarator)
		return false;
	const PA10AstNode& inner = nested.children.front();
	if (inner.children.size() != 1 ||
		inner.children.front().kind != PA10NodeKind::Identifier)
		return false;
	const NamePath function_name = name_path(spec);
	if (lookup_type_path(function_name, scope).valid())
		return false;
	const std::vector<ValueRef> values = lookup_value_path(function_name, scope);
	bool has_function = false;
	for (std::size_t i = 0; i < values.size(); ++i)
		if (binding(values[i].binding).kind == BindingKind::Function)
		{
			has_function = true;
			break;
		}
	if (!has_function)
		return false;
	const PA10AstNode& initializer = init.children[1].children.front();
	if (initializer.kind != PA10NodeKind::IdExpression &&
		initializer.kind != PA10NodeKind::Literal &&
		initializer.kind != PA10NodeKind::KeywordLiteral &&
		initializer.kind != PA10NodeKind::ParenthesizedExpression &&
		initializer.kind != PA10NodeKind::BinaryExpression &&
		initializer.kind != PA10NodeKind::AssignmentExpression &&
		initializer.kind != PA10NodeKind::ConditionalExpression &&
		initializer.kind != PA10NodeKind::CastExpression)
		return false;
	if (callee != NULL)
		*callee = function_name;
	if (argument != NULL)
		*argument = &inner.children.front();
	if (right != NULL)
		*right = &initializer;
	return true;
}
void PA11SemanticModel::process_declaration(const PA10AstNode& node, ScopeId scope)
{
	switch (node.kind)
	{
	case PA10NodeKind::EmptyDeclaration:
		return;
	case PA10NodeKind::NamespaceDefinition:
		process_namespace(node, scope);
		return;
	case PA10NodeKind::NamespaceAliasDefinition:
		process_namespace_alias(node, scope);
		return;
	case PA10NodeKind::UsingDirective:
		process_using_directive(node, scope);
		return;
	case PA10NodeKind::UsingDeclaration:
		process_using_declaration(node, scope);
		return;
	case PA10NodeKind::AliasDeclaration:
		if (node.producer_spelling == 0 || node.children.size() != 1)
			throw std::runtime_error("invalid PA11 alias declaration");
		{
			const BindingId binding_id = add_type_alias(scope,
				name_from_spelling(node.producer_spelling),
				type_from_type_id(node.children.front(), scope),
				SourcePoint(node.source_begin));
			DeclarationFact declaration(&node, scope);
			declaration.binding_begin = declaration_bindings_.size();
			declaration_bindings_.push_back(binding_id);
			declaration.binding_count = 1;
			const DeclarationFactId declaration_id(declaration_facts_.size());
			declaration_facts_.push_back(declaration);
			declaration_fact_index_.set(&node, declaration_id);
		}
		return;
	case PA10NodeKind::SimpleDeclaration:
		process_simple_declaration(node, scope);
		return;
	case PA10NodeKind::BitFieldDeclaration:
		process_bit_field_declaration(node, scope);
		return;
	case PA10NodeKind::FunctionDefinition:
		process_function_definition(node, scope);
		return;
	case PA10NodeKind::SpecialMemberDeclaration:
	case PA10NodeKind::SpecialMemberDefinition:
		process_special_member(node, scope); return;
	case PA10NodeKind::ClassForwardDeclaration:
	{
		const NamePath name = class_name(node);
		if (name.empty())
			unsupported("anonymous class forward declaration");
		const ClassTag tag = class_tag(node);
		const ScopeId target = declaration_scope(name, scope);
		if (!target.valid())
			throw std::runtime_error("unresolved class declaration scope");
		const TypeId type = ensure_named_class(target, name.last(), tag, false);
		add_type_binding(target, name.last(), type, tag, true,
			SourcePoint(node.source_begin));
		apply_record_alignment(node, named_record_for_type(type), target, false);
		return;
	}
	case PA10NodeKind::ClassSpecifier:
	{
		const NamePath name = class_name(node);
		const ClassTag tag = class_tag(node);
		if (name.empty())
		{
			const TypeId type = create_anonymous_class(scope, tag, node);
			apply_record_alignment(node, named_record_for_type(type), scope, true);
			process_class_body(node, type, scope, true);
			if (tag == ClassTag::Union)
				inject_anonymous_union(type, scope,
					scopes_[scope.value].kind == ScopeKind::Block, &node);
			return;
		}
		const ScopeId target = declaration_scope(name, scope);
		if (!target.valid())
			throw std::runtime_error("unresolved class declaration scope");
		const TypeId type = ensure_named_class(target, name.last(), tag, true);
		add_type_binding(target, name.last(), type, tag, true,
			SourcePoint(node.source_begin));
		apply_record_alignment(node, named_record_for_type(type), target, true);
		process_class_body(node, type, target, true);
		return;
	}
	case PA10NodeKind::LinkageSpecification:
	{
		const LanguageLinkage previous = current_language_linkage_;
		current_language_linkage_ = language_linkage(node);
		for (std::size_t i = 0; i < node.children.size(); ++i)
			process_declaration(node.children[i], scope);
		current_language_linkage_ = previous;
		return;
	}
	case PA10NodeKind::TemplateDeclaration:
		process_template_declaration(node, scope);
		return;
	case PA10NodeKind::StaticAssertDeclaration:
		if (node.children.empty() || eval_constexpr(node.children.front(), scope,
			true).value == 0)
			throw std::runtime_error("static assertion failed");
		return;
	case PA10NodeKind::EnumSpecifier:
	{
		NamedRecordId anonymous_record;
		process_enum_specifier(node, scope, &anonymous_record);
		if (anonymous_record.valid())
		{
			if (named_[anonymous_record.value].scoped_enum)
				throw std::runtime_error("anonymous scoped enum needs a name");
		}
		return;
	}
	default:
		unsupported("declaration form");
	}
}
bool PA11SemanticModel::simple_declaration_has_initializer(
	const PA10AstNode& node) const
{
	if (node.kind != PA10NodeKind::SimpleDeclaration)
		return false;
	if (node.children.empty())
		return false;
	for (std::size_t i = 0; i < node.children.size(); ++i)
	{
		const PA10AstNode& child = node.children[i];
		if (child.kind == PA10NodeKind::InitDeclarator)
		{
			if (child.children.size() > 1) return true;
			continue;
		}
		for (std::size_t j = 0; j < child.children.size(); ++j)
			if (child.children[j].kind == PA10NodeKind::InitDeclarator &&
				child.children[j].children.size() > 1)
				return true;
	}
	return false;
}
void PA11SemanticModel::collect_switch_transfer_points(
	const PA10AstNode& node, ScopeId scope,
	SwitchInitializationState* state) const
{
	if (state == NULL)
		throw std::runtime_error("PA12 switch initialization state is missing");
	// A nested switch owns its labels and its declaration-entry checks.  The
	// enclosing switch resumes after the nested statement with its own state.
	if (node.kind == PA10NodeKind::SwitchStatement ||
		node.kind == PA10NodeKind::ClassSpecifier ||
		node.kind == PA10NodeKind::FunctionDefinition ||
		node.kind == PA10NodeKind::LambdaExpression)
		return;
	ScopeId effective = scope;
	if (node.kind == PA10NodeKind::CompoundStatement)
		effective = compound_scope(node);
	else if (node.kind == PA10NodeKind::IfStatement ||
		node.kind == PA10NodeKind::WhileStatement ||
		node.kind == PA10NodeKind::DoStatement ||
		node.kind == PA10NodeKind::ForStatement)
	{
		const StatementFact* statement = statement_fact(node);
		if (statement != NULL) effective = statement->scope;
	}
	if (!effective.valid()) effective = scope;
	const bool entered = state->lexical_frames.empty() ||
		state->lexical_frames.back().scope != effective;
	if (entered)
		state->lexical_frames.push_back(SwitchInitializationFrame(effective));

	if (node.kind == PA10NodeKind::SimpleDeclaration)
	{
		const DeclarationFact* declaration = declaration_fact(node);
		if (declaration != NULL && declaration->automatic_storage &&
			simple_declaration_has_initializer(node))
		{
			++state->lexical_frames.back().initialized;
			++state->active;
		}
	}
	else if (node.kind == PA10NodeKind::ConditionDeclaration)
	{
		const DeclarationFact* declaration = declaration_fact(node);
		if (declaration != NULL && declaration->automatic_storage)
		{
			++state->lexical_frames.back().initialized;
			++state->active;
		}
	}
	else if (node.kind == PA10NodeKind::Condition)
	{
		if (node.children.size() == 1 &&
			node.children.front().kind == PA10NodeKind::ConditionDeclaration)
			collect_switch_transfer_points(node.children.front(), effective, state);
	}
	else if (node.kind == PA10NodeKind::CaseStatement ||
		node.kind == PA10NodeKind::DefaultStatement)
	{
		if (state->active != 0)
			throw std::runtime_error(
				"PA12 case or default label bypasses variable initialization");
		const std::size_t body_index = node.kind == PA10NodeKind::CaseStatement ?
			1 : 0;
		if (node.children.size() <= body_index)
			throw std::runtime_error("PA12 switch label body is missing");
		const PA10AstNode& body = node.children[body_index];
		const ScopeId body_scope = body.kind == PA10NodeKind::CompoundStatement ?
			compound_scope(body) : effective;
		collect_switch_transfer_points(body, body_scope, state);
	}
	else if (node.kind == PA10NodeKind::CompoundStatement)
	{
		for (std::size_t i = 0; i < node.children.size(); ++i)
			collect_switch_transfer_points(node.children[i], effective, state);
	}
	else if (node.kind == PA10NodeKind::IfStatement)
	{
		if (!node.children.empty())
			collect_switch_transfer_points(node.children.front(), effective, state);
		for (std::size_t i = 1; i < node.children.size(); ++i)
		{
			const PA10AstNode& wrapper = node.children[i];
			if (wrapper.children.size() != 1) continue;
			const PA10AstNode& body = wrapper.children.front();
			const ScopeId body_scope = body.kind == PA10NodeKind::CompoundStatement ?
				compound_scope(body) : substatement_scope(body);
			collect_switch_transfer_points(body, body_scope, state);
		}
	}
	else if (node.kind == PA10NodeKind::WhileStatement ||
		node.kind == PA10NodeKind::DoStatement)
	{
		const bool is_while = node.kind == PA10NodeKind::WhileStatement;
		const std::size_t condition_index = is_while ? 0 : 1;
		const std::size_t body_index = is_while ? 1 : 0;
		if (node.children.size() == 2)
		{
			collect_switch_transfer_points(node.children[condition_index],
				effective, state);
			const PA10AstNode& body = node.children[body_index];
			const ScopeId body_scope = body.kind == PA10NodeKind::CompoundStatement ?
				compound_scope(body) : substatement_scope(body);
			collect_switch_transfer_points(body, body_scope, state);
		}
	}
	else if (node.kind == PA10NodeKind::ForStatement)
	{
		if (node.children.size() >= 3)
		{
			collect_switch_transfer_points(node.children.front(), effective, state);
			for (std::size_t i = 1; i + 1 < node.children.size(); ++i)
				if (node.children[i].kind == PA10NodeKind::Condition)
					collect_switch_transfer_points(node.children[i], effective,
						state);
			const PA10AstNode& body = node.children.back();
			const ScopeId body_scope = body.kind == PA10NodeKind::CompoundStatement ?
				compound_scope(body) : substatement_scope(body);
			collect_switch_transfer_points(body, body_scope, state);
		}
	}
	else
	{
		for (std::size_t i = 0; i < node.children.size(); ++i)
			collect_switch_transfer_points(node.children[i], effective, state);
	}

	if (entered)
	{
		state->active -= state->lexical_frames.back().initialized;
		state->lexical_frames.pop_back();
	}
}
} // namespace pa11_semantic_internal
