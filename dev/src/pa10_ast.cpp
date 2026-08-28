#include "pa10_ast.h"
#include "pa10_declarator_shape.h"
#include "pa10_parser_support.h"
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <utility>
namespace
{
struct PA10Name
{
	bool global;
	bool has_decltype_root;
	PA10AstNode decltype_root;
	std::vector<PA10NameComponent> parts;
	bool has_unqualified_id;
	PA10AstNode unqualified_id;
	PA10Name()
		: global(false), has_decltype_root(false), decltype_root(), parts(),
		  has_unqualified_id(false), unqualified_id()
	{}
};
class PA10Parser
{
public:
	PA10Parser(const std::vector<PA10Token>& tokens,
		const PPSpellingTable& producer_spellings)
		: tokens_(tokens), position_(0), work_(0),
		  work_limit_(PA10ParserSupport::parser_work_limit_for(tokens.size())), nesting_(0),
		  recursion_depth_(0), angle_depth_(0), non_angle_depth_(0),
		  angle_bases_(),
		  template_close_index_(tokens.size(), tokens.size()),
		  template_top_level_or_(tokens.size(), 0),
		  template_top_level_comma_(tokens.size(), 0),
		  rshift_piece1_nested_close_(tokens.size(), 0),
		  delimiter_close_index_(tokens.size(), tokens.size()),
		  parenthesized_group_kind_(tokens.size(), PA10ParserSupport::PA10ParenthesizedGroupKind::None),
		  ast_(), presentation_ids_(), name_scopes_()
	{
		ast_.snapshot_producer_spellings(producer_spellings);
		const std::size_t indexed_work =
			PA10ParserSupport::build_indexes(
				tokens_, template_close_index_, template_top_level_or_,
				template_top_level_comma_,
				rshift_piece1_nested_close_, delimiter_close_index_,
				parenthesized_group_kind_);
		for (std::size_t i = 0; i < indexed_work; ++i)
			charge();
	}
	PA10Ast parse()
	{
		if (tokens_.empty() || tokens_.back().kind != PA10TokenKind::End)
			fail("missing PA10 EOF");
		ast_.root = node(PA10NodeKind::TranslationUnit);
		while (!at_end())
			ast_.root.children.push_back(parse_declaration());
		return ast_;
	}
private:
	const std::vector<PA10Token>& tokens_;
	std::size_t position_;
	std::size_t work_;
	std::size_t work_limit_;
	std::size_t nesting_;
	std::size_t recursion_depth_;
	std::size_t angle_depth_;
	std::size_t non_angle_depth_;
	std::vector<std::size_t> angle_bases_;
	std::vector<std::size_t> template_close_index_;
	std::vector<unsigned char> template_top_level_or_;
	std::vector<unsigned char> template_top_level_comma_;
	std::vector<unsigned char> rshift_piece1_nested_close_;
	std::vector<std::size_t> delimiter_close_index_;
	std::vector<PA10ParserSupport::PA10ParenthesizedGroupKind> parenthesized_group_kind_;
	PA10Ast ast_;
	std::unordered_map<std::string, PA10StringId> presentation_ids_;
	PA10ParserSupport::PA10NameScopes name_scopes_;
	static const std::size_t recursion_limit_ = PA10_MAX_AST_NESTING;
	void fail(const char* message) const
	{
		std::ostringstream text;
		text << message << " at token " << position_;
		throw std::runtime_error(text.str());
	}
	void charge()
	{
		if (work_ >= work_limit_)
			fail("PA10 parser work limit reached");
		++work_;
	}
	void enter()
	{
		if (nesting_ >= PA10_MAX_AST_NESTING)
			fail("PA10 parser nesting limit reached");
		charge();
		++nesting_;
		name_scopes_.enter();
	}
	void leave()
	{
		if (nesting_ == 0)
			fail("PA10 parser nesting underflow");
		--nesting_;
		name_scopes_.leave();
	}
	void enter_recursion()
	{
		if (recursion_depth_ >= recursion_limit_)
			fail("PA10 parser recursion limit reached");
		++recursion_depth_;
	}
	void leave_recursion()
	{
		if (recursion_depth_ == 0)
			fail("PA10 parser recursion underflow");
		--recursion_depth_;
	}
	class RecursionGuard
	{
	public:
		explicit RecursionGuard(PA10Parser& parser) : parser_(parser)
		{
			parser_.enter_recursion();
		}
		~RecursionGuard()
		{
			parser_.leave_recursion();
		}
	private:
		PA10Parser& parser_;
	};
	const PA10Token& look(std::size_t offset = 0) const
	{
		if (position_ >= tokens_.size() ||
			offset >= tokens_.size() - position_)
			fail("PA10 parser read past end");
		return tokens_[position_ + offset];
	}
	bool at_end() const { return look().kind == PA10TokenKind::End; }
	bool fixed(SimpleTokenType type, std::size_t offset = 0) const
	{
		return look(offset).kind == PA10TokenKind::Fixed &&
			look(offset).fixed == type;
	}
	bool identifier(std::size_t offset = 0) const
	{
		return look(offset).kind == PA10TokenKind::Identifier;
	}
	bool token_fixed_at(std::size_t absolute, std::size_t offset,
		SimpleTokenType type) const
	{
		return PA10ParserSupport::token_fixed_at(tokens_, absolute, offset,
			type);
	}
	bool literal(std::size_t offset = 0) const
	{
		return look(offset).kind == PA10TokenKind::Literal;
	}
	bool rshift_piece_first(std::size_t offset = 0) const
	{
		return look(offset).kind == PA10TokenKind::RShiftPiece1;
	}
	bool rshift_piece_second(std::size_t offset = 0) const
	{
		return look(offset).kind == PA10TokenKind::RShiftPiece2;
	}
	bool rshift_operator()
	{
		if (!rshift_piece_first())
			return false;
		charge();
		return rshift_piece_second(1);
	}
	bool consume_fixed(SimpleTokenType type)
	{
		if (!fixed(type))
			fail("unexpected fixed token");
		charge();
		++position_;
		return true;
	}
	PA10Token consume_token()
	{
		if (at_end())
			fail("unexpected end of PA10 input");
		charge();
		PA10Token result = look();
		++position_;
		return result;
	}
	void begin_angle()
	{
		if (angle_depth_ >= PA10_MAX_AST_NESTING)
			fail("PA10 parser angle nesting limit reached");
		charge();
		++angle_depth_;
		angle_bases_.push_back(non_angle_depth_);
	}
	bool can_use_angle_operator() const
	{
		return angle_depth_ == 0 ||
			non_angle_depth_ > angle_bases_.back();
	}
	void begin_non_angle()
	{
		if (non_angle_depth_ >= PA10_MAX_AST_NESTING)
			fail("PA10 parser non-angle nesting limit reached");
		charge();
		++non_angle_depth_;
	}
	void end_non_angle()
	{
		if (non_angle_depth_ == 0)
			fail("PA10 parser non-angle nesting underflow");
		--non_angle_depth_;
	}
	bool at_angle_close() const
	{
		return fixed(SimpleTokenType::OP_GT) || rshift_piece_first() ||
			rshift_piece_second();
	}
	void close_angle()
	{
		if (angle_depth_ == 0 || !at_angle_close())
			fail("expected template close angle");
		if (fixed(SimpleTokenType::OP_GT))
			consume_fixed(SimpleTokenType::OP_GT);
		else
			consume_token();
		--angle_depth_;
		angle_bases_.pop_back();
	}
	void consume_shift_operator(PA10Token* spelling)
	{
		if (!rshift_operator())
			fail("expected shift operator");
		const PA10Token first = consume_token();
		const PA10Token second = consume_token();
		if (spelling != NULL)
		{
			*spelling = first;
			spelling->fixed = SimpleTokenType::OP_RSHIFT;
			spelling->source = first.source + second.source;
		}
	}
	PA10StringId intern(const std::string& value)
	{
		std::unordered_map<std::string, PA10StringId>::const_iterator found =
			presentation_ids_.find(value);
		if (found != presentation_ids_.end())
			return found->second;
		const PA10StringId id = ast_.presentation_spellings.size();
		ast_.presentation_spellings.push_back(value);
		presentation_ids_[value] = id;
		return id;
	}
	void append_operator_presentation(PA10AstNode& owner,
		const std::string& value)
	{
		if (owner.operator_presentation_count == 0)
			owner.operator_presentation_begin =
				ast_.operator_presentation_spellings.size();
		ast_.operator_presentation_spellings.push_back(intern(value));
		++owner.operator_presentation_count;
	}
	void append_semantic_child(PA10AstNode& owner, PA10AstNode child)
	{
		if (owner.semantic_child_count == 0)
			owner.semantic_child_begin = ast_.semantic_child_nodes.size();
		ast_.semantic_child_nodes.push_back(std::move(child));
		++owner.semantic_child_count;
	}
	PA10AstNode node(PA10NodeKind kind) const
	{
		PA10AstNode result(kind);
		result.source_begin = position_;
		result.source_end = position_;
		return result;
	}
	PA10AstNode fixed_node(PA10NodeKind kind)
	{
		const PA10Token token = consume_token();
		if (token.kind != PA10TokenKind::Fixed)
			fail("expected fixed syntax token");
		PA10AstNode result = node(kind);
		result.has_token = true;
		result.token = token.fixed;
		result.token_spelling = intern(token.source);
		return result;
	}
	PA10AstNode literal_node()
	{
		const PA10Token token = consume_token();
		if (token.kind != PA10TokenKind::Literal)
			fail("expected literal");
		PA10AstNode result = node(PA10NodeKind::Literal);
		result.text = intern(token.source);
		result.has_literal = true;
		result.literal = token.literal;
		return result;
	}
	PA10AstNode name_node(PA10NodeKind kind, const PA10Name& name)
	{
		PA10AstNode result = node(kind);
		result.global_name = name.global;
		result.name_parts = name.parts;
		if (name.has_unqualified_id)
		{
			const PA10AstNode& id = name.unqualified_id;
			result.unqualified_id_kind = id.unqualified_id_kind;
			result.unqualified_id_token = id.unqualified_id_token;
			result.unqualified_id_token_spelling =
				id.unqualified_id_token_spelling;
			result.unqualified_id_spelling = id.unqualified_id_spelling;
			result.operator_function_kind = id.operator_function_kind;
			result.operator_token = id.operator_token;
			result.operator_presentation_begin =
				id.operator_presentation_begin;
			result.operator_presentation_count =
				id.operator_presentation_count;
			result.semantic_child_begin = id.semantic_child_begin;
			result.semantic_child_count = id.semantic_child_count;
		}
		if (name.has_decltype_root)
		{
			result.name_prefix_begin = ast_.name_prefix_nodes.size();
			result.name_prefix_count = 1;
			ast_.name_prefix_nodes.push_back(name.decltype_root);
		}
		return result;
	}
	PA10Token consume_identifier_token()
	{
		if (!identifier())
			fail("expected identifier");
		return consume_token();
	}
	PA10Name parse_name(bool template_expected = false,
		bool allow_standalone_decltype = false,
		bool allow_unqualified_id = false);
	PA10AstNode parse_id_expression_node(PA10NodeKind kind,
		bool template_expected = true);
	PA10AstNode parse_unqualified_id();
	PA10AstNode parse_destructor_name();
	PA10NameComponent parse_name_component(bool template_expected,
		bool template_disambiguator);
	void parse_template_arguments(PA10NameComponent& component);
	bool template_suffix_candidate();
	bool template_declaration_start();
	bool template_argument_starts_type();
	PA10TemplateArgument parse_template_argument();
	bool decltype_qualified_name_start();
	bool name_start(std::size_t offset = 0) const
	{
		return identifier(offset) || fixed(SimpleTokenType::OP_COLON2, offset);
	}
	bool id_expression_start(std::size_t offset = 0) const
	{
		return name_start(offset) ||
			fixed(SimpleTokenType::KW_TEMPLATE, offset) ||
			fixed(SimpleTokenType::KW_OPERATOR, offset) ||
			fixed(SimpleTokenType::OP_COMPL, offset);
	}
	PA10AstNode parse_declaration(bool in_class_member = false);
	PA10AstNode parse_namespace(bool inline_namespace);
	PA10AstNode parse_linkage_specification();
	PA10AstNode parse_using();
	PA10AstNode parse_static_assert();
	PA10AstNode parse_template_declaration(bool in_class_member = false);
	PA10AstNode parse_class_declaration(bool in_decl_specifier = false);
	void parse_class_name(PA10AstNode& result);
	PA10AstNode parse_enum_declaration(bool in_decl_specifier = false);
	PA10AstNode parse_class_specifier();
	PA10AstNode parse_enum_specifier();
	PA10AstNode parse_base_specifier();
	PA10AlignmentSpecifier parse_alignment_specifier();
	bool alignment_argument_starts_type() const;
	PA10AstNode parse_elaborated_declaration_or_function(
		bool in_class_member);
	PA10AstNode parse_decl_or_function(bool in_class_member = false);
	PA10AstNode parse_explicit_instantiation();
	PA10AstNode parse_simple_declaration(PA10AstNode spec,
		PA10AstNode declarator, bool is_typedef = false);
	PA10AstNode parse_bit_field_declaration(PA10AstNode spec,
		PA10AstNode first_declarator, bool has_first_declarator);
	PA10AstNode parse_special_member();
	PA10AstNode parse_member_specifiers();
	PA10AstNode parse_operator_name();
	PA10AstNode parse_ctor_initializer();
	PA10AstNode parse_paren_argument_list();
	PA10AstNode parse_decl_specifier_seq(bool type_context = false);
	PA10AstNode parse_decltype_specifier();
	PA10ParserSupport::PA10ElaboratedSpecifierClassification
		elaborated_specifier_classification();
	PA10AstNode parse_type_id(bool conversion_target = false,
		bool new_expression_context = false, bool parenthesized_new_type_id = false);
	PA10AstNode parse_abstract_declarator(bool stop_at_empty_parameter_clause = false,
		bool first_member_pointer_checked = false, bool new_expression_context = false);
	PA10AstNode parse_declarator(bool allow_abstract = false, bool force_parameter_suffix = false,
		bool prefer_parameter_clause_at_root = false, bool stop_at_parameter_attributes = false);
	PA10AstNode parse_ptr_operator();
	bool member_pointer_operator_start();
	PA10Name parse_member_pointer_qualifier();
	void parse_function_suffixes(PA10AstNode& result, bool new_expression_context = false);
	PA10AstNode parse_noexcept_function_qualifier();
	PA10AstNode parse_throw_function_qualifier();
	PA10AstNode parse_noexcept_specification();
	PA10AstNode parse_virt_specifier();
	PA10AstNode parse_parameter_clause();
	PA10AstNode parse_parameter_declaration();
	PA10AstNode parse_initializer();
	PA10AstNode parse_initializer_clause();
	PA10AstNode parse_braced_init_list();
	PA10AstNode parse_expression();
	PA10AstNode parse_assignment_expression();
	PA10AstNode parse_conditional_expression();
	PA10AstNode parse_binary_expression(int level);
	PA10AstNode parse_unary_expression();
	PA10AstNode parse_unary_expression_base();
	PA10AstNode parse_type_id_or_expression();
	bool looks_like_c_style_cast() const;
	PA10AstNode parse_c_style_cast();
	PA10AstNode parse_type_trait(SimpleTokenType keyword);
	PA10AstNode parse_keyword_cast();
	PA10AstNode parse_new_expression();
	PA10AstNode parse_delete_expression();
	PA10AstNode parse_postfix_expression();
	PA10AstNode parse_postfix_expression_seed();
	PA10AstNode parse_postfix_suffixes(PA10AstNode result);
	PA10AstNode parse_primary_expression();
	PA10AstNode parse_lambda_expression();
	PA10AstNode parse_lambda_declarator();
	PA10AstNode parse_argument_list();
	PA10AstNode parse_compound_statement();
	PA10AstNode parse_statement();
	PA10AstNode parse_condition();
	PA10AstNode parse_if_statement();
	PA10AstNode parse_switch_statement();
	PA10AstNode parse_iteration_statement();
	PA10AstNode parse_try_block();
	PA10AstNode parse_class_member();
	PA10AstNode parse_template_parameter_clause();
	PA10AstNode parse_template_parameter();
	void parse_attribute_specifiers(PA10AstNode& owner);
	void skip_attribute_specifiers();
	bool special_member_definition_start(bool in_class_member = false);
	bool parenthesized_group_is(std::size_t absolute, PA10ParserSupport::PA10ParenthesizedGroupKind kind) const
	{
		return absolute < parenthesized_group_kind_.size() && parenthesized_group_kind_[absolute] == kind;
	}
	bool parenthesized_declaration_start_at(std::size_t open) const
	{
		return PA10ParserSupport::parenthesized_declaration_start_at(
			tokens_, delimiter_close_index_, parenthesized_group_kind_, open);
	}
	bool looks_like_parameter_clause() const
	{
		return fixed(SimpleTokenType::OP_LPAREN) &&
			parenthesized_group_is(position_,
				PA10ParserSupport::PA10ParenthesizedGroupKind::ParameterClause);
	}
	bool declaration_start()
	{
		if (fixed(SimpleTokenType::KW_NAMESPACE) || fixed(SimpleTokenType::KW_USING) || fixed(SimpleTokenType::KW_STATIC_ASSERT) || fixed(SimpleTokenType::KW_TEMPLATE) ||
			fixed(SimpleTokenType::KW_CLASS) || fixed(SimpleTokenType::KW_STRUCT) || fixed(SimpleTokenType::KW_UNION) ||
			fixed(SimpleTokenType::KW_ENUM) || fixed(SimpleTokenType::KW_DECLTYPE) || fixed(SimpleTokenType::OP_SEMICOLON))
			return true;
		if (look().kind == PA10TokenKind::Fixed &&
			PA10ParserSupport::is_decl_specifier(look().fixed))
			return true;
		if (fixed(SimpleTokenType::OP_COLON2))
		{
			std::size_t work = 0;
			const bool qualified = PA10ParserSupport::qualified_declaration_start(tokens_, delimiter_close_index_, parenthesized_group_kind_, position_, &work);
			while (work-- != 0) charge();
			if (qualified) return true;
		}
		if (identifier())
		{
			if (PA10ParserSupport::token_identifier_at(tokens_, position_, 1))
			{
				charge();
				return true;
			}
			std::size_t cv_type_end = position_ + 1;
			while (token_fixed_at(cv_type_end, 0, SimpleTokenType::KW_CONST) ||
				token_fixed_at(cv_type_end, 0, SimpleTokenType::KW_VOLATILE))
			{
				charge();
				++cv_type_end;
			}
			if (PA10ParserSupport::token_identifier_at(tokens_, cv_type_end) &&
				PA10ParserSupport::declaration_follow_is_valid(tokens_,
					cv_type_end))
			{
				charge();
				return true;
			}
			std::size_t cv_pointer = cv_type_end;
			while (token_fixed_at(cv_pointer, 0, SimpleTokenType::OP_STAR) ||
				token_fixed_at(cv_pointer, 0, SimpleTokenType::OP_AMP) ||
				token_fixed_at(cv_pointer, 0, SimpleTokenType::OP_LAND))
			{
				charge();
				++cv_pointer;
				while (token_fixed_at(cv_pointer, 0, SimpleTokenType::KW_CONST) ||
					token_fixed_at(cv_pointer, 0, SimpleTokenType::KW_VOLATILE))
				{
					charge();
					++cv_pointer;
				}
			}
			if (cv_pointer != cv_type_end &&
				PA10ParserSupport::token_identifier_at(tokens_, cv_pointer) &&
				PA10ParserSupport::declaration_follow_is_valid(tokens_,
					cv_pointer))
			{
				charge();
				return true;
			}
			std::size_t pointer = position_ + 1;
			bool saw_pointer = false;
			while (token_fixed_at(pointer, 0, SimpleTokenType::OP_STAR) ||
				token_fixed_at(pointer, 0, SimpleTokenType::OP_AMP) ||
				token_fixed_at(pointer, 0, SimpleTokenType::OP_LAND))
			{
				charge();
				saw_pointer = true;
				++pointer;
				while (token_fixed_at(pointer, 0, SimpleTokenType::KW_CONST) ||
					token_fixed_at(pointer, 0, SimpleTokenType::KW_VOLATILE))
				{
					charge();
					++pointer;
				}
			}
			if (saw_pointer &&
				PA10ParserSupport::token_identifier_at(tokens_, pointer))
			{
				charge();
				if (PA10ParserSupport::declaration_follow_is_valid(tokens_, pointer))
					return true;
			}
			if (parenthesized_declaration_start_at(position_ + 1))
			{
				charge();
				return true;
			}
			if (token_fixed_at(position_, 1, SimpleTokenType::OP_COLON2))
			{
				std::size_t type_end = position_;
				while (PA10ParserSupport::token_identifier_at(tokens_, type_end) &&
					token_fixed_at(type_end, 1,
						SimpleTokenType::OP_COLON2) &&
					PA10ParserSupport::token_identifier_at(tokens_, type_end, 2))
				{
					charge();
					std::size_t next = 0;
					if (!PA10ParserSupport::advance_token_position(
						tokens_, type_end, 2, &next))
						return false;
					type_end = next;
				}
				std::size_t after_type = 0;
				if (!PA10ParserSupport::advance_token_position(
					tokens_, type_end, 1, &after_type))
					return false;
				charge();
				if (PA10ParserSupport::token_identifier_at(tokens_, after_type) ||
					token_fixed_at(after_type, 0,
						SimpleTokenType::OP_STAR) ||
					token_fixed_at(after_type, 0,
						SimpleTokenType::OP_AMP) ||
					token_fixed_at(after_type, 0,
						SimpleTokenType::OP_LAND))
					return true;
				if (token_fixed_at(after_type, 0,
					SimpleTokenType::OP_LPAREN))
				{
					if (parenthesized_declaration_start_at(after_type))
					{
						charge();
						return true;
					}
					std::size_t member = 0;
					if (!PA10ParserSupport::advance_token_position(
						tokens_, after_type, 1, &member))
						return false;
					while (PA10ParserSupport::token_identifier_at(tokens_, member) &&
						token_fixed_at(member, 1,
							SimpleTokenType::OP_COLON2) &&
						PA10ParserSupport::token_identifier_at(tokens_, member, 2))
					{
						charge();
						std::size_t next = 0;
						if (!PA10ParserSupport::advance_token_position(
							tokens_, member, 2, &next))
							return false;
						member = next;
					}
					if (token_fixed_at(member, 1,
						SimpleTokenType::OP_COLON2) &&
						token_fixed_at(member, 2,
							SimpleTokenType::OP_STAR))
						return true;
				}
			}
		}
		return template_declaration_start();
	}
};
bool PA10Parser::decltype_qualified_name_start()
{
	std::size_t charged_work = 0;
	const bool result = PA10ParserSupport::decltype_qualified_name_start(tokens_, delimiter_close_index_, position_, &charged_work);
	for (std::size_t i = 0; i < charged_work; ++i)
		charge();
	return result;
}
bool PA10Parser::template_suffix_candidate()
{
	if (!fixed(SimpleTokenType::OP_LT))
		return false;
	std::size_t close = 0;
	if (!PA10ParserSupport::find_template_close(
		tokens_, template_close_index_, position_, &close))
		return false;
	std::size_t charged_work = 0;
	const bool follow_is_valid =
		PA10ParserSupport::template_follow_is_valid(
			tokens_, rshift_piece1_nested_close_, close, &charged_work);
	for (std::size_t i = 0; i < charged_work; ++i)
		charge();
	if (!follow_is_valid)
		return false;
	// An indexed top-level comma followed by an assignment belongs to the
	// enclosing dependent non-type argument rather than a nested template-id.
	// Keep this ownership test constant-time; close and rshift ownership remain
	// supplied by the existing indexes.
	if (position_ < template_top_level_comma_.size())
	{
		charge();
		if (template_top_level_comma_[position_] && close + 1 < tokens_.size() &&
			tokens_[close + 1].kind == PA10TokenKind::Fixed &&
			tokens_[close + 1].fixed == SimpleTokenType::OP_ASS)
			return false;
	}
	return !template_top_level_or_[position_] ||
		close + 1 >= tokens_.size() ||
		tokens_[close + 1].kind != PA10TokenKind::Fixed ||
		tokens_[close + 1].fixed != SimpleTokenType::OP_LPAREN;
}
PA10NameComponent PA10Parser::parse_name_component(bool template_expected,
	bool template_disambiguator)
{
	if (!identifier())
		fail("expected name component");
	PA10NameComponent component;
	component.spelling = consume_identifier_token().spelling;
	component.template_disambiguator = template_disambiguator;
	if (fixed(SimpleTokenType::OP_LT))
	{
		if (!template_expected && !template_disambiguator &&
			!template_suffix_candidate())
			return component;
		parse_template_arguments(component);
	}
	else if (template_disambiguator)
		fail("template disambiguator needs a template-id");
	return component;
}
void PA10Parser::parse_template_arguments(PA10NameComponent& component)
{
	consume_fixed(SimpleTokenType::OP_LT);
	begin_angle();
	component.has_template_id = true;
	std::vector<PA10TemplateArgument> direct_arguments;
	if (!at_angle_close())
	{
		while (true)
		{
			direct_arguments.push_back(parse_template_argument());
			if (!fixed(SimpleTokenType::OP_COMMA))
				break;
			consume_fixed(SimpleTokenType::OP_COMMA);
			if (at_angle_close())
				fail("trailing template argument comma");
		}
	}
	close_angle();
	component.template_argument_begin = ast_.template_arguments.size();
	component.template_argument_count = direct_arguments.size();
	for (std::size_t i = 0; i < direct_arguments.size(); ++i)
		ast_.template_arguments.push_back(std::move(direct_arguments[i]));
}
bool PA10Parser::template_argument_starts_type()
{
	if (look().kind == PA10TokenKind::Fixed)
	{
		if (PA10ParserSupport::is_type_keyword(look().fixed) ||
			PA10ParserSupport::is_cv(look().fixed) ||
			look().fixed == SimpleTokenType::KW_TYPENAME ||
			look().fixed == SimpleTokenType::KW_DECLTYPE)
			return true;
	}
	if (!identifier())
		return false;
	charge();
	return fixed(SimpleTokenType::OP_STAR, 1) ||
		fixed(SimpleTokenType::OP_AMP, 1) ||
		fixed(SimpleTokenType::OP_LAND, 1) ||
		fixed(SimpleTokenType::OP_LSQUARE, 1);
}
PA10TemplateArgument PA10Parser::parse_template_argument()
{
	if (template_argument_starts_type())
	{
		const PA10TemplateArgumentKind kind = identifier() ?
			PA10TemplateArgumentKind::Unresolved :
			PA10TemplateArgumentKind::TypeId;
		return PA10TemplateArgument(kind, parse_type_id());
	}
	if (identifier() || name_start())
		return PA10TemplateArgument(PA10TemplateArgumentKind::Unresolved,
			parse_assignment_expression());
	return PA10TemplateArgument(PA10TemplateArgumentKind::Expression,
		parse_assignment_expression());
}
PA10Name PA10Parser::parse_name(bool template_expected,
	bool allow_standalone_decltype, bool allow_unqualified_id)
{
	PA10Name result;
	if (fixed(SimpleTokenType::KW_DECLTYPE))
	{
		result.has_decltype_root = true;
		result.decltype_root = parse_decltype_specifier();
		if (fixed(SimpleTokenType::OP_COLON2))
			consume_fixed(SimpleTokenType::OP_COLON2);
		else if (allow_standalone_decltype)
			return result;
		else
			fail("decltype root requires qualified-name separator");
	}
	else if (fixed(SimpleTokenType::OP_COLON2))
	{
		result.global = true;
		consume_fixed(SimpleTokenType::OP_COLON2);
	}
	if (allow_unqualified_id &&
		(fixed(SimpleTokenType::KW_OPERATOR) ||
		 fixed(SimpleTokenType::OP_COMPL)))
	{
		result.has_unqualified_id = true;
		result.unqualified_id = parse_unqualified_id();
		return result;
	}
	while (true)
	{
		bool template_disambiguator = false;
		if (fixed(SimpleTokenType::KW_TEMPLATE))
		{
			consume_fixed(SimpleTokenType::KW_TEMPLATE);
			template_disambiguator = true;
		}
		result.parts.push_back(parse_name_component(template_expected,
			template_disambiguator));
		if (!fixed(SimpleTokenType::OP_COLON2))
			break;
		consume_fixed(SimpleTokenType::OP_COLON2);
		if (allow_unqualified_id &&
			(fixed(SimpleTokenType::KW_OPERATOR) ||
			 fixed(SimpleTokenType::OP_COMPL)))
		{
			result.has_unqualified_id = true;
			result.unqualified_id = parse_unqualified_id();
			break;
		}
		if (!identifier() && !fixed(SimpleTokenType::KW_TEMPLATE))
			fail("missing qualified-name component");
	}
	return result;
}
PA10AstNode PA10Parser::parse_id_expression_node(PA10NodeKind kind,
	bool template_expected)
{
	return name_node(kind, parse_name(template_expected, false, true));
}
PA10AstNode PA10Parser::parse_destructor_name()
{
	const PA10Token tilde = consume_token();
	if (tilde.kind != PA10TokenKind::Fixed ||
		tilde.fixed != SimpleTokenType::OP_COMPL)
		fail("expected destructor token");
	PA10AstNode result = node(PA10NodeKind::Identifier);
	result.unqualified_id_kind = PA10UnqualifiedIdKind::Destructor;
	result.unqualified_id_token = tilde.fixed;
	result.unqualified_id_token_spelling = intern(tilde.source);
	if (identifier())
	{
		const PA10Token name = consume_identifier_token();
		if (fixed(SimpleTokenType::OP_LT))
		{
			PA10NameComponent component;
			component.spelling = name.spelling;
			parse_template_arguments(component);
			PA10AstNode template_name = node(PA10NodeKind::Identifier);
			template_name.name_parts.push_back(component);
			append_semantic_child(result, std::move(template_name));
		}
		else
			result.unqualified_id_spelling = name.spelling;
		return result;
	}
	if (fixed(SimpleTokenType::KW_DECLTYPE))
	{
		append_semantic_child(result, parse_decltype_specifier());
		return result;
	}
	fail("expected destructor name");
	return result;
}
PA10AstNode PA10Parser::parse_unqualified_id()
{
	if (fixed(SimpleTokenType::KW_OPERATOR))
		return parse_operator_name();
	if (fixed(SimpleTokenType::OP_COMPL))
		return parse_destructor_name();
	fail("expected unqualified-id");
	return node(PA10NodeKind::Identifier);
}
bool PA10Parser::special_member_definition_start(bool in_class_member)
{
	std::size_t charged_work = 0;
	const bool result = PA10ParserSupport::special_member_definition_start(
		tokens_, template_close_index_, rshift_piece1_nested_close_, delimiter_close_index_,
		position_, in_class_member, &charged_work);
	for (std::size_t i = 0; i < charged_work; ++i)
		charge();
	return result;
}
PA10ParserSupport::PA10ElaboratedSpecifierClassification
PA10Parser::elaborated_specifier_classification()
{
	PA10ParserSupport::PA10ElaboratedSpecifierClassification result =
		PA10ParserSupport::classify_elaborated_specifier(
			tokens_, template_close_index_, rshift_piece1_nested_close_,
			delimiter_close_index_, position_);
	for (std::size_t i = 0; i < result.charged_work; ++i)
		charge();
	return result;
}
bool PA10Parser::template_declaration_start()
{
	if (!identifier())
		return false;
	std::size_t offset = 0;
	while (offset < tokens_.size() - position_ &&
		tokens_[position_ + offset].kind == PA10TokenKind::Identifier)
	{
		charge();
		if (offset + 1 < tokens_.size() - position_ &&
			tokens_[position_ + offset + 1].kind == PA10TokenKind::Fixed &&
			tokens_[position_ + offset + 1].fixed == SimpleTokenType::OP_LT)
		{
			std::size_t close = 0;
			if (!PA10ParserSupport::find_template_close(
				tokens_, template_close_index_, position_ + offset + 1,
				&close))
				return false;
			std::size_t next = close + 1;
			if (tokens_[close].kind == PA10TokenKind::RShiftPiece1 &&
				next < tokens_.size() &&
				tokens_[next].kind == PA10TokenKind::RShiftPiece2 &&
				close < rshift_piece1_nested_close_.size() && rshift_piece1_nested_close_[close])
			{
				charge();
				++next;
			}
			if (next >= tokens_.size())
				return false;
			charge();
			return next < tokens_.size() &&
				tokens_[next].kind == PA10TokenKind::Identifier;
		}
		if (offset + 1 >= tokens_.size() - position_ ||
			tokens_[position_ + offset + 1].kind != PA10TokenKind::Fixed ||
			tokens_[position_ + offset + 1].fixed != SimpleTokenType::OP_COLON2)
			break;
		offset += 2;
	}
	return false;
}
PA10AstNode PA10Parser::parse_declaration(bool in_class_member)
{
	RecursionGuard recursion(*this);
	if (fixed(SimpleTokenType::OP_SEMICOLON))
	{
		consume_fixed(SimpleTokenType::OP_SEMICOLON);
		return node(PA10NodeKind::EmptyDeclaration);
	}
	if (fixed(SimpleTokenType::KW_INLINE))
	{
		if (fixed(SimpleTokenType::KW_NAMESPACE, 1))
		{
			consume_fixed(SimpleTokenType::KW_INLINE);
			return parse_namespace(true);
		}
	}
	if (fixed(SimpleTokenType::KW_NAMESPACE))
		return parse_namespace(false);
	if (special_member_definition_start(in_class_member))
		return parse_special_member();
	if (fixed(SimpleTokenType::KW_EXTERN) && literal(1))
		return parse_linkage_specification();
	if (fixed(SimpleTokenType::KW_EXTERN) &&
		fixed(SimpleTokenType::KW_TEMPLATE, 1))
		return parse_explicit_instantiation();
	if (fixed(SimpleTokenType::KW_USING))
		return parse_using();
	if (fixed(SimpleTokenType::KW_STATIC_ASSERT))
		return parse_static_assert();
	if (fixed(SimpleTokenType::KW_TEMPLATE))
		return parse_template_declaration(in_class_member);
	return parse_elaborated_declaration_or_function(in_class_member);
}
PA10AstNode PA10Parser::parse_elaborated_declaration_or_function(
	bool in_class_member)
{
	const PA10ParserSupport::PA10ElaboratedSpecifierClassification classification =
		elaborated_specifier_classification();
	switch (classification.context)
	{
	case PA10ParserSupport::PA10ElaboratedSpecifierContext::StandaloneForward:
	case PA10ParserSupport::PA10ElaboratedSpecifierContext::StandaloneDefinition:
		return fixed(SimpleTokenType::KW_ENUM) ?
			parse_enum_declaration() : parse_class_declaration();
	default:
		return parse_decl_or_function(in_class_member);
	}
}
PA10AstNode PA10Parser::parse_namespace(bool inline_namespace)
{
	consume_fixed(SimpleTokenType::KW_NAMESPACE);
	PPSpellingId producer_name = 0;
	bool anonymous = true;
	if (identifier())
	{
		const PA10Token token = consume_identifier_token();
		producer_name = token.spelling;
		anonymous = false;
	}
	if (fixed(SimpleTokenType::OP_ASS))
	{
		if (anonymous)
			fail("unnamed namespace alias");
		consume_fixed(SimpleTokenType::OP_ASS);
		const PA10Name target = parse_name(true);
		consume_fixed(SimpleTokenType::OP_SEMICOLON);
		PA10AstNode result = node(PA10NodeKind::NamespaceAliasDefinition);
		result.producer_spelling = producer_name;
		result.children.push_back(name_node(PA10NodeKind::Target, target));
		return result;
	}
	consume_fixed(SimpleTokenType::OP_LBRACE);
	PA10AstNode result = node(PA10NodeKind::NamespaceDefinition);
	if (anonymous)
		result.text = intern("<unnamed>");
	else
	{
		result.producer_spelling = producer_name;
	}
	if (inline_namespace)
		result.children.push_back(node(PA10NodeKind::InlineMarker));
	enter();
	while (!fixed(SimpleTokenType::OP_RBRACE))
	{
		if (at_end())
			fail("unterminated namespace");
		result.children.push_back(parse_declaration());
	}
	consume_fixed(SimpleTokenType::OP_RBRACE);
	leave();
	return result;
}
PA10AstNode PA10Parser::parse_linkage_specification()
{
	consume_fixed(SimpleTokenType::KW_EXTERN);
	if (!literal())
		fail("linkage specification needs a string literal");
	const PA10Token linkage = consume_token();
	PA10AstNode result = node(PA10NodeKind::LinkageSpecification);
	result.has_literal = true;
	result.literal = linkage.literal;
	if (!fixed(SimpleTokenType::OP_LBRACE))
	{
		result.children.push_back(parse_declaration());
		return result;
	}
	consume_fixed(SimpleTokenType::OP_LBRACE);
	enter();
	while (!fixed(SimpleTokenType::OP_RBRACE))
	{
		if (at_end())
			fail("unterminated linkage specification");
		result.children.push_back(parse_declaration());
	}
	consume_fixed(SimpleTokenType::OP_RBRACE);
	leave();
	return result;
}
PA10AstNode PA10Parser::parse_using()
{
	consume_fixed(SimpleTokenType::KW_USING);
	if (fixed(SimpleTokenType::KW_NAMESPACE))
	{
		consume_fixed(SimpleTokenType::KW_NAMESPACE);
		PA10AstNode result = node(PA10NodeKind::UsingDirective);
		result.children.push_back(name_node(PA10NodeKind::Target,
			parse_name(true)));
		consume_fixed(SimpleTokenType::OP_SEMICOLON);
		return result;
	}
	const PA10Name name = parse_name(true);
	if (fixed(SimpleTokenType::OP_ASS))
	{
		if (name.global || name.parts.size() != 1)
			fail("qualified alias name");
		consume_fixed(SimpleTokenType::OP_ASS);
		PA10AstNode result = node(PA10NodeKind::AliasDeclaration);
		result.producer_spelling = name.parts.back().spelling;
		name_scopes_.declare_type(result.producer_spelling);
		result.children.push_back(parse_type_id());
		consume_fixed(SimpleTokenType::OP_SEMICOLON);
		return result;
	}
	PA10AstNode result = node(PA10NodeKind::UsingDeclaration);
	result.children.push_back(name_node(PA10NodeKind::Target, name));
	consume_fixed(SimpleTokenType::OP_SEMICOLON);
	return result;
}
PA10AstNode PA10Parser::parse_static_assert()
{
	consume_fixed(SimpleTokenType::KW_STATIC_ASSERT);
	consume_fixed(SimpleTokenType::OP_LPAREN);
	PA10AstNode result = node(PA10NodeKind::StaticAssertDeclaration);
	result.children.push_back(parse_assignment_expression());
	if (fixed(SimpleTokenType::OP_COMMA))
	{
		consume_fixed(SimpleTokenType::OP_COMMA);
		if (!literal())
			fail("static_assert message must be a literal");
		const PA10Token token = consume_token();
		PA10AstNode message = node(PA10NodeKind::Message);
		message.text = intern(token.source);
		message.has_literal = true;
		message.literal = token.literal;
		result.children.push_back(std::move(message));
	}
	consume_fixed(SimpleTokenType::OP_RPAREN);
	consume_fixed(SimpleTokenType::OP_SEMICOLON);
	return result;
}
PA10AstNode PA10Parser::parse_decl_or_function(bool in_class_member)
{
	PA10AstNode spec = parse_decl_specifier_seq();
	const bool is_typedef = PA10ParserSupport::is_typedef_specifier(spec);
	if (fixed(SimpleTokenType::OP_SEMICOLON))
	{
		consume_fixed(SimpleTokenType::OP_SEMICOLON);
		PA10AstNode result = node(PA10NodeKind::SimpleDeclaration);
		result.children.push_back(std::move(spec));
		return result;
	}
	if (in_class_member && fixed(SimpleTokenType::OP_COLON))
		return parse_bit_field_declaration(std::move(spec),
			PA10AstNode(), false);
	name_scopes_.begin_parameter_collection();
	PA10AstNode declarator = parse_declarator(false);
	const bool is_function = PA10DeclaratorShape::is_function(declarator);
	name_scopes_.finish_parameter_collection(
		is_function && fixed(SimpleTokenType::OP_LBRACE));
	if (in_class_member && fixed(SimpleTokenType::OP_COLON))
		return parse_bit_field_declaration(std::move(spec),
			std::move(declarator), true);
	if (fixed(SimpleTokenType::OP_LBRACE) &&
		is_function)
	{
		PA10ParserSupport::record_declarator_name(name_scopes_, declarator,
			false);
		PA10AstNode result = node(PA10NodeKind::FunctionDefinition);
		result.children.push_back(std::move(spec));
		result.children.push_back(std::move(declarator));
		result.children.push_back(parse_compound_statement());
		return result;
	}
	return parse_simple_declaration(spec, declarator, is_typedef);
}
PA10AstNode PA10Parser::parse_explicit_instantiation()
{
	consume_fixed(SimpleTokenType::KW_EXTERN);
	consume_fixed(SimpleTokenType::KW_TEMPLATE);
	PA10AstNode result = node(PA10NodeKind::ExplicitInstantiationDeclaration);
	if (fixed(SimpleTokenType::KW_CLASS) ||
		fixed(SimpleTokenType::KW_STRUCT) ||
		fixed(SimpleTokenType::KW_UNION))
		result.children.push_back(parse_class_declaration());
	else
		result.children.push_back(parse_decl_or_function());
	return result;
}
PA10AstNode PA10Parser::parse_bit_field_declaration(PA10AstNode spec,
	PA10AstNode first_declarator, bool has_first_declarator)
{
	PA10AstNode result = node(PA10NodeKind::BitFieldDeclaration);
	result.children.push_back(std::move(spec));
	bool has_declarator = has_first_declarator;
	while (true)
	{
		PA10AstNode field = node(PA10NodeKind::BitFieldDeclarator);
		if (has_declarator)
			field.children.push_back(std::move(first_declarator));
		consume_fixed(SimpleTokenType::OP_COLON);
		field.children.push_back(parse_assignment_expression());
		result.children.push_back(std::move(field));
		if (!fixed(SimpleTokenType::OP_COMMA))
			break;
		consume_fixed(SimpleTokenType::OP_COMMA);
		has_declarator = !fixed(SimpleTokenType::OP_COLON);
		if (has_declarator)
			first_declarator = parse_declarator(false);
	}
	consume_fixed(SimpleTokenType::OP_SEMICOLON);
	return result;
}
PA10AstNode PA10Parser::parse_simple_declaration(PA10AstNode spec,
	PA10AstNode declarator, bool is_typedef)
{
	PA10AstNode result = node(PA10NodeKind::SimpleDeclaration);
	result.children.push_back(std::move(spec));
	PA10AstNode list = node(PA10NodeKind::InitDeclaratorList);
	PA10AstNode init = node(PA10NodeKind::InitDeclarator);
	const bool is_function = PA10DeclaratorShape::is_function(declarator);
	init.children.push_back(std::move(declarator));
	PA10ParserSupport::record_declarator_name(name_scopes_,
		init.children.front(), is_typedef);
	if (fixed(SimpleTokenType::OP_ASS) || fixed(SimpleTokenType::OP_LBRACE) ||
		(fixed(SimpleTokenType::OP_LPAREN) && !is_function))
		init.children.push_back(parse_initializer());
	list.children.push_back(std::move(init));
	while (fixed(SimpleTokenType::OP_COMMA))
	{
		consume_fixed(SimpleTokenType::OP_COMMA);
		PA10AstNode next = node(PA10NodeKind::InitDeclarator);
		next.children.push_back(parse_declarator(false));
		PA10ParserSupport::record_declarator_name(name_scopes_,
			next.children.front(), is_typedef);
		if (fixed(SimpleTokenType::OP_ASS) || fixed(SimpleTokenType::OP_LBRACE) ||
			(fixed(SimpleTokenType::OP_LPAREN) &&
				!PA10DeclaratorShape::is_function(next.children.back())))
			next.children.push_back(parse_initializer());
		list.children.push_back(std::move(next));
	}
	result.children.push_back(std::move(list));
	consume_fixed(SimpleTokenType::OP_SEMICOLON);
	return result;
}
PA10AstNode PA10Parser::parse_decl_specifier_seq(bool type_context)
{
	PA10AstNode result = node(type_context ? PA10NodeKind::TypeSpecifierSeq :
		PA10NodeKind::DeclSpecifierSeq);
	parse_attribute_specifiers(result);
	bool consumed = false;
	bool saw_type = false;
	while (true)
	{
		if (fixed(SimpleTokenType::KW_TYPENAME) && !saw_type)
		{
			const PA10Token keyword = consume_token();
			const PA10Name name = parse_name(true);
			PA10AstNode child = name_node(type_context ?
				PA10NodeKind::TypeName : PA10NodeKind::DeclSpecifier, name);
			child.has_token = true;
			child.token = keyword.fixed;
			child.token_spelling = intern(keyword.source);
			result.children.push_back(std::move(child));
			consumed = true;
			saw_type = true;
			continue;
		}
		if (fixed(SimpleTokenType::KW_DECLTYPE) && !saw_type)
		{
			PA10AstNode decltype_child;
			if (decltype_qualified_name_start())
			{
				const PA10Name name = parse_name(true);
				decltype_child = name_node(type_context ? PA10NodeKind::TypeName : PA10NodeKind::DeclSpecifier, name);
			}
			else
			{
				decltype_child = parse_decltype_specifier();
				if (!type_context)
					decltype_child.kind = PA10NodeKind::DeclSpecifier;
			}
			result.children.push_back(std::move(decltype_child));
			consumed = true;
			saw_type = true;
			continue;
		}
		if (name_start() && !saw_type)
		{
			const PA10Name name = parse_name(true);
			PA10AstNode child = name_node(type_context ? PA10NodeKind::TypeName :
				PA10NodeKind::DeclSpecifier, name);
			if (!type_context && !name.global && name.parts.size() == 1 &&
				!name.parts.front().has_template_id)
				child.identifier_declspecifier = true;
			result.children.push_back(std::move(child));
			consumed = true;
			saw_type = true;
			continue;
		}
		if (fixed(SimpleTokenType::KW_CLASS) ||
			fixed(SimpleTokenType::KW_STRUCT) || fixed(SimpleTokenType::KW_UNION))
		{
			if (saw_type)
				break;
			result.children.push_back(parse_class_declaration(true));
			consumed = true;
			saw_type = true;
			continue;
		}
		if (fixed(SimpleTokenType::KW_ENUM))
		{
			if (saw_type)
				break;
			result.children.push_back(parse_enum_declaration(true));
			consumed = true;
			saw_type = true;
			continue;
		}
		if (look().kind != PA10TokenKind::Fixed ||
			!PA10ParserSupport::is_decl_specifier(look().fixed))
			break;
		const SimpleTokenType spec_type = look().fixed;
		const PA10NodeKind kind = type_context ?
			(PA10ParserSupport::is_cv(spec_type) ? PA10NodeKind::CvQualifier :
			 PA10NodeKind::TypeSpecifier) : PA10NodeKind::DeclSpecifier;
		result.children.push_back(fixed_node(kind));
		consumed = true;
		if (PA10ParserSupport::is_type_keyword(spec_type))
			saw_type = true;
	}
	if (!consumed)
		fail("missing declaration specifier");
	return result;
}
PA10AstNode PA10Parser::parse_decltype_specifier()
{
	PA10AstNode result = node(PA10NodeKind::DecltypeSpecifier);
	const PA10Token keyword = consume_token();
	if (keyword.kind != PA10TokenKind::Fixed ||
		keyword.fixed != SimpleTokenType::KW_DECLTYPE)
		fail("expected decltype");
	result.has_token = true;
	result.token = keyword.fixed;
	result.token_spelling = intern(keyword.source);
	consume_fixed(SimpleTokenType::OP_LPAREN);
	begin_non_angle();
	result.children.push_back(parse_expression());
	consume_fixed(SimpleTokenType::OP_RPAREN);
	end_non_angle();
	return result;
}
bool PA10Parser::member_pointer_operator_start()
{
	std::size_t charged_work = 0;
	const bool result = PA10ParserSupport::member_pointer_operator_start(
		tokens_, template_close_index_, rshift_piece1_nested_close_,
		position_, &charged_work);
	for (std::size_t i = 0; i < charged_work; ++i)
		charge();
	return result;
}
PA10Name PA10Parser::parse_member_pointer_qualifier()
{
	PA10Name result;
	if (fixed(SimpleTokenType::OP_COLON2))
	{
		result.global = true;
		consume_fixed(SimpleTokenType::OP_COLON2);
	}
	while (true)
	{
		bool template_disambiguator = false;
		if (fixed(SimpleTokenType::KW_TEMPLATE))
		{
			consume_fixed(SimpleTokenType::KW_TEMPLATE);
			template_disambiguator = true;
		}
		result.parts.push_back(parse_name_component(true,
			template_disambiguator));
		consume_fixed(SimpleTokenType::OP_COLON2);
		if (fixed(SimpleTokenType::OP_STAR))
		{
			consume_fixed(SimpleTokenType::OP_STAR);
			break;
		}
		if (!identifier() && !fixed(SimpleTokenType::KW_TEMPLATE))
			fail("missing member-pointer name component");
	}
	return result;
}
PA10AstNode PA10Parser::parse_ptr_operator()
{
	// The declarator loop has already established this qualified form with
	// member_pointer_operator_start().  Consume it directly so a long
	// qualifier is not rescanned here.
	if (identifier() || fixed(SimpleTokenType::OP_COLON2) ||
		fixed(SimpleTokenType::KW_TEMPLATE))
	{
		const PA10Name qualifier = parse_member_pointer_qualifier();
		PA10AstNode result = name_node(PA10NodeKind::PtrOperator, qualifier);
		result.has_token = true;
		result.token = SimpleTokenType::OP_STAR;
		result.token_spelling = intern("*");
		return result;
	}
	if (fixed(SimpleTokenType::OP_STAR) ||
		fixed(SimpleTokenType::OP_AMP) || fixed(SimpleTokenType::OP_LAND))
		return fixed_node(PA10NodeKind::PtrOperator);
	fail("expected ptr-operator");
	return node(PA10NodeKind::PtrOperator);
}
PA10AstNode PA10Parser::parse_type_id(bool conversion_target, bool new_expression_context, bool parenthesized_new_type_id)
{
	PA10AstNode result = node(PA10NodeKind::TypeId);
	result.children.push_back(parse_decl_specifier_seq(true));
	bool member_abstract_start = false;
	const bool parenthesized_abstract_start =
		fixed(SimpleTokenType::OP_LPAREN) &&
		(new_expression_context ?
			PA10ParserSupport::new_parenthesized_abstract_declarator_start(
				tokens_, delimiter_close_index_, parenthesized_group_kind_,
				position_, parenthesized_new_type_id) :
			(fixed(SimpleTokenType::OP_STAR, 1) ||
			 fixed(SimpleTokenType::OP_AMP, 1) ||
			 fixed(SimpleTokenType::OP_LAND, 1) ||
			 fixed(SimpleTokenType::OP_LPAREN, 1) ||
			 (looks_like_parameter_clause() &&
				(!conversion_target ||
					!fixed(SimpleTokenType::OP_RPAREN, 1)))));
	const bool abstract_start =
		fixed(SimpleTokenType::OP_STAR) ||
		fixed(SimpleTokenType::OP_AMP) ||
		fixed(SimpleTokenType::OP_LAND) ||
		fixed(SimpleTokenType::OP_LSQUARE) ||
		(member_abstract_start = member_pointer_operator_start()) ||
		parenthesized_abstract_start;
	if (abstract_start)
		result.children.push_back(parse_abstract_declarator(
			conversion_target, member_abstract_start, new_expression_context));
	return result;
}
PA10AstNode PA10Parser::parse_noexcept_function_qualifier()
{
	PA10AstNode result = fixed_node(PA10NodeKind::FunctionQualifier);
	if (fixed(SimpleTokenType::OP_LPAREN))
	{
		consume_fixed(SimpleTokenType::OP_LPAREN);
		begin_non_angle();
		if (!fixed(SimpleTokenType::OP_RPAREN))
			result.children.push_back(parse_expression());
		consume_fixed(SimpleTokenType::OP_RPAREN);
		end_non_angle();
	}
	return result;
}
PA10AstNode PA10Parser::parse_throw_function_qualifier()
{
	PA10AstNode result = fixed_node(PA10NodeKind::FunctionQualifier);
	consume_fixed(SimpleTokenType::OP_LPAREN);
	begin_non_angle();
	if (!fixed(SimpleTokenType::OP_RPAREN))
	{
		append_semantic_child(result, parse_type_id());
		while (fixed(SimpleTokenType::OP_COMMA))
		{
			consume_fixed(SimpleTokenType::OP_COMMA);
			append_semantic_child(result, parse_type_id());
		}
	}
	consume_fixed(SimpleTokenType::OP_RPAREN);
	end_non_angle();
	return result;
}
PA10AstNode PA10Parser::parse_noexcept_specification()
{
	consume_fixed(SimpleTokenType::KW_NOEXCEPT);
	PA10AstNode result = node(PA10NodeKind::NoexceptSpecification);
	if (fixed(SimpleTokenType::OP_LPAREN))
	{
		consume_fixed(SimpleTokenType::OP_LPAREN);
		begin_non_angle();
		if (!fixed(SimpleTokenType::OP_RPAREN))
			result.children.push_back(parse_expression());
		consume_fixed(SimpleTokenType::OP_RPAREN);
		end_non_angle();
	}
	return result;
}
PA10AstNode PA10Parser::parse_virt_specifier()
{
	const PA10Token token = consume_identifier_token();
	PA10AstNode result = node(PA10NodeKind::VirtSpecifier);
	result.producer_spelling = token.spelling;
	return result;
}
void PA10Parser::parse_function_suffixes(PA10AstNode& result, bool new_expression_context)
{
	while (true)
	{
		if (PA10ParserSupport::is_cv(look().fixed) &&
			look().kind == PA10TokenKind::Fixed)
		{
			result.children.push_back(fixed_node(PA10NodeKind::CvQualifier));
			continue;
		}
		if (fixed(SimpleTokenType::OP_AMP) ||
			fixed(SimpleTokenType::OP_LAND))
		{
			result.children.push_back(fixed_node(PA10NodeKind::RefQualifier));
			continue;
		}
		if (fixed(SimpleTokenType::KW_NOEXCEPT))
		{
			result.children.push_back(parse_noexcept_function_qualifier());
			continue;
		}
		if (fixed(SimpleTokenType::KW_THROW))
		{
			result.children.push_back(parse_throw_function_qualifier());
			continue;
		}
		if (PA10ParserSupport::virt_specifier_start(tokens_, position_))
		{
			result.children.push_back(parse_virt_specifier());
			continue;
		}
		if (fixed(SimpleTokenType::OP_ARROW))
		{
			consume_fixed(SimpleTokenType::OP_ARROW);
			PA10AstNode trailing = node(PA10NodeKind::TrailingReturnType);
			PA10AstNode type = parse_type_id(false, new_expression_context);
			if (!type.children.empty())
			{
				const PA10AstNode& seq = type.children.front();
				if (!seq.children.empty())
				{
					const PA10AstNode& first = seq.children.back();
					if (first.name_prefix_count != 0 ||
						!first.name_parts.empty())
					{
						trailing.name_prefix_begin = first.name_prefix_begin;
						trailing.name_prefix_count = first.name_prefix_count;
						trailing.name_parts = first.name_parts;
					}
				}
			}
			trailing.children.push_back(std::move(type));
			result.children.push_back(std::move(trailing));
			continue;
		}
		break;
	}
}
PA10AstNode PA10Parser::parse_abstract_declarator(bool stop_at_empty_parameter_clause,
	bool first_member_pointer_checked, bool new_expression_context)
{
	RecursionGuard recursion(*this);
	PA10AstNode result = node(PA10NodeKind::AbstractDeclarator);
	while (fixed(SimpleTokenType::OP_STAR) ||
		fixed(SimpleTokenType::OP_AMP) || fixed(SimpleTokenType::OP_LAND) ||
		first_member_pointer_checked || member_pointer_operator_start())
	{
		PA10AstNode pointer = parse_ptr_operator();
		first_member_pointer_checked = false;
		result.children.push_back(std::move(pointer));
		while (PA10ParserSupport::is_cv(look().fixed) &&
			look().kind == PA10TokenKind::Fixed)
			result.children.push_back(fixed_node(PA10NodeKind::CvQualifier));
	}
	if (fixed(SimpleTokenType::OP_LPAREN) &&
		!(stop_at_empty_parameter_clause &&
			fixed(SimpleTokenType::OP_RPAREN, 1)))
	{
		const bool group_is_abstract_declarator =
			new_expression_context && parenthesized_group_is(position_,
				PA10ParserSupport::PA10ParenthesizedGroupKind::AbstractDeclarator);
		const bool group_is_parameter_clause =
			new_expression_context && parenthesized_group_is(position_,
				PA10ParserSupport::PA10ParenthesizedGroupKind::ParameterClause);
		const bool nested_pointer_shape =
			fixed(SimpleTokenType::OP_STAR, 1) ||
			fixed(SimpleTokenType::OP_AMP, 1) ||
			fixed(SimpleTokenType::OP_LAND, 1);
		if ((looks_like_parameter_clause() || group_is_parameter_clause) &&
			!nested_pointer_shape && !group_is_abstract_declarator)
		{
			result.children.push_back(parse_parameter_clause());
			parse_function_suffixes(result, new_expression_context);
		}
		else
		{
			consume_fixed(SimpleTokenType::OP_LPAREN);
			PA10AstNode nested = node(PA10NodeKind::NestedDeclarator);
			nested.children.push_back(parse_abstract_declarator(false, false, new_expression_context));
			consume_fixed(SimpleTokenType::OP_RPAREN);
			result.children.push_back(std::move(nested));
		}
	}
	while (fixed(SimpleTokenType::OP_LPAREN) ||
		fixed(SimpleTokenType::OP_LSQUARE))
	{
		if (fixed(SimpleTokenType::OP_LPAREN))
		{
			const bool group_is_abstract_declarator =
				new_expression_context && parenthesized_group_is(position_,
					PA10ParserSupport::PA10ParenthesizedGroupKind::AbstractDeclarator);
			const bool group_is_parameter_clause =
				new_expression_context && parenthesized_group_is(position_,
					PA10ParserSupport::PA10ParenthesizedGroupKind::ParameterClause);
			if ((!looks_like_parameter_clause() && !group_is_parameter_clause) ||
				group_is_abstract_declarator ||
				(stop_at_empty_parameter_clause && fixed(SimpleTokenType::OP_RPAREN, 1)))
				break;
			result.children.push_back(parse_parameter_clause());
			parse_function_suffixes(result, new_expression_context);
			continue;
		}
		consume_fixed(SimpleTokenType::OP_LSQUARE);
		PA10AstNode suffix = node(PA10NodeKind::ArraySuffix);
		if (!fixed(SimpleTokenType::OP_RSQUARE))
			suffix.children.push_back(parse_expression());
		consume_fixed(SimpleTokenType::OP_RSQUARE);
		result.children.push_back(std::move(suffix));
	}
	parse_function_suffixes(result, new_expression_context);
	return result;
}
PA10AstNode PA10Parser::parse_declarator(bool allow_abstract,
	bool force_parameter_suffix, bool prefer_parameter_clause_at_root, bool stop_at_parameter_attributes)
{
	enter();
	PA10AstNode result = node(PA10NodeKind::Declarator);
	while (fixed(SimpleTokenType::OP_STAR) ||
		fixed(SimpleTokenType::OP_AMP) || fixed(SimpleTokenType::OP_LAND) ||
		member_pointer_operator_start())
	{
		PA10AstNode pointer = parse_ptr_operator();
		result.children.push_back(std::move(pointer));
		while (PA10ParserSupport::is_cv(look().fixed) &&
			look().kind == PA10TokenKind::Fixed)
			result.children.push_back(fixed_node(PA10NodeKind::CvQualifier));
	}
	if (fixed(SimpleTokenType::OP_DOTS))
	{
		consume_fixed(SimpleTokenType::OP_DOTS);
		result.children.push_back(node(PA10NodeKind::ParameterPack));
	}
	if (id_expression_start() || decltype_qualified_name_start())
		result.children.push_back(parse_id_expression_node(PA10NodeKind::Identifier));
	else if (fixed(SimpleTokenType::OP_LPAREN))
	{
		if (prefer_parameter_clause_at_root && looks_like_parameter_clause())
			result.children.push_back(parse_parameter_clause());
		else
		{
			consume_fixed(SimpleTokenType::OP_LPAREN);
			PA10AstNode nested = node(PA10NodeKind::NestedDeclarator);
			nested.children.push_back(parse_declarator(true));
			consume_fixed(SimpleTokenType::OP_RPAREN);
			result.children.push_back(std::move(nested));
		}
	}
	else if (!allow_abstract)
		fail("expected declarator-id");
	while (fixed(SimpleTokenType::OP_LPAREN) ||
		(fixed(SimpleTokenType::OP_LSQUARE) &&
			!(stop_at_parameter_attributes &&
				PA10ParserSupport::attribute_specifier_start(
					tokens_, delimiter_close_index_, position_))))
	{
		if (fixed(SimpleTokenType::OP_LPAREN) &&
			(force_parameter_suffix || looks_like_parameter_clause()))
		{
			result.children.push_back(parse_parameter_clause());
			parse_function_suffixes(result);
		}
		else if (fixed(SimpleTokenType::OP_LPAREN))
			break;
		else
		{
			consume_fixed(SimpleTokenType::OP_LSQUARE);
			PA10AstNode suffix = node(PA10NodeKind::ArraySuffix);
			if (!fixed(SimpleTokenType::OP_RSQUARE))
				suffix.children.push_back(parse_expression());
			consume_fixed(SimpleTokenType::OP_RSQUARE);
			result.children.push_back(std::move(suffix));
		}
	}
	parse_function_suffixes(result);
	leave();
	return result;
}
PA10AstNode PA10Parser::parse_parameter_clause()
{
	consume_fixed(SimpleTokenType::OP_LPAREN);
	PA10AstNode result = node(PA10NodeKind::ParameterClause);
	name_scopes_.enter_parameter_clause();
	if (fixed(SimpleTokenType::OP_RPAREN))
	{
		consume_fixed(SimpleTokenType::OP_RPAREN);
		name_scopes_.leave_parameter_clause(result);
		return result;
	}
	if (fixed(SimpleTokenType::OP_DOTS))
	{
		consume_fixed(SimpleTokenType::OP_DOTS);
		result.children.push_back(node(PA10NodeKind::ParameterPack));
		consume_fixed(SimpleTokenType::OP_RPAREN);
		name_scopes_.leave_parameter_clause(result);
		return result;
	}
	while (true)
	{
		if (fixed(SimpleTokenType::OP_DOTS))
		{
			consume_fixed(SimpleTokenType::OP_DOTS);
			result.children.push_back(node(PA10NodeKind::ParameterPack));
			break;
		}
		result.children.push_back(parse_parameter_declaration());
		if (fixed(SimpleTokenType::OP_RPAREN))
			break;
		consume_fixed(SimpleTokenType::OP_COMMA);
		if (fixed(SimpleTokenType::OP_DOTS))
		{
			consume_fixed(SimpleTokenType::OP_DOTS);
			result.children.push_back(node(PA10NodeKind::ParameterPack));
			break;
		}
	}
	consume_fixed(SimpleTokenType::OP_RPAREN);
	name_scopes_.leave_parameter_clause(result);
	return result;
}
PA10AstNode PA10Parser::parse_parameter_declaration()
{
	PA10AstNode result = node(PA10NodeKind::ParameterDeclaration);
	result.children.push_back(parse_decl_specifier_seq());
	if (fixed(SimpleTokenType::OP_DOTS))
	{
		if (fixed(SimpleTokenType::OP_COMMA, 1) ||
			fixed(SimpleTokenType::OP_RPAREN, 1))
		{
			consume_fixed(SimpleTokenType::OP_DOTS);
			PA10AstNode declarator = node(PA10NodeKind::Declarator);
			declarator.children.push_back(node(PA10NodeKind::ParameterPack));
			result.children.push_back(std::move(declarator));
		}
		else
			result.children.push_back(parse_declarator(true, false, true, true));
	}
	else if (!fixed(SimpleTokenType::OP_COMMA) &&
		!fixed(SimpleTokenType::OP_RPAREN) &&
		!fixed(SimpleTokenType::OP_DOTS) &&
		!PA10ParserSupport::attribute_specifier_start(
			tokens_, delimiter_close_index_, position_))
		result.children.push_back(parse_declarator(true, false, true, true));
	skip_attribute_specifiers();
	if (fixed(SimpleTokenType::OP_ASS))
	{
		PA10AstNode argument = node(PA10NodeKind::DefaultArgument);
		argument.children.push_back(parse_initializer());
		result.children.push_back(std::move(argument));
	}
	return result;
}
PA10AstNode PA10Parser::parse_initializer()
{
	PA10AstNode result = node(PA10NodeKind::Initializer);
	if (fixed(SimpleTokenType::OP_ASS))
	{
		consume_fixed(SimpleTokenType::OP_ASS);
		result.has_token = true;
		result.token = SimpleTokenType::OP_ASS;
		if (fixed(SimpleTokenType::KW_DEFAULT) ||
			fixed(SimpleTokenType::KW_DELETE))
		{
			PA10AstNode special_initializer =
				fixed_node(PA10NodeKind::SpecialInitializer);
			result.children.push_back(std::move(special_initializer));
			return result;
		}
	}
	if (fixed(SimpleTokenType::OP_LBRACE))
		result.children.push_back(parse_braced_init_list());
	else if (fixed(SimpleTokenType::OP_LPAREN) && !result.has_token)
	{
		consume_fixed(SimpleTokenType::OP_LPAREN);
		begin_non_angle();
		PA10AstNode paren = node(PA10NodeKind::ParenInitializer);
		if (!fixed(SimpleTokenType::OP_RPAREN))
		{
			paren.children.push_back(parse_initializer_clause());
			while (fixed(SimpleTokenType::OP_COMMA))
			{
				consume_fixed(SimpleTokenType::OP_COMMA);
				paren.children.push_back(parse_initializer_clause());
			}
		}
		consume_fixed(SimpleTokenType::OP_RPAREN);
		end_non_angle();
		result.children.push_back(std::move(paren));
	}
	else
		result.children.push_back(parse_initializer_clause());
	return result;
}
PA10AstNode PA10Parser::parse_initializer_clause()
{
	if (fixed(SimpleTokenType::OP_LBRACE))
		return parse_braced_init_list();
	PA10AstNode result = parse_assignment_expression();
	if (fixed(SimpleTokenType::OP_DOTS))
	{
		consume_fixed(SimpleTokenType::OP_DOTS);
		PA10AstNode expansion = node(PA10NodeKind::PackExpansionExpression);
		expansion.children.push_back(std::move(result));
		return expansion;
	}
	return result;
}
PA10AstNode PA10Parser::parse_braced_init_list()
{
	RecursionGuard recursion(*this);
	consume_fixed(SimpleTokenType::OP_LBRACE);
	PA10AstNode result = node(PA10NodeKind::BracedInitList);
	if (!fixed(SimpleTokenType::OP_RBRACE))
	{
		result.children.push_back(parse_initializer_clause());
		while (fixed(SimpleTokenType::OP_COMMA))
		{
			consume_fixed(SimpleTokenType::OP_COMMA);
			if (fixed(SimpleTokenType::OP_RBRACE))
				break;
			result.children.push_back(parse_initializer_clause());
		}
	}
	consume_fixed(SimpleTokenType::OP_RBRACE);
	return result;
}
PA10AstNode PA10Parser::parse_expression()
{
	RecursionGuard recursion(*this);
	PA10AstNode result = parse_assignment_expression();
	while (fixed(SimpleTokenType::OP_COMMA))
	{
		const PA10Token op = consume_token();
		PA10AstNode right = parse_assignment_expression();
		PA10AstNode combined = node(PA10NodeKind::BinaryExpression);
		combined.has_token = true;
		combined.token = op.fixed;
		combined.token_spelling = intern(op.source);
		combined.children.push_back(std::move(result));
		combined.children.push_back(std::move(right));
		result = std::move(combined);
	}
	return result;
}
PA10AstNode PA10Parser::parse_assignment_expression()
{
	RecursionGuard recursion(*this);
	PA10AstNode result = parse_conditional_expression();
	if (look().kind == PA10TokenKind::Fixed &&
		PA10ParserSupport::is_assignment_operator(look().fixed))
	{
		const PA10Token op = consume_token();
		PA10AstNode assignment = node(PA10NodeKind::AssignmentExpression);
		assignment.has_token = true;
		assignment.token = op.fixed;
		assignment.token_spelling = intern(op.source);
		assignment.children.push_back(std::move(result));
		assignment.children.push_back(parse_assignment_expression());
		return assignment;
	}
	return result;
}
PA10AstNode PA10Parser::parse_conditional_expression()
{
	PA10AstNode result = parse_binary_expression(0);
	if (fixed(SimpleTokenType::OP_QMARK))
	{
		consume_fixed(SimpleTokenType::OP_QMARK);
		PA10AstNode conditional = node(PA10NodeKind::ConditionalExpression);
		conditional.children.push_back(std::move(result));
		conditional.children.push_back(parse_expression());
		consume_fixed(SimpleTokenType::OP_COLON);
		conditional.children.push_back(parse_assignment_expression());
		return conditional;
	}
	return result;
}
PA10AstNode PA10Parser::parse_binary_expression(int level)
{
	PA10AstNode result;
	if (level == 10)
		result = parse_unary_expression();
	else
		result = parse_binary_expression(level + 1);
	while ((look().kind == PA10TokenKind::Fixed &&
			PA10ParserSupport::is_binary_operator(level, look().fixed)) ||
		(level == 7 && rshift_operator()))
	{
		if (level == 6 && fixed(SimpleTokenType::OP_GT) &&
			!can_use_angle_operator())
			break;
		const bool is_shift = level == 7 && rshift_operator();
		if (is_shift && !can_use_angle_operator())
			break;
		if (level == 7 && !is_shift &&
			!(look().kind == PA10TokenKind::Fixed &&
				look().fixed == SimpleTokenType::OP_LSHIFT))
			break;
		PA10Token op;
		if (is_shift)
			consume_shift_operator(&op);
		else
			op = consume_token();
		PA10AstNode binary = node(PA10NodeKind::BinaryExpression);
		binary.has_token = true;
		binary.token = op.fixed;
		binary.token_spelling = intern(op.source);
		binary.children.push_back(std::move(result));
		if (level == 10)
			binary.children.push_back(parse_unary_expression());
		else
			binary.children.push_back(parse_binary_expression(level + 1));
		result = std::move(binary);
	}
	return result;
}
bool PA10Parser::looks_like_c_style_cast() const
{
	if (!fixed(SimpleTokenType::OP_LPAREN))
		return false;
	const std::size_t local_close = position_ < delimiter_close_index_.size() ? delimiter_close_index_[position_] : tokens_.size();
	if (local_close == tokens_.size() || local_close <= position_)
		return false;
	std::size_t close = 1;
	bool simple_type = false;
	while (close < local_close - position_ &&
		look(close).kind == PA10TokenKind::Fixed &&
		(PA10ParserSupport::is_type_keyword(look(close).fixed) ||
			PA10ParserSupport::is_cv(look(close).fixed)))
	{
		simple_type = true;
		++close;
	}
	const bool identifier_type = !simple_type && identifier(1);
	const bool named_type = identifier_type && name_scopes_.is_type(look(1).spelling);
	if (!simple_type && !identifier_type)
		return false;
	if (!simple_type &&
		!PA10ParserSupport::qualified_cast_close(tokens_, template_close_index_, rshift_piece1_nested_close_, position_, local_close, &close))
		return false;
	const bool pointer_shape = fixed(SimpleTokenType::OP_STAR, close) || fixed(SimpleTokenType::OP_AMP, close) || fixed(SimpleTokenType::OP_LAND, close);
	close += pointer_shape;
	if (!fixed(SimpleTokenType::OP_RPAREN, close))
		return false;
	const PA10TokenKind next_kind = look(close + 1).kind;
	if (next_kind == PA10TokenKind::Identifier ||
		next_kind == PA10TokenKind::Literal)
		return true;
	if (simple_type && next_kind == PA10TokenKind::Fixed &&
		(fixed(SimpleTokenType::OP_INC, close + 1) || fixed(SimpleTokenType::OP_DEC, close + 1) ||
		 fixed(SimpleTokenType::OP_STAR, close + 1) || fixed(SimpleTokenType::OP_AMP, close + 1) ||
		 fixed(SimpleTokenType::OP_PLUS, close + 1) || fixed(SimpleTokenType::OP_MINUS, close + 1) ||
		 fixed(SimpleTokenType::OP_LNOT, close + 1) || fixed(SimpleTokenType::OP_COMPL, close + 1)))
		return true;
	if (fixed(SimpleTokenType::OP_LPAREN, close + 1) &&
		!simple_type && !pointer_shape && !named_type)
		return false;
	return fixed(SimpleTokenType::OP_LPAREN, close + 1) ||
		fixed(SimpleTokenType::KW_TRUE, close + 1) ||
		fixed(SimpleTokenType::KW_FALSE, close + 1) ||
		fixed(SimpleTokenType::KW_NULLPTR, close + 1);
}
PA10AstNode PA10Parser::parse_c_style_cast()
{
	consume_fixed(SimpleTokenType::OP_LPAREN);
	PA10AstNode result = node(PA10NodeKind::CastExpression);
	result.has_token = true;
	result.token = SimpleTokenType::OP_LPAREN;
	result.token_spelling = 0;
	result.children.push_back(parse_type_id());
	consume_fixed(SimpleTokenType::OP_RPAREN);
	result.children.push_back(parse_unary_expression());
	return result;
}
PA10AstNode PA10Parser::parse_unary_expression()
{
	RecursionGuard recursion(*this);
	std::vector<PA10Token> prefixes;
	while (look().kind == PA10TokenKind::Fixed &&
		(fixed(SimpleTokenType::OP_INC) || fixed(SimpleTokenType::OP_DEC) ||
			fixed(SimpleTokenType::OP_STAR) || fixed(SimpleTokenType::OP_AMP) ||
			fixed(SimpleTokenType::OP_PLUS) || fixed(SimpleTokenType::OP_MINUS) ||
			fixed(SimpleTokenType::OP_LNOT) || fixed(SimpleTokenType::OP_COMPL)))
		prefixes.push_back(consume_token());
	PA10AstNode result = parse_unary_expression_base();
	for (std::size_t i = prefixes.size(); i != 0; --i)
	{
		const PA10Token& op = prefixes[i - 1];
		PA10AstNode unary = node(PA10NodeKind::UnaryExpression);
		unary.has_token = true;
		unary.token = op.fixed;
		unary.token_spelling = intern(op.source);
		unary.children.push_back(std::move(result));
		result = std::move(unary);
	}
	return result;
}
PA10AstNode PA10Parser::parse_unary_expression_base()
{
	if (looks_like_c_style_cast())
		return parse_c_style_cast();
	if (fixed(SimpleTokenType::KW_SIZEOF))
	{
		consume_fixed(SimpleTokenType::KW_SIZEOF);
		PA10AstNode result = node(PA10NodeKind::SizeofExpression);
		if (fixed(SimpleTokenType::OP_LPAREN))
		{
			consume_fixed(SimpleTokenType::OP_LPAREN);
			result.children.push_back(parse_type_id_or_expression());
			consume_fixed(SimpleTokenType::OP_RPAREN);
		}
		else
			result.children.push_back(parse_unary_expression());
		return result;
	}
	if (fixed(SimpleTokenType::KW_ALIGNOF))
		return parse_type_trait(SimpleTokenType::KW_ALIGNOF);
	if (fixed(SimpleTokenType::KW_NOEXCEPT))
		return parse_type_trait(SimpleTokenType::KW_NOEXCEPT);
	if (fixed(SimpleTokenType::KW_STATIC_CAST) ||
		fixed(SimpleTokenType::KW_DYNAMIC_CAST) ||
		fixed(SimpleTokenType::KW_CONST_CAST) ||
		fixed(SimpleTokenType::KW_REINTERPET_CAST))
		return parse_postfix_suffixes(parse_keyword_cast());
	if (fixed(SimpleTokenType::KW_NEW) ||
		(fixed(SimpleTokenType::OP_COLON2) &&
		 fixed(SimpleTokenType::KW_NEW, 1)))
		return parse_new_expression();
	if (fixed(SimpleTokenType::KW_DELETE) ||
		(fixed(SimpleTokenType::OP_COLON2) &&
			fixed(SimpleTokenType::KW_DELETE, 1)))
		return parse_delete_expression();
	return parse_postfix_expression();
}
PA10AstNode PA10Parser::parse_type_id_or_expression()
{
	if ((look().kind == PA10TokenKind::Fixed &&
		PA10ParserSupport::is_type_keyword(look().fixed)) ||
		(PA10ParserSupport::is_cv(look().fixed) &&
			look().kind == PA10TokenKind::Fixed))
		return parse_type_id();
	if (fixed(SimpleTokenType::KW_CLASS) ||
		fixed(SimpleTokenType::KW_STRUCT) ||
		fixed(SimpleTokenType::KW_UNION) ||
		fixed(SimpleTokenType::KW_ENUM))
		return parse_type_id();
	if (identifier() && (fixed(SimpleTokenType::OP_AMP, 1) ||
		fixed(SimpleTokenType::OP_LAND, 1) ||
		fixed(SimpleTokenType::OP_STAR, 1)))
		return parse_type_id();
	return parse_expression();
}
PA10AstNode PA10Parser::parse_type_trait(SimpleTokenType keyword)
{
	PA10AstNode result = node(PA10NodeKind::TypeTraitExpression);
	result.has_token = true;
	const PA10Token token = consume_token();
	if (token.kind != PA10TokenKind::Fixed || token.fixed != keyword)
		fail("unexpected type-trait keyword");
	result.token = token.fixed;
	result.token_spelling = intern(token.source);
	consume_fixed(SimpleTokenType::OP_LPAREN);
	result.children.push_back(parse_type_id_or_expression());
	consume_fixed(SimpleTokenType::OP_RPAREN);
	return result;
}
PA10AstNode PA10Parser::parse_keyword_cast()
{
	const PA10Token keyword = consume_token();
	PA10AstNode result = node(PA10NodeKind::CastExpression);
	result.has_token = true;
	result.token = keyword.fixed;
	result.token_spelling = intern(keyword.source);
	consume_fixed(SimpleTokenType::OP_LT);
	begin_angle();
	result.children.push_back(parse_type_id());
	close_angle();
	consume_fixed(SimpleTokenType::OP_LPAREN);
	result.children.push_back(parse_expression());
	consume_fixed(SimpleTokenType::OP_RPAREN);
	return result;
}
PA10AstNode PA10Parser::parse_new_expression()
{
	PA10AstNode result = node(PA10NodeKind::NewExpression);
	if (fixed(SimpleTokenType::OP_COLON2))
		result.children.push_back(fixed_node(PA10NodeKind::GlobalScope));
	const PA10Token keyword = consume_token();
	if (keyword.kind != PA10TokenKind::Fixed ||
		keyword.fixed != SimpleTokenType::KW_NEW)
		fail("expected new-expression keyword");
	result.has_token = true;
	result.token = keyword.fixed;
	result.token_spelling = intern(keyword.source);
	std::size_t placement_work = 0;
	const bool placement = fixed(SimpleTokenType::OP_LPAREN) &&
		PA10ParserSupport::new_first_parenthesized_group_is_placement(
			tokens_, delimiter_close_index_, position_, &placement_work);
	for (std::size_t i = 0; i < placement_work; ++i)
		charge();
	if (placement)
	{
		PA10AstNode node_placement = node(PA10NodeKind::NewPlacement);
		node_placement.children.push_back(parse_paren_argument_list());
		result.children.push_back(std::move(node_placement));
	}
	if (fixed(SimpleTokenType::OP_LPAREN))
	{
		consume_fixed(SimpleTokenType::OP_LPAREN);
		begin_non_angle();
		result.children.push_back(parse_type_id(false, true, true));
		consume_fixed(SimpleTokenType::OP_RPAREN);
		end_non_angle();
	}
	else result.children.push_back(parse_type_id(false, true));
	if (fixed(SimpleTokenType::OP_LPAREN) || fixed(SimpleTokenType::OP_LBRACE))
		result.children.push_back(parse_initializer());
	return result;
}
PA10AstNode PA10Parser::parse_delete_expression()
{
	PA10AstNode result = node(PA10NodeKind::DeleteExpression);
	if (fixed(SimpleTokenType::OP_COLON2))
		result.children.push_back(fixed_node(PA10NodeKind::GlobalScope));
	consume_fixed(SimpleTokenType::KW_DELETE);
	if (fixed(SimpleTokenType::OP_LSQUARE))
	{
		consume_fixed(SimpleTokenType::OP_LSQUARE);
		consume_fixed(SimpleTokenType::OP_RSQUARE);
		result.children.push_back(node(PA10NodeKind::ArrayDeleteMarker));
	}
	result.children.push_back(parse_unary_expression());
	return result;
}
PA10AstNode PA10Parser::parse_postfix_expression_seed()
{
	if (fixed(SimpleTokenType::KW_TYPEID))
		return parse_type_trait(SimpleTokenType::KW_TYPEID);
	const PA10ParserSupport::PA10FunctionStyleCastClassification classification =
		PA10ParserSupport::classify_function_style_cast(
			tokens_, delimiter_close_index_, position_);
	for (std::size_t i = 0; i < classification.charged_work; ++i)
		charge();
	if (classification.kind ==
		PA10ParserSupport::PA10FunctionStyleCastKind::LegacyBuiltin)
	{
		const PA10Token type = consume_token();
		PA10AstNode callee = node(PA10NodeKind::IdExpression);
		callee.has_token = true;
		callee.token = type.fixed;
		callee.token_spelling = intern(type.source);
		PA10AstNode result = node(PA10NodeKind::CallExpression);
		result.children.push_back(std::move(callee));
		result.children.push_back(parse_paren_argument_list());
		return result;
	}
	if (classification.kind ==
		PA10ParserSupport::PA10FunctionStyleCastKind::TypeId)
	{
		PA10AstNode type_id = node(PA10NodeKind::TypeId);
		type_id.children.push_back(parse_decl_specifier_seq(true));
		PA10AstNode result = node(PA10NodeKind::CallExpression);
		result.children.push_back(std::move(type_id));
		result.children.push_back(parse_paren_argument_list());
		return result;
	}
	return parse_primary_expression();
}
PA10AstNode PA10Parser::parse_postfix_suffixes(PA10AstNode result)
{
	while (true)
	{
		if (fixed(SimpleTokenType::OP_LPAREN))
		{
			PA10AstNode call = node(PA10NodeKind::CallExpression);
			call.children.push_back(std::move(result));
			call.children.push_back(parse_argument_list());
			result = std::move(call);
			continue;
		}
		if (fixed(SimpleTokenType::OP_LSQUARE))
		{
			consume_fixed(SimpleTokenType::OP_LSQUARE);
			PA10AstNode subscript = node(PA10NodeKind::SubscriptExpression);
			subscript.children.push_back(std::move(result));
			subscript.children.push_back(parse_expression());
			consume_fixed(SimpleTokenType::OP_RSQUARE);
			result = std::move(subscript);
			continue;
		}
		if (fixed(SimpleTokenType::OP_DOT) ||
			fixed(SimpleTokenType::OP_ARROW))
		{
			const PA10Token op = consume_token();
			PA10AstNode member = node(PA10NodeKind::MemberExpression);
			member.has_token = true;
			member.token = op.fixed;
			member.token_spelling = intern(op.source);
			member.children.push_back(std::move(result));
			if (!id_expression_start() &&
				!decltype_qualified_name_start())
				fail("expected member name");
			member.children.push_back(name_node(PA10NodeKind::Identifier,
				parse_name(false, false, true)));
			result = std::move(member);
			continue;
		}
		if (fixed(SimpleTokenType::OP_INC) || fixed(SimpleTokenType::OP_DEC))
		{
			const PA10Token op = consume_token();
			PA10AstNode postfix = node(PA10NodeKind::PostfixExpression);
			postfix.has_token = true;
			postfix.token = op.fixed;
			postfix.token_spelling = intern(op.source);
			postfix.children.push_back(std::move(result));
			result = std::move(postfix);
			continue;
		}
		break;
	}
	return result;
}
PA10AstNode PA10Parser::parse_postfix_expression()
{
	return parse_postfix_suffixes(parse_postfix_expression_seed());
}
PA10AstNode PA10Parser::parse_primary_expression()
{
	RecursionGuard recursion(*this);
	if (literal())
		return literal_node();
	if (fixed(SimpleTokenType::KW_TRUE) ||
		fixed(SimpleTokenType::KW_FALSE) ||
		fixed(SimpleTokenType::KW_NULLPTR) ||
		fixed(SimpleTokenType::KW_THIS))
	{
		const PA10Token token = consume_token();
		PA10AstNode result = node(PA10NodeKind::KeywordLiteral);
		result.has_token = true;
		result.token = token.fixed;
		result.token_spelling = intern(token.source);
		return result;
	}
	if (id_expression_start() || decltype_qualified_name_start())
		return parse_id_expression_node(PA10NodeKind::IdExpression, false);
	if (fixed(SimpleTokenType::OP_LPAREN))
	{
		consume_fixed(SimpleTokenType::OP_LPAREN);
		begin_non_angle();
		PA10AstNode result = node(PA10NodeKind::ParenthesizedExpression);
		result.children.push_back(parse_expression());
		consume_fixed(SimpleTokenType::OP_RPAREN);
		end_non_angle();
		return result;
	}
	if (fixed(SimpleTokenType::OP_LBRACE))
		return parse_braced_init_list();
	if (fixed(SimpleTokenType::OP_LSQUARE))
		return parse_lambda_expression();
	fail("expected primary expression");
	return node(PA10NodeKind::Literal);
}
PA10AstNode PA10Parser::parse_lambda_expression()
{
	consume_fixed(SimpleTokenType::OP_LSQUARE);
	PA10AstNode result = node(PA10NodeKind::LambdaExpression);
	PA10AstNode introducer = node(PA10NodeKind::LambdaIntroducer);
	PA10ParserSupport::PA10LambdaIntroducerFacts facts;
	if (!PA10ParserSupport::scan_lambda_introducer_facts(
		tokens_, position_, &facts))
	{
		for (std::size_t i = 0; i < facts.charged_work; ++i)
			charge();
		fail("malformed lambda introducer");
	}
	for (std::size_t i = 0; i < facts.charged_work; ++i)
		charge();
	position_ += facts.consumed;
	introducer.lambda_capture_default = facts.capture_default;
	introducer.lambda_capture_begin = ast_.lambda_captures.size();
	introducer.lambda_capture_count = facts.captures.size();
	ast_.lambda_captures.insert(ast_.lambda_captures.end(),
		facts.captures.begin(), facts.captures.end());
	result.children.push_back(std::move(introducer));
	if (fixed(SimpleTokenType::OP_LPAREN))
		result.children.push_back(parse_lambda_declarator());
	result.children.push_back(parse_compound_statement());
	return result;
}
PA10AstNode PA10Parser::parse_lambda_declarator()
{
	PA10AstNode result = node(PA10NodeKind::LambdaDeclarator);
	result.children.push_back(parse_parameter_clause());
	if (fixed(SimpleTokenType::KW_MUTABLE))
		result.children.push_back(fixed_node(PA10NodeKind::LambdaSpecifier));
	if (fixed(SimpleTokenType::KW_NOEXCEPT))
		result.children.push_back(parse_noexcept_specification());
	if (fixed(SimpleTokenType::OP_ARROW))
	{
		consume_fixed(SimpleTokenType::OP_ARROW);
		PA10AstNode trailing = node(PA10NodeKind::TrailingReturnType);
		trailing.children.push_back(parse_type_id());
		result.children.push_back(std::move(trailing));
	}
	return result;
}
PA10AstNode PA10Parser::parse_argument_list()
{
	consume_fixed(SimpleTokenType::OP_LPAREN);
	begin_non_angle();
	PA10AstNode result = node(PA10NodeKind::ArgumentList);
	if (!fixed(SimpleTokenType::OP_RPAREN))
	{
		result.children.push_back(parse_initializer_clause());
		while (fixed(SimpleTokenType::OP_COMMA))
		{
			consume_fixed(SimpleTokenType::OP_COMMA);
			result.children.push_back(parse_initializer_clause());
		}
	}
	consume_fixed(SimpleTokenType::OP_RPAREN);
	end_non_angle();
	return result;
}
PA10AstNode PA10Parser::parse_compound_statement()
{
	RecursionGuard recursion(*this);
	consume_fixed(SimpleTokenType::OP_LBRACE);
	PA10AstNode result = node(PA10NodeKind::CompoundStatement);
	enter();
	while (!fixed(SimpleTokenType::OP_RBRACE))
	{
		if (at_end())
			fail("unterminated compound statement");
		result.children.push_back(parse_statement());
	}
	consume_fixed(SimpleTokenType::OP_RBRACE);
	leave();
	return result;
}
PA10AstNode PA10Parser::parse_statement()
{
	RecursionGuard recursion(*this);
	if (fixed(SimpleTokenType::OP_LBRACE))
		return parse_compound_statement();
	if (fixed(SimpleTokenType::KW_RETURN))
	{
		consume_fixed(SimpleTokenType::KW_RETURN);
		PA10AstNode result = node(PA10NodeKind::ReturnStatement);
		if (!fixed(SimpleTokenType::OP_SEMICOLON))
			result.children.push_back(parse_expression());
		consume_fixed(SimpleTokenType::OP_SEMICOLON);
		return result;
	}
	if (fixed(SimpleTokenType::KW_IF))
		return parse_if_statement();
	if (fixed(SimpleTokenType::KW_SWITCH))
		return parse_switch_statement();
	if (fixed(SimpleTokenType::KW_WHILE) || fixed(SimpleTokenType::KW_DO) ||
		fixed(SimpleTokenType::KW_FOR))
		return parse_iteration_statement();
	if (fixed(SimpleTokenType::KW_TRY))
		return parse_try_block();
	if (fixed(SimpleTokenType::KW_BREAK))
	{
		consume_fixed(SimpleTokenType::KW_BREAK);
		consume_fixed(SimpleTokenType::OP_SEMICOLON);
		return node(PA10NodeKind::BreakStatement);
	}
	if (fixed(SimpleTokenType::KW_CONTINUE))
	{
		consume_fixed(SimpleTokenType::KW_CONTINUE);
		consume_fixed(SimpleTokenType::OP_SEMICOLON);
		return node(PA10NodeKind::ContinueStatement);
	}
	if (fixed(SimpleTokenType::KW_GOTO))
	{
		consume_fixed(SimpleTokenType::KW_GOTO);
		PA10AstNode result = node(PA10NodeKind::GotoStatement);
		const PA10Token label = consume_identifier_token();
		result.producer_spelling = label.spelling;
		consume_fixed(SimpleTokenType::OP_SEMICOLON);
		return result;
	}
	if (fixed(SimpleTokenType::KW_THROW))
	{
		consume_fixed(SimpleTokenType::KW_THROW);
		PA10AstNode result = node(PA10NodeKind::ThrowStatement);
		if (!fixed(SimpleTokenType::OP_SEMICOLON))
			result.children.push_back(parse_assignment_expression());
		consume_fixed(SimpleTokenType::OP_SEMICOLON);
		return result;
	}
	if (fixed(SimpleTokenType::KW_CASE))
	{
		consume_fixed(SimpleTokenType::KW_CASE);
		PA10AstNode result = node(PA10NodeKind::CaseStatement);
		result.children.push_back(parse_expression());
		consume_fixed(SimpleTokenType::OP_COLON);
		result.children.push_back(parse_statement());
		return result;
	}
	if (fixed(SimpleTokenType::KW_DEFAULT))
	{
		consume_fixed(SimpleTokenType::KW_DEFAULT);
		PA10AstNode result = node(PA10NodeKind::DefaultStatement);
		consume_fixed(SimpleTokenType::OP_COLON);
		result.children.push_back(parse_statement());
		return result;
	}
	if (identifier() && fixed(SimpleTokenType::OP_COLON, 1))
	{
		PA10AstNode result = node(PA10NodeKind::LabeledStatement);
		const PA10Token label = consume_identifier_token();
		result.producer_spelling = label.spelling;
		consume_fixed(SimpleTokenType::OP_COLON);
		result.children.push_back(parse_statement());
		return result;
	}
	if (declaration_start())
		return parse_declaration();
	PA10AstNode result = node(PA10NodeKind::ExpressionStatement);
	if (!fixed(SimpleTokenType::OP_SEMICOLON))
		result.children.push_back(parse_expression());
	consume_fixed(SimpleTokenType::OP_SEMICOLON);
	return result;
}
PA10AstNode PA10Parser::parse_condition()
{
	PA10AstNode result = node(PA10NodeKind::Condition);
	const bool declaration =
		(look().kind == PA10TokenKind::Fixed &&
		 PA10ParserSupport::is_type_keyword(look().fixed)) ||
		(identifier() && identifier(1));
	if (declaration)
	{
		PA10AstNode condition = node(PA10NodeKind::ConditionDeclaration);
		condition.children.push_back(parse_decl_specifier_seq());
		condition.children.push_back(parse_declarator(false));
		if (fixed(SimpleTokenType::OP_ASS) || fixed(SimpleTokenType::OP_LBRACE) ||
			fixed(SimpleTokenType::OP_LPAREN))
			condition.children.push_back(parse_initializer());
		else
			fail("condition declaration needs initializer");
		result.children.push_back(std::move(condition));
	}
	else
		result.children.push_back(parse_expression());
	return result;
}
PA10AstNode PA10Parser::parse_if_statement()
{
	consume_fixed(SimpleTokenType::KW_IF);
	consume_fixed(SimpleTokenType::OP_LPAREN);
	PA10AstNode result = node(PA10NodeKind::IfStatement);
	result.children.push_back(parse_condition());
	consume_fixed(SimpleTokenType::OP_RPAREN);
	PA10AstNode then_branch = node(PA10NodeKind::ThenBranch);
	then_branch.children.push_back(parse_statement());
	result.children.push_back(std::move(then_branch));
	if (fixed(SimpleTokenType::KW_ELSE))
	{
		consume_fixed(SimpleTokenType::KW_ELSE);
		PA10AstNode else_branch = node(PA10NodeKind::ElseBranch);
		else_branch.children.push_back(parse_statement());
		result.children.push_back(std::move(else_branch));
	}
	return result;
}
PA10AstNode PA10Parser::parse_switch_statement()
{
	consume_fixed(SimpleTokenType::KW_SWITCH);
	consume_fixed(SimpleTokenType::OP_LPAREN);
	PA10AstNode result = node(PA10NodeKind::SwitchStatement);
	result.children.push_back(parse_condition());
	consume_fixed(SimpleTokenType::OP_RPAREN);
	result.children.push_back(parse_statement());
	return result;
}
PA10AstNode PA10Parser::parse_iteration_statement()
{
	if (fixed(SimpleTokenType::KW_WHILE))
	{
		consume_fixed(SimpleTokenType::KW_WHILE);
		consume_fixed(SimpleTokenType::OP_LPAREN);
		PA10AstNode result = node(PA10NodeKind::WhileStatement);
		result.children.push_back(parse_condition());
		consume_fixed(SimpleTokenType::OP_RPAREN);
		result.children.push_back(parse_statement());
		return result;
	}
	if (fixed(SimpleTokenType::KW_DO))
	{
		consume_fixed(SimpleTokenType::KW_DO);
		PA10AstNode result = node(PA10NodeKind::DoStatement);
		result.children.push_back(parse_statement());
		consume_fixed(SimpleTokenType::KW_WHILE);
		consume_fixed(SimpleTokenType::OP_LPAREN);
		result.children.push_back(node(PA10NodeKind::Condition));
		result.children.back().children.push_back(parse_expression());
		consume_fixed(SimpleTokenType::OP_RPAREN);
		consume_fixed(SimpleTokenType::OP_SEMICOLON);
		return result;
	}
	consume_fixed(SimpleTokenType::KW_FOR);
	consume_fixed(SimpleTokenType::OP_LPAREN);
	PA10AstNode result = node(PA10NodeKind::ForStatement);
	PA10AstNode init = node(PA10NodeKind::ForInitStatement);
	if (declaration_start())
		init.children.push_back(parse_declaration());
	else if (!fixed(SimpleTokenType::OP_SEMICOLON))
	{
		init.children.push_back(parse_expression());
		consume_fixed(SimpleTokenType::OP_SEMICOLON);
	}
	else
		consume_fixed(SimpleTokenType::OP_SEMICOLON);
	result.children.push_back(std::move(init));
	if (!fixed(SimpleTokenType::OP_SEMICOLON))
		result.children.push_back(parse_condition());
	else
		result.children.push_back(node(PA10NodeKind::Condition));
	consume_fixed(SimpleTokenType::OP_SEMICOLON);
	if (!fixed(SimpleTokenType::OP_RPAREN))
	{
		PA10AstNode iteration = node(PA10NodeKind::Iteration);
		iteration.children.push_back(parse_expression());
		result.children.push_back(std::move(iteration));
	}
	consume_fixed(SimpleTokenType::OP_RPAREN);
	result.children.push_back(parse_statement());
	return result;
}
PA10AstNode PA10Parser::parse_try_block()
{
	consume_fixed(SimpleTokenType::KW_TRY);
	PA10AstNode result = node(PA10NodeKind::TryBlock);
	result.children.push_back(parse_compound_statement());
	while (fixed(SimpleTokenType::KW_CATCH))
	{
		consume_fixed(SimpleTokenType::KW_CATCH);
		consume_fixed(SimpleTokenType::OP_LPAREN);
		PA10AstNode handler = node(PA10NodeKind::Handler);
		PA10AstNode exception = node(PA10NodeKind::ExceptionDeclaration);
		if (fixed(SimpleTokenType::OP_DOTS))
		{
			const PA10Token ellipsis = consume_token();
			PA10AstNode leaf = node(PA10NodeKind::LeafFixed);
			leaf.has_token = true;
			leaf.token = ellipsis.fixed;
			leaf.token_spelling = intern(ellipsis.source);
			exception.children.push_back(std::move(leaf));
		}
		else
		{
			exception.children.push_back(parse_decl_specifier_seq());
			if (!fixed(SimpleTokenType::OP_RPAREN))
				exception.children.push_back(parse_declarator(true));
		}
		handler.children.push_back(std::move(exception));
		consume_fixed(SimpleTokenType::OP_RPAREN);
		handler.children.push_back(parse_compound_statement());
		result.children.push_back(std::move(handler));
	}
	if (result.children.size() == 1)
		fail("try block needs handler");
	return result;
}
PA10AstNode PA10Parser::parse_class_declaration(bool in_decl_specifier)
{
	const PA10ParserSupport::PA10ElaboratedSpecifierClassification classification = elaborated_specifier_classification();
	const bool forward_declaration = !classification.has_body && !classification.has_colon_clause;
	if (forward_declaration) {
		PA10AstNode result = node(PA10NodeKind::ClassForwardDeclaration); result.text = intern("<unnamed>");
		result.children.push_back(fixed_node(PA10NodeKind::ClassKey)); parse_attribute_specifiers(result);
		parse_class_name(result); name_scopes_.declare_type(result.producer_spelling);
		if (!in_decl_specifier) consume_fixed(SimpleTokenType::OP_SEMICOLON);
		return result;
	}
	PA10AstNode result = parse_class_specifier();
	if (!in_decl_specifier) { consume_fixed(SimpleTokenType::OP_SEMICOLON); result.source_end = position_; }
	return result;
}
PA10AstNode PA10Parser::parse_class_specifier()
{
	if (!(fixed(SimpleTokenType::KW_CLASS) || fixed(SimpleTokenType::KW_STRUCT) || fixed(SimpleTokenType::KW_UNION))) fail("expected class-key");
	PA10AstNode result = node(PA10NodeKind::ClassSpecifier); PA10AstNode key = fixed_node(PA10NodeKind::ClassKey);
	result.children.push_back(std::move(key)); parse_attribute_specifiers(result);
	parse_class_name(result); name_scopes_.declare_type(result.producer_spelling);
	if (fixed(SimpleTokenType::OP_COLON)) {
		consume_fixed(SimpleTokenType::OP_COLON);
		PA10AstNode base = node(PA10NodeKind::BaseClause);
		base.children.push_back(parse_base_specifier());
		while (fixed(SimpleTokenType::OP_COMMA)) { consume_fixed(SimpleTokenType::OP_COMMA); base.children.push_back(parse_base_specifier()); }
		result.children.push_back(std::move(base));
	}
	consume_fixed(SimpleTokenType::OP_LBRACE); enter();
	while (!fixed(SimpleTokenType::OP_RBRACE)) {
		if (at_end())
			fail("unterminated class specifier");
		result.children.push_back(parse_class_member());
	}
	consume_fixed(SimpleTokenType::OP_RBRACE); leave();
	result.source_end = position_;
	return result;
}
void PA10Parser::parse_class_name(PA10AstNode& result)
{
	if (!identifier())
	{
		result.text = intern("<unnamed>"); return;
	}
	const PA10Name name = parse_name(true);
	if (!name.global && name.parts.size() == 1 && !name.parts.front().has_template_id)
		result.producer_spelling = name.parts.front().spelling;
	else
	{
		result.global_name = name.global; result.name_parts = name.parts;
	}
}
PA10AstNode PA10Parser::parse_base_specifier()
{
	PA10AstNode result = node(PA10NodeKind::BaseSpecifier);
	if (fixed(SimpleTokenType::KW_VIRTUAL))
		result.children.push_back(fixed_node(PA10NodeKind::VirtualSpecifier));
	if (fixed(SimpleTokenType::KW_PUBLIC) || fixed(SimpleTokenType::KW_PROTECTED) || fixed(SimpleTokenType::KW_PRIVATE))
		result.children.push_back(fixed_node(PA10NodeKind::AccessSpecifier));
	if (fixed(SimpleTokenType::KW_VIRTUAL))
		result.children.push_back(fixed_node(PA10NodeKind::VirtualSpecifier));
	if (!name_start() && !fixed(SimpleTokenType::KW_DECLTYPE))
		fail("expected base name");
	result.children.push_back(name_node(PA10NodeKind::BaseName, parse_name(true, true)));
	if (fixed(SimpleTokenType::OP_DOTS))
		consume_fixed(SimpleTokenType::OP_DOTS);
	return result;
}
bool PA10Parser::alignment_argument_starts_type() const { return PA10ParserSupport::alignment_argument_starts_type(tokens_, name_scopes_, position_); }

PA10AlignmentSpecifier PA10Parser::parse_alignment_specifier()
{
	const std::size_t begin = position_;
	consume_fixed(SimpleTokenType::KW_ALIGNAS); consume_fixed(SimpleTokenType::OP_LPAREN); begin_non_angle();
	PA10AlignmentSpecifier result;
	result.source_begin = begin;
	result.argument_kind = alignment_argument_starts_type() ?
		PA10AlignmentArgumentKind::TypeId : PA10AlignmentArgumentKind::Expression;
	result.argument = result.argument_kind == PA10AlignmentArgumentKind::TypeId ?
		parse_type_id() : parse_assignment_expression();
	consume_fixed(SimpleTokenType::OP_RPAREN); end_non_angle();
	result.source_end = position_;
	return result;
}
void PA10Parser::parse_attribute_specifiers(PA10AstNode& owner)
{
	while (PA10ParserSupport::attribute_specifier_start(
		tokens_, delimiter_close_index_, position_))
	{
		if (fixed(SimpleTokenType::KW_ALIGNAS))
		{
			const std::size_t begin = ast_.alignment_specifiers.size();
			const PA10AlignmentSpecifier alignment = parse_alignment_specifier();
			if (owner.alignment_specifier_count == 0)
				owner.alignment_specifier_begin = begin;
			ast_.alignment_specifiers.push_back(std::move(alignment));
			++owner.alignment_specifier_count;
			continue;
		}
		std::size_t after = position_, consumed = 0;
		const bool valid = PA10ParserSupport::skip_attribute_specifier(
			tokens_, delimiter_close_index_, position_, &after, &consumed);
		for (std::size_t i = 0; i < consumed; ++i)
			charge();
		if (!valid)
			fail("unterminated attribute");
		position_ = after;
	}
}
PA10AstNode PA10Parser::parse_operator_name()
{
	const PA10Token keyword = consume_token();
	if (keyword.kind != PA10TokenKind::Fixed ||
		keyword.fixed != SimpleTokenType::KW_OPERATOR)
		fail("expected operator keyword");
	PA10AstNode result = node(PA10NodeKind::Identifier);
	result.unqualified_id_kind = PA10UnqualifiedIdKind::OperatorFunction;
	result.unqualified_id_token = keyword.fixed;
	append_operator_presentation(result, keyword.source);
	if (fixed(SimpleTokenType::OP_LSQUARE))
	{
		const PA10Token open = consume_token();
		if (!fixed(SimpleTokenType::OP_RSQUARE))
			fail("operator[] needs closing bracket");
		const PA10Token close = consume_token();
		result.operator_function_kind = PA10OperatorFunctionKind::Subscript;
		result.operator_token = open.fixed;
		append_operator_presentation(result, open.source);
		append_operator_presentation(result, close.source);
	}
	else if (fixed(SimpleTokenType::OP_LPAREN))
	{
		const PA10Token open = consume_token();
		if (!fixed(SimpleTokenType::OP_RPAREN))
			fail("operator() needs closing parenthesis");
		const PA10Token close = consume_token();
		result.operator_function_kind = PA10OperatorFunctionKind::Call;
		result.operator_token = open.fixed;
		append_operator_presentation(result, open.source);
		append_operator_presentation(result, close.source);
	}
	else if (rshift_operator())
	{
		const PA10Token first = consume_token();
		const PA10Token second = consume_token();
		result.operator_function_kind = PA10OperatorFunctionKind::Token;
		result.operator_token = SimpleTokenType::OP_RSHIFT;
		append_operator_presentation(result, first.source);
		append_operator_presentation(result, second.source);
	}
	else if (look().kind == PA10TokenKind::Fixed &&
		PA10ParserSupport::is_operator_function_token(look().fixed))
	{
		const PA10Token op = consume_token();
		result.operator_function_kind = op.fixed == SimpleTokenType::KW_NEW ?
			PA10OperatorFunctionKind::New :
			op.fixed == SimpleTokenType::KW_DELETE ?
			PA10OperatorFunctionKind::Delete :
			PA10OperatorFunctionKind::Token;
		result.operator_token = op.fixed;
		append_operator_presentation(result, op.source);
		if ((op.fixed == SimpleTokenType::KW_NEW ||
			op.fixed == SimpleTokenType::KW_DELETE) &&
			fixed(SimpleTokenType::OP_LSQUARE))
		{
			const PA10Token open = consume_token();
			if (!fixed(SimpleTokenType::OP_RSQUARE))
				fail("allocation operator[] needs closing bracket");
			const PA10Token close = consume_token();
			result.operator_function_kind = op.fixed == SimpleTokenType::KW_NEW ?
				PA10OperatorFunctionKind::NewArray :
				PA10OperatorFunctionKind::DeleteArray;
			append_operator_presentation(result, open.source);
			append_operator_presentation(result, close.source);
		}
	}
	else if (look().kind == PA10TokenKind::UserDefinedLiteral)
	{
		const PA10Token literal = consume_token();
		result.operator_function_kind = PA10OperatorFunctionKind::Literal;
		append_operator_presentation(result, literal.source);
	}
	else
	{
		const std::size_t begin = position_;
		PA10AstNode conversion_type = parse_type_id(true);
		result.operator_function_kind = PA10OperatorFunctionKind::Conversion;
		append_semantic_child(result, std::move(conversion_type));
		for (std::size_t i = begin; i < position_; ++i)
			append_operator_presentation(result, tokens_[i].source);
	}
	return result;
}
PA10AstNode PA10Parser::parse_paren_argument_list()
{
	consume_fixed(SimpleTokenType::OP_LPAREN);
	begin_non_angle();
	PA10AstNode result = node(PA10NodeKind::ParenArgumentList);
	if (!fixed(SimpleTokenType::OP_RPAREN))
	{
		result.children.push_back(parse_initializer_clause());
		while (fixed(SimpleTokenType::OP_COMMA))
		{
			consume_fixed(SimpleTokenType::OP_COMMA);
			result.children.push_back(parse_initializer_clause());
		}
	}
	consume_fixed(SimpleTokenType::OP_RPAREN);
	end_non_angle();
	return result;
}
PA10AstNode PA10Parser::parse_ctor_initializer()
{
	consume_fixed(SimpleTokenType::OP_COLON);
	PA10AstNode result = node(PA10NodeKind::CtorInitializer);
	while (true)
	{
		if (!name_start() && !fixed(SimpleTokenType::KW_DECLTYPE))
			fail("expected mem-initializer-id");
		PA10AstNode initializer = node(PA10NodeKind::MemInitializer);
		initializer.children.push_back(
			name_node(PA10NodeKind::MemInitializerId,
				parse_name(true, true)));
		if (fixed(SimpleTokenType::OP_LPAREN))
			initializer.children.push_back(parse_paren_argument_list());
		else if (fixed(SimpleTokenType::OP_LBRACE))
			initializer.children.push_back(parse_braced_init_list());
		else
			fail("mem-initializer needs an argument list");
		result.children.push_back(std::move(initializer));
		if (!fixed(SimpleTokenType::OP_COMMA))
			break;
		consume_fixed(SimpleTokenType::OP_COMMA);
	}
	return result;
}
void PA10Parser::skip_attribute_specifiers()
{
	std::size_t after = 0, consumed = 0;
	const bool valid = PA10ParserSupport::skip_attribute_specifiers(tokens_,
		delimiter_close_index_, position_, &after, &consumed);
	for (std::size_t i = 0; i < consumed; ++i) charge();
	if (!valid) fail("unterminated attribute");
	position_ = after;
}
PA10AstNode PA10Parser::parse_member_specifiers()
{
	PA10AstNode result = node(PA10NodeKind::MemberSpecifiers);
	skip_attribute_specifiers();
	while (fixed(SimpleTokenType::KW_INLINE) ||
		fixed(SimpleTokenType::KW_VIRTUAL) ||
		fixed(SimpleTokenType::KW_EXPLICIT) ||
		fixed(SimpleTokenType::KW_CONSTEXPR) ||
		fixed(SimpleTokenType::KW_FRIEND) ||
		fixed(SimpleTokenType::KW_STATIC))
	{
		result.children.push_back(fixed_node(PA10NodeKind::MemberSpecifier));
		skip_attribute_specifiers();
	}
	return result;
}
PA10AstNode PA10Parser::parse_special_member()
{
	PA10AstNode member_specifiers = parse_member_specifiers();
	PA10AstNode declarator = parse_declarator(false, true);
	PA10AstNode result = node(PA10NodeKind::SpecialMemberDeclaration);
	if (!member_specifiers.children.empty())
		result.children.push_back(std::move(member_specifiers));
	result.children.push_back(std::move(declarator));
	if (fixed(SimpleTokenType::OP_COLON))
		result.children.push_back(parse_ctor_initializer());
	if (fixed(SimpleTokenType::OP_LBRACE))
	{
		result.kind = PA10NodeKind::SpecialMemberDefinition;
		result.children.push_back(parse_compound_statement());
		return result;
	}
	if (fixed(SimpleTokenType::OP_ASS))
		result.children.push_back(parse_initializer());
	consume_fixed(SimpleTokenType::OP_SEMICOLON);
	result.kind = PA10NodeKind::SpecialMemberDeclaration;
	return result;
}
PA10AstNode PA10Parser::parse_class_member()
{
	if (fixed(SimpleTokenType::KW_PUBLIC) ||
		fixed(SimpleTokenType::KW_PROTECTED) ||
		fixed(SimpleTokenType::KW_PRIVATE))
	{
		PA10AstNode result = fixed_node(PA10NodeKind::AccessSpecifier);
		consume_fixed(SimpleTokenType::OP_COLON);
		return result;
	}
	if (fixed(SimpleTokenType::OP_SEMICOLON))
	{
		consume_fixed(SimpleTokenType::OP_SEMICOLON);
		return node(PA10NodeKind::EmptyDeclaration);
	}
	if (special_member_definition_start(true))
		return parse_special_member();
	if (fixed(SimpleTokenType::KW_USING) ||
		fixed(SimpleTokenType::KW_TEMPLATE) ||
		fixed(SimpleTokenType::KW_NAMESPACE) ||
		fixed(SimpleTokenType::KW_STATIC_ASSERT))
		return parse_declaration(true);
	return parse_elaborated_declaration_or_function(true);
}
PA10AstNode PA10Parser::parse_enum_declaration(bool in_decl_specifier)
{
	PA10AstNode result = parse_enum_specifier();
	if (!in_decl_specifier)
	{
		if (!fixed(SimpleTokenType::OP_SEMICOLON))
			fail("enum declaration needs semicolon");
		consume_fixed(SimpleTokenType::OP_SEMICOLON);
	}
	return result;
}
PA10AstNode PA10Parser::parse_enum_specifier()
{
	consume_fixed(SimpleTokenType::KW_ENUM);
	PA10AstNode result = node(PA10NodeKind::EnumSpecifier);
	if (fixed(SimpleTokenType::KW_CLASS) || fixed(SimpleTokenType::KW_STRUCT))
		result.children.push_back(fixed_node(PA10NodeKind::EnumKey));
	if (identifier())
	{
		const PA10Name name = parse_name(true);
		if (!name.global && name.parts.size() == 1 &&
			!name.parts.front().has_template_id)
		{
			result.producer_spelling = name.parts.front().spelling;
		}
		else
		{
			result.global_name = name.global;
			result.name_parts = name.parts;
		}
	}
	name_scopes_.declare_type(result.producer_spelling);
	if (fixed(SimpleTokenType::OP_COLON))
	{
		consume_fixed(SimpleTokenType::OP_COLON);
		result.children.push_back(parse_type_id());
	}
	if (!fixed(SimpleTokenType::OP_LBRACE))
		return result;
	consume_fixed(SimpleTokenType::OP_LBRACE);
	if (!fixed(SimpleTokenType::OP_RBRACE))
	{
		while (true)
		{
			if (!identifier())
				fail("expected enumerator");
			PA10AstNode enumerator = node(PA10NodeKind::Enumerator);
			const PA10Token name = consume_identifier_token();
			enumerator.producer_spelling = name.spelling;
			if (fixed(SimpleTokenType::OP_ASS))
			{
				consume_fixed(SimpleTokenType::OP_ASS);
				enumerator.children.push_back(parse_assignment_expression());
			}
			result.children.push_back(std::move(enumerator));
			if (!fixed(SimpleTokenType::OP_COMMA))
				break;
			consume_fixed(SimpleTokenType::OP_COMMA);
			if (fixed(SimpleTokenType::OP_RBRACE))
				break;
		}
	}
	consume_fixed(SimpleTokenType::OP_RBRACE);
	return result;
}
PA10AstNode PA10Parser::parse_template_declaration(bool in_class_member)
{
	consume_fixed(SimpleTokenType::KW_TEMPLATE);
	PA10AstNode result = node(PA10NodeKind::TemplateDeclaration);
	result.children.push_back(parse_template_parameter_clause());
	result.children.push_back(parse_declaration(in_class_member));
	return result;
}
PA10AstNode PA10Parser::parse_template_parameter_clause()
{
	consume_fixed(SimpleTokenType::OP_LT);
	begin_angle();
	PA10AstNode result = node(PA10NodeKind::TemplateParameterClause);
	if (!at_angle_close())
	{
		PA10AstNode list = node(PA10NodeKind::TemplateParameterList);
		list.children.push_back(parse_template_parameter());
		while (fixed(SimpleTokenType::OP_COMMA))
		{
			consume_fixed(SimpleTokenType::OP_COMMA);
			list.children.push_back(parse_template_parameter());
		}
		result.children.push_back(std::move(list));
	}
	close_angle();
	return result;
}
PA10AstNode PA10Parser::parse_template_parameter()
{
	if (fixed(SimpleTokenType::KW_TEMPLATE))
	{
		PA10AstNode result = node(PA10NodeKind::TypeParameter);
		result.children.push_back(node(PA10NodeKind::TemplateTemplateParameter));
		consume_fixed(SimpleTokenType::KW_TEMPLATE);
		result.children.push_back(parse_template_parameter_clause());
		result.children.push_back(fixed_node(PA10NodeKind::ParameterKey));
		if (fixed(SimpleTokenType::OP_DOTS))
			result.children.push_back(fixed_node(PA10NodeKind::ParameterPack));
		if (identifier())
		{
			PA10AstNode id = node(PA10NodeKind::Identifier);
			const PA10Token name = consume_identifier_token();
			id.producer_spelling = name.spelling;
			result.children.push_back(std::move(id));
		}
		if (fixed(SimpleTokenType::OP_ASS))
		{
			consume_fixed(SimpleTokenType::OP_ASS);
			PA10AstNode argument = node(PA10NodeKind::DefaultTemplateArgument);
			argument.children.push_back(parse_type_id());
			result.children.push_back(std::move(argument));
		}
		return result;
	}
	if (fixed(SimpleTokenType::KW_CLASS) ||
		fixed(SimpleTokenType::KW_TYPENAME))
	{
		PA10AstNode result = node(PA10NodeKind::TypeParameter);
		result.children.push_back(fixed_node(PA10NodeKind::ParameterKey));
		if (fixed(SimpleTokenType::OP_DOTS))
			result.children.push_back(fixed_node(PA10NodeKind::ParameterPack));
		if (identifier())
		{
			PA10AstNode id = node(PA10NodeKind::Identifier);
			const PA10Token name = consume_identifier_token();
			id.producer_spelling = name.spelling;
			result.children.push_back(std::move(id));
		}
		if (fixed(SimpleTokenType::OP_ASS))
		{
			consume_fixed(SimpleTokenType::OP_ASS);
			PA10AstNode argument = node(PA10NodeKind::DefaultTemplateArgument);
			argument.children.push_back(parse_type_id());
			result.children.push_back(std::move(argument));
		}
		return result;
	}
	PA10AstNode result = node(PA10NodeKind::NonTypeTemplateParameter);
	PA10AstNode spec = parse_decl_specifier_seq();
	const bool anonymous_builtin = spec.children.size() == 1 &&
		spec.children.front().kind == PA10NodeKind::DeclSpecifier &&
		spec.children.front().has_token &&
		PA10ParserSupport::is_type_keyword(spec.children.front().token);
	result.children.push_back(std::move(spec));
	bool has_parameter_pack = false;
	if (fixed(SimpleTokenType::OP_DOTS))
	{
		result.children.push_back(fixed_node(PA10NodeKind::ParameterPack));
		has_parameter_pack = true;
	}
	bool has_declarator = false;
	if (name_start() || fixed(SimpleTokenType::OP_STAR) ||
		fixed(SimpleTokenType::OP_AMP) || fixed(SimpleTokenType::OP_LAND) ||
		fixed(SimpleTokenType::OP_LPAREN))
	{
		result.children.push_back(parse_declarator(true));
		has_declarator = true;
	}
	if (fixed(SimpleTokenType::OP_ASS))
	{
		consume_fixed(SimpleTokenType::OP_ASS);
		PA10AstNode argument = node(PA10NodeKind::DefaultTemplateArgument);
		argument.children.push_back(parse_assignment_expression());
		if (anonymous_builtin && !has_parameter_pack && !has_declarator &&
			argument.children.size() == 1 &&
			argument.children.front().kind == PA10NodeKind::Literal)
			argument.default_template_argument_form =
				PA10DefaultTemplateArgumentForm::AnonymousNonTypeLiteral;
		result.children.push_back(std::move(argument));
	}
	return result;
}
} // namespace
PA10Ast parse_pa10_ast(const PPTokenBuffer& input)
{
	std::vector<PA10Token> tokens;
	if (!PA10ParserSupport::collect_tokens(input, tokens))
		throw std::runtime_error("invalid phase-7 token");
	PA10Parser parser(tokens, input.spellings);
	return parser.parse();
}
