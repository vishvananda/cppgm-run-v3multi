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
std::string PA11SemanticModel::render_record_name(const NamedRecord& record) const
{
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
	result << ' ' << (display_path == NULL ? render_record_name(record) :
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
		const char* value;

		Task(bool text, TypeId type, const char* value)
			: text(text), type(type), value(value)
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
			tasks.push_back(Task(true, TypeId(), ") returning "));
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
std::string PA11SemanticModel::render_binding_type(const Binding& binding) const
{
	if (binding.has_tag && type_kind(binding.type) == TypeKind::Named)
		return render_named(binding.type, binding.class_tag, true);
	return render_type(binding.type);
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
void PA11SemanticModel::dump_binding(std::ostream& output, const Binding& value,
	std::size_t depth, const NamePath* display_path ) const
{
	const std::size_t tag_count = value.kind == BindingKind::Type &&
		display_path == NULL && !value.declaration_tags.empty() ?
		value.declaration_tags.size() : 1;
	for (std::size_t tag_index = 0; tag_index < tag_count; ++tag_index)
	{
		for (std::size_t indent = 0; indent < depth; ++indent)
			output << "  ";
		output << binding_label(value.kind) << ' ';
		if (value.name.valid())
			output << name_text(value.name);
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
		dump_binding(output, binding(source.bindings[i]), depth + 1,
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
			(current.name.valid() ? name_text(current.name) : "<global>");
		break;
	case ScopeKind::Class:
		output << "scope class " <<
			render_record_name(named_[current.record.value]);
		break;
	case ScopeKind::Function:
		output << "scope function " << name_text(current.name);
		break;
	case ScopeKind::Block:
		output << "scope block";
		break;
	case ScopeKind::Enum:
		output << "scope enum " <<
			render_record_name(named_[current.record.value]);
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
				dump_binding(output, binding(view.binding), depth + 1);
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
		dump_binding(output, binding(current.bindings[binding_index]),
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
} // namespace pa11_semantic_internal

void emit_pa11_types(const PA10Ast& ast, std::ostream& output)
{
	pa11_semantic_internal::PA11SemanticModel model(ast);
	model.analyze();
	model.dump(output);
}
