#include "pa10_ast.h"
#include "pa10_parser_support.h"

#include <sstream>
#include <stdexcept>

namespace
{

const char* node_kind_name(PA10NodeKind kind)
{
	switch (kind)
	{
	case PA10NodeKind::TranslationUnit: return "translation-unit";
	case PA10NodeKind::EmptyDeclaration: return "empty-declaration";
	case PA10NodeKind::SimpleDeclaration: return "simple-declaration";
	case PA10NodeKind::NamespaceDefinition: return "namespace-definition";
	case PA10NodeKind::InlineMarker: return "inline";
	case PA10NodeKind::NamespaceAliasDefinition: return "namespace-alias-definition";
	case PA10NodeKind::UsingDirective: return "using-directive";
	case PA10NodeKind::UsingDeclaration: return "using-declaration";
	case PA10NodeKind::AliasDeclaration: return "alias-declaration";
	case PA10NodeKind::LinkageSpecification: return "linkage-specification";
	case PA10NodeKind::Target: return "target";
	case PA10NodeKind::StaticAssertDeclaration: return "static-assert-declaration";
	case PA10NodeKind::Message: return "message";
	case PA10NodeKind::DeclSpecifierSeq: return "decl-specifier-seq";
	case PA10NodeKind::DeclSpecifier: return "decl-specifier";
	case PA10NodeKind::TypeSpecifierSeq: return "type-specifier-seq";
	case PA10NodeKind::TypeSpecifier: return "type-specifier";
	case PA10NodeKind::TypeName: return "type-name";
	case PA10NodeKind::DecltypeSpecifier: return "decltype-specifier";
	case PA10NodeKind::CvQualifier: return "cv-qualifier";
	case PA10NodeKind::TypeId: return "type-id";
	case PA10NodeKind::AbstractDeclarator: return "abstract-declarator";
	case PA10NodeKind::InitDeclaratorList: return "init-declarator-list";
	case PA10NodeKind::InitDeclarator: return "init-declarator";
	case PA10NodeKind::Declarator: return "declarator";
	case PA10NodeKind::NestedDeclarator: return "nested-declarator";
	case PA10NodeKind::Identifier: return "identifier";
	case PA10NodeKind::PtrOperator: return "ptr-operator";
	case PA10NodeKind::ParameterClause: return "parameter-clause";
	case PA10NodeKind::ParameterDeclaration: return "parameter-declaration";
	case PA10NodeKind::ParameterPack: return "parameter-pack";
	case PA10NodeKind::DefaultArgument: return "default-argument";
	case PA10NodeKind::DefaultTemplateArgument:
		return "default-template-argument";
	case PA10NodeKind::FunctionQualifier: return "function-qualifier";
	case PA10NodeKind::NoexceptSpecification:
		return "noexcept-specification";
	case PA10NodeKind::RefQualifier: return "ref-qualifier";
	case PA10NodeKind::TrailingReturnType: return "trailing-return-type";
	case PA10NodeKind::ArraySuffix: return "array-suffix";
	case PA10NodeKind::Initializer: return "initializer";
	case PA10NodeKind::ParenInitializer: return "paren-initializer";
	case PA10NodeKind::BracedInitList: return "braced-init-list";
	case PA10NodeKind::FunctionDefinition: return "function-definition";
	case PA10NodeKind::CompoundStatement: return "compound-statement";
	case PA10NodeKind::ReturnStatement: return "return-statement";
	case PA10NodeKind::ExpressionStatement: return "expression-statement";
	case PA10NodeKind::IfStatement: return "if-statement";
	case PA10NodeKind::ThenBranch: return "then";
	case PA10NodeKind::ElseBranch: return "else";
	case PA10NodeKind::SwitchStatement: return "switch-statement";
	case PA10NodeKind::WhileStatement: return "while-statement";
	case PA10NodeKind::DoStatement: return "do-statement";
	case PA10NodeKind::ForStatement: return "for-statement";
	case PA10NodeKind::ForInitStatement: return "for-init-statement";
	case PA10NodeKind::Condition: return "condition";
	case PA10NodeKind::ConditionDeclaration: return "condition-declaration";
	case PA10NodeKind::Iteration: return "iteration";
	case PA10NodeKind::CaseStatement: return "case-statement";
	case PA10NodeKind::DefaultStatement: return "default-statement";
	case PA10NodeKind::LabeledStatement: return "labeled-statement";
	case PA10NodeKind::BreakStatement: return "break-statement";
	case PA10NodeKind::ContinueStatement: return "continue-statement";
	case PA10NodeKind::GotoStatement: return "goto-statement";
	case PA10NodeKind::ThrowStatement: return "throw-statement";
	case PA10NodeKind::TryBlock: return "try-block";
	case PA10NodeKind::Handler: return "handler";
	case PA10NodeKind::ExceptionDeclaration: return "exception-declaration";
	case PA10NodeKind::IdExpression: return "id-expression";
	case PA10NodeKind::Literal: return "literal";
	case PA10NodeKind::KeywordLiteral: return "keyword-literal";
	case PA10NodeKind::ParenthesizedExpression: return "parenthesized-expression";
	case PA10NodeKind::CallExpression: return "call-expression";
	case PA10NodeKind::ArgumentList: return "argument-list";
	case PA10NodeKind::MemberExpression: return "member-expression";
	case PA10NodeKind::SubscriptExpression: return "subscript-expression";
	case PA10NodeKind::UnaryExpression: return "unary-expression";
	case PA10NodeKind::PostfixExpression: return "postfix-expression";
	case PA10NodeKind::BinaryExpression: return "binary-expression";
	case PA10NodeKind::AssignmentExpression: return "assignment-expression";
	case PA10NodeKind::ConditionalExpression: return "conditional-expression";
	case PA10NodeKind::PackExpansionExpression:
		return "pack-expansion-expression";
	case PA10NodeKind::CastExpression: return "cast-expression";
	case PA10NodeKind::SizeofExpression: return "sizeof-expression";
	case PA10NodeKind::TypeTraitExpression: return "type-trait-expression";
	case PA10NodeKind::NewExpression: return "new-expression";
	case PA10NodeKind::GlobalScope: return "global-scope";
	case PA10NodeKind::NewPlacement: return "placement";
	case PA10NodeKind::DeleteExpression: return "delete-expression";
	case PA10NodeKind::ArrayDeleteMarker: return "array-delete";
	case PA10NodeKind::LambdaExpression: return "lambda-expression";
	case PA10NodeKind::LambdaIntroducer: return "lambda-introducer";
	case PA10NodeKind::LambdaDeclarator: return "lambda-declarator";
	case PA10NodeKind::LambdaSpecifier: return "lambda-specifier";
	case PA10NodeKind::ClassSpecifier: return "class-specifier";
	case PA10NodeKind::ClassForwardDeclaration: return "class-forward-declaration";
	case PA10NodeKind::SpecialMemberDeclaration:
		return "special-member-declaration";
	case PA10NodeKind::SpecialMemberDefinition:
		return "special-member-definition";
	case PA10NodeKind::MemberSpecifiers: return "member-specifiers";
	case PA10NodeKind::MemberSpecifier: return "specifier";
	case PA10NodeKind::ClassKey: return "class-key";
	case PA10NodeKind::AccessSpecifier: return "access-specifier";
	case PA10NodeKind::VirtualSpecifier: return "virtual";
	case PA10NodeKind::VirtSpecifier: return "virt-specifier";
	case PA10NodeKind::BaseClause: return "base-clause";
	case PA10NodeKind::BaseSpecifier: return "base-specifier";
	case PA10NodeKind::BaseName: return "base-name";
	case PA10NodeKind::BitFieldDeclaration: return "bit-field-declaration";
	case PA10NodeKind::BitFieldDeclarator: return "bit-field-declarator";
	case PA10NodeKind::EnumSpecifier: return "enum-specifier";
	case PA10NodeKind::EnumKey: return "enum-key";
	case PA10NodeKind::Enumerator: return "enumerator";
	case PA10NodeKind::TemplateDeclaration: return "template-declaration";
	case PA10NodeKind::TemplateParameterClause: return "template-parameter-clause";
	case PA10NodeKind::TemplateParameterList: return "template-parameter-list";
	case PA10NodeKind::TypeParameter: return "type-parameter";
	case PA10NodeKind::NonTypeTemplateParameter: return "non-type-template-parameter";
	case PA10NodeKind::TemplateTemplateParameter: return "template-template-parameter";
	case PA10NodeKind::ParameterKey: return "parameter-key";
	case PA10NodeKind::ExplicitInstantiationDeclaration:
		return "explicit-instantiation-declaration";
	case PA10NodeKind::ExplicitSpecializationDeclaration:
		return "explicit-specialization-declaration";
	case PA10NodeKind::CtorInitializer: return "ctor-initializer";
	case PA10NodeKind::MemInitializer: return "mem-initializer";
	case PA10NodeKind::MemInitializerId: return "mem-initializer-id";
	case PA10NodeKind::ParenArgumentList: return "paren-argument-list";
	case PA10NodeKind::SpecialInitializer: return "special-initializer";
	case PA10NodeKind::LeafFixed: return "fixed-token";
	}
	return "unknown";
}

void append_name(const PA10Ast& ast, const PA10AstNode& node,
	std::ostream& output, std::size_t depth);

void append_inline_node(const PA10Ast& ast, const PA10AstNode& node,
	std::ostream& output, std::size_t depth);

void append_destructor_name(const PA10Ast& ast, const PA10AstNode& node,
	std::ostream& output, std::size_t depth);

void validate_node_sidecar_ranges(const PA10Ast& ast,
	const PA10AstNode& node);

bool has_non_token_payload(const PA10AstNode& node)
{
	return node.identifier_declspecifier || node.text != 0 || node.global_name ||
		!node.name_parts.empty() || node.name_prefix_begin != 0 ||
		node.name_prefix_count != 0 || node.producer_spelling != 0 ||
		node.unqualified_id_kind != PA10UnqualifiedIdKind::None ||
		node.unqualified_id_token != SimpleTokenType::OP_SEMICOLON ||
		node.unqualified_id_token_spelling != 0 ||
		node.unqualified_id_spelling != 0 ||
		node.operator_function_kind != PA10OperatorFunctionKind::None ||
		node.operator_token != SimpleTokenType::OP_SEMICOLON ||
		node.operator_presentation_begin != 0 ||
		node.operator_presentation_count != 0 ||
		node.semantic_child_begin != 0 || node.semantic_child_count != 0 ||
		node.lambda_capture_default != PA10LambdaCaptureDefault::None ||
		node.lambda_capture_begin != 0 || node.lambda_capture_count != 0 ||
		node.default_template_argument_form !=
			PA10DefaultTemplateArgumentForm::Normal ||
		node.has_literal;
}

void append_fixed_presentation(const PA10Ast& ast, const PA10AstNode& node,
	std::ostream& output)
{
	if (node.token_spelling != 0)
	{
		output << ast.spelling(node.token_spelling);
		return;
	}
	output << simple_token_type_name(node.token);
}

void append_fixed_id_expression(const PA10Ast& ast,
	const PA10AstNode& node, std::ostream& output)
{
	if (!node.has_token ||
		!PA10ParserSupport::is_builtin_function_style_cast_keyword(node.token) ||
		node.token_spelling == 0 || node.identifier_declspecifier ||
		node.global_name || node.name_prefix_begin != 0 ||
		node.name_prefix_count != 0 || !node.name_parts.empty() ||
		node.producer_spelling != 0 || node.unqualified_id_kind !=
		PA10UnqualifiedIdKind::None || node.unqualified_id_token !=
		SimpleTokenType::OP_SEMICOLON || node.unqualified_id_token_spelling != 0 ||
		node.unqualified_id_spelling != 0 || node.operator_function_kind !=
		PA10OperatorFunctionKind::None || node.operator_token !=
		SimpleTokenType::OP_SEMICOLON || node.operator_presentation_begin != 0 ||
		node.operator_presentation_count != 0 || node.semantic_child_begin != 0 ||
		node.semantic_child_count != 0 || node.text != 0 ||
		node.has_literal || !node.children.empty())
		throw std::runtime_error("invalid PA10 fixed-token id-expression");
	const std::string& spelling = ast.spelling(node.token_spelling);
	if (spelling.empty())
		throw std::runtime_error("empty PA10 fixed-token id-expression spelling");
	output << spelling;
}

void append_inline_children(const PA10Ast& ast,
	const std::vector<PA10AstNode>& children, std::ostream& output,
	std::size_t depth, const char* separator)
{
	for (std::size_t i = 0; i < children.size(); ++i)
	{
		if (i != 0)
			output << separator;
		append_inline_node(ast, children[i], output, depth + 1);
	}
}

void validate_node_sidecar_ranges(const PA10Ast& ast,
	const PA10AstNode& node)
{
	if (node.name_prefix_begin > ast.name_prefix_nodes.size() ||
		node.name_prefix_count > ast.name_prefix_nodes.size() -
			node.name_prefix_begin)
		throw std::runtime_error("invalid PA10 name prefix range");
	if (node.operator_presentation_begin >
		ast.operator_presentation_spellings.size() ||
		node.operator_presentation_count >
		ast.operator_presentation_spellings.size() -
			node.operator_presentation_begin)
		throw std::runtime_error("invalid PA10 operator presentation range");
	if (node.semantic_child_begin > ast.semantic_child_nodes.size() ||
		node.semantic_child_count > ast.semantic_child_nodes.size() -
			node.semantic_child_begin)
		throw std::runtime_error("invalid PA10 semantic child range");
	if (node.lambda_capture_begin > ast.lambda_captures.size() ||
		node.lambda_capture_count > ast.lambda_captures.size() -
			node.lambda_capture_begin)
		throw std::runtime_error("invalid PA10 lambda capture range");
	if (node.default_template_argument_form !=
		PA10DefaultTemplateArgumentForm::Normal &&
		(node.kind != PA10NodeKind::DefaultTemplateArgument ||
			node.children.size() != 1 ||
			node.children.front().kind != PA10NodeKind::Literal ||
			!node.children.front().has_literal))
		throw std::runtime_error("invalid PA10 default template argument form");
	for (std::size_t i = 0; i < node.name_parts.size(); ++i)
	{
		const PA10NameComponent& component = node.name_parts[i];
		if (component.template_argument_begin > ast.template_arguments.size() ||
			component.template_argument_count > ast.template_arguments.size() -
				component.template_argument_begin)
			throw std::runtime_error("invalid PA10 template argument range");
	}
	switch (node.kind)
	{
	case PA10NodeKind::GlobalScope:
		if (!node.has_token || node.token != SimpleTokenType::OP_COLON2 ||
			node.token_spelling == 0 || ast.spelling(node.token_spelling) != "::" ||
			has_non_token_payload(node) || !node.children.empty())
			throw std::runtime_error("invalid PA10 global-scope marker");
		break;
	case PA10NodeKind::NewPlacement:
		if (node.has_token || node.token != SimpleTokenType::OP_SEMICOLON ||
			node.token_spelling != 0 || has_non_token_payload(node) ||
			node.children.size() != 1 ||
			node.children.front().kind != PA10NodeKind::ParenArgumentList)
			throw std::runtime_error("invalid PA10 new-placement shape");
		break;
	case PA10NodeKind::PackExpansionExpression:
		if (node.has_token || node.token != SimpleTokenType::OP_SEMICOLON ||
			node.token_spelling != 0 || has_non_token_payload(node) ||
			node.children.size() != 1)
			throw std::runtime_error("invalid PA10 pack-expansion shape");
		break;
	case PA10NodeKind::NewExpression:
	{
		if (!node.has_token || node.token != SimpleTokenType::KW_NEW ||
			node.token_spelling == 0 || ast.spelling(node.token_spelling) != "new" ||
			has_non_token_payload(node))
			throw std::runtime_error("invalid PA10 new-expression keyword");
		std::size_t child = 0;
		if (child < node.children.size() &&
			node.children[child].kind == PA10NodeKind::GlobalScope)
			++child;
		if (child < node.children.size() &&
			node.children[child].kind == PA10NodeKind::NewPlacement)
			++child;
		if (child >= node.children.size() ||
			node.children[child].kind != PA10NodeKind::TypeId)
			throw std::runtime_error("invalid PA10 new-expression type-id order");
		++child;
		if (child < node.children.size() &&
			node.children[child].kind != PA10NodeKind::Initializer)
			throw std::runtime_error("invalid PA10 new-expression initializer order");
		if (child < node.children.size())
		{
			const PA10AstNode& initializer = node.children[child];
			if (initializer.children.size() != 1 ||
				(initializer.children.front().kind != PA10NodeKind::ParenInitializer &&
				 initializer.children.front().kind != PA10NodeKind::BracedInitList))
				throw std::runtime_error("invalid PA10 new-expression initializer shape");
			++child;
		}
		if (child != node.children.size())
			throw std::runtime_error("invalid PA10 new-expression child order");
		break;
	}
	default:
		break;
	}
}

void append_name(const PA10Ast& ast, const PA10AstNode& node,
	std::ostream& output, std::size_t depth)
{
	if (depth >= PA10_MAX_AST_NESTING)
		throw std::runtime_error("PA10 renderer name nesting limit reached");
	validate_node_sidecar_ranges(ast, node);
	if (node.global_name)
		output << "::";
	if (node.name_prefix_count != 0)
	{
		for (std::size_t prefix = 0; prefix < node.name_prefix_count; ++prefix)
		{
			if (prefix != 0)
				output << "::";
			append_inline_node(ast,
				ast.name_prefix_nodes[node.name_prefix_begin + prefix], output,
				depth + 1);
		}
		if (!node.name_parts.empty())
			output << "::";
	}
	for (std::size_t i = 0; i < node.name_parts.size(); ++i)
	{
		if (i != 0)
			output << "::";
		const PA10NameComponent& component = node.name_parts[i];
		if (component.template_disambiguator)
			output << "template ";
		output << ast.producer_spelling(component.spelling);
		if (!component.has_template_id)
			continue;
		if (component.template_argument_begin > ast.template_arguments.size() ||
			component.template_argument_count > ast.template_arguments.size() -
				component.template_argument_begin)
			throw std::runtime_error("invalid PA10 template argument range");
		output << '<';
		for (std::size_t argument = 0;
			argument < component.template_argument_count; ++argument)
		{
			if (argument != 0)
				output << ',';
			append_inline_node(ast,
				ast.template_arguments[component.template_argument_begin + argument].syntax,
				output, depth + 1);
		}
		output << '>';
	}
	if (node.unqualified_id_kind != PA10UnqualifiedIdKind::None)
	{
		if (node.name_prefix_count != 0 || !node.name_parts.empty())
			output << "::";
		if (node.unqualified_id_kind == PA10UnqualifiedIdKind::Destructor)
			append_destructor_name(ast, node, output, depth + 1);
		else if (node.operator_function_kind ==
			PA10OperatorFunctionKind::Conversion &&
			node.semantic_child_count == 1)
		{
			if (node.operator_presentation_begin >=
				ast.operator_presentation_spellings.size())
				throw std::runtime_error("invalid PA10 conversion presentation range");
			output << ast.spelling(ast.operator_presentation_spellings[
				node.operator_presentation_begin]) << ' ';
			if (node.semantic_child_begin >= ast.semantic_child_nodes.size())
				throw std::runtime_error("invalid PA10 conversion semantic range");
			append_inline_node(ast,
				ast.semantic_child_nodes[node.semantic_child_begin], output,
				depth + 1);
		}
		else
		{
			if (node.operator_presentation_begin >
				ast.operator_presentation_spellings.size() ||
				node.operator_presentation_count >
				ast.operator_presentation_spellings.size() -
					node.operator_presentation_begin)
				throw std::runtime_error("invalid PA10 operator presentation range");
			for (std::size_t i = 0; i < node.operator_presentation_count; ++i)
				output << ast.spelling(ast.operator_presentation_spellings[
					node.operator_presentation_begin + i]);
		}
	}
}

void append_inline_new_expression(const PA10Ast& ast,
	const PA10AstNode& node, std::ostream& output, std::size_t depth)
{
	std::size_t child = 0;
	if (node.children[child].kind == PA10NodeKind::GlobalScope)
		append_inline_node(ast, node.children[child++], output, depth + 1);
	output << ast.spelling(node.token_spelling);
	if (child < node.children.size() &&
		node.children[child].kind == PA10NodeKind::NewPlacement)
		append_inline_node(ast, node.children[child++], output, depth + 1);
	append_inline_node(ast, node.children[child++], output, depth + 1);
	if (child == node.children.size())
		return;
	const PA10AstNode& initializer = node.children[child];
	validate_node_sidecar_ranges(ast, initializer);
	const PA10AstNode& syntax = initializer.children.front();
	validate_node_sidecar_ranges(ast, syntax);
	const bool paren = syntax.kind == PA10NodeKind::ParenInitializer;
	output << (paren ? '(' : '{');
	append_inline_children(ast, syntax.children, output, depth + 1, ",");
	output << (paren ? ')' : '}');
}

void append_inline_node(const PA10Ast& ast, const PA10AstNode& node,
	std::ostream& output, std::size_t depth)
{
	if (depth >= PA10_MAX_AST_NESTING)
		throw std::runtime_error("PA10 renderer inline nesting limit reached");
	validate_node_sidecar_ranges(ast, node);
	switch (node.kind)
	{
	case PA10NodeKind::TypeId:
	case PA10NodeKind::DefaultTemplateArgument:
	case PA10NodeKind::DefaultArgument:
		append_inline_children(ast, node.children, output, depth + 1, "");
		break;
	case PA10NodeKind::TypeSpecifierSeq:
	case PA10NodeKind::DeclSpecifierSeq:
		append_inline_children(ast, node.children, output, depth + 1, " ");
		break;
	case PA10NodeKind::TypeName:
	case PA10NodeKind::IdExpression:
	case PA10NodeKind::Target:
	case PA10NodeKind::BaseName:
	case PA10NodeKind::MemInitializerId:
	case PA10NodeKind::Identifier:
		if (node.kind == PA10NodeKind::IdExpression && node.has_token)
		{
			append_fixed_id_expression(ast, node, output);
			break;
		}
		if (node.kind == PA10NodeKind::TypeName && node.has_token &&
			node.token == SimpleTokenType::KW_TYPENAME)
			output << ast.spelling(node.token_spelling) << ' ';
		if (node.global_name || node.name_prefix_count != 0 ||
			!node.name_parts.empty())
			append_name(ast, node, output, depth + 1);
		else if (node.unqualified_id_kind != PA10UnqualifiedIdKind::None)
		{
			if (node.unqualified_id_kind == PA10UnqualifiedIdKind::Destructor)
				append_destructor_name(ast, node, output, depth + 1);
			else
				for (std::size_t i = 0; i < node.operator_presentation_count; ++i)
					output << ast.spelling(ast.operator_presentation_spellings[
						node.operator_presentation_begin + i]);
		}
		else if (node.producer_spelling != 0)
			output << ast.producer_spelling(node.producer_spelling);
		else if (node.text != 0)
			output << ast.spelling(node.text);
		break;
	case PA10NodeKind::DeclSpecifier:
	case PA10NodeKind::TypeSpecifier:
	case PA10NodeKind::CvQualifier:
	case PA10NodeKind::MemberSpecifier:
	case PA10NodeKind::RefQualifier:
	case PA10NodeKind::FunctionQualifier:
	case PA10NodeKind::SpecialInitializer:
	case PA10NodeKind::ClassKey:
	case PA10NodeKind::EnumKey:
	case PA10NodeKind::AccessSpecifier:
	case PA10NodeKind::VirtualSpecifier:
	case PA10NodeKind::LambdaSpecifier:
	case PA10NodeKind::ParameterKey:
	case PA10NodeKind::LeafFixed:
		if (node.has_token)
		{
			if (node.kind == PA10NodeKind::DeclSpecifier &&
				node.token == SimpleTokenType::KW_DECLTYPE)
			{
				output << "decltype(";
				if (!node.children.empty())
					append_inline_node(ast, node.children.front(), output, depth + 1);
				output << ')';
			}
			else if (node.kind == PA10NodeKind::MemberSpecifier &&
				node.token == SimpleTokenType::KW_EXPLICIT)
			{
				output << ' ' << ast.spelling(node.token_spelling);
				break;
			}
			append_fixed_presentation(ast, node, output);
		}
		break;
	case PA10NodeKind::PtrOperator:
		if (node.name_prefix_count != 0 || !node.name_parts.empty())
		{
			append_name(ast, node, output, depth + 1);
			output << "::*";
		}
		else if (node.has_token)
			append_fixed_presentation(ast, node, output);
		break;
	case PA10NodeKind::DecltypeSpecifier:
		output << "decltype(";
		if (!node.children.empty())
			append_inline_node(ast, node.children.front(), output, depth + 1);
		output << ')';
		break;
	case PA10NodeKind::Literal:
		if (node.text != 0)
			output << ast.spelling(node.text);
		break;
	case PA10NodeKind::KeywordLiteral:
		append_fixed_presentation(ast, node, output);
		break;
	case PA10NodeKind::ParenthesizedExpression:
		output << '(';
		if (!node.children.empty())
			append_inline_node(ast, node.children.front(), output, depth + 1);
		output << ')';
		break;
	case PA10NodeKind::UnaryExpression:
	case PA10NodeKind::PostfixExpression:
		if (node.kind == PA10NodeKind::UnaryExpression)
			append_fixed_presentation(ast, node, output);
		if (!node.children.empty())
			append_inline_node(ast, node.children.front(), output, depth + 1);
		if (node.kind == PA10NodeKind::PostfixExpression)
			append_fixed_presentation(ast, node, output);
		break;
	case PA10NodeKind::BinaryExpression:
	case PA10NodeKind::AssignmentExpression:
		if (!node.children.empty())
			append_inline_node(ast, node.children.front(), output, depth + 1);
		append_fixed_presentation(ast, node, output);
		if (node.children.size() > 1)
			append_inline_node(ast, node.children[1], output, depth + 1);
		break;
	case PA10NodeKind::ConditionalExpression:
		if (node.children.size() > 0)
			append_inline_node(ast, node.children[0], output, depth + 1);
		output << '?';
		if (node.children.size() > 1)
			append_inline_node(ast, node.children[1], output, depth + 1);
		output << ':';
		if (node.children.size() > 2)
			append_inline_node(ast, node.children[2], output, depth + 1);
		break;
	case PA10NodeKind::PackExpansionExpression:
		if (!node.children.empty())
			append_inline_node(ast, node.children.front(), output, depth + 1);
		output << "...";
		break;
	case PA10NodeKind::CallExpression:
		if (!node.children.empty())
			append_inline_node(ast, node.children[0], output, depth + 1);
		if (node.children.size() > 1)
		{
			output << '(';
			append_inline_children(ast, node.children[1].children, output,
				depth + 1, ",");
			output << ')';
		}
		break;
	case PA10NodeKind::ArgumentList:
	case PA10NodeKind::ParenArgumentList:
	case PA10NodeKind::ParameterClause:
		output << '(';
		append_inline_children(ast, node.children, output, depth + 1, ",");
		output << ')';
		break;
	case PA10NodeKind::ParameterDeclaration:
		append_inline_children(ast, node.children, output, depth + 1, " ");
		break;
	case PA10NodeKind::MemberExpression:
		if (!node.children.empty())
			append_inline_node(ast, node.children[0], output, depth + 1);
		append_fixed_presentation(ast, node, output);
		if (node.children.size() > 1)
			append_inline_node(ast, node.children[1], output, depth + 1);
		break;
	case PA10NodeKind::SubscriptExpression:
		if (!node.children.empty())
			append_inline_node(ast, node.children[0], output, depth + 1);
		output << '[';
		if (node.children.size() > 1)
			append_inline_node(ast, node.children[1], output, depth + 1);
		output << ']';
		break;
	case PA10NodeKind::CastExpression:
		if (node.token == SimpleTokenType::OP_LPAREN)
		{
			output << '(';
			if (!node.children.empty())
				append_inline_node(ast, node.children[0], output, depth + 1);
			output << ')';
			if (node.children.size() > 1)
				append_inline_node(ast, node.children[1], output, depth + 1);
		}
		else
		{
			append_fixed_presentation(ast, node, output);
			output << '<';
			if (!node.children.empty())
				append_inline_node(ast, node.children[0], output, depth + 1);
			output << ">(";
			if (node.children.size() > 1)
				append_inline_node(ast, node.children[1], output, depth + 1);
			output << ')';
		}
		break;
	case PA10NodeKind::SizeofExpression:
		output << "sizeof";
		output << '(';
		if (!node.children.empty())
			append_inline_node(ast, node.children.front(), output, depth + 1);
		output << ')';
		break;
	case PA10NodeKind::TypeTraitExpression:
		append_fixed_presentation(ast, node, output);
		output << '(';
		if (!node.children.empty())
			append_inline_node(ast, node.children.front(), output, depth + 1);
		output << ')';
		break;
	case PA10NodeKind::NewExpression:
		append_inline_new_expression(ast, node, output, depth);
		break;
	case PA10NodeKind::GlobalScope:
		if (node.token_spelling != 0)
			output << ast.spelling(node.token_spelling);
		else
			output << "::";
		break;
	case PA10NodeKind::NewPlacement:
		if (!node.children.empty())
			append_inline_node(ast, node.children.front(), output, depth + 1);
		break;
	case PA10NodeKind::Initializer:
	case PA10NodeKind::BracedInitList:
	case PA10NodeKind::ParenInitializer:
		append_inline_children(ast, node.children, output, depth + 1, ",");
		break;
	case PA10NodeKind::ArraySuffix:
		output << '[';
		append_inline_children(ast, node.children, output, depth + 1, "");
		output << ']';
		break;
	default:
		append_inline_children(ast, node.children, output, depth + 1, "");
		break;
	}
}

void append_destructor_name(const PA10Ast& ast, const PA10AstNode& node,
	std::ostream& output, std::size_t depth)
{
	validate_node_sidecar_ranges(ast, node);
	if (node.unqualified_id_token_spelling != 0)
		output << ast.spelling(node.unqualified_id_token_spelling);
	if (node.semantic_child_count != 0)
	{
		if (node.semantic_child_count != 1)
			throw std::runtime_error("invalid PA10 destructor semantic range");
		append_inline_node(ast,
			ast.semantic_child_nodes[node.semantic_child_begin], output, depth + 1);
	}
	else if (node.unqualified_id_spelling != 0)
		output << ast.producer_spelling(node.unqualified_id_spelling);
}

std::string join_name(const PA10Ast& ast, const PA10AstNode& node)
{
	std::ostringstream result;
	append_name(ast, node, result, 0);
	return result.str();
}

std::string node_text(const PA10Ast& ast, PA10StringId id)
{
	return ast.spelling(id);
}

void render_decoded_linkage_literal(const PA10AstNode& node,
	std::ostream& output)
{
	if (!node.has_literal || node.literal.type != FundamentalType::Char ||
		node.literal.element_count == 0)
		return;
	std::size_t count = node.literal.element_count;
	if (count > node.literal.bytes.size())
		count = node.literal.bytes.size();
	if (count != 0 && node.literal.bytes[count - 1] == 0)
		--count;
	output << ' ';
	for (std::size_t i = 0; i < count; ++i)
		output << static_cast<char>(node.literal.bytes[i]);
}

const PA10AstNode* find_declarator_name(const PA10AstNode& node,
	std::size_t depth)
{
	if (depth >= PA10_MAX_AST_NESTING)
		throw std::runtime_error("PA10 renderer nesting limit reached");
	if (node.kind == PA10NodeKind::Identifier &&
		(node.name_prefix_count != 0 || !node.name_parts.empty() ||
			node.unqualified_id_kind != PA10UnqualifiedIdKind::None ||
			node.producer_spelling != 0 ||
			node.text != 0))
		return &node;
	for (std::size_t i = 0; i < node.children.size(); ++i)
	{
		const PA10AstNode* found = find_declarator_name(node.children[i],
			depth + 1);
		if (found != NULL)
			return found;
	}
	return NULL;
}

void render_unqualified_id(const PA10Ast& ast, const PA10AstNode& node,
	std::ostream& output)
{
	output << ' ';
	if (node.unqualified_id_kind == PA10UnqualifiedIdKind::Destructor)
	{
		append_destructor_name(ast, node, output, 1);
		return;
	}
	if (node.operator_presentation_begin >
		ast.operator_presentation_spellings.size() ||
		node.operator_presentation_count >
		ast.operator_presentation_spellings.size() -
			node.operator_presentation_begin)
		throw std::runtime_error("invalid PA10 operator presentation range");
	for (std::size_t i = 0; i < node.operator_presentation_count; ++i)
		output << ast.spelling(ast.operator_presentation_spellings[
			node.operator_presentation_begin + i]);
}

void render_special_member_name(const PA10Ast& ast,
	const PA10AstNode& node, std::ostream& output)
{
	const PA10AstNode* name = NULL;
	for (std::size_t i = 0; i < node.children.size(); ++i)
	{
		name = find_declarator_name(node.children[i], 1);
		if (name != NULL)
			break;
	}
	if (name == NULL)
		return;
	if (name->global_name || name->name_prefix_count != 0 ||
		!name->name_parts.empty())
		output << ' ' << join_name(ast, *name);
	else if (name->unqualified_id_kind != PA10UnqualifiedIdKind::None)
		render_unqualified_id(ast, *name, output);
	else if (name->producer_spelling != 0)
		output << ' ' << ast.producer_spelling(name->producer_spelling);
	else if (name->text != 0)
		output << ' ' << node_text(ast, name->text);
}

void render_scalar_presentation(const PA10Ast& ast,
	const PA10AstNode& node, std::ostream& output)
{
	if (node.producer_spelling != 0)
		output << ' ' << ast.producer_spelling(node.producer_spelling);
	else if (node.text != 0)
		output << ' ' << node_text(ast, node.text);
}

void render_fixed_label(const PA10Ast& ast, const PA10AstNode& node,
	std::ostream& output)
{
	if (node.token_spelling != 0)
		output << ' ' << ast.spelling(node.token_spelling);
}

void render_fixed_suffix(const PA10Ast& ast, const PA10AstNode& node,
	std::ostream& output)
{
	output << ' ' << simple_token_type_name(node.token) << ':';
	if (node.token_spelling != 0)
		output << ast.spelling(node.token_spelling);
}

void render_function_qualifier(const PA10Ast& ast,
	const PA10AstNode& node, std::ostream& output)
{
	output << ' ' << ast.spelling(node.token_spelling);
	if (node.token == SimpleTokenType::KW_NOEXCEPT)
	{
		if (!node.children.empty())
		{
			output << '(';
			append_inline_node(ast, node.children.front(), output, 1);
			output << ')';
		}
		return;
	}
	if (node.token == SimpleTokenType::KW_THROW)
	{
		output << '(';
		for (std::size_t i = 0; i < node.semantic_child_count; ++i)
		{
			if (i != 0)
				output << ',';
			append_inline_node(ast,
				ast.semantic_child_nodes[node.semantic_child_begin + i], output, 1);
		}
		output << ')';
		return;
	}
	if (node.token_spelling != 0)
		output << ast.spelling(node.token_spelling);
}

void append_lambda_introducer(const PA10Ast& ast,
	const PA10AstNode& node, std::ostream& output)
{
	if (node.lambda_capture_begin > ast.lambda_captures.size() ||
		node.lambda_capture_count > ast.lambda_captures.size() -
			node.lambda_capture_begin)
		throw std::runtime_error("invalid PA10 lambda capture range");
	output << " [";
	if (node.lambda_capture_default == PA10LambdaCaptureDefault::Reference)
		output << '&';
	else if (node.lambda_capture_default == PA10LambdaCaptureDefault::Copy)
		output << '=';
	for (std::size_t i = 0; i < node.lambda_capture_count; ++i)
	{
		if (i != 0 || node.lambda_capture_default !=
			PA10LambdaCaptureDefault::None)
			output << ',';
		const PA10LambdaCapture& capture =
			ast.lambda_captures[node.lambda_capture_begin + i];
		switch (capture.kind)
		{
		case PA10LambdaCaptureKind::This:
			output << "this";
			break;
		case PA10LambdaCaptureKind::Identifier:
			if (capture.spelling == 0 ||
				capture.spelling >= ast.producer_spellings.size())
				throw std::runtime_error("invalid PA10 lambda capture identifier");
			output << ast.producer_spelling(capture.spelling);
			break;
		case PA10LambdaCaptureKind::ReferenceIdentifier:
			if (capture.spelling == 0 ||
				capture.spelling >= ast.producer_spellings.size())
				throw std::runtime_error("invalid PA10 lambda reference capture");
			output << '&' << ast.producer_spelling(capture.spelling);
			break;
		}
		if (capture.pack)
			output << "...";
	}
	output << ']';
}

void render_node(const PA10Ast& ast, const PA10AstNode& node,
	std::ostream& output, std::size_t indent,
	bool anonymous_ntp_literal = false)
{
	if (indent >= PA10_MAX_AST_NESTING)
		throw std::runtime_error("PA10 renderer nesting limit reached");
	validate_node_sidecar_ranges(ast, node);
	for (std::size_t i = 0; i < indent; ++i)
		output << "  ";
	if (node.kind == PA10NodeKind::LeafFixed &&
		node.token == SimpleTokenType::OP_DOTS)
		output << "ellipsis";
	else
		output << node_kind_name(node.kind);
	switch (node.kind)
	{
	case PA10NodeKind::NamespaceDefinition:
	case PA10NodeKind::ClassForwardDeclaration:
	case PA10NodeKind::EnumSpecifier:
	case PA10NodeKind::NamespaceAliasDefinition:
	case PA10NodeKind::AliasDeclaration:
		if (node.name_prefix_count != 0 || !node.name_parts.empty())
			output << ' ' << join_name(ast, node);
		else
			render_scalar_presentation(ast, node, output);
		break;
	case PA10NodeKind::LinkageSpecification:
		render_decoded_linkage_literal(node, output);
		break;
	case PA10NodeKind::SpecialMemberDeclaration:
	case PA10NodeKind::SpecialMemberDefinition:
		render_special_member_name(ast, node, output);
		break;
	case PA10NodeKind::ClassSpecifier:
		if (node.name_prefix_count != 0 || !node.name_parts.empty())
			output << ' ' << join_name(ast, node);
		else if (node.producer_spelling != 0)
			output << ' ' << ast.producer_spelling(node.producer_spelling);
		else if (node.text != 0 && node_text(ast, node.text) != "<unnamed>")
			output << ' ' << node_text(ast, node.text);
		break;
	case PA10NodeKind::Target:
	case PA10NodeKind::BaseName:
	case PA10NodeKind::MemInitializerId:
		output << ' ' << join_name(ast, node);
		break;
	case PA10NodeKind::IdExpression:
		if (node.has_token)
		{
			output << ' ';
			append_fixed_id_expression(ast, node, output);
		}
		else if (node.global_name || node.name_prefix_count != 0 ||
			!node.name_parts.empty() ||
			node.unqualified_id_kind != PA10UnqualifiedIdKind::None)
			output << ' ' << join_name(ast, node);
		else
			render_scalar_presentation(ast, node, output);
		break;
	case PA10NodeKind::Identifier:
		if (node.global_name || node.name_prefix_count != 0 ||
			!node.name_parts.empty())
			output << ' ' << join_name(ast, node);
		else if (node.unqualified_id_kind != PA10UnqualifiedIdKind::None)
			render_unqualified_id(ast, node, output);
		else
			render_scalar_presentation(ast, node, output);
		break;
	case PA10NodeKind::Enumerator:
	case PA10NodeKind::GotoStatement:
	case PA10NodeKind::LabeledStatement:
		render_scalar_presentation(ast, node, output);
		break;
	case PA10NodeKind::Message:
		if (node.text != 0)
			output << ' ' << node_text(ast, node.text);
		break;
	case PA10NodeKind::FunctionQualifier:
		render_function_qualifier(ast, node, output);
		break;
	case PA10NodeKind::SpecialInitializer:
		render_fixed_label(ast, node, output);
		break;
	case PA10NodeKind::NewPlacement:
		if (!node.children.empty())
		{
			output << ' ';
			append_inline_node(ast, node.children.front(), output, indent + 1);
		}
		break;
	case PA10NodeKind::DeclSpecifier:
		if (node.name_prefix_count != 0 || !node.name_parts.empty())
		{
			output << ' ';
			if (node.identifier_declspecifier)
				output << "TT_IDENTIFIER:";
			output << join_name(ast, node);
		}
		else if (node.has_token && node.token == SimpleTokenType::KW_DECLTYPE)
		{
			output << " decltype(";
			if (!node.children.empty())
				append_inline_node(ast, node.children.front(), output, 1);
			output << ')';
		}
		else if (node.has_token)
			render_fixed_suffix(ast, node, output);
		break;
	case PA10NodeKind::MemberSpecifier:
		if (node.has_token && node.token == SimpleTokenType::KW_EXPLICIT)
			output << ' ' << ast.spelling(node.token_spelling);
		else if (node.has_token)
			render_fixed_suffix(ast, node, output);
		break;
	case PA10NodeKind::TypeSpecifier:
	case PA10NodeKind::CvQualifier:
	case PA10NodeKind::ClassKey:
	case PA10NodeKind::EnumKey:
	case PA10NodeKind::AccessSpecifier:
	case PA10NodeKind::VirtualSpecifier:
	case PA10NodeKind::ParameterKey:
	case PA10NodeKind::LambdaSpecifier:
	case PA10NodeKind::LeafFixed:
	case PA10NodeKind::UnaryExpression:
	case PA10NodeKind::PostfixExpression:
	case PA10NodeKind::BinaryExpression:
	case PA10NodeKind::AssignmentExpression:
	case PA10NodeKind::MemberExpression:
	case PA10NodeKind::CastExpression:
	case PA10NodeKind::TypeTraitExpression:
		if (node.has_token)
		{
			if (node.token == SimpleTokenType::OP_DOTS)
				output << " ...";
			else
				render_fixed_suffix(ast, node, output);
		}
		break;
	case PA10NodeKind::VirtSpecifier:
		output << " TT_IDENTIFIER:" << ast.producer_spelling(
			node.producer_spelling);
		break;
	case PA10NodeKind::PtrOperator:
		if (node.name_prefix_count != 0 || !node.name_parts.empty())
			output << ' ' << join_name(ast, node) << "::*";
		else if (node.has_token)
			render_fixed_suffix(ast, node, output);
		break;
	case PA10NodeKind::TypeName:
		output << ' ';
		output << join_name(ast, node);
		break;
	case PA10NodeKind::DecltypeSpecifier:
		output << " decltype(";
		if (!node.children.empty())
			append_inline_node(ast, node.children.front(), output, 1);
		output << ')';
		break;
	case PA10NodeKind::TypeSpecifierSeq:
		break;
	case PA10NodeKind::TrailingReturnType:
		if (node.name_prefix_count != 0 || !node.name_parts.empty())
			output << ' ' << join_name(ast, node);
		break;
	case PA10NodeKind::RefQualifier:
		if (node.has_token)
			render_fixed_suffix(ast, node, output);
		break;
	case PA10NodeKind::ParameterPack:
		output << " ...";
		break;
	case PA10NodeKind::ArrayDeleteMarker:
		break;
	case PA10NodeKind::Literal:
		if (anonymous_ntp_literal)
			output << " TT_LITERAL:";
		else
			output << ' ';
		output << node_text(ast, node.text);
		break;
	case PA10NodeKind::KeywordLiteral:
		if (node.has_token)
			render_fixed_suffix(ast, node, output);
		break;
	case PA10NodeKind::LambdaIntroducer:
		append_lambda_introducer(ast, node, output);
		break;
	default:
		break;
	}
	output << '\n';
	for (std::size_t i = 0; i < node.children.size(); ++i)
	{
		const bool child_anonymous_ntp_literal =
			node.kind == PA10NodeKind::DefaultTemplateArgument &&
			node.default_template_argument_form ==
				PA10DefaultTemplateArgumentForm::AnonymousNonTypeLiteral &&
			i == 0;
		render_node(ast, node.children[i], output, indent + 1,
			child_anonymous_ntp_literal);
	}
}

} // namespace

void render_pa10_ast(const PA10Ast& ast, std::ostream& output)
{
	render_node(ast, ast.root, output, 0);
}
