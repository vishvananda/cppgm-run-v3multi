#include "pa11_semantic_model.h"
namespace pa11_semantic_internal
{
using namespace pa11_semantic_storage;
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
	  named_record_sidecars_(), scopes_(), unnamed_namespace_index_(),
	  namespace_alias_declaration_points_(),
	  type_declaration_points_(), inline_namespace_declaration_points_(),
	  scope_declaration_points_(), function_definition_points_(),
	  bindings_(), binding_sidecars_(),
	  global_(), deferred_scopes_(),
	  dump_binding_views_(), dump_scope_views_(),
	  anonymous_union_count_(0), anonymous_enum_count_(0), creation_order_(0),
	  lookup_marks_(),
	lookup_generation_(0), lexical_marks_(), lexical_generation_(0),
	lookup_frames_(), declaration_facts_(),
	declaration_fact_index_(), declaration_bindings_(), function_facts_(),
	function_fact_index_(), class_function_facts_(), synthetic_function_facts_(), namespace_facts_(), namespace_fact_index_(),
	compound_facts_(), compound_scope_index_(), statement_facts_(),
	statement_fact_index_(), substatement_scope_index_(), semantic_facts_(),
	semantic_children_(),
	conversion_facts_(), declaration_semantic_ids_(),
	semantic_name_components_(), anonymous_union_fact_index_(),
	builtin_constant_p_name_(), builtin_abort_name_(),
	builtin_abort_binding_(), pa12_render_mode_(false)
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
bool PA11SemanticModel::find_declarator_name(const PA10AstNode& node, NamePath* result)
{
	if (node.kind == PA10NodeKind::Identifier &&
		(node.producer_spelling != 0 || !node.name_parts.empty() ||
		 node.global_name || node.name_prefix_count != 0))
	{
		*result = name_path(node);
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
	result.found = find_declarator_name(node, &result.path);
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
				named_[record.value].defined);
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
	if ((source_key.cv & ~target_key.cv) != 0)
		return false;
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
	if (pointer_convertible(left, right))
		return strip_top_cv_type(right);
	if (pointer_convertible(right, left))
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
	const BindingId result(bindings_.size());
	bindings_.push_back(binding);
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
const NamedRecordSidecar* PA11SemanticModel::named_record_sidecar(
	NamedRecordId id) const
{
	return id.valid() ? named_record_sidecars_.find(id) : NULL;
}
void PA11SemanticModel::set_named_record_sidecar(NamedRecordId id,
	const NamedRecordSidecar& sidecar)
{
	if (!id.valid() || id.value >= named_.size())
		throw std::runtime_error("invalid PA11 named-record sidecar identity");
	named_record_sidecars_.set(id, sidecar);
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
		record_id = NamedRecordId(named_.size());
		named_.push_back(record);
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
	const NamedRecordId record_id(named_.size());
	named_.push_back(record);
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
		record_id = NamedRecordId(named_.size());
		named_.push_back(record);
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
	const NamedRecordId record_id(named_.size());
	named_.push_back(record);
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
TypeId PA11SemanticModel::process_enum_specifier(const PA10AstNode& node, ScopeId scope,
	NamedRecordId* anonymous_record)
{
	const bool scoped = enum_is_scoped(node);
	const NamePath name = enum_name(node);
	const bool definition = !node.children.empty() &&
		child_of_kind(node, PA10NodeKind::Enumerator) != NULL;
	bool has_underlying = false;
	TypeId underlying = fundamental(FundamentalType::Int);
	for (std::size_t i = 0; i < node.children.size(); ++i)
		if (node.children[i].kind == PA10NodeKind::TypeId)
		{
			underlying = type_from_type_id(node.children[i], scope);
			has_underlying = true;
		}
	if (name.empty() && !scoped && !definition)
		throw std::runtime_error("opaque unscoped enum");
	if (!scoped && !definition && !has_underlying && !name.empty())
	{
		const TypeId existing = lookup_type_path(name, scope);
		const NamedRecordId record = named_record_for_type(existing);
		if (!record.valid() || record.value >= named_.size() ||
			named_[record.value].kind != NamedKind::Enum)
			throw std::runtime_error("undeclared elaborated enum");
		return existing;
	}
	ScopeId owner = scope;
	if (!name.empty())
	{
		owner = declaration_scope(name, scope);
		if (!owner.valid())
			throw std::runtime_error("unresolved enum declaration scope");
	}
	TypeId type;
	if (name.empty())
	{
		type = create_anonymous_enum(owner, scoped, has_underlying,
			underlying, definition);
		if (anonymous_record != NULL)
			*anonymous_record = named_record_for_type(type);
	}
	else
	{
		type = ensure_named_enum(owner, name.last(), scoped, has_underlying,
			underlying, definition);
		add_type_binding(owner, name.last(), type, ClassTag::Struct, false,
			SourcePoint(node.source_begin));
		if (definition && name.components.size() > 1)
			add_qualified_enum_view(scope, named_record_for_type(type), name);
	}
	ScopeId value_scope = owner;
	const NamedRecordId record_id = named_record_for_type(type);
	if (record_id.valid() && named_[record_id.value].scope.valid())
		value_scope = named_[record_id.value].scope;
	std::int64_t next_value = 0;
	bool have_next = false;
	for (std::size_t i = 0; i < node.children.size(); ++i)
	{
		const PA10AstNode& child = node.children[i];
		if (child.kind != PA10NodeKind::Enumerator)
			continue;
		ConstValue value;
		if (!child.children.empty())
			value = eval_constexpr(child.children.front(), owner);
		if (!value.valid)
		{
			if (have_next && next_value == std::numeric_limits<std::int64_t>::max())
				throw std::runtime_error("enumerator value overflow");
			value = ConstValue(true, have_next ?
				static_cast<__int128>(next_value) : 0, false);
		}
		if (value.value < static_cast<__int128>(std::numeric_limits<std::int64_t>::min()) ||
			value.value > static_cast<__int128>(std::numeric_limits<std::int64_t>::max()))
			throw std::runtime_error("enumerator value overflow");
		add_enumerator(value_scope, name_from_spelling(child.producer_spelling),
			type, static_cast<std::int64_t>(value.value),
			SourcePoint(node.source_begin));
		next_value = static_cast<std::int64_t>(value.value) + 1;
		have_next = true;
	}
	return type;
}
void PA11SemanticModel::add_enumerator(ScopeId scope, NameId name, TypeId type,
	std::int64_t value, SourcePoint declaration_point)
{
	Scope& current = scopes_[scope.value];
	if (current.types.find(name) != NULL ||
		direct_namespace_exists(scope, name) || direct_value_exists(scope, name))
		throw std::runtime_error("enumerator conflicts with binding");
	Binding enumerator(BindingKind::Enumerator, name, type);
	enumerator.has_value = true;
	enumerator.value = value;
	const BindingId index = store_binding(scope, enumerator);
	append_value_index(scope, name, index, ScopeId(), declaration_point);
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
std::size_t PA11SemanticModel::type_size(TypeId type) const
{
	if (!type.valid() || type.value >= types_.size())
		throw std::runtime_error("invalid sizeof type");
	const TypeKey& key = types_[type.value];
	switch (key.kind)
	{
	case TypeKind::Cv:
		return type_size(key.child);
	case TypeKind::Fundamental:
		switch (key.fundamental)
		{
		case FundamentalType::SignedChar:
		case FundamentalType::UnsignedChar:
		case FundamentalType::Char:
		case FundamentalType::Bool:
			return 1;
		case FundamentalType::ShortInt:
		case FundamentalType::UnsignedShortInt:
		case FundamentalType::Char16T:
			return 2;
		case FundamentalType::Int:
		case FundamentalType::UnsignedInt:
		case FundamentalType::Float:
			return 4;
		case FundamentalType::LongInt:
		case FundamentalType::LongLongInt:
		case FundamentalType::UnsignedLongInt:
		case FundamentalType::UnsignedLongLongInt:
		case FundamentalType::Double:
			return 8;
		case FundamentalType::WcharT:
		case FundamentalType::Char32T:
			return 4;
		case FundamentalType::LongDouble:
			return 16;
		default:
			throw std::runtime_error("sizeof void type");
		}
	case TypeKind::Pointer:
	case TypeKind::MemberPointer:
	case TypeKind::LvalueReference:
	case TypeKind::RvalueReference:
		return 8;
	case TypeKind::Array:
		if (key.unknown_bound)
			throw std::runtime_error("sizeof incomplete array");
		return key.bound.value * type_size(key.child);
	case TypeKind::Function:
		throw std::runtime_error("sizeof function type");
	case TypeKind::Named:
	{
		const NamedRecordId record_id = key.named;
		if (!record_id.valid() || record_id.value >= named_.size())
			throw std::runtime_error("invalid named sizeof type");
		const NamedRecord& record = named_[record_id.value];
		if (record.kind == NamedKind::Enum)
			return type_size(record.has_underlying ? record.underlying :
				fundamental(FundamentalType::Int));
		if (record.kind == NamedKind::TemplateParameter)
			throw std::runtime_error("sizeof template parameter");
		if (!record.defined)
			throw std::runtime_error("sizeof incomplete class");
		return 1;
	}
	}
	throw std::runtime_error("unhandled sizeof type");
}
TypeId PA11SemanticModel::expression_type(const PA10AstNode& node, ScopeId scope)
{
	if (node.kind == PA10NodeKind::Literal)
		return fundamental(node.literal.type);
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
		if (!values.empty())
			return binding(values.front().binding).type;
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
	if (node.kind == PA10NodeKind::SizeofExpression ||
		node.kind == PA10NodeKind::TypeTraitExpression)
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
		const TypeId type = lookup_type_path(name_path(operand), scope);
		if (type.valid())
			return type;
	}
	return expression_type(operand, scope);
}
ConstValue PA11SemanticModel::eval_constexpr(const PA10AstNode& node, ScopeId scope)
{
	if (node.kind == PA10NodeKind::Literal)
		return literal_constant(node);
	if (node.kind == PA10NodeKind::KeywordLiteral)
		return ConstValue(true, node.has_token &&
			node.token == SimpleTokenType::KW_TRUE ? 1 : 0, false);
	if (node.kind == PA10NodeKind::ParenthesizedExpression)
	{
		if (node.children.empty())
			throw std::runtime_error("empty constant expression");
		return eval_constexpr(node.children.front(), scope);
	}
	if (node.kind == PA10NodeKind::IdExpression)
	{
		const std::vector<ValueRef> values = lookup_value_path(name_path(node),
			scope);
		if (values.empty())
			throw std::runtime_error("constant name is not a value");
		const Binding& value_binding = binding(values.front().binding);
		if (!value_binding.has_value)
			throw NonConstantExpression("value is not a constant");
		return ConstValue(true, value_binding.value, false);
	}
	if (node.kind == PA10NodeKind::UnaryExpression)
	{
		if (node.children.size() != 1)
			throw std::runtime_error("invalid unary constant expression");
		ConstValue value = eval_constexpr(node.children.front(), scope);
		switch (node.token)
		{
		case SimpleTokenType::OP_PLUS:
			break;
		case SimpleTokenType::OP_MINUS:
			value.value = -value.value;
			break;
		case SimpleTokenType::OP_LNOT:
			value.value = value.value == 0 ? 1 : 0;
			value.is_unsigned = false;
			break;
		case SimpleTokenType::OP_COMPL:
			value.value = ~value.value;
			break;
		default:
			throw NonConstantExpression("unsupported unary constant operator");
		}
		check_constant_range(value);
		return value;
	}
	if (node.kind == PA10NodeKind::BinaryExpression ||
		node.kind == PA10NodeKind::AssignmentExpression)
	{
		if (node.children.size() != 2)
			throw std::runtime_error("invalid binary constant expression");
		const ConstValue left = eval_constexpr(node.children[0], scope);
		if (node.token == SimpleTokenType::OP_LAND && left.value == 0)
			return ConstValue(true, 0, false);
		if (node.token == SimpleTokenType::OP_LOR && left.value != 0)
			return ConstValue(true, 1, false);
		const ConstValue right = eval_constexpr(node.children[1], scope);
		ConstValue result(true, 0, left.is_unsigned || right.is_unsigned);
		switch (node.token)
		{
		case SimpleTokenType::OP_PLUS: result.value = left.value + right.value; break;
		case SimpleTokenType::OP_MINUS: result.value = left.value - right.value; break;
		case SimpleTokenType::OP_STAR:
			if (result.is_unsigned)
			{
				const unsigned __int128 product =
					static_cast<unsigned __int128>(left.value) *
					static_cast<unsigned __int128>(right.value);
				if (product > static_cast<unsigned __int128>(
					std::numeric_limits<std::uint64_t>::max()))
					throw NonConstantExpression("constant expression overflow");
				result.value = static_cast<__int128>(product);
			}
			else
				result.value = left.value * right.value;
			break;
		case SimpleTokenType::OP_DIV:
			if (right.value == 0) throw NonConstantExpression("constant division by zero");
			if (!result.is_unsigned &&
				left.value == static_cast<__int128>(std::numeric_limits<std::int64_t>::min()) &&
				right.value == -1)
				throw NonConstantExpression("constant signed division overflow");
			result.value = left.value / right.value; break;
		case SimpleTokenType::OP_MOD:
			if (right.value == 0) throw NonConstantExpression("constant modulo by zero");
			if (!result.is_unsigned &&
				left.value == static_cast<__int128>(std::numeric_limits<std::int64_t>::min()) &&
				right.value == -1)
				throw NonConstantExpression("constant signed modulo overflow");
			result.value = left.value % right.value; break;
		case SimpleTokenType::OP_LSHIFT:
		{
			if (right.value < 0 || right.value >= 64)
				throw NonConstantExpression("constant shift count out of range");
			const unsigned int shift = static_cast<unsigned int>(right.value);
			const unsigned __int128 shifted =
				static_cast<unsigned __int128>(left.value) << shift;
			if (left.is_unsigned)
			{
				if (shifted > static_cast<unsigned __int128>(
					std::numeric_limits<std::uint64_t>::max()))
					throw NonConstantExpression("constant expression overflow");
				result.value = static_cast<__int128>(shifted);
			}
			else
			{
				if (left.value < 0 || shifted > static_cast<unsigned __int128>(
					std::numeric_limits<std::int64_t>::max()))
					throw NonConstantExpression("constant expression overflow");
				result.value = static_cast<__int128>(shifted);
			}
			break;
		}
		case SimpleTokenType::OP_RSHIFT:
		{
			if (right.value < 0 || right.value >= 64)
				throw NonConstantExpression("constant shift count out of range");
			const unsigned int shift = static_cast<unsigned int>(right.value);
			result.value = left.is_unsigned ?
				static_cast<__int128>(static_cast<unsigned __int128>(left.value) >> shift) :
				left.value >> shift;
			break;
		}
		case SimpleTokenType::OP_BOR: result.value = left.value | right.value; break;
		case SimpleTokenType::OP_XOR: result.value = left.value ^ right.value; break;
		case SimpleTokenType::OP_AMP: result.value = left.value & right.value; break;
		case SimpleTokenType::OP_EQ: result.value = left.value == right.value; result.is_unsigned = false; break;
		case SimpleTokenType::OP_NE: result.value = left.value != right.value; result.is_unsigned = false; break;
		case SimpleTokenType::OP_LT: result.value = left.value < right.value; result.is_unsigned = false; break;
		case SimpleTokenType::OP_LE: result.value = left.value <= right.value; result.is_unsigned = false; break;
		case SimpleTokenType::OP_GT: result.value = left.value > right.value; result.is_unsigned = false; break;
		case SimpleTokenType::OP_GE: result.value = left.value >= right.value; result.is_unsigned = false; break;
		case SimpleTokenType::OP_LAND: result.value = (left.value != 0 && right.value != 0); result.is_unsigned = false; break;
		case SimpleTokenType::OP_LOR: result.value = (left.value != 0 || right.value != 0); result.is_unsigned = false; break;
		default: throw NonConstantExpression("unsupported binary constant operator");
		}
		check_constant_range(result);
		return result;
	}
	if (node.kind == PA10NodeKind::ConditionalExpression)
	{
		if (node.children.size() != 3)
			throw std::runtime_error("invalid conditional constant expression");
		return eval_constexpr(node.children[eval_constexpr(node.children[0],
			scope).value != 0 ? 1 : 2], scope);
	}
	if (node.kind == PA10NodeKind::CastExpression)
	{
		if (node.children.size() < 2)
			throw std::runtime_error("invalid cast constant expression");
		return eval_constexpr(node.children.back(), scope);
	}
	if (node.kind == PA10NodeKind::SizeofExpression ||
		node.kind == PA10NodeKind::TypeTraitExpression)
		return ConstValue(true, static_cast<__int128>(type_size(
			sizeof_operand_type(node, scope))), false);
	throw NonConstantExpression("unsupported constant expression");
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
		const TypeId parameter = normalize_parameter_type(
			normalized.parameters[i]);
		if (parameter != normalized.parameters[i])
		{
			normalized.parameters[i] = parameter;
			changed = true;
		}
	}
	return changed ? intern_type(normalized) : type;
}
BindingId PA11SemanticModel::add_value(ScopeId scope, NameId name, TypeId type,
	bool function, bool definition, bool lexical_view, BindingId backing_storage,
	SourcePoint declaration_point)
{
	Scope& current = scopes_[scope.value];
	if (direct_namespace_exists(scope, name))
		throw std::runtime_error("value conflicts with namespace");
	const TypeId* type_found = current.types.find(name);
	if (type_found != NULL && type_kind(*type_found) != TypeKind::Named)
		throw std::runtime_error("value conflicts with type alias");
	if (function)
		type = normalize_function_type(type);
	const ValueList* existing_values = current.values.find(name);
	if (existing_values != NULL)
	{
		for (std::size_t i = 0; i < existing_values->entries.size(); ++i)
		{
			const BindingId existing_id = existing_values->entries[i].binding;
			const Binding& existing = binding(existing_id);
			if (!function || existing.kind != BindingKind::Function)
				throw std::runtime_error("incompatible value redeclaration");
			if (type_kind(existing.type) != TypeKind::Function || type_kind(type) != TypeKind::Function)
				throw std::runtime_error("invalid function redeclaration");
			const TypeKey& existing_function = types_[existing.type.value];
			const TypeKey& candidate_function = types_[type.value];
			if (existing_function.cv != candidate_function.cv ||
				existing_function.variadic != candidate_function.variadic ||
				existing_function.parameters != candidate_function.parameters)
				continue;
			if (existing_function.result != candidate_function.result)
				throw std::runtime_error("conflicting function return type");
			if (definition && existing.has_definition)
				throw std::runtime_error("duplicate function definition");
			if (definition) binding(existing_id).has_definition = true;
			if (lexical_view) add_dump_binding_view(scope, existing_id);
			return existing_id;
		}
	}
	Binding value(function ? BindingKind::Function : BindingKind::Variable, name, type);
	value.has_definition = function && definition;
	const BindingId binding_id = store_binding(scope, value);
	if (backing_storage.valid()) { BindingSidecar sidecar; sidecar.backing_storage = backing_storage; set_binding_sidecar(binding_id, sidecar); }
	append_value_index(scope, name, binding_id, ScopeId(), declaration_point);
	return binding_id;
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
	bool has_signed = false;
	bool has_unsigned = false;
	bool has_short = false;
	unsigned int long_count = 0;
	bool has_int = false;
	bool has_char = false;
	bool has_char16 = false;
	bool has_char32 = false;
	bool has_wchar = false;
	bool has_bool = false;
	bool has_float = false;
	bool has_double = false;
	bool has_void = false;
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
			process_class_body(child, type, owner);
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
	if (!result.has_base)
	{
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
void PA11SemanticModel::process_class_body(const PA10AstNode& node, TypeId type, ScopeId owner)
{
	const ScopeId class_scope = class_scope_for_type(type);
	if (!class_scope.valid())
		throw std::runtime_error("class has no class scope");
	for (std::size_t i = 0; i < node.children.size(); ++i)
	{
		const PA10NodeKind kind = node.children[i].kind;
		if (kind == PA10NodeKind::ClassKey || kind == PA10NodeKind::BaseClause)
			continue;
		if (kind == PA10NodeKind::AccessSpecifier ||
			kind == PA10NodeKind::EmptyDeclaration)
			continue;
		process_declaration(node.children[i], class_scope);
	}
	(void)owner;
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
bool PA11SemanticModel::direct_initializer_operand(const PA10AstNode& node,
	ScopeId scope, const PA10AstNode** operand)
{
	if (node.kind != PA10NodeKind::InitDeclarator || node.children.size() != 1)
		return false;
	const PA10AstNode& declarator = node.children.front();
	if (declarator.kind != PA10NodeKind::Declarator ||
		declarator.children.size() != 2 ||
		declarator.children[0].kind != PA10NodeKind::Identifier ||
		declarator.children[1].kind != PA10NodeKind::ParameterClause)
		return false;
	const PA10AstNode& clause = declarator.children[1];
	if (clause.children.size() != 1 ||
		clause.children.front().kind != PA10NodeKind::ParameterDeclaration)
		return false;
	const PA10AstNode& parameter = clause.children.front();
	if (parameter.children.size() != 1 ||
		parameter.children.front().kind != PA10NodeKind::DeclSpecifierSeq)
		return false;
	const PA10AstNode& spec = parameter.children.front();
	if (spec.children.size() != 1 ||
		spec.children.front().kind != PA10NodeKind::DeclSpecifier)
		return false;
	const PA10AstNode& name_node = spec.children.front();
	if (!name_node.identifier_declspecifier || name_node.has_token ||
		name_node.name_parts.empty())
		return false;
	const NamePath name = name_path(name_node);
	if (lookup_type_path(name, scope).valid() ||
		lookup_value_path(name, scope).empty())
		return false;
	if (operand != NULL)
		*operand = &name_node;
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
	if (node.children.size() == 1)
	{
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
		const ScopeId target = declaration_scope(name.path, scope);
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
		TypeId type = direct_initializer ? spec.base :
			apply_declarator(declarator, spec.base, target);
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
			binding_id = add_value(target, name.path.last(), type,
				function, false, true, BindingId(),
				SourcePoint(node.source_begin));
			const NamedRecordId constant_record = named_record_for_type(type);
			const bool ordinary_const_record = !spec.is_constexpr &&
				((spec.cv & 1u) != 0) && constant_record.valid() &&
				constant_record.value < named_.size() &&
				named_[constant_record.value].kind == NamedKind::Class;
			if (!ordinary_const_record && (spec.is_constexpr ||
				((spec.cv & 1u) != 0 && type_kind(type) == TypeKind::Cv)))
			{
				if (init.children.size() > 1)
				{
					const PA10AstNode& initializer = init.children[1];
					if (initializer.children.empty())
						throw std::runtime_error("empty constant initializer");
					const ConstValue value = eval_constexpr(
						initializer.children.front(), target);
					if (value.value < static_cast<__int128>(std::numeric_limits<std::int64_t>::min()) ||
						value.value > static_cast<__int128>(std::numeric_limits<std::int64_t>::max()))
						throw std::runtime_error("constant initializer overflow");
					binding(binding_id).has_value = true;
					binding(binding_id).value =
						static_cast<std::int64_t>(value.value);
				}
			}
		}
		if (spec.is_static && target.value < scopes_.size() &&
			scopes_[target.value].kind == ScopeKind::Class &&
			type_kind(type) == TypeKind::Function)
			mark_static_member(binding_id);
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
	const NamedRecordId record_id(named_.size());
	named_.push_back(record);
	const TypeId type = named_type(record_id);
	current.types.set(name, type);
	add_type_binding(scope, name, type, ClassTag::Struct, false,
		SourcePoint(node.source_begin));
	// The parameter list nested inside a template-template parameter is a
	// separate scope owned by the template argument grammar.  Its names are
	// deliberately not visible in the surrounding declaration.
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
	case PA10NodeKind::FunctionDefinition:
		process_function_definition(node, scope);
		return;
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
		return;
	}
	case PA10NodeKind::ClassSpecifier:
	{
		const NamePath name = class_name(node);
		const ClassTag tag = class_tag(node);
		if (name.empty())
		{
			const TypeId type = create_anonymous_class(scope, tag, node);
			process_class_body(node, type, scope);
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
		process_class_body(node, type, target);
		return;
	}
	case PA10NodeKind::LinkageSpecification:
		for (std::size_t i = 0; i < node.children.size(); ++i)
			process_declaration(node.children[i], scope);
		return;
	case PA10NodeKind::TemplateDeclaration:
		process_template_declaration(node, scope);
		return;
	case PA10NodeKind::StaticAssertDeclaration:
		if (node.children.empty() || eval_constexpr(node.children.front(), scope).value == 0)
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
} // namespace pa11_semantic_internal
