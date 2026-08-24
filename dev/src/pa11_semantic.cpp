#include "pa11_semantic.h"
#include "pa11_semantic_model.h"

namespace pa11_semantic_internal
{
using namespace pa11_semantic_storage;

void PA11SemanticModel::dump(std::ostream& output) const
{
	output << "translation-unit\n";
	dump_scope(output, global_, 1);
}
std::string PA11SemanticModel::render_name_path(const NamePath& path) const
{
	std::ostringstream result;
	if (path.global)
		result << "::";
	for (std::size_t i = 0; i < path.components.size(); ++i)
	{
		if (i != 0)
			result << "::";
		result << name_text(path.components[i]);
	}
	return result.str();
}
std::string PA11SemanticModel::render_generated_name(const GeneratedIdentity& generated) const
{
	std::ostringstream result;
	switch (generated.kind)
	{
	case GeneratedEntityKind::AnonymousUnion:
		result << "__anonymous_union_type__" << generated.source.begin.value << '_'
			<< generated.source.end.value;
		break;
	case GeneratedEntityKind::AnonymousEnum:
		result << "__anonymous_enum" << (generated.ordinal.value + 1);
		break;
	}
	return result.str();
}
std::string PA11SemanticModel::render_record_name(NamedRecordId record_id) const
{
	if (!record_id.valid() || record_id.value >= named_.size())
		throw std::runtime_error("invalid PA11 named record");
	const NamedRecord& record = named_[record_id.value];
	const NamedRecordSidecar* sidecar = named_record_sidecar(record_id);
	if (pa12_render_mode_ && sidecar != NULL && sidecar->local_object_name &&
		record.has_generated_identity)
	{
		std::ostringstream result;
		result << "__local_type" << (record.generated_identity.ordinal.value + 1);
		return result.str();
	}
	if (record.name.valid())
		return name_text(record.name);
	if (record.has_generated_identity)
		return render_generated_name(record.generated_identity);
	return "<anonymous>";
}
std::string PA11SemanticModel::render_named_record(NamedRecordId record_id,
	ClassTag override_tag, bool use_override,
	const NamePath* display_path ) const
{
	if (!record_id.valid() || record_id.value >= named_.size())
		throw std::runtime_error("invalid PA11 named type");
	const NamedRecord& record = named_[record_id.value];
	std::ostringstream result;
	if (record.kind == NamedKind::Enum)
	{
		result << "enum";
		if (record.scoped_enum)
			result << " class";
	}
	else if (record.kind == NamedKind::TemplateParameter)
		result << (record.template_template ? "template-parameter" : "typename");
	else
	{
		const ClassTag tag = use_override ? override_tag : record.class_tag;
		result << (tag == ClassTag::Class ? "class" :
			tag == ClassTag::Union ? "union" : "struct");
	}
	result << ' ' << (display_path == NULL ? render_record_name(record_id) :
		render_name_path(*display_path));
	return result.str();
}
std::string PA11SemanticModel::render_named(TypeId type, ClassTag override_tag,
	bool use_override) const
{
	return render_named_record(named_record_for_type(type), override_tag,
		use_override);
}
std::string PA11SemanticModel::render_type(TypeId type) const
{
	struct Task
	{
		bool text;
		TypeId type;
		std::string value;

		Task(bool text, TypeId type, const char* value)
			: text(text), type(type), value(value == NULL ? "" : value)
		{}
	};
	std::string result;
	std::vector<Task> tasks;
	tasks.push_back(Task(false, type, NULL));
	const std::size_t limit = types_.size() + 1;
	std::size_t steps = 0;
	while (!tasks.empty())
	{
		if (++steps > limit * 8 + 32)
			throw std::runtime_error("PA11 type rendering cycle");
		const Task task = tasks.back();
		tasks.pop_back();
		if (task.text)
		{
			result += task.value;
			continue;
		}
		if (!task.type.valid() || task.type.value >= types_.size())
			throw std::runtime_error("invalid PA11 type for rendering");
		const TypeKey& key = types_[task.type.value];
		switch (key.kind)
		{
		case TypeKind::Fundamental:
			result += fundamental_type_name(key.fundamental);
			break;
		case TypeKind::Named:
			result += render_named(task.type, ClassTag::Struct, false);
			break;
		case TypeKind::Cv:
			if ((key.cv & 1u) != 0)
				result += "const ";
			if ((key.cv & 2u) != 0)
				result += "volatile ";
			tasks.push_back(Task(false, key.child, NULL));
			break;
		case TypeKind::Pointer:
			if ((key.cv & 1u) != 0)
				result += "const ";
			if ((key.cv & 2u) != 0)
				result += "volatile ";
			result += "pointer to ";
			tasks.push_back(Task(false, key.child, NULL));
			break;
		case TypeKind::MemberPointer:
			if ((key.cv & 1u) != 0)
				result += "const ";
			if ((key.cv & 2u) != 0)
				result += "volatile ";
			result += "member-pointer of ";
			result += render_named_record(key.named, ClassTag::Struct, false);
			result += " to ";
			tasks.push_back(Task(false, key.child, NULL));
			break;
		case TypeKind::LvalueReference:
			result += "lvalue-reference to ";
			tasks.push_back(Task(false, key.child, NULL));
			break;
		case TypeKind::RvalueReference:
			result += "rvalue-reference to ";
			tasks.push_back(Task(false, key.child, NULL));
			break;
		case TypeKind::Array:
			result += "array of ";
			if (key.unknown_bound)
				result += "unknown bound of ";
			else
			{
				std::ostringstream bound;
				bound << key.bound.value << ' ';
				result += bound.str();
			}
			tasks.push_back(Task(false, key.child, NULL));
			break;
		case TypeKind::Function:
			result += "function of (";
			tasks.push_back(Task(false, key.result, NULL));
			std::string function_suffix = ")";
			if ((key.cv & 1u) != 0)
				function_suffix += " const";
			if ((key.cv & 2u) != 0)
				function_suffix += " volatile";
			function_suffix += " returning ";
			tasks.push_back(Task(true, TypeId(), function_suffix.c_str()));
			if (key.variadic)
			{
				tasks.push_back(Task(true, TypeId(), "..."));
				if (!key.parameters.empty())
					tasks.push_back(Task(true, TypeId(), ", "));
			}
			for (std::size_t i = key.parameters.size(); i != 0; --i)
			{
				tasks.push_back(Task(false, key.parameters[i - 1], NULL));
				if (i > 1)
					tasks.push_back(Task(true, TypeId(), ", "));
			}
			break;
		}
	}
	return result;
}
std::string PA11SemanticModel::render_member_object_parameter(
	TypeId function_type, ScopeId member_scope) const
{
	if (!member_scope.valid() || member_scope.value >= scopes_.size() ||
		scopes_[member_scope.value].kind != ScopeKind::Class ||
		type_kind(function_type) != TypeKind::Function)
		return render_type(function_type);
	const NamedRecordId owner = scopes_[member_scope.value].record;
	if (!owner.valid())
		throw std::runtime_error("PA12 member function has no object owner");
	const unsigned int qualifiers = types_[function_type.value].cv;
	std::string result = "pointer to ";
	if ((qualifiers & 1u) != 0)
		result += "const ";
	if ((qualifiers & 2u) != 0)
		result += "volatile ";
	result += render_named_record(owner, ClassTag::Struct, false);
	return result;
}
std::string PA11SemanticModel::render_member_function_type(
	TypeId function_type, ScopeId member_scope, BindingId binding_id) const
{
	if (!member_scope.valid() || member_scope.value >= scopes_.size() ||
		scopes_[member_scope.value].kind != ScopeKind::Class ||
		type_kind(function_type) != TypeKind::Function ||
		is_static_member(binding_id))
		return render_type(function_type);
	const TypeKey& function = types_[function_type.value];
	std::string result = "function of (";
	result += render_member_object_parameter(function_type, member_scope);
	for (std::size_t i = 0; i < function.parameters.size(); ++i)
	{
		result += ", ";
		result += render_type(function.parameters[i]);
	}
	if (function.variadic)
	{
		if (!function.parameters.empty())
			result += ", ";
		result += "...";
	}
	result += ") returning ";
	result += render_type(function.result);
	return result;
}
std::string PA11SemanticModel::render_binding_type(const Binding& binding) const
{
	if (binding.has_tag && type_kind(binding.type) == TypeKind::Named)
		return render_named(binding.type, binding.class_tag, true);
	return render_type(binding.type);
}
std::string PA11SemanticModel::binding_display_name(BindingId binding_id) const
{
	const Binding& value = binding(binding_id);
	if (value.name.valid())
		return name_text(value.name);
	const BindingSidecar* sidecar = binding_sidecar(binding_id);
	if (sidecar != NULL && sidecar->generated_name_record.valid() &&
		sidecar->generated_name_record.value < named_.size())
	{
		const NamedRecord& record =
			named_[sidecar->generated_name_record.value];
		if (record.has_generated_identity &&
			record.generated_identity.kind == GeneratedEntityKind::AnonymousUnion)
		{
			std::ostringstream result;
			result << "__anonymous_union_storage__" <<
				record.generated_identity.source.begin.value << '_' <<
				record.generated_identity.source.end.value;
			return result.str();
		}
	}
	if (sidecar != NULL && sidecar->constructor_record.valid() &&
		sidecar->constructor_record.value < named_.size())
		return render_record_name(sidecar->constructor_record);
	return std::string();
}
std::string PA11SemanticModel::qualified_binding_name(ScopeId owner,
	BindingId binding_id) const
{
	const Binding& value = binding(binding_id);
	const BindingSidecar* sidecar = binding_sidecar(binding_id);
	if (sidecar != NULL && sidecar->constructor_record.valid() &&
		sidecar->constructor_record.value < named_.size())
	{
		const std::string name = render_record_name(sidecar->constructor_record);
		return name + "::" + name;
	}
	if (value.name.valid())
		return qualified_binding_name(owner, value.name);
	return binding_display_name(binding_id);
}
const char* PA11SemanticModel::binding_label(BindingKind kind) const
{
	switch (kind)
	{
	case BindingKind::Type: return "type";
	case BindingKind::TypeAlias: return "type-alias";
	case BindingKind::Function: return "function";
	case BindingKind::Variable: return "variable";
	case BindingKind::Parameter: return "parameter";
	case BindingKind::Enumerator: return "enumerator";
	}
	return "binding";
}
void PA11SemanticModel::dump_binding(std::ostream& output, BindingId binding_id,
	std::size_t depth, const NamePath* display_path ) const
{
	const Binding& value = binding(binding_id);
	const std::size_t tag_count = value.kind == BindingKind::Type &&
		display_path == NULL && !value.declaration_tags.empty() ?
		value.declaration_tags.size() : 1;
	for (std::size_t tag_index = 0; tag_index < tag_count; ++tag_index)
	{
		for (std::size_t indent = 0; indent < depth; ++indent)
			output << "  ";
		output << binding_label(value.kind) << ' ';
		output << binding_display_name(binding_id);
		output << ' ';
		if (display_path != NULL &&
			type_kind(value.type) == TypeKind::Named)
			output << render_named_record(named_record_for_type(value.type),
				ClassTag::Struct, false, display_path);
		else if (value.kind == BindingKind::Type &&
			!value.declaration_tags.empty())
			output << render_named(value.type,
				value.declaration_tags[tag_index], true);
		else
			output << render_binding_type(value);
		if (value.kind == BindingKind::Enumerator && value.has_value)
			output << ' ' << value.value;
		output << '\n';
	}
}
bool PA11SemanticModel::has_dump_scope_view(NamedRecordId record) const
{
	return record.valid() && record.value < named_.size() &&
		named_[record.value].dump_scope_view.valid();
}
void PA11SemanticModel::dump_scope_view(std::ostream& output, const DumpScopeView& view,
	std::size_t depth) const
{
	for (std::size_t indent = 0; indent < depth; ++indent)
		output << "  ";
	output << "scope enum " << render_name_path(view.qualified_name) << '\n';
	const NamedRecord& record = named_[view.record.value];
	if (!record.scope.valid())
		throw std::runtime_error("qualified enum has no canonical scope");
	const Scope& source = scopes_[record.scope.value];
	for (std::size_t i = 0; i < source.bindings.size(); ++i)
		dump_binding(output, source.bindings[i], depth + 1,
			&view.qualified_name);
}
void PA11SemanticModel::dump_scope(std::ostream& output, ScopeId scope, std::size_t depth) const
{
	if (!scope.valid() || scope.value >= scopes_.size())
		throw std::runtime_error("invalid PA11 scope identity");
	const Scope& current = scopes_[scope.value];
	for (std::size_t i = 0; i < depth; ++i)
		output << "  ";
	switch (current.kind)
	{
	case ScopeKind::Namespace:
		output << "scope namespace " <<
			(current.name.valid() ? name_text(current.name) :
				(scope == global_ ? "<global>" : "<unnamed>"));
		break;
	case ScopeKind::Class:
		output << "scope class " <<
		render_record_name(current.record);
		break;
	case ScopeKind::Function:
		output << "scope function " << name_text(current.name);
		break;
	case ScopeKind::Block:
		output << "scope block";
		break;
	case ScopeKind::Enum:
		output << "scope enum " <<
		render_record_name(current.record);
		break;
	case ScopeKind::TemplateParameters:
		output << "scope template-parameters";
		break;
	}
	output << '\n';

	std::size_t view_index = 0;
	for (std::size_t binding_index = 0;
		binding_index <= current.bindings.size(); ++binding_index)
	{
		while (view_index < current.binding_views.size() &&
			dump_binding_views_[current.binding_views[view_index].value].position ==
				binding_index)
		{
			const DumpBindingView& view = dump_binding_views_[
				current.binding_views[view_index].value];
			if (view.binding.valid())
				dump_binding(output, view.binding, depth + 1);
			else
			{
				for (std::size_t indent = 0; indent < depth + 1; ++indent)
					output << "  ";
				output << "type " << render_name_path(view.qualified_name) << ' '
					<< render_named_record(view.record, ClassTag::Struct, false,
						&view.qualified_name) << '\n';
			}
			++view_index;
		}
		if (binding_index == current.bindings.size())
			break;
		if (current.kind == ScopeKind::Enum &&
			has_dump_scope_view(current.record))
			continue;
		dump_binding(output, current.bindings[binding_index],
			depth + 1);
	}

	for (int function_pass = 0; function_pass != 2; ++function_pass)
	{
		if (function_pass == 1)
		{
			for (std::size_t i = 0; i < current.children.size(); ++i)
				if (scopes_[current.children[i].value].kind ==
					ScopeKind::Function)
					dump_scope(output, current.children[i], depth + 1);
			continue;
		}
		std::size_t child_index = 0;
		std::size_t scope_view_index = 0;
		while (true)
		{
			while (child_index < current.children.size() &&
				scopes_[current.children[child_index].value].kind ==
					ScopeKind::Function)
				++child_index;
			const bool have_child = child_index < current.children.size();
			const bool have_view = scope_view_index < current.scope_views.size();
			if (!have_child && !have_view)
				break;
			const bool take_view = have_view && (!have_child ||
				dump_scope_views_[current.scope_views[scope_view_index].value].order <
					scopes_[current.children[child_index].value].creation_order);
			if (take_view)
			{
				dump_scope_view(output, dump_scope_views_[
					current.scope_views[scope_view_index].value], depth + 1);
				++scope_view_index;
			}
			else
			{
				dump_scope(output, current.children[child_index], depth + 1);
				++child_index;
			}
		}
	}
}
void PA11SemanticModel::process_function_definition(const PA10AstNode& node, ScopeId scope)
{
	if (node.children.size() != 3)
		throw std::runtime_error("invalid PA11 function definition");
	const PA10AstNode& declarator = node.children[1];
	const DeclaratorName name = declarator_name(declarator);
	if (!name.found)
		throw std::runtime_error("unnamed PA11 function definition");
	const ScopeId target = declaration_scope(name.path, scope);
	if (!target.valid())
		throw std::runtime_error("unresolved PA11 function scope");
	const SpecFact spec = spec_fact(node.children[0], target);
	const TypeId type = apply_declarator(declarator, spec.base, target);
	if (type_kind(type) != TypeKind::Function)
		throw std::runtime_error("PA11 definition is not a function");
	const BindingId function_binding = add_value(target, name.path.last(),
		type, true, true, true, BindingId(), SourcePoint(node.source_begin));
	if (spec.is_static && target.value < scopes_.size() &&
		scopes_[target.value].kind == ScopeKind::Class)
		mark_static_member(function_binding);
	const ScopeId function_scope = create_scope(ScopeKind::Function, target,
		name.path.last());
	FunctionFact function_fact(&node, target, function_binding,
		function_scope, ScopeId());
	function_definition_points_.set(function_scope,
		SourcePoint(node.source_begin));
	const PA10AstNode* clause = top_parameter_clause(declarator);
	if (clause != NULL)
	{
		bool variadic = false;
		std::vector<ParamFact> facts;
		parameter_types(*clause, target, &variadic, &facts);
		(void)variadic;
		for (std::size_t i = 0; i < facts.size(); ++i)
		{
			Binding parameter(BindingKind::Parameter, facts[i].name,
				facts[i].type);
			const BindingId parameter_id = store_binding(function_scope, parameter);
			if (facts[i].name.valid())
				append_value_index(function_scope, facts[i].name, parameter_id,
					ScopeId(), SourcePoint(node.source_begin));
		}
	}
	const ScopeId body_scope = process_compound_statement(node.children[2],
		function_scope);
	function_fact.body_scope = body_scope;
	const FunctionFactId function_id(function_facts_.size());
	function_facts_.push_back(function_fact);
	function_fact_index_.set(&node, function_id);
}
ScopeId PA11SemanticModel::process_compound_statement(const PA10AstNode& node, ScopeId parent)
{
	if (node.kind != PA10NodeKind::CompoundStatement)
		throw std::runtime_error("invalid PA11 compound statement");
	const ScopeId block = create_scope(ScopeKind::Block, parent, NameId());
	compound_facts_.push_back(CompoundFact(&node, block));
	compound_scope_index_.set(&node, block);
	for (std::size_t i = 0; i < node.children.size(); ++i)
	{
		const PA10AstNode& child = node.children[i];
		switch (child.kind)
		{
		case PA10NodeKind::SimpleDeclaration:
		case PA10NodeKind::AliasDeclaration:
		case PA10NodeKind::NamespaceAliasDefinition:
		case PA10NodeKind::UsingDirective:
		case PA10NodeKind::UsingDeclaration:
			// Block declarations are formed once, in source order, before
			// PA12 traverses their statement facts.
			process_declaration(child, block);
			break;
		case PA10NodeKind::ClassSpecifier:
			// A standalone block anonymous union is a declaration whose
			// backing storage is synthesized by the typed PA11 owner.
			process_declaration(child, block);
			break;
		case PA10NodeKind::EnumSpecifier:
			// A block enum publishes its typed enumerator bindings before
			// PA12 analyzes the following expressions.
			process_declaration(child, block);
			break;
		case PA10NodeKind::CompoundStatement:
			process_compound_statement(child, block);
			break;
		case PA10NodeKind::EmptyDeclaration:
			break;
		default:
			// PA11's first semantic layer creates block scopes; expression
			// and statement semantics belong to later assignments.
			break;
		}
	}
	return block;
}
void PA11SemanticModel::process_namespace(const PA10AstNode& node, ScopeId parent)
{
	ScopeId namespace_id;
	const SourcePoint declaration_point(node.source_begin);
	if (node.producer_spelling == 0)
	{
		const ScopeId* existing = unnamed_namespace_index_.find(parent);
		if (existing != NULL)
			namespace_id = *existing;
		else
		{
			namespace_id = create_scope(ScopeKind::Namespace, parent,
				NameId());
			unnamed_namespace_index_.set(parent, namespace_id);
			scope_declaration_points_.set(namespace_id, declaration_point);
			// An unnamed namespace is visible through a typed implicit
			// using-directive in its enclosing namespace.  The relation is
			// installed once, even when the syntax is reopened later.
			scopes_[parent.value].using_directives.push_back(
				UsingDirectiveRelation(namespace_id, declaration_point));
			scopes_[parent.value].effective_using_directives.push_back(
				EffectiveUsingDirective(namespace_id, parent,
					declaration_point));
		}
	}
	else
	{
		const NameId name = name_from_spelling(node.producer_spelling);
		namespace_id = named_namespace(parent, name);
		if (scope_declaration_points_.find(namespace_id) == NULL)
			scope_declaration_points_.set(namespace_id, declaration_point);
	}
	const NamespaceFactId namespace_fact_id(namespace_facts_.size());
	namespace_facts_.push_back(NamespaceFact(&node, namespace_id));
	namespace_fact_index_.set(&node, namespace_fact_id);
	for (std::size_t i = 0; i < node.children.size(); ++i)
		if (node.children[i].kind == PA10NodeKind::InlineMarker)
		{
			scopes_[namespace_id.value].inline_namespace = true;
			const SourcePoint* existing =
				inline_namespace_declaration_points_.find(namespace_id);
			if (existing == NULL || !existing->valid() ||
				(existing->value > declaration_point.value))
				inline_namespace_declaration_points_.set(namespace_id,
					declaration_point);
		}
	for (std::size_t i = 0; i < node.children.size(); ++i)
	{
		if (node.children[i].kind == PA10NodeKind::InlineMarker)
			continue;
		process_declaration(node.children[i], namespace_id);
	}
}
template<typename Relation>
bool source_point_relation_visible(const std::vector<Relation>& entries,
	NameId name, SourcePoint point)
{
	for (std::size_t i = 0; i < entries.size(); ++i)
		if (entries[i].name == name)
			return !entries[i].declaration_point.valid() ||
				entries[i].declaration_point.value <= point.value;
	return true;
}
bool namespace_source_point_applicable(const std::vector<Scope>& scopes,
	ScopeId scope, SourcePoint point)
{
	return point.valid() && scope.valid() && scope.value < scopes.size() &&
		scopes[scope.value].kind == ScopeKind::Namespace;
}
void PA11SemanticModel::record_namespace_alias(ScopeId scope, NameId name,
	SourcePoint declaration_point)
{
	NamespaceAliasList* list = namespace_alias_declaration_points_.find(scope);
	if (list == NULL)
	{
		namespace_alias_declaration_points_.set(scope, NamespaceAliasList());
		list = namespace_alias_declaration_points_.find(scope);
	}
	for (std::size_t i = 0; i < list->entries.size(); ++i)
		if (list->entries[i].name == name)
			return;
	list->entries.push_back(NamespaceAliasRelation(name, declaration_point));
}
bool PA11SemanticModel::namespace_alias_visible_at(ScopeId scope,
	NameId name, SourcePoint point) const
{
	if (!namespace_source_point_applicable(scopes_, scope, point))
		return true;
	const NamespaceAliasList* list =
		namespace_alias_declaration_points_.find(scope);
	return list == NULL ? true : source_point_relation_visible(
		list->entries, name, point);
}
void PA11SemanticModel::record_type_declaration(ScopeId scope, NameId name,
	SourcePoint declaration_point, BindingId declaration)
{
	if (!namespace_source_point_applicable(scopes_, scope, declaration_point))
		return;
	TypeDeclarationList* list = type_declaration_points_.find(scope);
	if (list == NULL)
	{
		type_declaration_points_.set(scope, TypeDeclarationList());
		list = type_declaration_points_.find(scope);
	}
	for (std::size_t i = 0; i < list->entries.size(); ++i)
	{
		TypeDeclarationRelation& entry = list->entries[i];
		if (entry.name == name)
		{
			const bool earlier = !entry.declaration_point.valid() ||
				(declaration_point.valid() && declaration_point.value <
					entry.declaration_point.value);
			if (earlier)
			{
				entry.declaration_point = declaration_point;
				if (declaration.valid())
					entry.declaration = declaration;
			}
			else if (!entry.declaration.valid() && declaration.valid())
				entry.declaration = declaration;
			return;
		}
	}
	list->entries.push_back(TypeDeclarationRelation(name, declaration_point,
		declaration));
}
bool PA11SemanticModel::type_visible_at(ScopeId scope, NameId name,
	SourcePoint point) const
{
	if (!namespace_source_point_applicable(scopes_, scope, point))
		return true;
	const TypeDeclarationList* list = type_declaration_points_.find(scope);
	return list == NULL ? true : source_point_relation_visible(
		list->entries, name, point);
}
BindingId PA11SemanticModel::type_declaration_identity(ScopeId scope,
	NameId name) const
{
	const TypeDeclarationList* list = type_declaration_points_.find(scope);
	if (list != NULL)
		for (std::size_t i = 0; i < list->entries.size(); ++i)
			if (list->entries[i].name == name &&
				list->entries[i].declaration.valid())
				return list->entries[i].declaration;
	const TypeId* type = scopes_[scope.value].types.find(name);
	if (type == NULL)
		type = scopes_[scope.value].using_types.find(name);
	if (type == NULL)
		return BindingId();
	const Scope& current = scopes_[scope.value];
	for (std::size_t i = 0; i < current.bindings.size(); ++i)
	{
		const BindingId id = current.bindings[i];
		const Binding& candidate = binding(id);
		if (candidate.name == name && candidate.type == *type &&
			(candidate.kind == BindingKind::Type ||
				candidate.kind == BindingKind::TypeAlias))
			return id;
	}
	return BindingId();
}
bool PA11SemanticModel::inline_namespace_visible_at(ScopeId scope,
	SourcePoint point) const
{
	if (!namespace_source_point_applicable(scopes_, scope, point))
		return true;
	const SourcePoint* marker = inline_namespace_declaration_points_.find(scope);
	return marker == NULL || !marker->valid() || marker->value <= point.value;
}
void PA11SemanticModel::process_namespace_alias(const PA10AstNode& node, ScopeId scope)
{
	if (node.producer_spelling == 0 || node.children.size() != 1)
		throw std::runtime_error("invalid PA11 namespace alias");
	const NameId name = name_from_spelling(node.producer_spelling);
	const NamePath target_name = name_path(node.children.front());
	const ScopeId target = resolve_namespace_path(target_name, scope);
	if (!target.valid())
		throw std::runtime_error("namespace alias target is not a namespace");
	Scope& current = scopes_[scope.value];
	if (current.namespaces.find(name) != NULL ||
		current.types.find(name) != NULL ||
		direct_value_exists(scope, name))
		throw std::runtime_error("namespace alias conflicts with binding");
	const ScopeId* old = current.namespace_aliases.find(name);
	if (old != NULL && *old != target)
		throw std::runtime_error("namespace alias redefinition");
	if (old == NULL)
		record_namespace_alias(scope, name, SourcePoint(node.source_begin));
	current.namespace_aliases.set(name, target);
}
void PA11SemanticModel::process_using_directive(const PA10AstNode& node, ScopeId scope)
{
	if (node.children.size() != 1)
		throw std::runtime_error("invalid PA11 using directive");
	const ScopeId target = resolve_namespace_path(name_path(node.children.front()), scope);
	if (!target.valid())
		throw std::runtime_error("using directive target is not a namespace");
	const ScopeId effective = common_ancestor(scope, target);
	if (!effective.valid())
		throw std::runtime_error("using directive has no common ancestor");
	const SourcePoint declaration_point(node.source_begin);
	scopes_[scope.value].using_directives.push_back(
		UsingDirectiveRelation(target, declaration_point));
	scopes_[effective.value].effective_using_directives.push_back(
		EffectiveUsingDirective(target, scope, declaration_point));
}
void PA11SemanticModel::process_using_declaration(const PA10AstNode& node, ScopeId scope)
{
	if (node.children.size() != 1)
		throw std::runtime_error("invalid PA11 using declaration");
	const NamePath target_name = name_path(node.children.front());
	BindingId origin;
	const TypeId type = lookup_type_path(target_name, scope, SourcePoint(),
		&origin);
	const NameId introduced = target_name.last();
	Scope& current = scopes_[scope.value];
	if (current.types.find(introduced) != NULL ||
		direct_namespace_exists(scope, introduced))
		throw std::runtime_error("using declaration conflicts with binding");
	if (type.valid())
	{
		if (direct_value_exists(scope, introduced))
			throw std::runtime_error("using declaration conflicts with binding");
		current.types.set(introduced, type);
		current.using_types.set(introduced, type);
		BindingKind kind = BindingKind::TypeAlias;
		if (origin.valid() && binding(origin).kind == BindingKind::Type)
			kind = BindingKind::Type;
		const BindingId introduced_binding = store_binding(scope,
			Binding(kind, introduced, type));
		record_type_declaration(scope, introduced,
			SourcePoint(node.source_begin), origin.valid() ? origin :
			introduced_binding);
		return;
	}
	const std::vector<ValueRef> values = lookup_value_path(target_name, scope);
	if (values.empty())
		throw std::runtime_error("using declaration target is not a binding");
	const ValueList* existing = current.values.find(introduced);
	bool existing_functions = existing != NULL && !existing->entries.empty();
	if (existing_functions)
	{
		for (std::size_t i = 0; i < existing->entries.size(); ++i)
		{
			const Binding& old = binding(existing->entries[i].binding);
			if (old.kind != BindingKind::Function ||
				type_kind(old.type) != TypeKind::Function)
			{
				existing_functions = false;
				break;
			}
		}
	}
	bool incoming_functions = true;
	bool incoming_nonfunctions = true;
	for (std::size_t i = 0; i < values.size(); ++i)
	{
		const Binding& imported = binding(values[i].binding);
		const bool is_function = imported.kind == BindingKind::Function &&
			type_kind(imported.type) == TypeKind::Function;
		incoming_functions = incoming_functions && is_function;
		incoming_nonfunctions = incoming_nonfunctions && !is_function;
	}
	if (!incoming_functions && !incoming_nonfunctions)
		throw std::runtime_error("using declaration mixes value kinds");
	std::vector<ValueRef> additions;
	for (std::size_t i = 0; i < values.size(); ++i)
	{
		bool duplicate = false;
		if (existing != NULL)
			for (std::size_t j = 0; j < existing->entries.size(); ++j)
				if (existing->entries[j].binding == values[i].binding &&
					existing->entries[j].origin == values[i].scope)
				{
					duplicate = true;
					break;
				}
		if (!duplicate)
			for (std::size_t j = 0; j < additions.size(); ++j)
				if (additions[j].binding == values[i].binding &&
					additions[j].scope == values[i].scope)
				{
					duplicate = true;
					break;
				}
		if (duplicate)
			continue;
		const Binding& imported = binding(values[i].binding);
		const bool is_function = imported.kind == BindingKind::Function &&
			type_kind(imported.type) == TypeKind::Function;
		if (existing != NULL && (!existing_functions || !is_function))
			throw std::runtime_error("using declaration conflicts with binding");
		additions.push_back(values[i]);
	}
	for (std::size_t i = 0; i < additions.size(); ++i)
	{
		append_value_index(scope, introduced, additions[i].binding,
			additions[i].scope, SourcePoint(node.source_begin));
		// Keep one PA11 dump view per imported canonical binding in this
		// scope.  Lookup retains the full (BindingId, origin ScopeId) pair.
		bool have_view = false;
		for (std::size_t j = 0; j < current.binding_views.size(); ++j)
		{
			const DumpBindingViewId view_id = current.binding_views[j];
			if (view_id.valid() && view_id.value < dump_binding_views_.size() &&
				dump_binding_views_[view_id.value].binding == additions[i].binding)
			{
				have_view = true;
				break;
			}
		}
		if (!have_view)
			add_dump_binding_view(scope, additions[i].binding);
	}
}
} // namespace pa11_semantic_internal

void emit_pa11_types(const PA10Ast& ast, std::ostream& output)
{
	pa11_semantic_internal::PA11SemanticModel model(ast);
	model.analyze();
	model.dump(output);
}

namespace pa11_semantic_internal
{
using namespace pa11_semantic_storage;

const char* PA11SemanticModel::semantic_category_name(
	SemanticValueCategory category) const
{
	switch (category)
	{
	case SemanticValueCategory::Lvalue: return "lvalue";
	case SemanticValueCategory::Prvalue: return "prvalue";
	case SemanticValueCategory::Xvalue: return "xvalue";
	}
	return "prvalue";
}
std::string PA11SemanticModel::semantic_operator(const SemanticFact& fact) const
{
	std::ostringstream result;
	result << simple_token_type_name(fact.token) << ':';
	if (fact.source != NULL && fact.source->has_token &&
		fact.source->token == fact.token && fact.source->token_spelling != 0)
		result << ast_.spelling(fact.source->token_spelling);
	else
	{
		switch (fact.token)
		{
		case SimpleTokenType::OP_ASS: result << '='; break;
		case SimpleTokenType::OP_AMP: result << '&'; break;
		case SimpleTokenType::OP_DOT: result << '.'; break;
		case SimpleTokenType::OP_ARROW: result << "->"; break;
		default: break;
		}
	}
	return result.str();
}
std::string PA11SemanticModel::semantic_literal_token(
	const SemanticFact& fact) const
{
	if (fact.source == NULL)
		return std::string();
	if (fact.has_literal_value)
	{
		std::ostringstream result;
		if (fact.literal_value_unsigned)
			result << fact.literal_value;
		else
		{
			if (fact.literal_value_negative)
				result << '-';
			result << fact.literal_value;
		}
		return result.str();
	}
	std::ostringstream result;
	if (fact.source->kind == PA10NodeKind::KeywordLiteral)
		result << simple_token_type_name(fact.source->token) << ':';
	if (fact.source->kind == PA10NodeKind::Literal)
	{
		if (fact.source->text != 0)
			result << ast_.spelling(fact.source->text);
	}
	else if (fact.source->token_spelling != 0)
		result << ast_.spelling(fact.source->token_spelling);
	return result.str();
}
void PA11SemanticModel::dump_pa12_fact(std::ostream& output, SemanticFactId id,
	std::size_t depth) const
{
	if (!id.valid() || id.value >= semantic_facts_.size())
		throw std::runtime_error("invalid PA12 semantic fact");
	const SemanticFact& fact = semantic_facts_[id.value];
	const std::string type = fact.type.valid() ? render_type(fact.type) :
		std::string();
	const std::string op = semantic_operator(fact);
	for (std::size_t indent = 0; indent < depth; ++indent)
		output << "  ";
	switch (fact.kind)
	{
	case SemanticFactKind::Variable:
	{
		const Binding& value = binding(fact.binding);
		if (value.kind == BindingKind::TypeAlias)
			output << "type-alias ";
		else if (value.kind == BindingKind::Function)
			output << "function-declaration ";
		else
			output << "variable ";
		if (value.kind == BindingKind::Function && fact.selected_scope.valid())
			output << qualified_binding_name(fact.selected_scope, value.name);
		else
			output << binding_display_name(fact.binding);
		output << ' ' << render_binding_type(value) << '\n';
		for (std::size_t i = 0; i < fact.child_count; ++i)
			dump_pa12_fact(output, semantic_children_[fact.child_begin + i],
				depth + 1);
		return;
	}
	case SemanticFactKind::TypeAlias:
	{
		const Binding& value = binding(fact.binding);
		output << "type-alias " << name_text(value.name) << ' ' <<
			render_binding_type(value) << '\n';
		return;
	}
	case SemanticFactKind::SimpleDeclaration:
		output << "simple-declaration\n";
		for (std::size_t i = 0; i < fact.child_count; ++i)
			dump_pa12_fact(output, semantic_children_[fact.child_begin + i],
				depth + 1);
		return;
	case SemanticFactKind::CompoundStatement:
		output << "compound-statement\n";
		for (std::size_t i = 0; i < fact.child_count; ++i)
			dump_pa12_fact(output, semantic_children_[fact.child_begin + i],
				depth + 1);
		return;
	case SemanticFactKind::ReturnStatement:
		output << "return-statement\n";
		for (std::size_t i = 0; i < fact.child_count; ++i)
			dump_pa12_fact(output, semantic_children_[fact.child_begin + i],
				depth + 1);
		return;
	case SemanticFactKind::ExpressionStatement:
		output << "expression-statement\n";
		for (std::size_t i = 0; i < fact.child_count; ++i)
			dump_pa12_fact(output, semantic_children_[fact.child_begin + i],
				depth + 1);
		return;
	case SemanticFactKind::CallExpression:
		output << "call-expression " << semantic_category_name(fact.category) <<
			' ' << type << '\n';
		if (fact.has_callee)
		{
			const Binding& callee = binding(fact.selected_binding);
			for (std::size_t indent = 0; indent < depth + 1; ++indent)
				output << "  ";
			output << "callee " << qualified_binding_name(fact.selected_scope,
				fact.selected_binding) << ' ' << render_binding_type(callee) << '\n';
		}
		for (std::size_t i = 0; i < fact.child_count; ++i)
			dump_pa12_fact(output, semantic_children_[fact.child_begin + i],
				depth + 1);
		return;
	case SemanticFactKind::IdExpression:
		output << "id-expression " << semantic_category_name(fact.category) <<
			' ' << type << ' ' << (fact.name_count != 0 ? semantic_name(fact) :
				binding_display_name(fact.binding)) << '\n';
		return;
	case SemanticFactKind::MemberExpression:
		output << "member-expression " << semantic_category_name(fact.category) <<
			' ' << type << ' ';
		if (fact.source != NULL &&
			fact.source->kind == PA10NodeKind::MemberExpression)
			output << simple_token_type_name(fact.token) << ':' <<
				semantic_name(fact);
		else
			output << semantic_name(fact);
		output << '\n';
		break;
	case SemanticFactKind::Literal:
		output << "literal " << semantic_category_name(fact.category) << ' ' <<
			type << ' ' << semantic_literal_token(fact) << '\n';
		return;
	case SemanticFactKind::UnaryExpression:
		output << "unary-expression " << semantic_category_name(fact.category) <<
			' ' << type << ' ' << op << '\n';
		break;
	case SemanticFactKind::PostfixExpression:
		output << "postfix-expression " << semantic_category_name(fact.category) <<
			' ' << type << ' ' << op << '\n';
		break;
	case SemanticFactKind::BinaryExpression:
		output << "binary-expression " << semantic_category_name(fact.category) <<
			' ' << type << ' ' << op << '\n';
		break;
	case SemanticFactKind::AssignmentExpression:
		output << "assignment-expression " << semantic_category_name(fact.category) <<
			' ' << type << ' ' << op << '\n';
		break;
	case SemanticFactKind::ConditionalExpression:
		output << "conditional-expression " << semantic_category_name(fact.category) <<
			' ' << type << '\n';
		break;
	case SemanticFactKind::CastExpression:
		output << "cast-expression " << semantic_category_name(fact.category) <<
			' ' << type;
		if (fact.source != NULL && fact.source->has_token)
			output << ' ' << op;
		output << '\n';
		break;
	case SemanticFactKind::SubscriptExpression:
		output << "subscript-expression " << semantic_category_name(fact.category) <<
			' ' << type << '\n';
		break;
	case SemanticFactKind::BracedInitList:
		output << "braced-init-list " << semantic_category_name(fact.category) <<
			' ' << type << '\n';
		break;
	case SemanticFactKind::SizeofExpression:
		output << "sizeof-expression " << semantic_category_name(fact.category) <<
			' ' << type << '\n';
		return;
	case SemanticFactKind::IfStatement:
		output << "if-statement\n";
		break;
	case SemanticFactKind::ThenBranch:
		output << "then\n";
		break;
	case SemanticFactKind::ElseBranch:
		output << "else\n";
		break;
	case SemanticFactKind::SwitchStatement:
		output << "switch-statement\n";
		break;
	case SemanticFactKind::WhileStatement:
		output << "while-statement\n";
		break;
	case SemanticFactKind::DoStatement:
		output << "do-statement\n";
		break;
	case SemanticFactKind::ForStatement:
		output << "for-statement\n";
		break;
	case SemanticFactKind::ForInitStatement:
		output << "for-init-statement\n";
		break;
	case SemanticFactKind::Condition:
		output << "condition\n";
		break;
	case SemanticFactKind::ConditionDeclaration:
		output << "condition-declaration\n";
		break;
	case SemanticFactKind::Iteration:
		output << "iteration\n";
		break;
	case SemanticFactKind::CaseStatement:
		output << "case-statement\n";
		break;
	case SemanticFactKind::DefaultStatement:
		output << "default-statement\n";
		break;
	case SemanticFactKind::BreakStatement:
		output << "break-statement\n";
		break;
	case SemanticFactKind::ContinueStatement:
		output << "continue-statement\n";
		break;
	case SemanticFactKind::ConstructorAction:
		output << "constructor-action " << qualified_binding_name(
			fact.selected_scope, fact.selected_binding) << '\n';
		break;
	}
	for (std::size_t i = 0; i < fact.child_count; ++i)
		dump_pa12_fact(output, semantic_children_[fact.child_begin + i],
			depth + 1);
}
void PA11SemanticModel::dump_pa12_function(std::ostream& output,
	const PA10AstNode& node, std::size_t depth) const
{
	const FunctionFact* function = function_fact(node);
	if (function == NULL || !function->body_fact.valid())
		throw std::runtime_error("PA12 function semantic fact is missing");
	const Binding& value = binding(function->binding);
	for (std::size_t indent = 0; indent < depth; ++indent)
		output << "  ";
	output << "function-definition " << qualified_binding_name(function->owner,
		value.name) << ' ' << render_member_function_type(value.type,
		function->owner, function->binding) << '\n';
	if (type_kind(value.type) != TypeKind::Function)
		throw std::runtime_error("PA12 function binding has non-function type");
	const TypeKey& function_type = types_[value.type.value];
	const bool member_function = function->owner.valid() &&
		function->owner.value < scopes_.size() &&
		scopes_[function->owner.value].kind == ScopeKind::Class &&
		!is_static_member(function->binding);
	const Scope& function_scope = scopes_[function->function_scope.value];
	std::size_t parameter_index = 0;
	for (std::size_t i = 0; i < function_scope.bindings.size(); ++i)
	{
		const Binding& parameter = binding(function_scope.bindings[i]);
		if (parameter.kind != BindingKind::Parameter)
			continue;
		for (std::size_t indent = 0; indent < depth + 1; ++indent)
			output << "  ";
		output << "parameter ";
		if (parameter.name.valid())
			output << name_text(parameter.name);
		if (member_function && parameter_index == 0)
			output << ' ' << render_member_object_parameter(value.type,
				function->owner);
		else
		{
			const std::size_t type_index = member_function ?
				parameter_index - 1 : parameter_index;
			output << ' ' << render_type(type_index < function_type.parameters.size() ?
				function_type.parameters[type_index] : parameter.type);
		}
		output << '\n';
		++parameter_index;
	}
	dump_pa12_fact(output, function->body_fact, depth + 1);
}
void PA11SemanticModel::dump_pa12_synthetic_function(
	std::ostream& output, const SyntheticFunctionFact& function,
	std::size_t depth) const
{
	if (!function.record.valid() || function.record.value >= named_.size())
		throw std::runtime_error("PA12 synthetic function record is missing");
	const Binding& value = binding(function.binding);
	if (value.kind != BindingKind::Function ||
		type_kind(value.type) != TypeKind::Function)
		throw std::runtime_error("PA12 synthetic function type is missing");
	for (std::size_t indent = 0; indent < depth; ++indent)
		output << "  ";
	output << "function-definition " << qualified_binding_name(
		named_[function.record.value].owner, function.binding) << ' ' <<
		render_binding_type(value) << '\n';
	const TypeKey& function_type = types_[value.type.value];
	if (function_type.parameters.size() != 1)
		throw std::runtime_error("PA12 synthetic constructor arity mismatch");
	for (std::size_t indent = 0; indent < depth + 1; ++indent)
		output << "  ";
	output << "parameter this " << render_type(function_type.parameters[0]) << '\n';
	for (std::size_t indent = 0; indent < depth + 1; ++indent)
		output << "  ";
	output << "compound-statement\n";
}
void PA11SemanticModel::dump_pa12_top_node(std::ostream& output,
	const PA10AstNode& node, ScopeId scope, std::size_t depth) const
{
	switch (node.kind)
	{
	case PA10NodeKind::NamespaceDefinition:
	{
		const NamespaceFact* namespace_fact = this->namespace_fact(node);
		const ScopeId namespace_scope = namespace_fact == NULL ?
			ScopeId() : namespace_fact->scope;
		if (!namespace_scope.valid())
			throw std::runtime_error("PA12 namespace dump fact is missing");
		for (std::size_t indent = 0; indent < depth; ++indent)
			output << "  ";
		output << "namespace-definition " << (node.producer_spelling == 0 ?
			"<unnamed>" : ast_.producer_spelling(node.producer_spelling)) << '\n';
		for (std::size_t i = 0; i < node.children.size(); ++i)
			if (node.children[i].kind != PA10NodeKind::InlineMarker)
				dump_pa12_top_node(output, node.children[i], namespace_scope,
					depth + 1);
		return;
	}
	case PA10NodeKind::LinkageSpecification:
		for (std::size_t i = 0; i < node.children.size(); ++i)
			dump_pa12_top_node(output, node.children[i], scope, depth);
		return;
	case PA10NodeKind::SimpleDeclaration:
	{
		const DeclarationFact* declaration = declaration_fact(node);
		if (declaration == NULL)
			return;
		for (std::size_t i = 0; i < declaration->semantic_count; ++i)
			dump_pa12_fact(output, declaration_semantic_ids_[
				declaration->semantic_begin + i], depth);
		return;
	}
	case PA10NodeKind::AliasDeclaration:
	{
		const DeclarationFact* declaration = declaration_fact(node);
		if (declaration == NULL || declaration->semantic_count != 1 ||
			declaration->semantic_begin == InvalidIdentityValue)
			throw std::runtime_error("PA12 alias semantic fact is missing");
		dump_pa12_fact(output, declaration_semantic_ids_[
			declaration->semantic_begin], depth);
		return;
	}
	case PA10NodeKind::FunctionDefinition:
		dump_pa12_function(output, node, depth);
		return;
	default:
		return;
	}
}
} // namespace pa11_semantic_internal
