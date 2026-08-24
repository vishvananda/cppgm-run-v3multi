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
			(current.name.valid() ? name_text(current.name) : "<global>");
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
		value.name) << ' ' << render_binding_type(value) << '\n';
	if (type_kind(value.type) != TypeKind::Function)
		throw std::runtime_error("PA12 function binding has non-function type");
	const TypeKey& function_type = types_[value.type.value];
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
		output << ' ' << render_type(parameter_index < function_type.parameters.size() ?
			function_type.parameters[parameter_index] : parameter.type) << '\n';
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
		output << "namespace-definition " << ast_.producer_spelling(
			node.producer_spelling) << '\n';
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
