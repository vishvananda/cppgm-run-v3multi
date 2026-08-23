#include "pa7_semantic.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <iostream>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "cpp_declaration_syntax.h"
#include "cpp_semantic_core.h"

using namespace CppSemantic;

struct PA7SemanticModel::Impl : CppSemantic::SemanticCore
{
	const PPTokenBuffer& source;
	std::vector<CppSyntaxToken> tokens;

	explicit Impl(const PPTokenBuffer& source)
		: SemanticCore(), source(source), tokens()
	{}
};

class PA7SemanticActions : public CppDeclarationSyntaxConsumer
{
public:
	explicit PA7SemanticActions(PA7SemanticModel::Impl& model)
		: model_(model), current_(model.global), namespace_parents_(),
		  active_spec_(), declaration_active_(false)
	{}

	bool accept_type_name(const CppSyntaxQualifiedName& name) const
	{
		return model_.accepts_lookup(name, current_, LookupCategory::Type);
	}

	bool accept_namespace_name(const CppSyntaxQualifiedName& name) const
	{
		return model_.accepts_lookup(name, current_,
			LookupCategory::Namespace);
	}

	bool accept_nested_name_specifier(
		const CppSyntaxQualifiedName& name) const
	{
		if (name.global)
			return true;
		if (name.components.size() < 2)
			return false;
		CppSyntaxQualifiedName prefix;
		prefix.components.assign(name.components.begin(),
			name.components.end() - 1);
		return model_.accepts_lookup(prefix, current_,
			LookupCategory::Namespace);
	}

	void on_namespace_begin(bool inline_namespace, bool anonymous_namespace,
		PPSpellingId name)
	{
		NamespaceId child;
		if (anonymous_namespace)
			child = model_.anonymous_namespace(current_, inline_namespace);
		else
			child = model_.named_namespace(current_, model_.intern_name(name),
				inline_namespace);
		namespace_parents_.push_back(current_);
		current_ = child;
	}

	void on_namespace_end()
	{
		if (namespace_parents_.empty())
			throw std::runtime_error("PA7 namespace action underflow");
		current_ = namespace_parents_.back();
		namespace_parents_.pop_back();
	}

	void on_namespace_alias(PPSpellingId name,
		const CppSyntaxQualifiedName& target)
	{
		model_.declare_namespace_alias(current_, model_.intern_name(name),
			model_.resolve_namespace_path(qualified_name(target), current_));
	}

	void on_using_directive(const CppSyntaxQualifiedName& target)
	{
		model_.add_using_directive(current_,
			model_.resolve_namespace_path(qualified_name(target), current_));
	}

	void on_using_declaration(const CppSyntaxQualifiedName& source)
	{
		QualifiedName introduced = qualified_name(source);
		LookupResult type = model_.lookup_path(introduced, current_,
			LookupCategory::Type);
		if (type.found())
		{
			model_.add_using_type(current_, introduced.last(), type.type);
			return;
		}
		LookupResult entity = model_.lookup_path(introduced, current_,
			LookupCategory::Entity);
		if (!entity.found())
			throw std::runtime_error("unresolved PA7 using declaration");
		model_.add_using_entity(current_, introduced.last(), entity.entity);
	}

	void on_alias_declaration(PPSpellingId name,
		const CppSyntaxTypeId& source)
	{
		model_.declare_alias(current_, model_.intern_name(name),
			type_id(source, current_));
	}

	void on_simple_declaration_begin(const CppSyntaxDeclSpec& source)
	{
		if (declaration_active_)
			throw std::runtime_error("nested PA7 declaration action");
		active_spec_ = decl_spec(source, current_);
		declaration_active_ = true;
	}

	void on_simple_declarator(
		const CppSyntaxDeclarator& declarator_source)
	{
		if (!declaration_active_)
			throw std::runtime_error("PA7 declaration action without begin");
		DeclaratorShape shape = declarator(declarator_source, current_);
		if (!shape.has_name)
			throw std::runtime_error("unnamed PA7 declaration");
		const TypeId type = apply_shape(active_spec_.resolved_type, shape);
		const NameId name = shape.name.last();
		const bool qualified = shape.name.global ||
			shape.name.components.size() > 1;
		const NamespaceId target =
			model_.resolve_declaration_target(shape.name, current_);
		if (active_spec_.is_typedef)
			model_.declare_alias(target, name, type);
		else
		{
			const bool is_function =
				model_.type_kind(type) == TypeKind::Function;
			if (qualified)
			{
				LookupResult existing = model_.lookup_qualified(target, name,
					LookupCategory::Entity);
				if (existing.found())
				{
					if ((is_function &&
						model_.entities[existing.entity.value].kind !=
							EntityKind::Function) ||
						(!is_function &&
						model_.entities[existing.entity.value].kind !=
							EntityKind::Variable))
						throw std::runtime_error(
							"qualified declaration kind conflict");
					model_.update_entity(existing.entity, type);
				}
				else
					model_.declare_value(target, name, type, is_function);
			}
			else
				model_.declare_value(current_, name, type, is_function);
		}
	}

	void on_simple_declaration_end()
	{
		if (!declaration_active_)
			throw std::runtime_error("PA7 declaration action without begin");
		declaration_active_ = false;
	}

private:
	PA7SemanticModel::Impl& model_;
	NamespaceId current_;
	std::vector<NamespaceId> namespace_parents_;
	BaseSpec active_spec_;
	bool declaration_active_;

	QualifiedName qualified_name(const CppSyntaxQualifiedName& source)
	{
		QualifiedName result;
		result.global = source.global;
		result.components.reserve(source.components.size());
		for (std::size_t i = 0; i < source.components.size(); ++i)
			result.components.push_back(model_.intern_name(source.components[i]));
		return result;
	}

	TypeId fundamental_from_spec(const BaseSpec& spec)
	{
		FundamentalType type;
		if (spec.has_char)
		{
			if (spec.has_signed)
				type = FundamentalType::SignedChar;
			else if (spec.has_unsigned)
				type = FundamentalType::UnsignedChar;
			else
				type = FundamentalType::Char;
		}
		else if (spec.has_char16)
			type = FundamentalType::Char16T;
		else if (spec.has_char32)
			type = FundamentalType::Char32T;
		else if (spec.has_wchar)
			type = FundamentalType::WcharT;
		else if (spec.has_bool)
			type = FundamentalType::Bool;
		else if (spec.has_float)
			type = FundamentalType::Float;
		else if (spec.has_double)
			type = spec.long_count == 0 ? FundamentalType::Double :
				FundamentalType::LongDouble;
		else if (spec.has_void)
			type = FundamentalType::Void;
		else if (spec.long_count >= 2)
			type = spec.has_unsigned ? FundamentalType::UnsignedLongLongInt :
				spec.has_signed ? FundamentalType::LongLongInt :
				FundamentalType::LongLongInt;
		else if (spec.long_count == 1)
			type = spec.has_unsigned ? FundamentalType::UnsignedLongInt :
				FundamentalType::LongInt;
		else if (spec.has_short)
			type = spec.has_unsigned ? FundamentalType::UnsignedShortInt :
				spec.has_signed ? FundamentalType::ShortInt :
				FundamentalType::ShortInt;
		else if (spec.has_unsigned)
			type = FundamentalType::UnsignedInt;
		else if (spec.has_signed || spec.has_int)
			type = FundamentalType::Int;
		else
			type = FundamentalType::Int;
		return model_.cv(model_.fundamental(type), spec.cv);
	}

	BaseSpec decl_spec(const CppSyntaxDeclSpec& source, NamespaceId scope)
	{
		BaseSpec result;
		result.is_typedef = source.is_typedef;
		result.has_named_type = source.has_named_type;
		result.named_type = qualified_name(source.named_type);
		result.cv = source.cv;
		result.has_char = source.has_char;
		result.has_short = source.has_short;
		result.has_int = source.has_int;
		result.long_count = source.long_count;
		result.has_signed = source.has_signed;
		result.has_unsigned = source.has_unsigned;
		result.has_bool = source.has_bool;
		result.has_wchar = source.has_wchar;
		result.has_char16 = source.has_char16;
		result.has_char32 = source.has_char32;
		result.has_float = source.has_float;
		result.has_double = source.has_double;
		result.has_void = source.has_void;

		if (result.has_named_type)
		{
			if (result.has_char || result.has_short || result.has_int ||
				result.long_count != 0 || result.has_signed ||
				result.has_unsigned || result.has_bool || result.has_wchar ||
				result.has_char16 || result.has_char32 || result.has_float ||
				result.has_double || result.has_void)
				throw std::runtime_error("mixed PA7 type specifiers");
			result.resolved_type = model_.cv(
				model_.lookup_type_path(result.named_type, scope), result.cv);
		}
		else
			result.resolved_type = fundamental_from_spec(result);
		return result;
	}

	std::vector<TypeId> parameters(const CppSyntaxDeclaratorOp& source,
		NamespaceId scope)
	{
		std::vector<TypeId> result;
		if (source.parameters.size() == 1 &&
			!source.parameters[0].has_declarator)
		{
			BaseSpec only = decl_spec(source.parameters[0].spec, scope);
			if (model_.type_kind(only.resolved_type) == TypeKind::Fundamental &&
				model_.types[only.resolved_type.value].key.fundamental ==
					FundamentalType::Void)
				return result;
		}
		result.reserve(source.parameters.size());
		for (std::size_t i = 0; i < source.parameters.size(); ++i)
		{
			const CppSyntaxParameter& parameter = source.parameters[i];
			BaseSpec spec = decl_spec(parameter.spec, scope);
			DeclaratorShape shape;
			if (parameter.has_declarator)
				shape = declarator(parameter.declarator, scope);
			TypeId type = apply_shape(spec.resolved_type, shape);
			type = model_.remove_top_cv(type);
			if (model_.type_kind(type) == TypeKind::Array)
				type = model_.pointer(model_.types[type.value].key.child);
			else if (model_.type_kind(type) == TypeKind::Function)
				type = model_.pointer(type);
			result.push_back(type);
		}
		return result;
	}

	DeclaratorShape declarator(const CppSyntaxDeclarator& source,
		NamespaceId scope)
	{
		DeclaratorShape result;
		result.has_name = source.has_name;
		result.name = qualified_name(source.name);
		result.operations.reserve(source.operations.size());
		for (std::size_t i = 0; i < source.operations.size(); ++i)
		{
			const CppSyntaxDeclaratorOp& source_operation =
				source.operations[i];
			DeclaratorOp operation;
			switch (source_operation.kind)
			{
			case CppSyntaxDeclaratorOpKind::Pointer:
				operation.kind = DeclaratorOpKind::Pointer;
				break;
			case CppSyntaxDeclaratorOpKind::LvalueReference:
				operation.kind = DeclaratorOpKind::LvalueReference;
				break;
			case CppSyntaxDeclaratorOpKind::RvalueReference:
				operation.kind = DeclaratorOpKind::RvalueReference;
				break;
			case CppSyntaxDeclaratorOpKind::Array:
				operation.kind = DeclaratorOpKind::Array;
				break;
			case CppSyntaxDeclaratorOpKind::Function:
				operation.kind = DeclaratorOpKind::Function;
				break;
			}
			operation.cv = source_operation.cv;
			operation.unknown_bound = source_operation.unknown_bound;
			operation.bound = source_operation.bound;
			operation.variadic = source_operation.variadic;
			if (source_operation.kind == CppSyntaxDeclaratorOpKind::Function)
				operation.parameters = parameters(source_operation, scope);
			result.operations.push_back(operation);
		}
		return result;
	}

	TypeId apply_shape(TypeId base, const DeclaratorShape& shape)
	{
		TypeId result = base;
		for (std::vector<DeclaratorOp>::const_reverse_iterator it =
			shape.operations.rbegin(); it != shape.operations.rend(); ++it)
		{
			const DeclaratorOp& operation = *it;
			switch (operation.kind)
			{
			case DeclaratorOpKind::Pointer:
				result = model_.pointer(result, operation.cv);
				break;
			case DeclaratorOpKind::LvalueReference:
				result = model_.reference(result, false);
				break;
			case DeclaratorOpKind::RvalueReference:
				result = model_.reference(result, true);
				break;
			case DeclaratorOpKind::Array:
				result = model_.array(result, operation.unknown_bound,
					operation.bound);
				break;
			case DeclaratorOpKind::Function:
				result = model_.function(operation.parameters, operation.variadic,
					result);
				break;
			}
		}
		return result;
	}

	TypeId type_id(const CppSyntaxTypeId& source, NamespaceId scope)
	{
		BaseSpec spec = decl_spec(source.spec, scope);
		if (!source.has_declarator)
			return spec.resolved_type;
		return apply_shape(spec.resolved_type,
			declarator(source.declarator, scope));
	}
};

PA7SemanticModel::PA7SemanticModel(const PPTokenBuffer& tokens)
	: impl_(new Impl(tokens))
{}

PA7SemanticModel::~PA7SemanticModel()
{
	delete impl_;
}

void PA7SemanticModel::analyze()
{
	impl_->begin_translation_unit(&impl_->source, 0);
	CppSyntaxTokenCollector collector;
	posttokenize_cpp_tokens(impl_->source, collector);
	if (collector.invalid)
		throw std::runtime_error("invalid PA7 posttoken stream");
	if (collector.tokens.empty() ||
		collector.tokens.back().kind != CppSyntaxTokenKind::End)
			throw std::runtime_error("PA7 token stream has no EOF");
	impl_->tokens.swap(collector.tokens);
	PA7SemanticActions actions(*impl_);
	{
		CppDeclarationSyntaxParser parser(impl_->tokens, actions);
		parser.parse();
	}
	impl_->end_translation_unit();
	std::vector<CppSyntaxToken>().swap(impl_->tokens);
#ifdef PA7_AUDIT_COUNTERS
	impl_->audit_report();
#endif
}

void PA7SemanticModel::render(std::ostream& output) const
{
	impl_->render_namespace(output, impl_->global);
}
